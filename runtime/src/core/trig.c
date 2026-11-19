/* The trigonometric and hyperbolic functions, the base-10 logarithm and the conversions between degrees and radians
 * of the CA-Tools library (rtl/numbers.c), correctly rounded as Exp() and Log() are (xbmath.h): the same doubles on
 * every system. Each computes its result as a double-double whose error bound decides the rounding (Ziv's strategy):
 * first by a fast form (Sinh() and Tanh() below |x| = 1 go straight to the full one), then by a full one (relative
 * error near 2^-100), which leaves undecided only the arguments whose result lies within about that much of a tie
 * between two doubles; those take the full form's nearest double.
 *
 * Sines and cosines reduce x to r = x - k pi / 2, |r| <= pi / 4, then r to a_j + t, a_j = j / 64 and |t| <= 1 / 128:
 * sin( r ) = S_j cos( t ) + C_j sin( t ), cos( r ) = C_j cos( t ) - S_j sin( t ), from a table of S_j = sin( a_j ) and
 * C_j = cos( a_j ) as double-doubles. Below |x| = 2^20 both forms reduce with pi / 2 in parts of 33 bits (k times a
 * part is exact), the fast one with four (two summed in double) and the full one with all five; beyond, only the full
 * form, which reduces x exactly from the bits of 2 / pi (Payne and Hanek: x 2 / pi modulo 4, with 1536 bits of 2 / pi,
 * enough for any double). The fast form sums the terms after the table's in double, the full one in double-double. Arc
 * tangents reduce u in [0, 1] to c_j = j / 64 and t = (u - c_j) / (1 + u c_j): atan( u ) = atan( c_j ) + atan( t ), the
 * series of atan( t ) to t^17. */
#include "xbrt_int.h"

#include <float.h>

static const uint32_t xb_trig_two_over_pi[48] = {
  0xA2F9836E, 0x4E441529, 0xFC2757D1, 0xF534DDC0, 0xDB629599, 0x3C439041,
  0xFE5163AB, 0xDEBBC561, 0xB7246E3A, 0x424DD2E0, 0x06492EEA, 0x09D1921C,
  0xFE1DEB1C, 0xB129A73E, 0xE88235F5, 0x2EBB4484, 0xE99C7026, 0xB45F7E41,
  0x3991D639, 0x835339F4, 0x9C845F8B, 0xBDF9283B, 0x1FF897FF, 0xDE05980F,
  0xEF2F118B, 0x5A0A6D1F, 0x6D367ECF, 0x27CB09B7, 0x4F463F66, 0x9E5FEA2D,
  0x7527BAC7, 0xEBE5F17B, 0x3D0739F7, 0x8A5292EA, 0x6BFB5FB1, 0x1F8D5D08,
  0x56033046, 0xFC7B6BAB, 0xF0CFBC20, 0x9AF4361D, 0xA9E39161, 0x5EE61B08,
  0x6599855F, 0x14A06840, 0x8DFFD880, 0x4D732731, 0x06061556, 0xCA73A8C9,
};

