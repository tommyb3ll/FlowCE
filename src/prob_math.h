// prob_math.h - DeltaCE's distributions in floats, for the probability screen (focus_stats.cc):
// giac's are compiled out of the calculator's build (XLIGHT), and these are small. The normal
// through the incomplete gamma function (erf(x) = P(1/2, x^2)), t and the binomial through the
// incomplete beta, chi-square and Poisson through the incomplete gamma (Numerical Recipes' series
// and continued fractions); inverses by bisection. The CE's double is a 32-bit float: about 6
// digits, the screen shows 4. Tested on the PC by tools/prob_test.cc.
#ifndef PROB_MATH_H
#define PROB_MATH_H
#include <math.h>
#ifndef PNOINL
#define PNOINL __attribute__((noinline))
#endif

static PNOINL float p_lgam(float x) { // ln Gamma(x), x > 0 (Lanczos)
  static const float c[6] = {76.18009172947146f, -86.50532032941677f, 24.01409824083091f, -1.231739572450155f, 0.1208650973866179e-2f, -0.5395239384953e-5f};
  float y = x, t = x + 5.5f, s = 1.000000000190015f;
  t -= (x + 0.5f) * logf(t);
  for (int j = 0; j < 6; ++j) s += c[j] / ++y;
  return -t + logf(2.5066282746310005f * s / x);
}
static float p_tiny(float d) { return fabsf(d) < 1e-30f ? 1e-30f : d; }
// The binomial's and Poisson's P(X = k) to float precision for large n (lgamma's differences lose
// 4 digits at n = 1000): Loader's saddle point form, as R's dbinom
static PNOINL float p_stirlerr(float n) { // ln(n!) - ln(sqrt(2 pi n) (n/e)^n)
  if (n > 15) { const float n2 = n * n; return (0.083333333f - (0.0027777778f - 0.00079365079f / n2) / n2) / n; }
  return p_lgam(n + 1) - (n + 0.5f) * logf(n) + n - 0.91893853f;
}
static PNOINL float p_bd0(float x, float m) { // x ln(x/m) + m - x, without its cancellation
  if (fabsf(x - m) < 0.1f * (x + m)) {
    const float v = (x - m) / (x + m);
    float s = (x - m) * v, ej = 2 * x * v;
    for (int j = 1; j < 40; ++j) { ej *= v * v; const float s1 = s + ej / (2 * j + 1); if (s1 == s) break; s = s1; }
    return s;
  }
  return x * logf(x / m) + m - x;
}
static PNOINL float p_dbin(float k, float n, float p) {
  if (k < 0 || k > n) return 0;
  if (p <= 0) return k == 0;
  if (p >= 1) return k == n;
  if (k == 0) return expf(n * logf(1 - p));
  if (k == n) return expf(n * logf(p));
  return expf(p_stirlerr(n) - p_stirlerr(k) - p_stirlerr(n - k) - p_bd0(k, n * p) - p_bd0(n - k, n * (1 - p))) * sqrtf(n / (6.2831853f * k * (n - k)));
}
static PNOINL float p_dpois(float k, float l) {
  if (k < 0) return 0;
  if (k == 0) return expf(-l);
  return expf(-p_stirlerr(k) - p_bd0(k, l)) / sqrtf(6.2831853f * k);
}
// P(a, x), the regularized lower incomplete gamma function, or Q = 1 - P (up: a tail without the
// cancellation: Phi(-5)); f its front factor x^a e^-x / Gamma(a) (0: computed here)
static PNOINL float p_gammp(float a, float x, float f = 0, int up = 0) {
  if (x <= 0) return up;
  if (!f) f = expf(-x + a * logf(x) - p_lgam(a));
  if (x < a + 1) { // its series
    float ap = a, del = 1 / a, sum = del;
    for (int n = 0; n < 300; ++n) { ap += 1; del *= x / ap; sum += del; if (fabsf(del) < fabsf(sum) * 1e-7f) break; }
    return up ? 1 - sum * f : sum * f;
  }
  float b = x + 1 - a, c = 1e30f, d = 1 / b, h = d; // Q's continued fraction (Lentz)
  for (int i = 1; i < 300; ++i) {
    const float an = -i * (i - a);
    b += 2;
    d = 1 / p_tiny(an * d + b);
    c = p_tiny(b + an / c);
    const float del = d * c;
    h *= del;
    if (fabsf(del - 1) < 1e-7f) break;
  }
  return up ? f * h : 1 - f * h;
}
static PNOINL float p_betacf(float a, float b, float x) {
  const float qab = a + b, qap = a + 1, qam = a - 1;
  float c = 1, d = 1 / p_tiny(1 - qab * x / qap), h = d;
  for (int m = 1; m <= 300; ++m) {
    const float m2 = 2.0f * m;
    float aa = m * (b - m) * x / ((qam + m2) * (a + m2));
    d = 1 / p_tiny(1 + aa * d); c = p_tiny(1 + aa / c); h *= d * c;
    aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
    d = 1 / p_tiny(1 + aa * d); c = p_tiny(1 + aa / c);
    const float del = d * c;
    h *= del;
    if (fabsf(del - 1) < 1e-7f) break;
  }
  return h;
}
// I_x(a, b), the regularized incomplete beta function; bt its front x^a (1-x)^b / B(a, b) (0: here)
static PNOINL float p_betai(float a, float b, float x, float bt = 0) {
  if (x <= 0) return 0;
  if (x >= 1) return 1;
  if (!bt) bt = expf(p_lgam(a + b) - p_lgam(a) - p_lgam(b) + a * logf(x) + b * logf(1 - x));
  return x < (a + 1) / (a + b + 2) ? bt * p_betacf(a, b, x) / a : 1 - bt * p_betacf(b, a, 1 - x) / b;
}

