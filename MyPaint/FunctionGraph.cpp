/**
 * @file FunctionGraph.cpp
 * @brief 다항 함수 그래프 그리기 구현 (GDI+ 안티앨리어싱)
 */
#include "pch.h"
#include "FunctionGraph.h"

#include <cmath>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    using namespace Gdiplus;

    Color ToColor(COLORREF c, BYTE alpha = 255)
    {
        return Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
    }

    double Luminance(COLORREF c)
    {
        return 0.299 * GetRValue(c) + 0.587 * GetGValue(c) + 0.114 * GetBValue(c);
    }

    /** @brief 1, 2, 5 × 10^k 중 raw 이상인 가장 작은 값 */
    double NiceStep(double raw)
    {
        if (!(raw > 0.0) || !std::isfinite(raw))
            return 1.0;
        const double exponent = std::floor(std::log10(raw));
        const double base = std::pow(10.0, exponent);
        const double f = raw / base;
        const double nice = f <= 1.0 ? 1.0 : f <= 2.0 ? 2.0 : f <= 5.0 ? 5.0 : 10.0;
        return nice * base;
    }

    /** @brief 눈금 간격을 표시하는 데 필요한 소수 자릿수 */
    int DecimalsFor(double step)
    {
        return std::clamp(static_cast<int>(std::ceil(-std::log10(step) - 1e-9)), 0, 8);
    }

    /** @brief 그래프 좌표 ↔ 그림 좌표 */
    struct Mapper
    {
        RectF plot;
        double xMin, xMax, yMin, yMax;
        float X(double x) const { return plot.X + static_cast<float>((x - xMin) / (xMax - xMin) * plot.Width); }
        float Y(double y) const { return plot.GetBottom() - static_cast<float>((y - yMin) / (yMax - yMin) * plot.Height); }
    };

    /** @brief 글자 크기 재기 */
    SizeF Measure(Graphics& g, const Font& font, const CString& text)
    {
        RectF box;
        StringFormat fmt(StringFormat::GenericTypographic());
        fmt.SetFormatFlags(fmt.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
        g.MeasureString(text, text.GetLength(), &font, PointF(0, 0), &fmt, &box);
        return SizeF(box.Width, box.Height);
    }

    void DrawText(Graphics& g, const Font& font, const Brush& brush, const CString& text, float x, float y)
    {
        StringFormat fmt(StringFormat::GenericTypographic());
        g.DrawString(text, text.GetLength(), &font, PointF(x, y), &fmt, &brush);
    }

    /** @brief 글자 뒤에 반투명 바탕 상자 (격자·곡선 위에서도 읽히게) */
    void DrawLabel(Graphics& g, const Font& font, const Brush& textBrush, const Brush* backBrush,
                   const CString& text, float x, float y, float pad)
    {
        const SizeF sz = Measure(g, font, text);
        if (backBrush)
            g.FillRectangle(backBrush, x - pad, y - pad * 0.5f, sz.Width + pad * 2, sz.Height + pad);
        DrawText(g, font, textBrush, text, x, y);
    }

    void DrawArrowHead(Graphics& g, const Brush& brush, PointF tip, PointF dir, float size)
    {
        const PointF n(-dir.Y, dir.X);
        PointF pts[3] = {
            tip,
            PointF(tip.X - dir.X * size + n.X * size * 0.45f, tip.Y - dir.Y * size + n.Y * size * 0.45f),
            PointF(tip.X - dir.X * size - n.X * size * 0.45f, tip.Y - dir.Y * size - n.Y * size * 0.45f),
        };
        g.FillPolygon(&brush, pts, 3);
    }
}

namespace graph
{
    void AutoYRange(const Polynomial& f, double xMin, double xMax, double& yMin, double& yMax)
    {
        constexpr int kSamples = 2000;
        double lo = f.Eval(xMin), hi = lo;
        for (int i = 1; i <= kSamples; ++i)
        {
            const double y = f.Eval(xMin + (xMax - xMin) * i / kSamples);
            lo = std::min(lo, y);
            hi = std::max(hi, y);
        }
        if (!std::isfinite(lo) || !std::isfinite(hi))
        {
            lo = -1e15;
            hi = 1e15;
        }
        const double span = hi - lo;
        if (span < 1e-9 * std::max(1.0, std::fabs(hi)))
        {
            // 상수 함수: 값을 가운데에 두고 위아래로 여유
            const double margin = std::max(1.0, std::fabs(hi) * 0.5);
            yMin = lo - margin;
            yMax = hi + margin;
            return;
        }
        yMin = lo - span * 0.08;
        yMax = hi + span * 0.08;
    }