/* sin( j / 64 ) and cos( j / 64 ), j = 0 .. 52 (52 / 64 is past pi / 4). */
static const struct xb_trig_row {
  xb_dd s, c;
} xb_trig_sincos[] = {
  {{0.0, 0.0}, {1.0, 0.0}},
  {{0.015624364224883372, -1.2650937552759816e-19}, {0.9998779321710066, 3.216122229972341e-17}},
  {{0.03124491398532608, -1.562781562225433e-18}, {0.9995117584851364, -3.418806487972947e-17}},
  {{0.04685783574813424, -2.3419368365610254e-18}, {0.9989015683384429, -2.1425557800399754e-17}},
  {{0.0624593178423802, -2.040259504585711e-18}, {0.9980475107000991, 3.3232291674141346e-17}},
  {{0.07804555138996731, -5.449443782005793e-18}, {0.9969497940760287, -1.2467075728553626e-17}},
  {{0.09361273123551289, 1.4628632005878733e-18}, {0.9956086864580017, 3.312922430932991e-17}},
  {{0.10915705687532236, 6.6284699502736666e-18}, {0.9940245152582091, 1.3287985046260087e-17}},
  {{0.12467473338522769, -2.925947496057858e-18}, {0.992197667229329, 4.754870575189364e-17}},
  {{0.1401619723470637, -9.946847113883478e-18}, {0.9901285883701071, -4.589906353553811e-18}},
  {{0.15561499277355603, 8.886053372342288e-18}, {0.9878177838164719, 4.91917302237681e-17}},
  {{0.17103002203139503, -9.954774726452923e-18}, {0.9852658177182139, -4.925721262944555e-17}},
  {{0.18640329676226988, 2.3493796901281573e-18}, {0.9824733131012553, -3.919920375420088e-17}},
  {{0.2017310638016388, 5.587232815460113e-18}, {0.9794409517155483, 1.3108769521526758e-17}},
  {{0.21700958109501015, 1.1170071073364376e-17}, {0.9761694738686353, -7.850690609285027e-18}},
  {{0.23223511861151147, -8.318080852687206e-18}, {0.9726596782449127, 2.3920264546490165e-17}},
  {{0.24740395925452294, -7.53102495590706e-18}, {0.9689124217106447, 5.071436662403936e-17}},
  {{0.2625123997691533, -2.2534597527902125e-17}, {0.964928619104771, -3.0345542681018625e-18}},
  {{0.2775567516463363, 1.7674070262791822e-17}, {0.9607092430155619, -2.807827063516729e-17}},
  {{0.29253334202332754, 7.516944930327352e-18}, {0.9562553235431753, -3.148450868841629e-17}},
  {{0.30743851458038085, 1.1004366442765296e-19}, {0.9515679480481722, -3.8614834675674123e-17}},
  {{0.3222686304333866, 2.093773358126606e-17}, {0.9466482608860534, -3.911683334934152e-17}},
  {{0.33702006902225307, 1.0312279860787216e-17}, {0.9414974631278811, -4.8523830236797095e-18}},
  {{0.3516892289948141, -2.5616208736069942e-17}, {0.9361168122670553, -5.2350302039683216e-17}},
  {{0.36627252908604757, -9.938814562106524e-18}, {0.9305076219123143, 4.488760003328074e-18}},
  {{0.38076640899239017, 2.1372528646211374e-17}, {0.924671261467036, 5.5444125388034563e-17}},
  {{0.39516733024093426, -1.9613487871414228e-17}, {0.9186091557949183, -4.0564150104514996e-17}},
  {{0.40947177705329507, -5.679403000091266e-18}, {0.9123227848721178, 2.6349040211413332e-17}},
  {{0.42367625720393803, -2.331800700068871e-17}, {0.9058136834259364, 4.2864666490805214e-17}},
  {{0.4377773028727551, 7.64345629962023e-18}, {0.8990834405601384, 9.076951775075616e-18}},
  {{0.4517714714916838, -8.234073942098903e-18}, {0.8921336993669944, 2.3160655211380166e-17}},
  {{0.46565534658516017, 1.459870391051426e-17}, {0.8849661565261433, -7.690557775987357e-18}},
  {{0.479425538604203, -5.103969860556013e-18}, {0.8775825618903728, -4.2623149864279997e-17}},
  {{0.49307868575392305, 5.605083973871755e-18}, {0.8699847180584174, 1.657385110740923e-17}},
  {{0.5066114548142574, -3.269413423618168e-17}, {0.8621744799348805, 4.4132427578105805e-18}},
  {{0.520020541953727, -3.983266745698455e-17}, {0.8541537542773854, 5.420565102675286e-18}},
  {{0.5333026735360201, 5.129318115032044e-17}, {0.8459244992310679, 1.549506647350329e-17}},
  {{0.5464546069192036, 8.399754840929507e-18}, {0.8374887238505236, 4.3337026043948396e-17}},
  {{0.5594731312473669, 1.575565514488728e-17}, {0.8288484876093257, 1.1163935406617444e-17}},
  {{0.5723550682345072, 2.6575872357215316e-17}, {0.820005899897234, -3.912431748209128e-17}},
  {{0.5850972729404622, -5.4883972461161805e-17}, {0.8109631195052179, -3.091333486122179e-17}},
  {{0.5976966345387015, 5.450323593054385e-17}, {0.8017223540984184, 4.0134533311087014e-17}},
  {{0.6101500770757914, -1.479826990758988e-17}, {0.7922858596771786, -2.9049779312834576e-17}},
  {{0.6224545602223437, -6.049035765709707e-18}, {0.7826559400262728, -1.474071641211487e-17}},
  {{0.6346070800152693, -3.4568582392624965e-17}, {0.7728349461524715, 4.231014921891023e-17}},
  {{0.6466046695911524, 4.567647714393289e-19}, {0.7628252757105762, 1.6672995021546628e-17}},
  {{0.6584443999105676, -3.7736386700306717e-17}, {0.7526293724180665, -1.2970993013150526e-17}},
  {{0.6701233804731629, 6.183536725574959e-18}, {0.7422497254585013, -1.2339303604869521e-17}},
  {{0.6816387600233341, 4.410467313197903e-17}, {0.7316888688738209, -1.0475824306512768e-17}},
  {{0.692987727246318, -5.3543290798909455e-17}, {0.7209493809456964, 3.494986701478816e-17}},
  {{0.7041675114545337, -3.94095700584825e-17}, {0.7100338835660797, 1.505272211891291e-17}},
  {{0.7151753832640076, -1.466099578328228e-17}, {0.6989450415971057, -5.5261332036460915e-18}},
  {{0.7260086552607126, -1.573621815339587e-17}, {0.6876855622205048, 3.5430696752823923e-17}}
};

/* atan( j / 64 ), j = 0 .. 64. */
static const xb_dd xb_trig_atan[] = {
  {0.0, 0.0}, {0.015623728620476831, -4.913600136566304e-19}, {0.031239833430268277, -1.188442711587748e-18},
  {0.046840712915969654, -1.655677442254952e-19}, {0.06241880999595735, -1.5490756308295046e-18},
  {0.0779666338315423, 5.804551873143357e-18}, {0.09347678115858947, -6.2844725995420954e-18},
  {0.10894195698986579, 6.8267122072409585e-18}, {0.12435499454676144, -3.1253241424539383e-18},
  {0.13970887428916365, -2.9579864247315813e-18}, {0.15499674192394097, 9.585415594114324e-18},
  {0.1702119252854744, -3.541164079802125e-18}, {0.18534794999569476, 4.180692268843079e-18},
  {0.2003985538258785, 3.1399542871844493e-18}, {0.21535769969773805, 4.738160130078733e-19},
  {0.23021958727684372, 1.2313404529142703e-17}, {0.24497866312686414, 1.0698755618734451e-17},
  {0.2596296294082575, 1.9238754924615304e-17}, {0.2741674511196588, 8.261353575163773e-18},
  {0.2885873618940774, -1.428369957377257e-17}, {0.3028848683749714, -1.1010827903001369e-17},
  {0.31705575320914703, -1.893928924292642e-17}, {0.3310960767041321, -7.952610375793799e-18},
  {0.34500217720710513, -2.2938804755578304e-17}, {0.35877067027057225, -2.4623815582638635e-17},
  {0.3723984466767542, 1.9612311504845653e-17}, {0.38588266939807375, 2.378822732491941e-17},
  {0.39922076957525254, 2.246598105617042e-17}, {0.4124104415973873, -1.587652227770689e-17},
  {0.42544963737004227, 2.3315530741892885e-17}, {0.43833655985795783, -2.494277030626541e-17},
  {0.4510696559885235, -2.2703795229420475e-17}, {0.4636476090008061, 2.2698777452961687e-17},
  {0.4760693303227612, 1.4654487332256713e-17}, {0.48833395105640554, -1.1373236189329585e-17},
  {0.5004408131472942, -4.7181675085518756e-17}, {0.5123894603107377, -2.5462781472855804e-17},
  {0.5241796287829132, 5.520094119641666e-18}, {0.5358112379604637, -4.0637956834825575e-18},
  {0.5472843809874369, 4.923709671396255e-17}, {0.5585993153435624, -5.4556305485916264e-18},
  {0.5697564534829784, 1.2255062085054184e-17}, {0.5807563535676704, -1.441464378193067e-17},
  {0.5915997103351114, 4.920495453686772e-17}, {0.6022873461349642, 2.950430737228402e-17},
  {0.6128202021652414, -3.1552061848586226e-17}, {0.6231993299340659, 2.672403885140095e-17},
  {0.6334258829691446, -2.7290767436015276e-17}, {0.6435011087932844, 1.5834785051444286e-17},
  {0.6534263411807619, 3.5800634857340095e-17}, {0.6632029927060933, -3.076054864429649e-17},
  {0.6728325475937632, -1.899315009714705e-17}, {0.6823165548747481, 6.943223671560008e-18},
  {0.6916566218531999, -8.117151192285796e-18}, {0.7008544078844502, -1.987626234335816e-17},
  {0.7099116184635249, -4.597166450584887e-17}, {0.7188299996216245, -2.1478388444456983e-17},
  {0.7276113326265107, 2.569325697391839e-18}, {0.7362574289814281, 3.473937648299457e-17},
  {0.7447701257160751, 3.708315849135547e-17}, {0.7531512809621944, -2.4256934659182068e-17},
  {0.7614027698055784, 9.850030332752822e-18}, {0.7695264804056583, -3.704991905602721e-17},
  {0.7775243103733478, -2.6676490951944502e-17}, {0.7853981633974483, 3.061616997868383e-17}
};