enum { PD_NORM, PD_T, PD_CHI, PD_BIN, PD_POIS, PD_N };
static int p_discrete(int d) { return d >= PD_BIN; }
// P(X <= x); the discrete ones at whole x (below 0: 0)
static PNOINL float p_cdf(int d, const float * q, float x) {
  switch (d) {
  case PD_NORM: { const float z = (x - q[0]) / q[1], g = 0.5f * p_gammp(0.5f, z * z / 2, 0, 1); return z < 0 ? g : 1 - g; }
  case PD_T: { const float v = q[0], b = 0.5f * p_betai(v / 2, 0.5f, v / (v + x * x)); return x < 0 ? b : 1 - b; }
  case PD_CHI: return p_gammp(q[0] / 2, x / 2);
  case PD_BIN: { const float k = floorf(x + 1e-4f), n = q[0], p = q[1]; // (I's front: P(X = k) (n-k) p)
    return k < 0 ? 0 : k >= n ? 1 : p <= 0 ? 1 : p >= 1 ? 0 : p_betai(n - k, k + 1, 1 - p, p_dbin(k, n, p) * (n - k) * p); }
  default: { const float k = floorf(x + 1e-4f); return k < 0 ? 0 : p_gammp(k + 1, q[0], p_dpois(k, q[0]) * q[0], 1); } // (Q's front: P(X = k) lambda)
  }
}
// the density (continuous) or P(X = k) (discrete)
static PNOINL float p_pdf(int d, const float * q, float x) {
  switch (d) {
  case PD_NORM: { const float z = (x - q[0]) / q[1]; return 0.39894228f / q[1] * expf(-z * z / 2); }
  case PD_T: { const float v = q[0]; return expf(p_lgam((v + 1) / 2) - p_lgam(v / 2) - 0.5f * logf(v * 3.14159265f) - (v + 1) / 2 * logf(1 + x * x / v)); }
  case PD_CHI: { const float k = q[0] / 2; return x <= 0 ? (k < 1 ? 1e9f : k == 1 ? 0.5f : 0) : expf((k - 1) * logf(x) - x / 2 - k * 0.69314718f - p_lgam(k)); }
  case PD_BIN: return p_dbin(x, q[0], q[1]);
  default: return p_dpois(x, q[0]);
  }
}
// x with P(X <= x) = a (continuous), by bisection in [lo, hi]
static PNOINL float p_inv(int d, const float * q, float a, float lo, float hi) {
  for (int i = 0; i < 48; ++i) {
    const float m = (lo + hi) / 2;
    if (p_cdf(d, q, m) < a) lo = m; else hi = m;
  }
  return (lo + hi) / 2;
}
#endif