    bool Render(const Polynomial& f, GraphOptions& o, CImage& out)
    {
        const int w = std::clamp<int>(o.size.cx, 16, kMaxSide);
        const int h = std::clamp<int>(o.size.cy, 16, kMaxSide);
        if (!(o.xMax > o.xMin))
            return false;
        if (o.autoY)
            AutoYRange(f, o.xMin, o.xMax, o.yMin, o.yMax);
        if (!(o.yMax > o.yMin))
            return false;

        // 32비트 위→아래 DIB (GDI+가 미리 곱한 알파로 바로 그림)
        out.Destroy();
        CImage temp;
        if (!temp.Create(w, -h, 32, CImage::createAlphaChannel))
            return false;
        out.Attach(temp.Detach(), CImage::DIBOR_TOPDOWN);

        {
            Bitmap bmp(w, h, out.GetPitch(), PixelFormat32bppPARGB, static_cast<BYTE*>(out.GetBits()));
            Graphics g(&bmp);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(PixelOffsetModeHalf);
            g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
            g.Clear(o.transparent ? Color(0, 0, 0, 0) : ToColor(o.background));

            // 색: 배경이 어두우면 밝은 축, 밝으면 어두운 축
            const bool darkBack = !o.transparent && Luminance(o.background) < 110.0;
            const COLORREF axisColor = darkBack ? RGB(220, 220, 220) : RGB(70, 70, 70);
            const Color gridColor = ToColor(axisColor, darkBack ? 50 : 40);
            const Color labelBack = o.transparent ? Color(0, 0, 0, 0) : ToColor(o.background, 210);

            // 글자 크기는 그림 크기에 비례
            const float fontPx = static_cast<float>(std::clamp(std::lround(std::min(w, h) * 0.03), 10L, 28L));
            FontFamily family(L"Segoe UI");
            FontFamily mathFamily(L"Cambria Math");
            const FontFamily* textFamily = family.IsAvailable() ? &family : FontFamily::GenericSansSerif();
            const FontFamily* mathFont = mathFamily.IsAvailable() ? &mathFamily : textFamily;
            Font font(textFamily, fontPx, FontStyleRegular, UnitPixel);
            Font italic(mathFont, fontPx * 1.1f, FontStyleItalic, UnitPixel);
            Font formulaFont(mathFont, fontPx * 1.25f, FontStyleRegular, UnitPixel);
            SolidBrush axisBrush(ToColor(axisColor));
            SolidBrush backBrush(labelBack);
            const Brush* back = o.transparent ? nullptr : &backBrush;

            const float pad = fontPx * 0.9f;
            Mapper m{ RectF(pad, pad, w - pad * 2, h - pad * 2), o.xMin, o.xMax, o.yMin, o.yMax };
            if (m.plot.Width < 4 || m.plot.Height < 4)
                m.plot = RectF(0, 0, static_cast<REAL>(w), static_cast<REAL>(h));

            // 눈금 간격: 약 글자 6개 너비마다
            const double target = std::max(48.0, fontPx * 5.5);
            const double stepX = NiceStep((o.xMax - o.xMin) * target / m.plot.Width);
            const double stepY = NiceStep((o.yMax - o.yMin) * target / m.plot.Height);

            // 축 위치: 0이 범위 안이면 0, 아니면 가까운 가장자리
            const bool xAxisInside = o.yMin <= 0.0 && 0.0 <= o.yMax;
            const bool yAxisInside = o.xMin <= 0.0 && 0.0 <= o.xMax;
            const float axisY = xAxisInside ? m.Y(0.0) : (o.yMin > 0.0 ? m.plot.GetBottom() : m.plot.Y);
            const float axisX = yAxisInside ? m.X(0.0) : (o.xMin > 0.0 ? m.plot.X : m.plot.GetRight());

            const double firstX = std::ceil(o.xMin / stepX - 1e-9) * stepX;
            const double firstY = std::ceil(o.yMin / stepY - 1e-9) * stepY;

            // 격자
            if (o.showGrid)
            {
                Pen gridPen(gridColor, 1.0f);
                for (double x = firstX; x <= o.xMax + stepX * 1e-9; x += stepX)
                    g.DrawLine(&gridPen, m.X(x), m.plot.Y, m.X(x), m.plot.GetBottom());
                for (double y = firstY; y <= o.yMax + stepY * 1e-9; y += stepY)
                    g.DrawLine(&gridPen, m.plot.X, m.Y(y), m.plot.GetRight(), m.Y(y));
            }

            // 축, 눈금, 숫자
            if (o.showAxes)
            {
                const float axisWidth = std::max(1.0f, fontPx / 12.0f);
                Pen axisPen(ToColor(axisColor), axisWidth);
                const float arrow = fontPx * 0.7f;
                g.DrawLine(&axisPen, m.plot.X - pad * 0.5f, axisY, m.plot.GetRight() + pad * 0.5f - arrow * 0.5f, axisY);
                g.DrawLine(&axisPen, axisX, m.plot.GetBottom() + pad * 0.5f, axisX, m.plot.Y - pad * 0.5f + arrow * 0.5f);
                DrawArrowHead(g, axisBrush, PointF(m.plot.GetRight() + pad * 0.5f, axisY), PointF(1, 0), arrow);
                DrawArrowHead(g, axisBrush, PointF(axisX, m.plot.Y - pad * 0.5f), PointF(0, -1), arrow);

                // 축 이름
                const SizeF xName = Measure(g, italic, _T("x"));
                const float xNameY = axisY + fontPx * 0.25f + xName.Height < h ? axisY + fontPx * 0.25f : axisY - xName.Height - fontPx * 0.25f;
                DrawText(g, italic, axisBrush, _T("x"), w - xName.Width - 2.0f, xNameY);
                const float yNameX = axisX + fontPx * 0.45f + Measure(g, italic, _T("y")).Width < w ? axisX + fontPx * 0.45f : axisX - fontPx * 1.0f;
                DrawText(g, italic, axisBrush, _T("y"), yNameX, 1.0f);

                const float tick = fontPx * 0.3f;
                const int decX = DecimalsFor(stepX);
                const int decY = DecimalsFor(stepY);
                const bool origin = xAxisInside && yAxisInside;
                // x 눈금: 축 아래에 숫자 (축이 아래 가장자리면 위에)
                for (double x = firstX; x <= o.xMax + stepX * 1e-9; x += stepX)
                {
                    const float px = m.X(x);
                    g.DrawLine(&axisPen, px, axisY - tick, px, axisY + tick);
                    if (!o.showLabels || (origin && std::fabs(x) < stepX * 1e-6))
                        continue;
                    const CString text = FormatNumber(x, decX, true);
                    const SizeF sz = Measure(g, font, text);
                    float ty = axisY + tick + 1.0f;
                    if (ty + sz.Height > h)
                        ty = axisY - tick - sz.Height - 1.0f;
                    const float tx = px - sz.Width / 2;
                    if (tx < 0 || tx + sz.Width > w)
                        continue;
                    DrawLabel(g, font, axisBrush, back, text, tx, ty, 1.5f);
                }
                // y 눈금: 축 왼쪽에 숫자 (축이 왼쪽 가장자리면 오른쪽에)
                for (double y = firstY; y <= o.yMax + stepY * 1e-9; y += stepY)
                {
                    const float py = m.Y(y);
                    g.DrawLine(&axisPen, axisX - tick, py, axisX + tick, py);
                    if (!o.showLabels || (origin && std::fabs(y) < stepY * 1e-6))
                        continue;
                    const CString text = FormatNumber(y, decY, true);
                    const SizeF sz = Measure(g, font, text);
                    float tx = axisX - tick - sz.Width - 2.0f;
                    if (tx < 0)
                        tx = axisX + tick + 2.0f;
                    const float ty = py - sz.Height / 2;
                    if (ty < 0 || ty + sz.Height > h || tx + sz.Width > w)
                        continue;
                    DrawLabel(g, font, axisBrush, back, text, tx, ty, 1.5f);
                }
                if (origin && o.showLabels)
                {
                    const SizeF sz = Measure(g, font, _T("0"));
                    DrawLabel(g, font, axisBrush, back, _T("0"), axisX - sz.Width - tick, axisY + tick, 1.5f);
                }
            }

            // 곡선: 그림 너비의 2배로 표본을 잡아 꺾은선으로
            {
                Pen curvePen(ToColor(o.curveColor), static_cast<REAL>(std::max(1, o.lineWidth)));
                curvePen.SetLineJoin(LineJoinRound);
                curvePen.SetStartCap(LineCapRound);
                curvePen.SetEndCap(LineCapRound);
                g.SetClip(RectF(0, 0, static_cast<REAL>(w), static_cast<REAL>(h)));

                const int samples = std::max(64, static_cast<int>(m.plot.Width * 2));
                std::vector<PointF> pts;
                pts.reserve(samples + 1);
                const float limitLo = -static_cast<float>(h) * 2, limitHi = static_cast<float>(h) * 3;
                for (int i = 0; i <= samples; ++i)
                {
                    const double x = o.xMin + (o.xMax - o.xMin) * i / samples;
                    double py = m.Y(f.Eval(x));
                    if (!std::isfinite(py))
                        py = f.Eval(x) > 0 ? limitLo : limitHi;
                    // 화면 밖은 잘라도 연속 함수라 모양이 바뀌지 않음 (GDI+ 좌표 넘침 방지)
                    pts.emplace_back(m.X(x), static_cast<REAL>(std::clamp<double>(py, limitLo, limitHi)));
                }
                g.DrawLines(&curvePen, pts.data(), static_cast<INT>(pts.size()));
                g.ResetClip();
            }

            // 근·극값
            if (o.showPoints && !f.IsConstant())
            {
                struct Mark { double x, y; int kind; }; // 0: 근, 1: 극대, 2: 극소
                std::vector<Mark> marks;
                for (double r : f.RealRoots(o.xMin, o.xMax))
                    marks.push_back({ r, 0.0, 0 });
                for (const auto& e : f.Extrema(o.xMin, o.xMax))
                {
                    // 극값이 근이기도 하면(중근) 한 번만
                    if (std::fabs(e.y) < 1e-9 * std::max(1.0, std::fabs(o.yMax - o.yMin)))
                        continue;
                    marks.push_back({ e.x, e.y, e.maximum ? 1 : 2 });
                }
                const float radius = std::max(3.5f, o.lineWidth * 0.8f + 2.5f);
                SolidBrush dotBrush(ToColor(o.curveColor));
                Pen ringPen(o.transparent ? Color(255, 255, 255, 255) : ToColor(o.background), 1.5f);
                int drawn = 0;
                for (const Mark& mk : marks)
                {
                    if (mk.y < o.yMin || mk.y > o.yMax || ++drawn > 16)
                        continue;
                    const float px = m.X(mk.x), py = m.Y(mk.y);
                    g.FillEllipse(&dotBrush, px - radius, py - radius, radius * 2, radius * 2);
                    g.DrawEllipse(&ringPen, px - radius, py - radius, radius * 2, radius * 2);
                    if (!o.showLabels)
                        continue;
                    const CString text = _T("(") + FormatNumber(mk.x, 2, true) + _T(", ") + FormatNumber(mk.y, 2, true) + _T(")");
                    const SizeF sz = Measure(g, font, text);
                    float tx = px + radius + 2.0f;
                    float ty = mk.kind == 1 ? py - radius - sz.Height - 1.0f   // 극대: 위
                             : mk.kind == 2 ? py + radius + 1.0f               // 극소: 아래
                                            : py - radius - sz.Height - 1.0f;  // 근: 위 (아래는 x 눈금 숫자)
                    tx = std::clamp(tx, 2.0f, std::max(2.0f, w - sz.Width - 2.0f));
                    ty = std::clamp(ty, 2.0f, std::max(2.0f, h - sz.Height - 2.0f));
                    DrawLabel(g, font, dotBrush, back, text, tx, ty, 2.0f);
                }
            }

            // 수식
            if (o.showFormula)
            {
                const CString text = _T("y = ") + f.ToString();
                SolidBrush textBrush(ToColor(o.curveColor));
                DrawLabel(g, formulaFont, textBrush, back, text, m.plot.X + fontPx * 0.3f, m.plot.Y + fontPx * 0.2f, 3.0f);
            }

            g.Flush(FlushIntentionSync);
        }
        return true;
    }
}