static const xb_dd xb_trig_pi_2 = {1.5707963267948966, 6.123233995736766e-17};
static const xb_dd xb_trig_pi = {XB_PI, 1.2246467991473532e-16};
static const xb_dd xb_trig_pi_180 = {0.017453292519943295, 2.9486522708701687e-19};
static const xb_dd xb_trig_180_pi = {57.29577951308232, -1.9878495670576283e-15};
static const xb_dd xb_trig_inv_ln10 = {0.4342944819032518, 1.098319650216765e-17};

/* pi / 2 in parts: the first four of 33 bits, so that k times one, k below 2^20, is exact. */
static const double xb_trig_cw[5] = {1.5707963267341256, 6.077100506303966e-11, 2.0222662487111665e-21,
                                     8.478427660348229e-32, 2.0670321098263988e-43};

/* Below this magnitude sin, tan, atan, asin, sinh and tanh of x round to x, and cos and cosh to 1: their next term is
 * below a quarter of a unit in the last place (2^-27). */
#define XB_TRIG_TINY 7.450580596923828e-09
/* Cody and Waite's reduction takes |x| below this (2^20); Payne and Hanek's any x. */
#define XB_TRIG_FAST_MAX 1048576.0
/* 2 / pi and pi / 4 as doubles; the tables' step, 1 / 64; the least |r| the fast form takes (2^-30: below, the
 * reduction's error would be too large a part of r). */
#define XB_TRIG_2_PI 0.6366197723675814
#define XB_TRIG_PI_4 0.7853981633974483
#define XB_TRIG_STEPS 64.0
#define XB_TRIG_FAST_MIN_R 9.313225746154785e-10
/* The relative error bounds: the fast forms' of sine, cosine and arc tangent (2^-62: their errors measured below
 * 2^-65.4 and 2^-66.5 of the result), of Tan() and Cot(), a quotient of two (2^-61), of those built on
 * xbmath.h's fast and quick forms, Log10() and the hyperbolic functions (2^-57), and the full forms' (2^-96). */
#define XB_TRIG_FAST_ERR 2.168404344971009e-19
#define XB_TRIG_TAN_ERR 4.336808689942018e-19
#define XB_TRIG_QUICK_ERR 6.938893903907228e-18
#define XB_TRIG_FULL_ERR 1.262177448353619e-29
/* Words of 2 / pi that one reduction multiplies (256 bits), and those of the table. */
#define XB_TRIG_WINDOW 8
#define XB_TRIG_WORDS ((int)(sizeof xb_trig_two_over_pi / sizeof xb_trig_two_over_pi[0]))

/* The double the double-double v rounds to: the correctly rounded one when an error of `err` (relative) cannot change
 * it, else v's nearest (an argument within about that of a tie). */
static double settle(xb_dd v, double err) {
  double r;
  if (xb_math_sure(v, 0, err, &r)) return r;
  return v.hi + v.lo;
}

static double settle_scaled(xb_dd v, int q, double err);

/* a / b for the fast forms: two quotient digits, the second from what the first leaves (a relative error near
 * 2^-104, one division less than xb_dd_div's three digits). */
static xb_dd div2(xb_dd a, xb_dd b) {
  double q1 = a.hi / b.hi;
  xb_dd p = xb_dd_prod(q1, b.hi);
  double rest = ((a.hi - p.hi) - p.lo) + a.lo - q1 * b.lo;
  return xb_dd_quick(q1, rest / b.hi);
}

/* Bits [pos, pos + n) of the little-endian 32-bit limbs p (n at most 32; bits outside the `count` limbs are 0). */
static uint64_t bits_at(const uint32_t *p, int count, int pos, int n) {
  uint64_t v = 0;
  for (int k = n - 1; k >= 0; --k) {
    int b = pos + k;
    v = v << 1 | (b >= 0 && b / 32 < count ? (p[b / 32] >> (b % 32)) & 1u : 0u);
  }
  return v;
}

/* x = k pi / 2 + r exactly enough for any finite x (|x| at least pi / 4): k modulo 4, r as a double-double with a
 * relative error near 2^-104. x 2 / pi modulo 4 is the product of x's 53-bit significand m and the 256 bits of 2 / pi
 * from where m's weight leaves only multiples of 4 above (Payne and Hanek), in integers; its integer part's low two
 * bits give k, rounded to the nearest with its fraction f (|f| <= 1/2), and r = f pi / 2. */
