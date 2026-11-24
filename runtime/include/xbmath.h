/* xbase's exponential, logarithm and power of doubles (^, Exp(), Log(), and ^ folded when compiling): the same
 * result on every system, unlike the C library's, whose last bit differs from one system to another. Each computes
 * in double-double arithmetic (an unevaluated sum hi + lo of two doubles, about 106 bits; table-driven reductions keep
 * the series short) and rounds once at the end, subnormal results included: the correctly rounded double, except for
 * an argument whose result lies within about 2^-90 of a tie between two doubles (Exp( 2^-53 ) is one). ^'s exact and
 * halfway results are computed exactly (xb_math_pow_exact). Only + - * / and sqrt of doubles, each rounded once: the
 * runtime and xbc are compiled without contracting into fused multiply-adds and, on 32-bit x86, with SSE2 instead of
 * the x87's longer registers (xb_exact_doubles in CMake); a product's error comes from Veltkamp's split (Dekker). No
 * function here writes errno (C's ldexp only scales within the normal doubles). Shared by the runtime and the
 * compiler. */
#ifndef XBMATH_H
#define XBMATH_H

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Every integer of at most 2^XB_DBL_EXACT_BITS in magnitude is a double exactly (the significand's bits, with its
 * hidden one). */
#define XB_DBL_EXACT_BITS 53

/* 2^52: every double of this magnitude or more is an integer (the step between doubles is 1 or more there); Round()
 * gives such a scaled value back as it is, in the runtime and in the compiler's inline code. */
#define XB_DBL_INTEGRAL ((double)((uint64_t)1 << (XB_DBL_EXACT_BITS - 1)))

/* A double's bits: the fraction's (below the exponent), and the exponent's bias. */
#define XB_DBL_FRACTION_BITS (XB_DBL_EXACT_BITS - 1)
#define XB_DBL_FRACTION_MASK (((uint64_t)1 << XB_DBL_FRACTION_BITS) - 1)
#define XB_DBL_BIAS 1023

/* 2^q of a q within the normal exponents (-1022 .. 1023), made from its bits: no call. */
static inline double xb_math_pow2(int q) {
  uint64_t bits = (uint64_t)(q + XB_DBL_BIAS) << XB_DBL_FRACTION_BITS;
  double r;
  memcpy(&r, &bits, sizeof r);
  return r;
}

typedef struct xb_dd {
  double hi, lo;
} xb_dd;

/* The integer nearest x, |x| below 2^51 (ties to even): adding 1.5 2^52 leaves no fraction bits, subtracting it is
 * exact (C's floor and nearbyint are calls on targets without the instruction). */
static inline double xb_math_nearest(double x) {
  const double shift = 1.5 * XB_DBL_INTEGRAL;
  return (x + shift) - shift;
}

/* a + b exactly (Knuth) */
static inline xb_dd xb_dd_sum(double a, double b) {
  double s = a + b, v = s - a;
  xb_dd r = {s, (a - (s - v)) + (b - v)};
  return r;
}

/* a + b exactly when |a| >= |b| (Dekker) */
static inline xb_dd xb_dd_quick(double a, double b) {
  double s = a + b;
  xb_dd r = {s, b - (s - a)};
  return r;
}

/* Whether the target computes fma() in one instruction (a fused multiply-add of the hardware, which the compiler
 * guarantees when it defines these): xb_dd_prod then takes it, else Dekker's product, the same exact result. */
#if defined(__FMA__) || defined(__ARM_FEATURE_FMA) || defined(__aarch64__) || defined(_M_ARM64)
#define XB_MATH_FMA 1
#else
#define XB_MATH_FMA 0
#endif

/* a * b exactly, |a| and |b| below 2^996: with the hardware's fused multiply-add, its rounding error fma( a, b, -p );
 * else Veltkamp's split in halves of 26 bits and Dekker's product. Both give the same exact pair. */
static inline xb_dd xb_dd_prod(double a, double b) {
#if XB_MATH_FMA
  double p = a * b;
  xb_dd r = {p, fma(a, b, -p)};
  return r;
#else
  const double split = 134217729.0; /* 2^27 + 1 */
  double p = a * b, t = split * a, ah = t - (t - a), al = a - ah;
  t = split * b;
  double bh = t - (t - b), bl = b - bh;
  xb_dd r = {p, ((ah * bh - p) + ah * bl + al * bh) + al * bl};
  return r;
#endif
}

static inline xb_dd xb_dd_add(xb_dd a, xb_dd b) {
  xb_dd s = xb_dd_sum(a.hi, b.hi), t = xb_dd_sum(a.lo, b.lo);
  s = xb_dd_quick(s.hi, s.lo + t.hi);
  return xb_dd_quick(s.hi, s.lo + t.lo);
}

static inline xb_dd xb_dd_mul(xb_dd a, xb_dd b) {
  xb_dd p = xb_dd_prod(a.hi, b.hi);
  return xb_dd_quick(p.hi, p.lo + (a.hi * b.lo + a.lo * b.hi));
}

static inline xb_dd xb_dd_mul_d(xb_dd a, double b) {
  xb_dd p = xb_dd_prod(a.hi, b);
  return xb_dd_quick(p.hi, p.lo + a.lo * b);
}

