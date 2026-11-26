/**
 * @file Polynomial.cpp
 * @brief 다항식 해석·계산·근 찾기 구현
 */
#include "pch.h"
#include "Polynomial.h"

#include <cmath>
#include <cwchar>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static_assert(sizeof(TCHAR) == sizeof(wchar_t), "유니코드 빌드가 필요합니다");

namespace
{
    using Coeffs = std::vector<double>;

    constexpr wchar_t kMinusSign = 0x2212; // −

    double MaxAbs(const Coeffs& c)
    {
        double m = 0.0;
        for (double v : c)
            m = std::max(m, std::fabs(v));
        return m;
    }

    /** @brief 계산 오차로 생긴 아주 작은 계수를 0으로, 최고차 0 계수 제거 */
    void Normalize(Coeffs& c)
    {
        const double limit = MaxAbs(c) * 1e-12;
        for (double& v : c)
            if (std::fabs(v) <= limit)
                v = 0.0;
        while (c.size() > 1 && c.back() == 0.0)
            c.pop_back();
        if (c.empty())
            c.push_back(0.0);
    }

    /** @brief 해석 오류: 메시지와 위치 */
    struct ParseError
    {
        CString message;
    };

    /**
     * @brief 재귀 하강 해석기. 해석하면서 바로 다항식 계수로 계산합니다.
     *
     *   expr    := term (('+' | '-') term)*
     *   term    := unary (('*' | '/' | 생략) power)*
     *   unary   := ('+' | '-') unary | power
     *   power   := primary (위첨자 | '^' unary)?
     *   primary := 숫자 | 'x' | '(' expr ')'
     */
    class Parser
    {
    public:
        Parser(const CString& text, int offset) : m_text(text), m_pos(offset) {}

        Coeffs Run()
        {
            Coeffs result = Expr();
            SkipSpace();
            if (!AtEnd())
            {
                if (Peek() == L')')
                    Fail(_T("여는 괄호 없이 ')'가 있습니다"));
                Fail(_T("수식을 이해할 수 없습니다"));
            }
            return result;
        }

    private:
        bool AtEnd() const { return m_pos >= m_text.GetLength(); }
        wchar_t Peek() const { return AtEnd() ? L'\0' : m_text[m_pos]; }
        void SkipSpace()
        {
            while (!AtEnd() && (Peek() == L' ' || Peek() == L'\t'))
                ++m_pos;
        }

        [[noreturn]] void Fail(LPCTSTR what) const
        {
            CString msg;
            if (AtEnd())
                msg.Format(_T("%s (수식 끝)"), what);
            else
                msg.Format(_T("%s (%d번째 글자 '%c')"), what, m_pos + 1, Peek());
            throw ParseError{ msg };
        }

        static bool IsMinus(wchar_t ch) { return ch == L'-' || ch == kMinusSign; }
        static bool IsSuperscriptDigit(wchar_t ch, int& digit)
        {
            switch (ch)
            {
            case 0x2070: digit = 0; return true;
            case 0x00B9: digit = 1; return true;
            case 0x00B2: digit = 2; return true;
            case 0x00B3: digit = 3; return true;
            default:
                if (ch >= 0x2074 && ch <= 0x2079) { digit = ch - 0x2070; return true; }
                return false;
            }
        }
        static bool StartsPrimary(wchar_t ch)
        {
            return (ch >= L'0' && ch <= L'9') || ch == L'.' || ch == L'x' || ch == L'X' || ch == L'(';
        }

        Coeffs CheckedMul(const Coeffs& a, const Coeffs& b) const
        {
            if ((a.size() - 1) + (b.size() - 1) > static_cast<size_t>(Polynomial::kMaxDegree))
            {
                CString msg;
                msg.Format(_T("차수가 너무 높습니다 (최대 %d차)"), Polynomial::kMaxDegree);
                Fail(msg);
            }
            Coeffs r(a.size() + b.size() - 1, 0.0);
            for (size_t i = 0; i < a.size(); ++i)
                for (size_t j = 0; j < b.size(); ++j)
                    r[i + j] += a[i] * b[j];
            Normalize(r);
            return r;
        }

        static Coeffs AddSub(Coeffs a, const Coeffs& b, double sign)
        {
            if (a.size() < b.size())
                a.resize(b.size(), 0.0);
            for (size_t i = 0; i < b.size(); ++i)
                a[i] += sign * b[i];
            Normalize(a);
            return a;
        }

        Coeffs Expr()
        {
            Coeffs left = Term();
            for (;;)
            {
                SkipSpace();
                const wchar_t ch = Peek();
                if (ch == L'+' || IsMinus(ch))
                {
                    ++m_pos;
                    left = AddSub(std::move(left), Term(), ch == L'+' ? 1.0 : -1.0);
                }
                else
                    return left;
            }
        }