static int reduce_full(double x, xb_dd *r) {
  int ex;
  double fr = frexp(fabs(x), &ex);
  uint64_t m = (uint64_t)ldexp(fr, XB_DBL_EXACT_BITS);
  int e = ex - XB_DBL_EXACT_BITS; /* |x| = m 2^e */
  int first = e >= 2 ? (e - 2) / 32 : 0; /* the words before it give multiples of 4 */
  uint32_t t[XB_TRIG_WINDOW], p[XB_TRIG_WINDOW + 2] = {0};
  for (int k = 0; k < XB_TRIG_WINDOW; ++k) {
    int w = first + XB_TRIG_WINDOW - 1 - k;
    t[k] = w < XB_TRIG_WORDS ? xb_trig_two_over_pi[w] : 0u;
  }
  uint32_t mw[2] = {(uint32_t)m, (uint32_t)(m >> 32)};
  for (int i = 0; i < 2; ++i) {
    uint64_t carry = 0;
    for (int k = 0; k < XB_TRIG_WINDOW; ++k) {
      uint64_t v = (uint64_t)mw[i] * t[k] + p[i + k] + carry;
      p[i + k] = (uint32_t)v;
      carry = v >> 32;
    }
    for (int k = i + XB_TRIG_WINDOW; carry && k < XB_TRIG_WINDOW + 2; ++k) {
      uint64_t v = (uint64_t)p[k] + carry;
      p[k] = (uint32_t)v;
      carry = v >> 32;
    }
  }
  /* x 2 / pi = p 2^-s: s bits of fraction */
  int s = 32 * (first + XB_TRIG_WINDOW) - e, count = XB_TRIG_WINDOW + 2;
  int k = (int)bits_at(p, count, s, 2);
  int negative = (int)bits_at(p, count, s - 1, 1);
  if (negative) { /* the fraction past 1/2: k + 1, and f = fraction - 1, its magnitude 2^s - fraction */
    k = (k + 1) & 3;
    uint64_t borrow = 1;
    for (int i = 0; i < count; ++i) {
      uint64_t v = (uint64_t)(uint32_t)~p[i] + borrow;
      p[i] = (uint32_t)v;
      borrow = v >> 32;
    }
  }
  int top = s - 1;
  while (top >= 32 && !bits_at(p, count, top - 31, 32)) top -= 32; /* a word at a time, then a bit */
  while (top >= 0 && !bits_at(p, count, top, 1)) --top;
  if (top < 0) {
    r->hi = r->lo = 0.0;
    return k;
  }
  /* the magnitude's first 53 bits and the next 53, truncated (a relative error below 2^-105) */
  int lead = XB_DBL_EXACT_BITS;
  uint64_t hi = bits_at(p, count, top - lead + 1, 32) | bits_at(p, count, top - lead + 33, lead - 32) << 32;
  uint64_t lo = bits_at(p, count, top - 2 * lead + 1, 32) | bits_at(p, count, top - 2 * lead + 33, lead - 32) << 32;
  xb_dd f = xb_dd_quick(ldexp((double)hi, top - lead + 1 - s), ldexp((double)lo, top - 2 * lead + 1 - s));
  if (negative) f = (xb_dd){-f.hi, -f.lo};
  *r = xb_dd_mul(f, xb_trig_pi_2);
  if (x < 0) {
    *r = (xb_dd){-r->hi, -r->lo};
    k = (4 - k) & 3;
  }
  return k;
}

/* sin( r ) and cos( r ) of |r| <= pi / 4 + 2^-20 in double-double (the full form). */
static void sincos_full(xb_dd r, xb_dd *sn, xb_dd *cs) {
  int neg = r.hi < 0;
  if (neg) r = (xb_dd){-r.hi, -r.lo};
  int j = (int)xb_math_nearest(r.hi * XB_TRIG_STEPS);
  xb_dd minus = {-j / XB_TRIG_STEPS, 0.0}, one = {1.0, 0.0};
  xb_dd t = xb_dd_add(r, minus), z = xb_dd_mul(t, t);
  /* sin( t ) = t (1 - z / 3! + z^2 / 5! - ... - z^5 / 11!), cos( t ) - 1 = z (-1 / 2! + z / 4! - ... + z^5 / 12!) */
  xb_dd ps = {0.0, 0.0}, pc = {0.0, 0.0};
  for (int n = 11; n >= 3; n -= 2) {
    xb_dd c = xb_math_fact[n - 2];
    if ((n / 2) % 2) c = (xb_dd){-c.hi, -c.lo};
    ps = xb_dd_add(c, xb_dd_mul(ps, z));
  }
  for (int n = 12; n >= 2; n -= 2) {
    xb_dd c = xb_math_fact[n - 2];
    if ((n / 2) % 2) c = (xb_dd){-c.hi, -c.lo};
    pc = xb_dd_add(c, xb_dd_mul(pc, z));
  }
  xb_dd st = xb_dd_mul(t, xb_dd_add(one, xb_dd_mul(ps, z))), ctm = xb_dd_mul(pc, z);
  const struct xb_trig_row *row = &xb_trig_sincos[j];
  /* S cos t + C sin t = S + S (cos t - 1) + C sin t; C cos t - S sin t = C + C (cos t - 1) - S sin t */
  *sn = xb_dd_add(row->s, xb_dd_add(xb_dd_mul(row->s, ctm), xb_dd_mul(row->c, st)));
  xb_dd sst = xb_dd_mul(row->s, st);
  *cs = xb_dd_add(row->c, xb_dd_add(xb_dd_mul(row->c, ctm), (xb_dd){-sst.hi, -sst.lo}));
  if (neg) *sn = (xb_dd){-sn->hi, -sn->lo};
}

/* The fast form's sin( r ) and cos( r ) of a double-double r, |r| <= pi / 4 + 2^-20 and above 2^-30 (when smaller,
 * its relative error would be too large: 0, and the full form takes it). The table's terms as double-doubles, the
 * rest in double: S + (S (cos t - 1) + C t + C (sin t - t)) with C t exact. */