/* a / b: three quotient digits, each from what the ones before leave */
static inline xb_dd xb_dd_div(xb_dd a, xb_dd b) {
  double q1 = a.hi / b.hi;
  xb_dd r = xb_dd_add(a, xb_dd_mul_d(b, -q1));
  double q2 = r.hi / b.hi;
  r = xb_dd_add(r, xb_dd_mul_d(b, -q2));
  double q3 = r.hi / b.hi;
  xb_dd q3d = {q3, 0.0};
  return xb_dd_add(xb_dd_quick(q1, q2), q3d);
}

/* v 2^q rounded once to a double, a subnormal one too (at the least subnormal's quantum, ties to even); infinity past
 * the doubles, 0 below half the least subnormal. Scales by multiplying with powers of two where ldexp would leave the
 * normal doubles (which may set errno). */
static inline double xb_dd_scale(xb_dd v, int q) {
  double r = v.hi + v.lo;
  int e;
  (void)frexp(r, &e); /* |r| in [2^(e-1), 2^e) */
  if (r == 0.0) return r;
  if (e + q > 1024) return copysign(HUGE_VAL, r);
  if (e + q >= -1021) return ldexp(r, q); /* a normal double: exact */
  if (e + q < -1075) return copysign(0.0, r);
  double sign = copysign(1.0, r), unit = ldexp(1.0, q + 1074); /* the result in least subnormals: below 2^53 */
  double h = fabs(v.hi) * unit, l = sign * v.lo * unit, n = floor(h), frac = h - n; /* exact: h normal, below 2^53 */
  if (frac > 0.5 || (frac == 0.5 && (l > 0.0 || (l == 0.0 && fmod(n, 2.0) != 0.0)))) n += 1.0;
  double tiny = ldexp(1.0, -537); /* 2^-1074 in two exact steps */
  return sign * (n * tiny * tiny);
}

/* For ln: x = m 2^e, m in [sqrt( 1/2 ), sqrt( 2 )); m's row j = round( (m - 1) 128 ) holds r_j, a double near
 * 1 / (1 + j / 128) (1 itself for j = 0, so ln near 1 cancels nothing), and ln( r_j ) as a double-double. */