        Coeffs Term()
        {
            Coeffs left = Unary();
            for (;;)
            {
                SkipSpace();
                const wchar_t ch = Peek();
                if (ch == L'*' || ch == 0x00D7 /* × */ || ch == 0x00B7 /* · */)
                {
                    ++m_pos;
                    left = CheckedMul(left, Unary());
                }
                else if (ch == L'/' || ch == 0x00F7 /* ÷ */)
                {
                    ++m_pos;
                    SkipSpace();
                    const int at = m_pos;
                    Coeffs d = Unary();
                    if (d.size() > 1)
                    {
                        m_pos = at;
                        Fail(_T("x가 들어간 식으로는 나눌 수 없습니다 (다항식만 그릴 수 있음)"));
                    }
                    if (d[0] == 0.0)
                    {
                        m_pos = at;
                        Fail(_T("0으로 나눌 수 없습니다"));
                    }
                    for (double& v : left)
                        v /= d[0];
                }
                else if (StartsPrimary(ch))
                {
                    left = CheckedMul(left, Power()); // 곱셈 기호 생략: 2x, x(x+1), (x-1)(x+2)
                }
                else
                    return left;
            }
        }

        Coeffs Unary()
        {
            SkipSpace();
            const wchar_t ch = Peek();
            if (ch == L'+')
            {
                ++m_pos;
                return Unary();
            }
            if (IsMinus(ch))
            {
                ++m_pos;
                Coeffs v = Unary();
                for (double& c : v)
                    c = -c;
                return v;
            }
            return Power();
        }

        Coeffs Power()
        {
            Coeffs base = Primary();
            SkipSpace();
            int exponent = -1;
            int digit = 0;
            if (IsSuperscriptDigit(Peek(), digit))
            {
                exponent = 0;
                while (IsSuperscriptDigit(Peek(), digit))
                {
                    exponent = std::min(exponent * 10 + digit, 1000);
                    ++m_pos;
                }
            }
            else if (Peek() == L'^')
            {
                ++m_pos;
                SkipSpace();
                const int at = m_pos;
                Coeffs e = Unary();
                const double value = e[0];
                if (e.size() > 1 || value < 0.0 || value != std::floor(value) || value > 1000.0)
                {
                    m_pos = at;
                    Fail(_T("지수는 0 이상의 정수만 쓸 수 있습니다"));
                }
                exponent = static_cast<int>(value);
            }
            if (exponent < 0)
                return base;

            Coeffs result{ 1.0 };
            for (int i = 0; i < exponent; ++i)
                result = CheckedMul(result, base);
            return result;
        }

        Coeffs Primary()
        {
            SkipSpace();
            const wchar_t ch = Peek();
            if ((ch >= L'0' && ch <= L'9') || ch == L'.')
            {
                const int start = m_pos;
                bool dot = false;
                while (!AtEnd() && ((Peek() >= L'0' && Peek() <= L'9') || (Peek() == L'.' && !dot)))
                {
                    dot = dot || Peek() == L'.';
                    ++m_pos;
                }
                const CString digits = m_text.Mid(start, m_pos - start);
                if (digits == _T("."))
                {
                    m_pos = start;
                    Fail(_T("숫자가 올바르지 않습니다"));
                }
                return Coeffs{ std::wcstod(digits, nullptr) };
            }
            if (ch == L'x' || ch == L'X')
            {
                ++m_pos;
                return Coeffs{ 0.0, 1.0 };
            }
            if (ch == L'(')
            {
                ++m_pos;
                Coeffs inner = Expr();
                SkipSpace();
                if (Peek() != L')')
                    Fail(_T("닫는 괄호 ')'가 필요합니다"));
                ++m_pos;
                return inner;
            }
            if (AtEnd())
                Fail(_T("수식이 덜 끝났습니다"));
            Fail(_T("알 수 없는 글자입니다"));
        }

        const CString& m_text;
        int m_pos;
    };

    /** @brief x 근처에서 각 항의 크기 합: 근 판정 허용 오차의 기준 */
    double TermScale(const Coeffs& c, double x)
    {
        double sum = 0.0;
        double power = 1.0;
        for (double v : c)
        {
            sum += std::fabs(v) * power;
            power *= std::fabs(x);
        }
        return sum;
    }

    void SortUnique(std::vector<double>& v)
    {
        std::sort(v.begin(), v.end());
        std::vector<double> out;
        for (double x : v)
            if (out.empty() || std::fabs(x - out.back()) > 1e-7 * std::max(1.0, std::fabs(x)))
                out.push_back(x);
        v.swap(out);
    }
}

// ---------------------------------------------------------------------------

Polynomial::Polynomial(std::vector<double> coeffs) : m_c(std::move(coeffs))
{
    Trim();
}

void Polynomial::Trim()
{
    Normalize(m_c);
}

bool Polynomial::Parse(const CString& text, Polynomial& out, CString& error)
{
    // "y =", "f(x) =" 같은 왼쪽 부분은 건너뜀
    int start = text.ReverseFind(L'=');
    start = start < 0 ? 0 : start + 1;
    CString rest = text.Mid(start);
    rest.Trim();
    if (rest.IsEmpty())
    {
        error = _T("수식을 입력하세요");
        return false;
    }
    try
    {
        Parser parser(text, start);
        out = Polynomial(parser.Run());
        error.Empty();
        return true;
    }
    catch (const ParseError& e)
    {
        error = e.message;
        return false;
    }
}