static int sincos_fast(xb_dd r, xb_dd *sn, xb_dd *cs) {
  int neg = r.hi < 0;
  if (neg) r = (xb_dd){-r.hi, -r.lo};
  if (r.hi < XB_TRIG_FAST_MIN_R) return 0;
  int j = (int)xb_math_nearest(r.hi * XB_TRIG_STEPS);
  xb_dd t = xb_dd_sum(r.hi, -j / XB_TRIG_STEPS); /* exact (a two-sum) */
  t.lo += r.lo;
  double a = t.hi, z = a * a;
  double sinRest = a * z * (-1.0 / 6 + z * (1.0 / 120 + z * (-1.0 / 5040 + z / 362880)));
  double cosRest = z * (-0.5 + z * (1.0 / 24 + z * (-1.0 / 720 + z / 40320)));
  const struct xb_trig_row *row = &xb_trig_sincos[j];
  xb_dd ct = xb_dd_prod(row->c.hi, a);
  xb_dd s1 = xb_dd_sum(row->s.hi, ct.hi);
  double tlo = t.lo * a; /* cos( t ) - 1 at t.hi + t.lo: its derivative's term, -t.hi t.lo */
  double sRest = s1.lo + ct.lo + row->s.lo + row->s.hi * (cosRest - tlo) + row->c.hi * (t.lo + sinRest) + row->c.lo * a;
  *sn = xb_dd_quick(s1.hi, sRest);
  xb_dd st = xb_dd_prod(row->s.hi, a);
  xb_dd c1 = xb_dd_sum(row->c.hi, -st.hi);
  double cRest = c1.lo - st.lo + row->c.lo + row->c.hi * (cosRest - tlo) - row->s.hi * (t.lo + sinRest) - row->s.lo * a;
  *cs = xb_dd_quick(c1.hi, cRest);
  if (neg) *sn = (xb_dd){-sn->hi, -sn->lo};
  return 1;
}

/* The fast form's reduction of |x| below 2^20: x - k pi / 2 with the parts of 33 bits (x - k p1 exact: the two are
 * within a factor 2), as a double-double. */
static int reduce_fast(double x, xb_dd *r) {
  double k = xb_math_nearest(x * XB_TRIG_2_PI);
  xb_dd a = xb_dd_sum(x - k * xb_trig_cw[0], -k * xb_trig_cw[1]);
  a.lo -= k * xb_trig_cw[2] + k * xb_trig_cw[3];
  *r = xb_dd_quick(a.hi, a.lo);
  return (int)((int64_t)k & 3);
}

/* The full form's reduction of |x| below 2^20: x - k pi / 2 with all five parts of pi / 2 (185 bits) in
 * double-double, the first four products exact: an absolute error below 2^-160. */
static int reduce_parts(double x, xb_dd *r) {
  double k = xb_math_nearest(x * XB_TRIG_2_PI);
  xb_dd a = xb_dd_sum(x - k * xb_trig_cw[0], -k * xb_trig_cw[1]);
  a = xb_dd_add(a, (xb_dd){-k * xb_trig_cw[2], 0.0});
  a = xb_dd_add(a, (xb_dd){-k * xb_trig_cw[3], 0.0});
  *r = xb_dd_add(a, xb_dd_prod(-k, xb_trig_cw[4]));
  return (int)((int64_t)k & 3);
}

/* sin( x ) (which 0), cos( x ) (1), tan( x ) (2) or cot( x ) (3) as a double-double for the quadrant k and the
 * reduced r; a quotient by div2 for the fast form. */
static xb_dd quadrant(int which, int k, xb_dd sn, xb_dd cs, int fast) {
  xb_dd neg_sn = {-sn.hi, -sn.lo}, neg_cs = {-cs.hi, -cs.lo};
  xb_dd (*div)(xb_dd, xb_dd) = fast ? div2 : xb_dd_div;
  switch (which) {
    case 0: return k == 0 ? sn : k == 1 ? cs : k == 2 ? neg_sn : neg_cs;
    case 1: return k == 0 ? cs : k == 1 ? neg_sn : k == 2 ? neg_cs : sn;
    case 2: return k % 2 ? div(neg_cs, sn) : div(sn, cs);
    default: return k % 2 ? div(neg_sn, cs) : div(cs, sn);
  }
}

/* cot( x ) of |x| below 2^-27: 1 / x (1 - x^2 / 3), the next term below 2^-108 of it; 1 / x computed for x scaled by
 * 2^600 (its exact products would overflow beyond 2^996) and scaled back in the rounding, an infinity past the
 * doubles (0 gives one with its sign). */
static double cot_tiny(double x) {
  if (x == 0.0) return copysign(HUGE_VAL, x);
  xb_dd v = xb_dd_div((xb_dd){1.0, 0.0}, (xb_dd){x * xb_math_pow2(600), 0.0});
  v = xb_dd_mul(v, (xb_dd){1.0, -x * x / 3.0});
  return settle_scaled(v, 600, XB_TRIG_FULL_ERR);
}

static double trig(double x, int which) {
  if (!xb_dbl_finite(x)) return NAN;
  if (fabs(x) < XB_TRIG_TINY) return which == 3 ? cot_tiny(x) : which == 1 ? 1.0 : x;
  xb_dd r, sn, cs;
  int k;
  if (fabs(x) < XB_TRIG_FAST_MAX) {
    k = reduce_fast(x, &r);
    if (sincos_fast(r, &sn, &cs)) {
      double v;
      if (xb_math_sure(quadrant(which, k, sn, cs, 1), 0, which >= 2 ? XB_TRIG_TAN_ERR : XB_TRIG_FAST_ERR, &v)) return v;
    }
  }
  if (fabs(x) <= XB_TRIG_PI_4) {
    k = 0;
    r = (xb_dd){x, 0.0};
  } else if (fabs(x) < XB_TRIG_FAST_MAX) {
    k = reduce_parts(x, &r);
  } else {
    k = reduce_full(x, &r);
  }
  sincos_full(r, &sn, &cs);
  return settle(quadrant(which, k, sn, cs, 0), XB_TRIG_FULL_ERR);
}

double xb_sin(double x) { return trig(x, 0); }
double xb_cos(double x) { return trig(x, 1); }
double xb_tan(double x) { return trig(x, 2); }
double xb_cot(double x) { return trig(x, 3); }

/* atan( u ) of a double-double u in [0, 1]: atan( c_j ) + atan( t ), t = (u - c_j) / (1 + u c_j), |t| <= 1/128 + ...,
 * the series t (1 - t^2 / 3 + t^4 / 5 - ... + t^16 / 17), below 2^-110 after it. */