#define XB_MATH_LOG_JMIN (-37)
static const struct xb_math_log_row {
  double r;
  xb_dd ln;
} xb_math_log_rows[] = {
  {1.4065934065934067, {0.3411707574027672, 3.1846151250956206e-18}},
  {1.391304347826087, {0.3302416868705768, 1.6927253978145054e-17}},
  {1.3763440860215055, {0.3194307707663613, 2.5640385520940108e-17}},
  {1.3617021276595744, {0.30873548164961323, 1.5025836482434425e-17}},
  {1.3473684210526315, {0.2981533723190763, 1.575278736910067e-17}},
  {1.3333333333333333, {0.28768207245178085, 2.6071606164425637e-17}},
  {1.3195876288659794, {0.27731928541623435, -2.652724229158001e-17}},
  {1.3061224489795917, {0.26706278524904514, 2.3896107240262357e-17}},
  {1.292929292929293, {0.2569104137850273, -9.92419178127068e-19}},
  {1.28, {0.2468600779315258, 6.678539813576451e-18}},
  {1.2673267326732673, {0.23690974707835774, -1.3644270985951448e-17}},
  {1.2549019607843137, {0.22705745063534608, -4.326372045075968e-18}},
  {1.2427184466019416, {0.2173012756899813, -1.8526017065773163e-18}},
  {1.2307692307692308, {0.20763936477824455, 1.2053243216686127e-17}},
  {1.2190476190476192, {0.19806991376209387, 1.0681737386368664e-17}},
  {1.2075471698113207, {0.18859116980754997, 9.915070540571144e-18}},
  {1.1962616822429906, {0.17920142945771092, -2.111400074974391e-18}},
  {1.1851851851851851, {0.16989903679539742, -4.868008764439086e-19}},
  {1.1743119266055047, {0.16068238169047352, -3.650183553047839e-18}},
  {1.1636363636363636, {0.15154989812720088, 1.2105853272368787e-17}},
  {1.1531531531531531, {0.142500062607283, 9.155570001519129e-18}},
  {1.1428571428571428, {0.13353139262452257, -3.664457663660086e-18}},
  {1.1327433628318584, {0.12464244520727659, -5.8089126789409715e-18}},
  {1.1228070175438596, {0.11583181552512165, 4.3384843698080944e-18}},
  {1.1130434782608696, {0.10709813555636712, -3.4717745161358675e-18}},
  {1.103448275862069, {0.09844007281325251, -4.439009633675136e-18}},
  {1.0940170940170941, {0.08985632912186114, 2.84207093558465e-18}},
  {1.0847457627118644, {0.0813456394539524, 1.6076294039775555e-18}},
  {1.0756302521008403, {0.07290677080808773, 5.836204074304871e-18}},
  {1.0666666666666667, {0.06453852113757116, -6.470486661692933e-18}},
  {1.0578512396694215, {0.05623971832287611, -3.2835149805605617e-18}},
  {1.0491803278688525, {0.04800921918636066, -2.030356617224395e-18}},
  {1.0406504065040652, {0.03984590854719978, -1.3948242043384064e-18}},
  {1.032258064516129, {0.03174869831458027, 3.0382263084680854e-18}},
  {1.024, {0.023716526617316065, -1.5774243488668216e-18}},
  {1.0158730158730158, {0.015748356968139112, 1.0021578630528958e-18}},
  {1.0078740157480315, {0.007843177461025879, 2.764708154124903e-19}}, {1.0, {0.0, 0.0}},
  {0.9922480620155039, {-0.007782140442054963, 1.2819179123343749e-20}},
  {0.9846153846153847, {-0.015504186535965199, 3.2783210228924137e-19}},
  {0.9770992366412213, {-0.023167059281534418, 3.095927552179262e-19}},
  {0.9696969696969697, {-0.03077165866675366, -1.0431732029005972e-18}},
  {0.9624060150375939, {-0.03831886430213666, 2.3579961573512846e-18}},
  {0.9552238805970149, {-0.04580953603129422, -1.6823639049745016e-19}},
  {0.9481481481481482, {-0.05324451451881224, -1.803871134979952e-18}},
  {0.9411764705882353, {-0.060624621816434854, -2.6424025938726934e-18}},
  {0.9343065693430657, {-0.06795066190850778, -3.9239563038692484e-18}},
  {0.927536231884058, {-0.07522342123758752, 4.195880720316434e-18}},
  {0.920863309352518, {-0.08244366921107454, 4.707903082046854e-18}},
  {0.9142857142857143, {-0.08961215868968717, 1.9573659817110993e-18}},
  {0.9078014184397163, {-0.09672962645855114, 4.0291867005826106e-18}},
  {0.9014084507042254, {-0.10379679368164355, 3.195893222617445e-18}},
  {0.8951048951048951, {-0.11081436634029011, -2.0511100808140527e-18}},
  {0.8888888888888888, {-0.11778303565638351, 1.1971685747593662e-18}},
  {0.8827586206896552, {-0.12470347850095725, 4.6522609636496624e-18}},
  {0.8767123287671232, {-0.13157635778871932, -1.112300087972959e-17}},
  {0.8707482993197279, {-0.1384023228591192, 1.3766819196398948e-17}},
  {0.8648648648648649, {-0.14518200984449783, -8.242418783022477e-18}},
  {0.8590604026845637, {-0.151916042025842, -4.1233095848339465e-19}},
  {0.8533333333333334, {-0.15860503017663852, -2.583386492298558e-18}},
  {0.847682119205298, {-0.16524957289530717, 9.227573884334224e-18}},
  {0.8421052631578947, {-0.17185025692665928, 6.022453821011369e-18}},
  {0.8366013071895425, {-0.17840765747281825, -1.2720936612962572e-17}},
  {0.8311688311688312, {-0.18492233849401193, 7.384679440503435e-18}},
  {0.8258064516129032, {-0.19139485299962947, 1.126213516780448e-17}},
  {0.8205128205128205, {-0.19782574332991992, 7.995487338741543e-18}},
  {0.8152866242038217, {-0.20421554142869083, -7.9379985298027e-18}},
  {0.810126582278481, {-0.21056476910734964, -1.136310596906137e-17}},
  {0.8050314465408805, {-0.2168739383006143, -6.285749669211092e-18}},
  {0.8, {-0.2231435513142097, 9.091270597324798e-18}},
  {0.7950310559006211, {-0.2293741010648459, 5.684839459813236e-18}},
  {0.7901234567901234, {-0.23556607131276697, 2.394337149518734e-18}},
  {0.7852760736196319, {-0.24171993688714513, -1.323779871210866e-17}},
  {0.7804878048780488, {-0.2478361639045812, -8.384472133019162e-18}},
  {0.7757575757575758, {-0.25391520998096345, 7.180735656435798e-18}},
  {0.7710843373493976, {-0.259957524436926, -2.4167516341742964e-17}},
  {0.7664670658682635, {-0.2659635484971379, -1.35209848201012e-19}},
  {0.7619047619047619, {-0.2719337154836418, -7.833196376974436e-19}},
  {0.757396449704142, {-0.2778684510034563, -2.2502748630777633e-17}},
  {0.7529411764705882, {-0.2837681731306446, 6.448868003452105e-18}},
  {0.7485380116959064, {-0.2896332925830427, -2.0535953219858177e-17}},
  {0.7441860465116279, {-0.2954642128938359, 7.768320796245443e-18}},
  {0.7398843930635838, {-0.30126133057816185, 1.5120043309967385e-17}},
  {0.735632183908046, {-0.3070250352949119, -1.5578716077124932e-18}},
  {0.7314285714285714, {-0.3127557100038969, 1.3650721793001109e-17}},
  {0.7272727272727273, {-0.3184537311185346, 6.407962483026777e-19}},
  {0.7231638418079096, {-0.324119468654212, 4.488767429940198e-18}},
  {0.7191011235955056, {-0.32975328637246804, 2.5633554999431966e-17}},
  {0.7150837988826816, {-0.3353555419211378, 1.3746739934976202e-17}},
  {0.7111111111111111, {-0.3409265869705932, 2.069678002794501e-17}},
  {0.7071823204419889, {-0.3464667673462086, 3.591951952851805e-18}}
};

