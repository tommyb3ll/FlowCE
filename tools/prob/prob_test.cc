// The probability screen's distributions (src/prob_math.h) against known values, in floats as on
// the calculator:  c++ -O1 -o /tmp/pt tools/prob/prob_test.cc && /tmp/pt
#include <stdio.h>
#include "../../src/prob_math.h"
static int bad = 0;
static void chk(const char * what, float got, double want, double tol = 2e-4) {
  const int ok = fabs(got - want) <= tol;
  bad += !ok;
  printf("%s %-36s %.6f  want %.6f\n", ok ? "  " : "XX", what, got, want);
}
int main() {
  float n01[2] = {0, 1}, iq[2] = {100, 15}, t24[1] = {24}, t5[1] = {5}, t1[1] = {1}, c5[1] = {5}, c1[1] = {1}, c30[1] = {30};
  float b20[2] = {20, 0.3f}, b1000[2] = {1000, 0.5f}, p4[1] = {4}, p50[1] = {50};
  chk("Phi(1)-Phi(-1)", p_cdf(PD_NORM, n01, 1) - p_cdf(PD_NORM, n01, -1), 0.682689);
  chk("Phi(1.96)", p_cdf(PD_NORM, n01, 1.96f), 0.975002);
  chk("Phi(-3)", p_cdf(PD_NORM, n01, -3), 0.001350, 2e-6);
  chk("Phi(-5)", p_cdf(PD_NORM, n01, -5) * 1e7f, 2.8665, 2e-3);
  chk("IQ P(X>130)", 1 - p_cdf(PD_NORM, iq, 130), 0.022750);
  chk("invNorm(.975)", p_inv(PD_NORM, n01, 0.975f, -10, 10), 1.959964, 5e-4);
  chk("invNorm(.05) IQ", p_inv(PD_NORM, iq, 0.05f, 100 - 150, 100 + 150), 75.326, 5e-3);
  chk("t24 cdf(2.31)", p_cdf(PD_T, t24, 2.31f), 0.985099);
  chk("t5 cdf(-1)", p_cdf(PD_T, t5, -1), 0.181609);
  chk("t1 cdf(1)", p_cdf(PD_T, t1, 1), 0.75);
  chk("invT(.975,24)", p_inv(PD_T, t24, 0.975f, -200, 200), 2.063899, 5e-4);
  chk("invT(.975,5)", p_inv(PD_T, t5, 0.975f, -200, 200), 2.570582, 5e-4);
  chk("chi5 cdf(3.2)", p_cdf(PD_CHI, c5, 3.2f), 0.330817);
  chk("chi1 cdf(3.841)", p_cdf(PD_CHI, c1, 3.841459f), 0.95);
  chk("chi30 cdf(40)", p_cdf(PD_CHI, c30, 40), 0.895136);
  chk("inv chi5 .95", p_inv(PD_CHI, c5, 0.95f, 0, 100), 11.070498, 2e-3);
  chk("bin20 P(X<=5)", p_cdf(PD_BIN, b20, 5), 0.416371);
  chk("bin20 P(2<=X<=5)", p_cdf(PD_BIN, b20, 5) - p_cdf(PD_BIN, b20, 1), 0.408732);
  chk("bin20 P(X=5)", p_pdf(PD_BIN, b20, 5), 0.178863);
  chk("bin1000 P(X<=520)", p_cdf(PD_BIN, b1000, 520), 0.902617);
  chk("bin1000 P(X=500)", p_pdf(PD_BIN, b1000, 500), 0.025225);
  chk("pois4 P(2<=X<=6)", p_cdf(PD_POIS, p4, 6) - p_cdf(PD_POIS, p4, 1), 0.797748);
  chk("pois4 P(X=3)", p_pdf(PD_POIS, p4, 3), 0.195367);
  float b500[2] = {500, 0.02f}, p1000[1] = {1000};
  chk("bin500 p.02 P(X=10)", p_pdf(PD_BIN, b500, 10), 0.126380, 2e-5);
  chk("pois1000 P(X=1000)", p_pdf(PD_POIS, p1000, 1000) * 100, 1.261461, 2e-4);
  chk("pois1000 P(X<=1050)", p_cdf(PD_POIS, p1000, 1050), 0.943971, 3e-4);
  chk("pois50 P(X<=60)", p_cdf(PD_POIS, p50, 60), 0.927840);
  chk("normal pdf(0)", p_pdf(PD_NORM, n01, 0), 0.398942);
  chk("t24 pdf(0)", p_pdf(PD_T, t24, 0), 0.394809);
  chk("chi5 pdf(3)", p_pdf(PD_CHI, c5, 3), 0.154180);
  printf(bad ? "%d wrong\n" : "all right\n", bad);
  return bad != 0;
}