static xb_dd atan_unit(xb_dd u) {
  int j = (int)xb_math_nearest(u.hi * XB_TRIG_STEPS);
  double c = j / XB_TRIG_STEPS;
  xb_dd one = {1.0, 0.0};
  xb_dd num = xb_dd_add(u, (xb_dd){-c, 0.0}), den = xb_dd_add(one, xb_dd_mul_d(u, c));
  xb_dd t = xb_dd_div(num, den), z = xb_dd_mul(t, t), p = {0.0, 0.0};
  size_t n = sizeof xb_math_odd / sizeof xb_math_odd[0];
  for (size_t i = n; i-- > 0;) {
    xb_dd coef = xb_math_odd[i];
    if (i % 2 == 0) coef = (xb_dd){-coef.hi, -coef.lo}; /* 1/3 negative, 1/5 positive, ... */
    p = xb_dd_add(coef, xb_dd_mul(p, z));
  }
  xb_dd at = xb_dd_mul(t, xb_dd_add(one, xb_dd_mul(p, z)));
  return xb_dd_add(xb_trig_atan[j], at);
}

/* The fast form of atan_unit: t as a double-double, its series after t in double (error measured below 2^-66.5). */
static xb_dd atan_unit_fast(xb_dd u) {
  int j = (int)xb_math_nearest(u.hi * XB_TRIG_STEPS);
  double c = j / XB_TRIG_STEPS;
  xb_dd num = xb_dd_sum(u.hi, -c);
  num.lo += u.lo;
  xb_dd den = xb_dd_add((xb_dd){1.0, 0.0}, xb_dd_mul_d(u, c));
  xb_dd t = div2(num, den);
  double a = t.hi, z = a * a;
  double tail = a * z * (-1.0 / 3 + z * (1.0 / 5 + z * (-1.0 / 7 + z * (1.0 / 9 + z * (-1.0 / 11 + z / 13)))));
  xb_dd sum = xb_dd_sum(xb_trig_atan[j].hi, a);
  return xb_dd_quick(sum.hi, sum.lo + (xb_trig_atan[j].lo + t.lo + tail));
}

/* atan2( y, x ) as a double-double, as C's atan2 for finite arguments (0 for 0, 0; the signs of zeros kept): by the
 * fast form of atan_unit, or the full one. */
static xb_dd atan2_dd(xb_dd y, xb_dd x, int fast) {
  /* the signs by copysign: C's signbit is a type-generic macro, which converts to float on some systems */
  int yneg = copysign(1.0, y.hi) < 0, xneg = copysign(1.0, x.hi) < 0;
  xb_dd ay = yneg ? (xb_dd){-y.hi, -y.lo} : y, ax = xneg ? (xb_dd){-x.hi, -x.lo} : x, v;
  if (ay.hi == 0.0) {
    v = xneg ? xb_trig_pi : (xb_dd){0.0, 0.0};
  } else if (ay.hi <= ax.hi) {
    xb_dd u = ax.hi == 1.0 && ax.lo == 0.0 ? ay : fast ? div2(ay, ax) : xb_dd_div(ay, ax);
    v = fast ? atan_unit_fast(u) : atan_unit(u);
    if (xneg) v = xb_dd_add(xb_trig_pi, (xb_dd){-v.hi, -v.lo});
  } else {
    xb_dd u = fast ? div2(ax, ay) : xb_dd_div(ax, ay);
    v = fast ? atan_unit_fast(u) : atan_unit(u);
    v = xneg ? xb_dd_add(xb_trig_pi_2, v) : xb_dd_add(xb_trig_pi_2, (xb_dd){-v.hi, -v.lo});
  }
  return yneg ? (xb_dd){-v.hi, -v.lo} : (xb_dd){v.hi, v.lo};
}

/* atan2( y, x ) rounded once: the fast form when its bound settles the double, else the full one. A y of zero gives
 * zero or pi with y's sign, as C's atan2 (the rounding would lose a zero's sign). Both scaled first by 2^-200 when the
 * larger is past 2^900, or by 2^200 when it is below 2^-900 (the angle is the same): the quotients' exact products
 * would overflow, or lose bits among the subnormals; a smaller one that the scaling makes vanish leaves an angle
 * that rounds to 0 or pi anyway. */
static double atan2_settled(xb_dd y, xb_dd x) {
  if (y.hi == 0.0) return copysign(1.0, x.hi) < 0 ? copysign(xb_trig_pi.hi, y.hi) : y.hi;
  double m = fmax(fabs(y.hi), fabs(x.hi));
  int shift = m > xb_math_pow2(900) ? -200 : m < xb_math_pow2(-900) ? 200 : 0;
  if (shift) {
    double s = xb_math_pow2(shift);
    y = (xb_dd){y.hi * s, y.lo * s};
    x = (xb_dd){x.hi * s, x.lo * s};
  }
  double r;
  if (xb_math_sure(atan2_dd(y, x, 1), 0, XB_TRIG_FAST_ERR, &r)) return r;
  return settle(atan2_dd(y, x, 0), XB_TRIG_FULL_ERR);
}

double xb_atan(double x) {
  if (x != x) return x;
  if (fabs(x) < XB_TRIG_TINY) return x;
  if (!xb_dbl_finite(x)) return x > 0 ? xb_trig_pi_2.hi : -xb_trig_pi_2.hi;
  return atan2_settled((xb_dd){x, 0.0}, (xb_dd){1.0, 0.0});
}

double xb_atan2(double y, double x) {
  if (x != x || y != y) return NAN;
  if (!xb_dbl_finite(x) || !xb_dbl_finite(y)) return atan2(y, x); /* the infinities: exact multiples of pi / 4 */
  return atan2_settled((xb_dd){y, 0.0}, (xb_dd){x, 0.0});
}

/* sqrt( 1 - x^2 ) as a double-double, |x| <= 1: (1 - x) (1 + x) exactly as double-doubles, then a Newton step. */
static xb_dd cofactor(double x) {
  xb_dd w = xb_dd_mul(xb_dd_sum(1.0, -x), xb_dd_sum(1.0, x));
  if (w.hi <= 0.0) return (xb_dd){0.0, 0.0};
  double s = sqrt(w.hi);
  xb_dd sq = xb_dd_prod(s, s);
  double corr = ((w.hi - sq.hi) - sq.lo + w.lo) / (2.0 * s);
  return xb_dd_sum(s, corr);
}