/* 1 / (2k + 1), k = 1 .. 8: atanh's series */
static const xb_dd xb_math_odd[] = {
  {0.3333333333333333, 1.850371707708594e-17}, {0.2, -1.1102230246251566e-17},
  {0.14285714285714285, 7.93016446160826e-18}, {0.1111111111111111, 6.1679056923619804e-18},
  {0.09090909090909091, -2.523234146875356e-18}, {0.07692307692307693, -4.270088556250602e-18},
  {0.06666666666666667, 9.251858538542971e-19}, {0.058823529411764705, 8.163404592832033e-19}
};

/* 2^(j / 64), j = 0 .. 63 */
static const xb_dd xb_math_exp2_64[] = {
  {1.0, 0.0}, {1.0108892860517005, -1.5234778603368577e-17}, {1.0218971486541166, 5.109225028973444e-17},
  {1.0330248790212284, 7.600838874027088e-18}, {1.0442737824274138, 8.551889705537965e-17},
  {1.0556451783605572, 1.759325738772092e-18}, {1.0671404006768237, -7.899853966841582e-17},
  {1.0787607977571199, -6.656660436056593e-17}, {1.0905077326652577, -3.046782079812471e-17},
  {1.102382583307841, 5.2660368715706944e-17}, {1.1143867425958924, 1.0410278456845571e-16},
  {1.1265216186082418, 5.165856758795457e-17}, {1.1387886347566916, 8.912812676025408e-17},
  {1.1511892299529827, 3.250710218863827e-17}, {1.1637248587775775, 3.8292048369240935e-17},
  {1.1763969916502812, 5.554203254218079e-17}, {1.189207115002721, 3.982015231465646e-17},
  {1.202156731452703, 6.644981499252301e-17}, {1.215247359980469, -7.712630692681488e-17},
  {1.22848053610687, -1.89878163130253e-17}, {1.241857812073484, 4.658027591836937e-17},
  {1.255380757024691, -6.7113898212968784e-18}, {1.2690509571917332, 2.667932131342186e-18},
  {1.2828700160787783, 1.713594918243561e-17}, {1.2968395546510096, 2.5382502794888315e-17},
  {1.3109612115247644, -7.181536135519454e-17}, {1.3252366431597413, -2.8587312100388614e-17},
  {1.339667524053303, 8.927282594831732e-17}, {1.3542555469368927, 7.70094837980299e-17},
  {1.3690024229745905, 9.593797919118849e-17}, {1.383909881963832, -6.770511658794786e-17},
  {1.3989796725383112, -9.614213209051323e-17}, {1.4142135623730951, -9.667293313452913e-17},
  {1.42961333839197, -1.2031642489053655e-17}, {1.4451808069770467, -3.0237581349939873e-17},
  {1.460917794180647, -5.600377186075216e-17}, {1.4768261459394993, -3.483994556892796e-17},
  {1.4929077282912648, 1.4192920154284036e-17}, {1.5091644275934228, -1.016455327754295e-16},
  {1.5255981507445384, -1.1024941712342561e-16}, {1.5422108254079407, 7.949834809697621e-17},
  {1.559004400237837, 3.7812070533575275e-17}, {1.5759808451078865, -1.0136916471278304e-17},
  {1.593142151342267, -1.0094406542311964e-16}, {1.6104903319492543, 2.4707192569797888e-17},
  {1.6280274218573478, -6.712955084707084e-17}, {1.645755478153965, -1.0125679913674773e-16},
  {1.6636765803267364, 5.8909926967131e-17}, {1.681792830507429, 8.199010020581497e-17},
  {1.7001063537185235, -8.0237193703977e-18}, {1.718619298122478, -1.851380418263111e-17},
  {1.7373338352737062, 3.164389299292957e-17}, {1.7562521603732995, 2.960140695448873e-17},
  {1.7753764925265212, 6.429731796556572e-17}, {1.7947090750031072, 1.8227458427912087e-17},
  {1.8142521755003989, -9.969531538920349e-17}, {1.8340080864093424, 3.283107224245627e-17},
  {1.8539791250833855, 9.761887490727594e-17}, {1.8741676341103, -6.122763413004143e-17},
  {1.8945759815869656, 3.4034035352165297e-17}, {1.9152065613971474, -1.0619946056195963e-16},
  {1.9360617934922943, 1.0332385960676326e-16}, {1.9571441241754002, 8.960767791036668e-17},
  {1.978456026387951, 4.0388753109278167e-17}
};

/* 1 / n!, n = 2 .. 13: e^r's series */
static const xb_dd xb_math_fact[] = {
  {0.5, 0.0}, {0.16666666666666666, 9.25185853854297e-18}, {0.041666666666666664, 2.3129646346357427e-18},
  {0.008333333333333333, 1.1564823173178714e-19}, {0.001388888888888889, -5.300543954373577e-20},
  {0.0001984126984126984, 1.7209558293420705e-22}, {2.48015873015873e-05, 2.1511947866775882e-23},
  {2.7557319223985893e-06, -1.858393274046472e-22}, {2.755731922398589e-07, 2.3767714622250297e-23},
  {2.505210838544172e-08, -1.448814070935912e-24}, {2.08767569878681e-09, -1.20734505911326e-25},
  {1.6059043836821613e-10, 1.2585294588752098e-26}
};

static const xb_dd xb_math_ln2 = {0.6931471805599453, 2.3190468138462996e-17};

/* x = m 2^e of a finite x > 0, m within [sqrt( 1/2 ), sqrt( 2 )), read from x's bits (a subnormal x scaled to a
 * normal one first), and m's row of the table. |m r_j - 1| is below 0.0056 (1/256 over the least 1 + j / 128). */
