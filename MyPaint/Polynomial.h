#pragma once

/**
 * @file Polynomial.h
 * @brief 다항식: 수식 문자열 해석, 계산, 미분, 실근·극값 찾기 (함수 그래프 기능용)
 */

/**
 * @brief 실수 계수 다항식 c[0] + c[1]x + c[2]x² + ...
 *
 * 수식은 전개해서 계수로 보관합니다. 예를 들어 "(x-1)(x+2)"는 [-2, 1, 1]이 됩니다.
 */
class Polynomial
{
public:
    /** @brief 해석할 수 있는 최고 차수 */
    static constexpr int kMaxDegree = 20;

    Polynomial() = default;
    explicit Polynomial(std::vector<double> coeffs);

    /**
     * @brief 수식 문자열을 해석합니다.
     *
     * 지원: 숫자(소수), x, + - * / ^, 괄호, 곱셈 기호 생략("2x", "x(x+1)", "(x-1)(x+2)"),
     *       위 첨자 ²·³, ×·÷·−, 앞의 "y =" / "f(x) =".
     * 거듭제곱 지수는 0~20 정수, 나눗셈은 0이 아닌 상수로만 할 수 있습니다.
     *
     * @param text 수식
     * @param[out] out 결과 다항식
     * @param[out] error 실패 이유 (사용자에게 보여 줄 문장)
     * @return 성공하면 true
     */
    static bool Parse(const CString& text, Polynomial& out, CString& error);

    /** @brief 차수 (0인 다항식은 0) */
    int Degree() const { return static_cast<int>(m_c.size()) - 1; }
    /** @brief 계수 (c[i]는 x^i의 계수) */
    const std::vector<double>& Coeffs() const { return m_c; }
    /** @brief f(x) (호너 방법) */
    double Eval(double x) const;
    /** @brief 도함수 */
    Polynomial Derivative() const;
    /** @brief 상수 함수인지 */
    bool IsConstant() const { return m_c.size() <= 1; }

    /**
     * @brief 보기 좋은 문자열. 예: "2x³ − x + 1"
     */
    CString ToString() const;

    /**
     * @brief [a, b] 안의 서로 다른 실근 (오름차순)
     *
     * 도함수의 근(임계점)으로 구간을 나누면 각 구간에서 함수가 단조이므로,
     * 부호가 바뀌는 구간은 이분법으로, 임계점에서 0에 닿는 중근은 따로 찾습니다.
     * 도함수의 근은 같은 방법을 재귀로 써서 구합니다.
     */
    std::vector<double> RealRoots(double a, double b) const;

    /** @brief 극값 종류 */
    struct Extremum
    {
        double x;
        double y;
        bool maximum; ///< true: 극대, false: 극소
    };
    /** @brief [a, b] 안의 극대·극소 (도함수의 부호가 바뀌는 점) */
    std::vector<Extremum> Extrema(double a, double b) const;

private:
    void Trim(); ///< 최고차 계수가 0이면 제거
    std::vector<double> m_c{ 0.0 };
};

/**
 * @brief 숫자를 짧게: 소수 decimals자리까지, 끝의 0 제거, "-0"은 "0"
 * @param unicodeMinus 음수 부호를 '−'(U+2212)로 (그래프 글자용)
 */
CString FormatNumber(double value, int decimals, bool unicodeMinus = false);