double xb_asin(double x) {
  if (!(fabs(x) <= 1.0)) return NAN;
  if (fabs(x) < XB_TRIG_TINY) return x;
  return atan2_settled((xb_dd){x, 0.0}, cofactor(x));
}

double xb_acos(double x) {
  if (!(fabs(x) <= 1.0)) return NAN;
  return atan2_settled(cofactor(x), (xb_dd){x, 0.0});
}

/* e^|x| = E 2^q (xb_math_exp_parts: the quick form, relative error below 2^-62, or the full one, below 2^-120), |x|
 * at most 40: E 2^q as one double-double. */
static xb_dd exp_dd(double ax, int quick) {
  int q;
  xb_dd y = {ax, 0.0}, v = xb_math_exp_parts(y, quick, &q);
  double scale = xb_math_pow2(q);
  return (xb_dd){v.hi * scale, v.lo * scale};
}

/* sinh( x ) of |x| below 1/32 as a double-double: x (1 + z / 3! + z^2 / 5! + ... + z^6 / 13!), z = x^2 (the next
 * term below 2^-110 of it). */
static xb_dd sinh_series(double x) {
  xb_dd z = xb_dd_prod(x, x), p = {0.0, 0.0}, one = {1.0, 0.0};
  for (int n = 13; n >= 3; n -= 2) p = xb_dd_add(xb_math_fact[n - 2], xb_dd_mul(p, z));
  return xb_dd_mul_d(xb_dd_add(one, xb_dd_mul(p, z)), x);
}

/* sinh (which 0), cosh (1) and tanh (2) of |x| from 2^-27. Up to |x| = 40: E = e^|x| (relative error below
 * 2^-120), cosh = (E + 1 / E) / 2, and from |x| = 1/32 D = E - 1 (its error below 2^-115 of it), sinh = D (E + 1) /
 * (2 E), tanh = D (E + 1) / (E^2 + 1); below 1/32 sinh by its series, tanh = sinh / cosh. Beyond 40 the term e^-|x|
 * is below 2^-115 of the result: sinh and cosh are e^(|x| - ln 2), tanh rounds to 1. */
static double hyperbolic(double x, int which) {
  if (x != x) return x;
  double ax = fabs(x), sign = x < 0 ? -1.0 : 1.0;
  if (ax < XB_TRIG_TINY) return which == 1 ? 1.0 : x;
  if (!xb_dbl_finite(x)) return which == 1 ? HUGE_VAL : which == 0 ? x : sign;
  xb_dd v;
  if (ax > 40.0) {
    if (which == 2) return sign;
    if (ax > 710.5) return which == 1 ? HUGE_VAL : sign * HUGE_VAL; /* past the largest double */
    int q;
    xb_dd y = xb_dd_add((xb_dd){ax, 0.0}, (xb_dd){-xb_math_ln2.hi, -xb_math_ln2.lo});
    v = xb_math_exp_parts(y, 0, &q);
    double r;
    if (!xb_math_sure(v, q, XB_TRIG_FULL_ERR, &r)) r = xb_dd_scale(v, q);
    return which == 1 ? r : sign * r;
  }
  for (int quick = which == 1 || ax >= 1.0;; quick = 0) {
    xb_dd e = exp_dd(ax, quick), one = {1.0, 0.0};
    xb_dd (*div)(xb_dd, xb_dd) = quick ? div2 : xb_dd_div;
    xb_dd cosh2 = xb_dd_add(e, div(one, e)); /* 2 cosh */
    if (which == 1) {
      v = xb_dd_mul_d(cosh2, 0.5);
    } else if (ax < 0.03125) {
      v = sinh_series(ax);
      if (which == 2) v = div(xb_dd_mul_d(v, 2.0), cosh2);
    } else {
      xb_dd d = xb_dd_add(e, (xb_dd){-1.0, 0.0}), ep1 = xb_dd_add(e, one);
      if (which == 0) v = div(xb_dd_mul(d, ep1), xb_dd_mul_d(e, 2.0));
      else v = div(xb_dd_mul(d, ep1), xb_dd_add(xb_dd_mul(e, e), one));
    }
    double r;
    /* the quick E's error, 2^-62 of E, is below 2^-61 of D = E - 1 from |x| = 1 on (cosh has no D) */
    if (quick && !xb_math_sure(v, 0, XB_TRIG_QUICK_ERR, &r)) continue;
    if (!quick) r = settle(v, XB_TRIG_FULL_ERR);
    return which == 1 ? r : sign * r;
  }
}

double xb_sinh(double x) { return hyperbolic(x, 0); }
double xb_cosh(double x) { return hyperbolic(x, 1); }
double xb_tanh(double x) { return hyperbolic(x, 2); }

/* log10( x ): ln( x ) / ln( 10 ) in double-double (exact powers of ten come out exact); 0 gives minus infinity, a
 * negative x a NaN. */
double xb_log10(double x) {
  if (x != x || x == HUGE_VAL) return x;
  if (x == 0.0) return -HUGE_VAL;
  if (x < 0.0) return NAN;
  double r;
  if (xb_math_sure(xb_dd_mul(xb_math_log_fast(x), xb_trig_inv_ln10), 0, XB_TRIG_QUICK_ERR, &r)) return r;
  return settle(xb_dd_mul(xb_math_log_dd(x), xb_trig_inv_ln10), XB_TRIG_FULL_ERR);
}

/* v 2^q rounded once, any v and q: v scaled into [1/2, 1) first, so that xb_math_sure rounds a subnormal or an
 * overflowing result as well (xb_dd_scale). */
static double settle_scaled(xb_dd v, int q, double err) {
  if (v.hi == 0.0) return v.hi;
  int e;
  (void)frexp(v.hi, &e);
  double s = xb_math_pow2(-e);
  xb_dd w = {v.hi * s, v.lo * s};
  double r;
  if (xb_math_sure(w, q + e, err, &r)) return r;
  return xb_dd_scale(w, q + e);
}