static inline const struct xb_math_log_row *xb_math_log_reduce(double x, int *e, double *m) {
  uint64_t bits;
  *e = 0;
  if (x < DBL_MIN) {
    x *= xb_math_pow2(XB_DBL_EXACT_BITS);
    *e = -XB_DBL_EXACT_BITS;
  }
  memcpy(&bits, &x, sizeof bits);
  *e += (int)(bits >> XB_DBL_FRACTION_BITS) - XB_DBL_BIAS;
  bits = (bits & XB_DBL_FRACTION_MASK) | (uint64_t)XB_DBL_BIAS << XB_DBL_FRACTION_BITS; /* m in [1, 2) */
  memcpy(m, &bits, sizeof *m);
  if (*m >= 1.4142135623730951) {
    *m *= 0.5;
    ++*e;
  }
  return &xb_math_log_rows[(int)xb_math_nearest((*m - 1.0) * 128.0) - XB_MATH_LOG_JMIN];
}

/* ln( x ) of a finite x > 0: ln( m ) = ln( m r_j ) - ln( r_j ), m r_j = 1 + f exactly as a double-double with
 * |f| < 0.0056, ln( 1 + f ) = 2 atanh( s ), s = f / (2 + f): 2 s (1 + s^2 / 3 + ... + s^16 / 17), s^18 < 2^-150. */
static inline xb_dd xb_math_log_dd(double x) {
  int e;
  double m;
  const struct xb_math_log_row *row = xb_math_log_reduce(x, &e, &m);
  xb_dd minus1 = {-1.0, 0.0}, two = {2.0, 0.0}, one = {1.0, 0.0};
  xb_dd f = xb_dd_add(xb_dd_prod(m, row->r), minus1);
  xb_dd s = xb_dd_div(f, xb_dd_add(two, f)), z = xb_dd_mul(s, s);
  size_t n = sizeof xb_math_odd / sizeof xb_math_odd[0];
  xb_dd sum = xb_math_odd[n - 1];
  for (size_t k = n - 1; k-- > 0;) sum = xb_dd_add(xb_math_odd[k], xb_dd_mul(sum, z));
  sum = xb_dd_add(one, xb_dd_mul(sum, z));
  xb_dd lnr = {-row->ln.hi, -row->ln.lo};
  xb_dd lnm = xb_dd_add(xb_dd_mul(xb_dd_mul_d(s, 2.0), sum), lnr);
  return xb_dd_add(xb_dd_mul_d(xb_math_ln2, (double)e), lnm);
}

/* The relative error bounds of the quick forms below (Ziv's first step: when the bound cannot move the result to
 * another double, it is the correctly rounded one; otherwise the full forms decide). 2^-66 and 2^-62. */
#define XB_MATH_LOG_QUICK_ERR 1.3552527156068805e-20
#define XB_MATH_EXP_QUICK_ERR 2.168404344971009e-19

/* ln( x ) as xb_math_log_dd, the series ln( 1 + f ) = f - f^2 / 2 + f^3 (1/3 - f / 4 + ... + f^6 / 9) with only the
 * first two terms in double-double (|f| < 0.0056: the rest is below 2^-68 of the result's size, f^10 / 10 below
 * 2^-78). */
static inline xb_dd xb_math_log_quick(double x) {
  int e;
  double m;
  const struct xb_math_log_row *row = xb_math_log_reduce(x, &e, &m);
  xb_dd t = xb_dd_prod(m, row->r);
  xb_dd f = xb_dd_quick(t.hi - 1.0, t.lo); /* exact: t.hi within a factor 2 of 1; t.lo below its half unit */
  double a = f.hi;
  double tail = a * a * a *
                (1.0 / 3 + a * (-1.0 / 4 + a * (1.0 / 5 + a * (-1.0 / 6 + a * (1.0 / 7 + a * (-1.0 / 8 + a * (1.0 / 9)))))));
  xb_dd half = xb_dd_mul(f, f);
  xb_dd sq = {-0.5 * half.hi, -0.5 * half.lo}, tl = {tail, 0.0};
  xb_dd lnr = {-row->ln.hi, -row->ln.lo};
  xb_dd lnm = xb_dd_add(xb_dd_add(xb_dd_add(f, sq), tl), lnr);
  return xb_dd_add(xb_dd_mul_d(xb_math_ln2, (double)e), lnm);
}

/* e^y = v 2^q of a double-double y within -745.2 .. 709.8: y = k ln( 2 ) / 64 + r, |r| <= ln( 2 ) / 128 (ln( 2 ) /
 * 64 in three parts, the first of 32 bits, so k times it is exact), k = 64 q + j: e^y = 2^q 2^(j / 64) e^r, e^r by
 * Taylor: quick, r + r^2 / 2 + ... + r^7 / 7! in double but for r itself (below 2^-64 of e^r off), or to r^13 / 13! in
 * double-double (below 2^-120). */
#define XB_MATH_64_OVER_LN2 92.33248261689366
static const double xb_math_ln2_64[3] = {0.01083042469326756, 2.9815858269852933e-12, 1.8144105045878957e-28};