double Polynomial::Eval(double x) const
{
    double y = 0.0;
    for (auto it = m_c.rbegin(); it != m_c.rend(); ++it)
        y = y * x + *it;
    return y;
}

Polynomial Polynomial::Derivative() const
{
    if (m_c.size() <= 1)
        return Polynomial();
    std::vector<double> d(m_c.size() - 1);
    for (size_t i = 1; i < m_c.size(); ++i)
        d[i - 1] = m_c[i] * static_cast<double>(i);
    return Polynomial(std::move(d));
}

CString Polynomial::ToString() const
{
    static const wchar_t kSup[] = { 0x2070, 0x00B9, 0x00B2, 0x00B3, 0x2074, 0x2075, 0x2076, 0x2077, 0x2078, 0x2079 };
    CString s;
    for (int i = Degree(); i >= 0; --i)
    {
        const double c = m_c[i];
        if (c == 0.0 && !(i == 0 && s.IsEmpty()))
            continue;
        const double a = std::fabs(c);
        if (s.IsEmpty())
        {
            if (c < 0.0)
                s += kMinusSign;
        }
        else
        {
            s += _T(" ");
            s += c < 0.0 ? kMinusSign : L'+';
            s += _T(" ");
        }
        const CString num = FormatNumber(a, 6);
        if (i == 0 || num != _T("1"))
            s += num;
        if (i >= 1)
            s += _T("x");
        if (i >= 2)
        {
            CString digits;
            digits.Format(_T("%d"), i);
            for (int k = 0; k < digits.GetLength(); ++k)
                s += kSup[digits[k] - L'0'];
        }
    }
    return s;
}

std::vector<double> Polynomial::RealRoots(double a, double b) const
{
    std::vector<double> roots;
    if (a > b || IsConstant())
        return roots; // 상수 함수: 근이 없거나(0이 아님) 모든 점(0)이라 표시하지 않음

    if (Degree() == 1)
    {
        const double r = -m_c[0] / m_c[1];
        if (r >= a && r <= b)
            roots.push_back(r);
        return roots;
    }

    // 임계점으로 나눈 각 구간에서 함수는 단조
    std::vector<double> points{ a };
    for (double c : Derivative().RealRoots(a, b))
        points.push_back(c);
    points.push_back(b);

    auto nearZero = [&](double x) { return std::fabs(Eval(x)) <= 1e-9 * TermScale(m_c, x); };

    for (size_t i = 0; i < points.size(); ++i)
    {
        if (nearZero(points[i]))
            roots.push_back(points[i]); // 끝점 또는 임계점에서 0 (중근 포함)
        if (i + 1 == points.size())
            break;
        double lo = points[i], hi = points[i + 1];
        double flo = Eval(lo), fhi = Eval(hi);
        if (flo == 0.0 || fhi == 0.0 || (flo < 0.0) == (fhi < 0.0))
            continue;
        for (int iter = 0; iter < 200; ++iter)
        {
            const double mid = 0.5 * (lo + hi);
            if (mid <= lo || mid >= hi)
                break;
            const double fm = Eval(mid);
            if (fm == 0.0)
            {
                lo = hi = mid;
                break;
            }
            if ((fm < 0.0) == (flo < 0.0))
            {
                lo = mid;
                flo = fm;
            }
            else
                hi = mid;
        }
        roots.push_back(0.5 * (lo + hi));
    }
    SortUnique(roots);
    return roots;
}

std::vector<Polynomial::Extremum> Polynomial::Extrema(double a, double b) const
{
    std::vector<Extremum> result;
    const Polynomial d = Derivative();
    const std::vector<double> crit = d.RealRoots(a, b);
    for (size_t i = 0; i < crit.size(); ++i)
    {
        const double c = crit[i];
        if (c <= a || c >= b)
            continue; // 구간 끝은 극값으로 보지 않음
        // 이웃 임계점(또는 구간 끝)과의 가운데에서 도함수 부호 비교
        const double left = 0.5 * (c + (i == 0 ? a : crit[i - 1]));
        const double right = 0.5 * (c + (i + 1 == crit.size() ? b : crit[i + 1]));
        const double dl = d.Eval(left), dr = d.Eval(right);
        if (dl > 0.0 && dr < 0.0)
            result.push_back({ c, Eval(c), true });
        else if (dl < 0.0 && dr > 0.0)
            result.push_back({ c, Eval(c), false });
    }
    return result;
}

CString FormatNumber(double value, int decimals, bool unicodeMinus)
{
    CString s;
    if (!std::isfinite(value))
        return CString(_T("?"));
    if (std::fabs(value) < 0.5 * std::pow(10.0, -decimals))
        return CString(_T("0"));
    if (std::fabs(value) >= 1e15)
        s.Format(_T("%.3g"), value);
    else
    {
        s.Format(_T("%.*f"), decimals, value);
        if (s.Find(L'.') >= 0)
        {
            s.TrimRight(L'0');
            s.TrimRight(L'.');
        }
    }
    if (s == _T("-0"))
        s = _T("0");
    if (unicodeMinus)
        s.Replace(L'-', kMinusSign);
    return s;
}