/* x pi / 180 and x 180 / pi, rounded once: the product of the constant and x as a double-double, x first scaled by
 * 2^-100 or 2^100 near the ends of the doubles (an overflowing split, a subnormal product), the scale given back in
 * the rounding. */
static double scaled(double x, xb_dd c) {
  if (!xb_dbl_finite(x) || x == 0.0) return x * c.hi;
  int shift = fabs(x) > xb_math_pow2(900) ? 100 : fabs(x) < xb_math_pow2(-900) ? -100 : 0;
  return settle_scaled(xb_dd_mul_d(c, x * xb_math_pow2(-shift)), shift, XB_TRIG_FULL_ERR);
}

double xb_dtor(double x) { return scaled(x, xb_trig_pi_180); }
double xb_rtod(double x) { return scaled(x, xb_trig_180_pi); }

/* n! of n from 21 to 170, correctly rounded (21! is the first past 2^63; 171! is past the largest double). */
static const double xb_trig_factorials[] = {
    5.109094217170944e+19, 1.1240007277776077e+21, 2.585201673888498e+22, 6.204484017332394e+23,
    1.5511210043330986e+25, 4.0329146112660565e+26, 1.0888869450418352e+28, 3.0488834461171387e+29,
    8.841761993739702e+30, 2.6525285981219107e+32, 8.222838654177922e+33, 2.631308369336935e+35, 8.683317618811886e+36,
    2.9523279903960416e+38, 1.0333147966386145e+40, 3.7199332678990125e+41, 1.3763753091226346e+43,
    5.230226174666011e+44, 2.0397882081197444e+46, 8.159152832478977e+47, 3.345252661316381e+49, 1.40500611775288e+51,
    6.041526306337383e+52, 2.658271574788449e+54, 1.1962222086548019e+56, 5.502622159812089e+57,
    2.5862324151116818e+59, 1.2413915592536073e+61, 6.082818640342675e+62, 3.0414093201713376e+64,
    1.5511187532873822e+66, 8.065817517094388e+67, 4.2748832840600255e+69, 2.308436973392414e+71,
    1.2696403353658276e+73, 7.109985878048635e+74, 4.0526919504877214e+76, 2.3505613312828785e+78,
    1.3868311854568984e+80, 8.32098711274139e+81, 5.075802138772248e+83, 3.146997326038794e+85, 1.98260831540444e+87,
    1.2688693218588417e+89, 8.247650592082472e+90, 5.443449390774431e+92, 3.647111091818868e+94,
    2.4800355424368305e+96, 1.711224524281413e+98, 1.1978571669969892e+100, 8.504785885678623e+101,
    6.1234458376886085e+103, 4.4701154615126844e+105, 3.307885441519386e+107, 2.48091408113954e+109,
    1.8854947016660504e+111, 1.4518309202828587e+113, 1.1324281178206297e+115, 8.946182130782976e+116,
    7.156945704626381e+118, 5.797126020747368e+120, 4.753643337012842e+122, 3.945523969720659e+124,
    3.314240134565353e+126, 2.81710411438055e+128, 2.4227095383672734e+130, 2.107757298379528e+132,
    1.8548264225739844e+134, 1.650795516090846e+136, 1.4857159644817615e+138, 1.352001527678403e+140,
    1.2438414054641308e+142, 1.1567725070816416e+144, 1.087366156656743e+146, 1.032997848823906e+148,
    9.916779348709496e+149, 9.619275968248212e+151, 9.426890448883248e+153, 9.332621544394415e+155,
    9.332621544394415e+157, 9.42594775983836e+159, 9.614466715035127e+161, 9.90290071648618e+163,
    1.0299016745145628e+166, 1.081396758240291e+168, 1.1462805637347084e+170, 1.226520203196138e+172,
    1.324641819451829e+174, 1.4438595832024937e+176, 1.588245541522743e+178, 1.7629525510902446e+180,
    1.974506857221074e+182, 2.2311927486598138e+184, 2.5435597334721877e+186, 2.925093693493016e+188,
    3.393108684451898e+190, 3.969937160808721e+192, 4.684525849754291e+194, 5.574585761207606e+196,
    6.689502913449127e+198, 8.094298525273444e+200, 9.875044200833601e+202, 1.214630436702533e+205,
    1.506141741511141e+207, 1.882677176888926e+209, 2.372173242880047e+211, 3.0126600184576594e+213,
    3.856204823625804e+215, 4.974504222477287e+217, 6.466855489220474e+219, 8.47158069087882e+221,
    1.1182486511960043e+224, 1.4872707060906857e+226, 1.9929427461615188e+228, 2.6904727073180504e+230,
    3.659042881952549e+232, 5.012888748274992e+234, 6.917786472619489e+236, 9.615723196941089e+238,
    1.3462012475717526e+241, 1.898143759076171e+243, 2.695364137888163e+245, 3.854370717180073e+247,
    5.5502938327393044e+249, 8.047926057471992e+251, 1.1749972043909107e+254, 1.727245890454639e+256,
    2.5563239178728654e+258, 3.80892263763057e+260, 5.713383956445855e+262, 8.62720977423324e+264,
    1.3113358856834524e+267, 2.0063439050956823e+269, 3.0897696138473508e+271, 4.789142901463394e+273,
    7.471062926282894e+275, 1.1729568794264145e+278, 1.853271869493735e+280, 2.9467022724950384e+282,
    4.7147236359920616e+284, 7.590705053947219e+286, 1.2296942187394494e+289, 2.0044015765453026e+291,
    3.287218585534296e+293, 5.423910666131589e+295, 9.003691705778438e+297, 1.503616514864999e+300,
    2.5260757449731984e+302, 4.269068009004705e+304, 7.257415615307999e+306
};

double xb_fact(double v) {
  if (!(v >= 0.0 && v < 171.0)) return -1.0; /* a NaN too */
  int64_t n = (int64_t)v; /* truncated */
  if (n > 20) return xb_trig_factorials[n - 21];
  int64_t f = 1;
  for (int64_t k = 2; k <= n; ++k) f *= k;
  return (double)f; /* exact: 20! has 44 significant bits */
}