/* k = 64 q + j, j within 0 .. 63: j, and q in *q. */
static inline int xb_math_exp_index(double k, int *q) {
  int ki = (int)k, j = ((ki % 64) + 64) % 64;
  *q = (ki - j) / 64;
  return j;
}

static inline xb_dd xb_math_exp_parts(xb_dd y, int quick, int *q) {
  const double *l = xb_math_ln2_64;
  double k = xb_math_nearest(y.hi * XB_MATH_64_OVER_LN2);
  xb_dd ylo = {y.lo, 0.0}, kl3 = {-k * l[2], 0.0}, one = {1.0, 0.0};
  xb_dd r = xb_dd_add(xb_dd_sum(y.hi, -k * l[0]), ylo);
  r = xb_dd_add(xb_dd_add(r, xb_dd_prod(-k, l[1])), kl3);
  xb_dd sum;
  if (quick) {
    double a = r.hi;
    double tail = a * a * (0.5 + a * (1.0 / 6 + a * (1.0 / 24 + a * (1.0 / 120 + a * (1.0 / 720 + a * (1.0 / 5040))))));
    xb_dd tl = {tail + a * r.lo, 0.0};
    sum = xb_dd_add(one, xb_dd_add(r, tl));
  } else {
    size_t n = sizeof xb_math_fact / sizeof xb_math_fact[0];
    sum = xb_math_fact[n - 1];
    for (size_t m = n - 1; m-- > 0;) sum = xb_dd_add(xb_math_fact[m], xb_dd_mul(sum, r));
    sum = xb_dd_add(one, xb_dd_mul(sum, r)); /* 1 + r + r^2 / 2 + ... */
    sum = xb_dd_add(one, xb_dd_mul(sum, r));
  }
  return xb_dd_mul(xb_math_exp2_64[xb_math_exp_index(k, q)], sum);
}

/* v 2^q's double when a relative error `err` of v cannot change it (1), else 0. v within [1/2, 4) (e^y's parts) or q
 * = 0 with a normal result (ln's): a normal result rounds as v does, scaled once after. */
static inline int xb_math_sure(xb_dd v, int q, double err, double *r) {
  double e = fabs(v.hi) * err;
  xb_dd hi = xb_dd_quick(v.hi, v.lo + e), lo = xb_dd_quick(v.hi, v.lo - e);
  double up, down;
  if (q == 0 || (q > -1021 && q < 1022)) {
    up = hi.hi + hi.lo;
    down = lo.hi + lo.lo;
    if (up != down) return 0;
    *r = up * xb_math_pow2(q); /* exact: 2^q a normal double */
    return 1;
  }
  up = xb_dd_scale(hi, q);
  down = xb_dd_scale(lo, q);
  if (up != down) return 0;
  *r = up;
  return 1;
}

/* e^y rounded once (0 below -745.2, infinity above 709.8): the quick form when its bound, with `err` (the relative
 * error y's own error makes in e^y), settles the double; else 0 (*r unset) for full, or the full form. */
static inline int xb_math_exp_dd(xb_dd y, double err, int full, double *r) {
  if (y.hi > 709.8) {
    *r = HUGE_VAL;
    return 1;
  }
  if (y.hi < -745.2) {
    *r = 0.0;
    return 1;
  }
  int q;
  xb_dd v = xb_math_exp_parts(y, 1, &q); /* q set before it is read */
  if (xb_math_sure(v, q, err + XB_MATH_EXP_QUICK_ERR, r)) return 1;
  if (!full) return 0;
  v = xb_math_exp_parts(y, 0, &q);
  *r = xb_dd_scale(v, q);
  return 1;
}

/* The relative error bound of the first step of Exp(): 2^-58 (its error measured below 2^-60.5). */
#define XB_MATH_EXP_FAST_ERR 3.469446951953614e-18

/* e^x = v 2^q for x within -745.2 .. 709.8, the first step of Exp(): a few operations, which depend on one another
 * little, where the double-double forms chain a hundred. x = k ln( 2 ) / 64 + r as xb_math_exp_parts makes them, r
 * = a + e: a = x - k l1 exactly (k l1 has 49 bits) plus -k l2, rounded, e the rounding's error; e^r = 1 + r + r^2 / 2
 * + ... + r^7 / 7! (the next term below 2^-65), and the table's 2^(j / 64) as a double-double T: v = T.hi + (T.hi a
 * + (T.hi (t + e) + T.lo (1 + a))), t the terms from a^2 on, T.hi a exact as a double-double. Its error: the
 * rounding of the sum, below 2^-60.5 of v, the reduction's (l3 left out, k l2 rounded), about 2^-74, and what the
 * terms leave, below 2^-64. */
static inline xb_dd xb_math_exp_fast_parts(double x, int *q) {
  const double *l = xb_math_ln2_64;
  double k = xb_math_nearest(x * XB_MATH_64_OVER_LN2);
  double r = x - k * l[0]; /* exact: k l1 has 49 bits, and r is far smaller than x or k l1 */
  xb_dd s = xb_dd_sum(r, -k * l[1]);
  double a = s.hi, e = s.lo;
  double t = a * a * (0.5 + a * (1.0 / 6 + a * (1.0 / 24 + a * (1.0 / 120 + a * (1.0 / 720 + a * (1.0 / 5040))))));
  xb_dd tj = xb_math_exp2_64[xb_math_exp_index(k, q)];
  xb_dd tr = xb_dd_prod(tj.hi, a);
  return xb_dd_quick(tj.hi, tr.hi + (tr.lo + tj.hi * (t + e) + tj.lo * (1.0 + a)));
}

/* 2^-51, the factor of |x| (Exp) and |x - 1| (Log) in the first steps' bounds near the table's row of 1. */
#define XB_MATH_NEAR_ROW1_ERR 4.440892098500626e-16

/* The first step's bound for |x| below 0.005, where k is 0: r is x exactly (a = x, e = 0) and the table's value is 1,
 * so v = 1 + (x + t) with only x + t rounded (half a unit of it: below 1.003 2^-53 |x|, t below 0.51 x^2), t's own
 * rounding (below 2^-51 of t) and the terms left out (below |x|^8 / 8!, 2^-67 of |x|): all below 2^-52 |x| of v (v
 * above 0.99), which XB_MATH_NEAR_ROW1_ERR bounds twice over. Far tighter than 2^-58 for small x, whose e^x lies near
 * 1, where a value near a rounding boundary would otherwise take the full form at every call; tried only when 2^-58
 * cannot settle it (no cost on the usual path). */
#define XB_MATH_EXP_ROW1 0.005
static inline int xb_math_exp_near_row1(double x) { return fabs(x) < XB_MATH_EXP_ROW1; }
static inline double xb_math_exp_near_row1_err(double x) { return fabs(x) * XB_MATH_NEAR_ROW1_ERR; }

/* Exp( x ): the first step when its bound settles the double, else xb_math_exp_dd. */
static inline double xb_math_exp(double x) {
  if (x != x) return x;
  double r;
  if (x <= 709.8 && x >= -745.2) {
    int q;
    xb_dd v = xb_math_exp_fast_parts(x, &q); /* q set before it is read */
    if (xb_math_sure(v, q, XB_MATH_EXP_FAST_ERR, &r)) return r;
    if (xb_math_exp_near_row1(x) && xb_math_sure(v, q, xb_math_exp_near_row1_err(x), &r)) return r;
  }
  xb_dd y = {x, 0.0};
  xb_math_exp_dd(y, 0.0, 1, &r);
  return r;
}

/* The relative error bound of the first step of Log(): 2^-58 (its error measured below 2^-60.3; |a| is below 0.0056,
 * so the rest it rounds is far below the result, which is a itself for x near 1). */
#define XB_MATH_LOG_FAST_ERR 3.469446951953614e-18

/* ln( x ) of a finite x above 0, the first step of Log(): x = m 2^e (xb_math_log_reduce), f = m r_j - 1 as
 * xb_math_log_quick makes it (its high part a, exact, and its low part), and e ln( 2 ) - ln( r_j ) + a summed exactly
 * with the rest (the low parts, -a^2 / 2 + a^3 / 3 - ... - a^8 / 8 + a^9 / 9 in double, and the low part of f times
 * 1 - a) added once: a few operations, which depend on one another little. */
static inline xb_dd xb_math_log_fast(double x) {
  int e;
  double m;
  const struct xb_math_log_row *row = xb_math_log_reduce(x, &e, &m);
  xb_dd t = xb_dd_prod(m, row->r);
  double a = t.hi - 1.0, lo = t.lo; /* exact: t.hi within a factor 2 of 1 */
  double tail = 1.0 / 5 + a * (-1.0 / 6 + a * (1.0 / 7 + a * (-1.0 / 8 + a * (1.0 / 9))));
  double terms = a * a * (-0.5 + a * (1.0 / 3 + a * (-1.0 / 4 + a * tail)));
  xb_dd le = xb_dd_mul_d(xb_math_ln2, (double)e);
  xb_dd s1 = xb_dd_sum(le.hi, -row->ln.hi), s2 = xb_dd_sum(s1.hi, a);
  double rest = s1.lo + s2.lo + (le.lo - row->ln.lo) + (lo - a * lo) + terms;
  return xb_dd_sum(s2.hi, rest);
}

/* The first step's bound for |x - 1| below 0.0039 (under 1/256: m is x, e is 0, the table's row is r = 1 with ln 0):
 * a = x - 1 exactly, the low parts are 0, and the result is a plus the terms rounded (below 2^-51.4 of the terms,
 * whose size is below 0.51 a^2) with those left out (below |a|^10 / 10): below 2^-51.9 |a| of the result (whose size
 * is above 0.997 |a|), which XB_MATH_NEAR_ROW1_ERR bounds; tried, as Exp's, only when 2^-58 cannot settle it. */
#define XB_MATH_LOG_ROW1 0.0039
static inline int xb_math_log_near_row1(double x) { return fabs(x - 1.0) < XB_MATH_LOG_ROW1; }
static inline double xb_math_log_near_row1_err(double x) { return fabs(x - 1.0) * XB_MATH_NEAR_ROW1_ERR; }

/* Log( x ): 0 gives minus infinity, a negative number a NaN (C's log; the callers apply Clipper's) */
static inline double xb_math_log(double x) {
  if (x != x || x == HUGE_VAL) return x;
  if (x == 0.0) return -HUGE_VAL;
  if (x < 0.0) return NAN;
  double r;
  xb_dd fast = xb_math_log_fast(x);
  if (xb_math_sure(fast, 0, XB_MATH_LOG_FAST_ERR, &r)) return r;
  if (xb_math_log_near_row1(x) && xb_math_sure(fast, 0, xb_math_log_near_row1_err(x), &r)) return r;
  if (xb_math_sure(xb_math_log_quick(x), 0, XB_MATH_LOG_QUICK_ERR, &r)) return r;
  xb_dd l = xb_math_log_dd(x);
  return l.hi + l.lo;
}

/* Whether y is an integer (every double of 2^52 or more is). */
static inline int xb_math_is_int(double y) {
  return fabs(y) >= XB_DBL_INTEGRAL || y == xb_math_nearest(y);
}

/* Whether y is an odd integer (every double of 2^53 or more is even). */
static inline int xb_math_odd_int(double y) {
  return xb_math_is_int(y) && !xb_math_is_int(y * 0.5);
}

/* |x| ^ y of a finite x != 0 whose result is a dyadic number of few bits (where e^(y ln |x|), which only comes near
 * it, could not tell a tie between two doubles): y an integer, or n / 2^k of a normal x whose 2^k-th root is a double
 * (each square root's square exactly x); |x| = o 2^s with o odd and o^|n| below 2^64, held exactly as a double-double
 * and rounded once (1 / o^n, never a tie but for o = 1, to double-double precision). Gives 0 when it is not such a
 * case. */
static inline int xb_math_pow_exact(double ax, double y, double *r) {
  if (fabs(y) > 4096.0 || !xb_math_is_int(y * 64.0)) return 0;
  for (int k = 0; k < 6 && !xb_math_is_int(y); ++k) { /* x^(n / 2^k): the root's power */
    if (ax < ldexp(1.0, -1022)) return 0; /* a subnormal's square root squared may lose bits */
    double root = sqrt(ax);
    xb_dd square = xb_dd_prod(root, root);
    if (square.hi != ax || square.lo != 0.0) return 0;
    ax = root;
    y *= 2.0;
  }
  if (!xb_math_is_int(y)) return 0;
  int e;
  double m = frexp(ax, &e);
  uint64_t o = (uint64_t)ldexp(m, XB_DBL_EXACT_BITS);
  long s = (long)e - XB_DBL_EXACT_BITS;
  while ((o & 1) == 0) {
    o >>= 1;
    ++s;
  }
  uint64_t n = (uint64_t)fabs(y), p = 1;
  for (uint64_t j = 0; o > 1 && j < n; ++j) { /* at most 40 times: 3^41 is past 2^64 */
    if (p > UINT64_MAX / o) return 0;         /* more than 64 bits: no tie */
    p *= o;
  }
  const uint64_t low = 0xFFFFFFFFu;
  xb_dd v = xb_dd_sum((double)(p & ~low), (double)(p & low)); /* exact: 32 bits in each */
  long scale = s * (long)n;                                    /* |s| < 1200, n <= 4096 */
  if (y < 0.0) {
    xb_dd one = {1.0, 0.0};
    v = xb_dd_div(one, v);
    scale = -scale;
  }
  *r = scale > 2200 ? HUGE_VAL : scale < -2300 ? 0.0 : xb_dd_scale(v, (int)scale);
  return 1;
}

/* x ^ y with C's special cases (pow, C99 F.10.4.4), a square root for y = 1/2 (IEEE's, rounded correctly),
 * xb_math_pow_exact's cases, and e^(y ln |x|) otherwise. */
static inline double xb_math_pow(double x, double y) {
  if (y == 0.0 || x == 1.0) return 1.0;
  if (x != x || y != y) return x + y;
  int odd = x < 0.0 || x == 0.0 || x == -HUGE_VAL ? xb_math_odd_int(y) : 0; /* where the sign matters */
  double ax = fabs(x);
  /* infinities by comparison: C's isinf and signbit are type-generic macros, which convert to float on some systems */
  if (fabs(y) == HUGE_VAL) {
    if (ax == 1.0) return 1.0;
    return (ax < 1.0) == (y < 0.0) ? HUGE_VAL : 0.0;
  }
  if (x == 0.0 || ax == HUGE_VAL) {
    int big = (x == 0.0) == (y < 0.0); /* 0 to a negative power, infinity to a positive one */
    double r = big ? HUGE_VAL : 0.0;
    return odd && copysign(1.0, x) < 0.0 ? -r : r;
  }
  if (x < 0.0 && !xb_math_is_int(y)) return NAN;
  if (ax == 1.0) return odd ? -1.0 : 1.0;
  if (y == 0.5) return sqrt(ax);
  double r;
  if (xb_math_pow_exact(ax, y, &r)) return x < 0.0 && odd ? -r : r;
  xb_dd l = xb_math_log_quick(ax);
  if (fabs(y) * fabs(l.hi) > 1000.0) { /* beyond what a double holds: overflow or underflow */
    r = (y > 0.0) == (l.hi > 0.0) ? HUGE_VAL : 0.0;
  } else {
    xb_dd t = xb_dd_mul_d(l, y); /* e^t's error from ln's: t's absolute error */
    if (!xb_math_exp_dd(t, fabs(t.hi) * XB_MATH_LOG_QUICK_ERR * 2.0, 0, &r))
      xb_math_exp_dd(xb_dd_mul_d(xb_math_log_dd(ax), y), 0.0, 1, &r);
  }
  return x < 0.0 && odd ? -r : r;
}

#endif
