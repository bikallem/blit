#ifdef __cplusplus
extern "C" {
#endif

#include "moonbit.h"

#ifdef _MSC_VER
#define _Noreturn __declspec(noreturn)
#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wshift-op-parentheses"
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif

MOONBIT_EXPORT _Noreturn void moonbit_panic(void);
MOONBIT_EXPORT void *moonbit_malloc_array(enum moonbit_block_kind kind,
                                          int elem_size_shift, int32_t len);
MOONBIT_EXPORT int moonbit_val_array_equal(const void *lhs, const void *rhs);
MOONBIT_EXPORT moonbit_string_t moonbit_add_string(moonbit_string_t s1,
                                                   moonbit_string_t s2);
MOONBIT_EXPORT void moonbit_unsafe_bytes_blit(moonbit_bytes_t dst,
                                              int32_t dst_start,
                                              moonbit_bytes_t src,
                                              int32_t src_offset, int32_t len);
MOONBIT_EXPORT moonbit_string_t moonbit_unsafe_bytes_sub_string(
    moonbit_bytes_t bytes, int32_t start, int32_t len);
MOONBIT_EXPORT void moonbit_println(moonbit_string_t str);
MOONBIT_EXPORT moonbit_bytes_t *moonbit_get_cli_args(void);
MOONBIT_EXPORT void moonbit_runtime_init(int argc, char **argv);
MOONBIT_EXPORT void moonbit_drop_object(void *);

#define Moonbit_make_regular_object_header(ptr_field_offset, ptr_field_count,  \
                                           tag)                                \
  (((uint32_t)moonbit_BLOCK_KIND_REGULAR << 30) |                              \
   (((uint32_t)(ptr_field_offset) & (((uint32_t)1 << 11) - 1)) << 19) |        \
   (((uint32_t)(ptr_field_count) & (((uint32_t)1 << 11) - 1)) << 8) |          \
   ((tag) & 0xFF))

// header manipulation macros
#define Moonbit_object_ptr_field_offset(obj)                                   \
  ((Moonbit_object_header(obj)->meta >> 19) & (((uint32_t)1 << 11) - 1))

#define Moonbit_object_ptr_field_count(obj)                                    \
  ((Moonbit_object_header(obj)->meta >> 8) & (((uint32_t)1 << 11) - 1))

#if !defined(_WIN64) && !defined(_WIN32)
void *malloc(size_t size);
void free(void *ptr);
#define libc_malloc malloc
#define libc_free free
#endif

// several important runtime functions are inlined
static void *moonbit_malloc_inlined(size_t size) {
  struct moonbit_object *ptr = (struct moonbit_object *)libc_malloc(
      sizeof(struct moonbit_object) + size);
  ptr->rc = 1;
  return ptr + 1;
}

#define moonbit_malloc(obj) moonbit_malloc_inlined(obj)
#define moonbit_free(obj) libc_free(Moonbit_object_header(obj))

static void moonbit_incref_inlined(void *ptr) {
  struct moonbit_object *header = Moonbit_object_header(ptr);
  int32_t const count = header->rc;
  if (count > 0) {
    header->rc = count + 1;
  }
}

#define moonbit_incref moonbit_incref_inlined

static void moonbit_decref_inlined(void *ptr) {
  struct moonbit_object *header = Moonbit_object_header(ptr);
  int32_t const count = header->rc;
  if (count > 1) {
    header->rc = count - 1;
  } else if (count == 1) {
    moonbit_drop_object(ptr);
  }
}

#define moonbit_decref moonbit_decref_inlined

#define moonbit_unsafe_make_string moonbit_make_string

// detect whether compiler builtins exist for advanced bitwise operations
#ifdef __has_builtin

#if __has_builtin(__builtin_clz)
#define HAS_BUILTIN_CLZ
#endif

#if __has_builtin(__builtin_ctz)
#define HAS_BUILTIN_CTZ
#endif

#if __has_builtin(__builtin_popcount)
#define HAS_BUILTIN_POPCNT
#endif

#if __has_builtin(__builtin_sqrt)
#define HAS_BUILTIN_SQRT
#endif

#if __has_builtin(__builtin_sqrtf)
#define HAS_BUILTIN_SQRTF
#endif

#if __has_builtin(__builtin_fabs)
#define HAS_BUILTIN_FABS
#endif

#if __has_builtin(__builtin_fabsf)
#define HAS_BUILTIN_FABSF
#endif

#endif

// if there is no builtin operators, use software implementation
#ifdef HAS_BUILTIN_CLZ
static inline int32_t moonbit_clz32(int32_t x) {
  return x == 0 ? 32 : __builtin_clz(x);
}

static inline int32_t moonbit_clz64(int64_t x) {
  return x == 0 ? 64 : __builtin_clzll(x);
}

#undef HAS_BUILTIN_CLZ
#else
// table for [clz] value of 4bit integer.
static const uint8_t moonbit_clz4[] = {4, 3, 2, 2, 1, 1, 1, 1,
                                       0, 0, 0, 0, 0, 0, 0, 0};

int32_t moonbit_clz32(uint32_t x) {
  /* The ideas is to:

     1. narrow down the 4bit block where the most signficant "1" bit lies,
        using binary search
     2. find the number of leading zeros in that 4bit block via table lookup

     Different time/space tradeoff can be made here by enlarging the table
     and do less binary search.
     One benefit of the 4bit lookup table is that it can fit into a single cache
     line.
  */
  int32_t result = 0;
  if (x > 0xffff) {
    x >>= 16;
  } else {
    result += 16;
  }
  if (x > 0xff) {
    x >>= 8;
  } else {
    result += 8;
  }
  if (x > 0xf) {
    x >>= 4;
  } else {
    result += 4;
  }
  return result + moonbit_clz4[x];
}

int32_t moonbit_clz64(uint64_t x) {
  int32_t result = 0;
  if (x > 0xffffffff) {
    x >>= 32;
  } else {
    result += 32;
  }
  return result + moonbit_clz32((uint32_t)x);
}
#endif

#ifdef HAS_BUILTIN_CTZ
static inline int32_t moonbit_ctz32(int32_t x) {
  return x == 0 ? 32 : __builtin_ctz(x);
}

static inline int32_t moonbit_ctz64(int64_t x) {
  return x == 0 ? 64 : __builtin_ctzll(x);
}

#undef HAS_BUILTIN_CTZ
#else
int32_t moonbit_ctz32(int32_t x) {
  /* The algorithm comes from:

       Leiserson, Charles E. et al. “Using de Bruijn Sequences to Index a 1 in a
     Computer Word.” (1998).

     The ideas is:

     1. leave only the least significant "1" bit in the input,
        set all other bits to "0". This is achieved via [x & -x]
     2. now we have [x * n == n << ctz(x)], if [n] is a de bruijn sequence
        (every 5bit pattern occurn exactly once when you cycle through the bit
     string), we can find [ctz(x)] from the most significant 5 bits of [x * n]
 */
  static const uint32_t de_bruijn_32 = 0x077CB531;
  static const uint8_t index32[] = {0,  1,  28, 2,  29, 14, 24, 3,  30, 22, 20,
                                    15, 25, 17, 4,  8,  31, 27, 13, 23, 21, 19,
                                    16, 7,  26, 12, 18, 6,  11, 5,  10, 9};
  return (x == 0) * 32 + index32[(de_bruijn_32 * (x & -x)) >> 27];
}

int32_t moonbit_ctz64(int64_t x) {
  static const uint64_t de_bruijn_64 = 0x0218A392CD3D5DBF;
  static const uint8_t index64[] = {
      0,  1,  2,  7,  3,  13, 8,  19, 4,  25, 14, 28, 9,  34, 20, 40,
      5,  17, 26, 38, 15, 46, 29, 48, 10, 31, 35, 54, 21, 50, 41, 57,
      63, 6,  12, 18, 24, 27, 33, 39, 16, 37, 45, 47, 30, 53, 49, 56,
      62, 11, 23, 32, 36, 44, 52, 55, 61, 22, 43, 51, 60, 42, 59, 58};
  return (x == 0) * 64 + index64[(de_bruijn_64 * (x & -x)) >> 58];
}
#endif

#ifdef HAS_BUILTIN_POPCNT

#define moonbit_popcnt32 __builtin_popcount
#define moonbit_popcnt64 __builtin_popcountll
#undef HAS_BUILTIN_POPCNT

#else
int32_t moonbit_popcnt32(uint32_t x) {
  /* The classic SIMD Within A Register algorithm.
     ref: [https://nimrod.blog/posts/algorithms-behind-popcount/]
 */
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  x = (x + (x >> 4)) & 0x0F0F0F0F;
  return (x * 0x01010101) >> 24;
}

int32_t moonbit_popcnt64(uint64_t x) {
  x = x - ((x >> 1) & 0x5555555555555555);
  x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
  x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0F;
  return (x * 0x0101010101010101) >> 56;
}
#endif

/* The following sqrt implementation comes from
   [musl](https://git.musl-libc.org/cgit/musl),
   with some helpers inlined to make it zero dependency.
 */
#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
const uint16_t __rsqrt_tab[128] = {
    0xb451, 0xb2f0, 0xb196, 0xb044, 0xaef9, 0xadb6, 0xac79, 0xab43, 0xaa14,
    0xa8eb, 0xa7c8, 0xa6aa, 0xa592, 0xa480, 0xa373, 0xa26b, 0xa168, 0xa06a,
    0x9f70, 0x9e7b, 0x9d8a, 0x9c9d, 0x9bb5, 0x9ad1, 0x99f0, 0x9913, 0x983a,
    0x9765, 0x9693, 0x95c4, 0x94f8, 0x9430, 0x936b, 0x92a9, 0x91ea, 0x912e,
    0x9075, 0x8fbe, 0x8f0a, 0x8e59, 0x8daa, 0x8cfe, 0x8c54, 0x8bac, 0x8b07,
    0x8a64, 0x89c4, 0x8925, 0x8889, 0x87ee, 0x8756, 0x86c0, 0x862b, 0x8599,
    0x8508, 0x8479, 0x83ec, 0x8361, 0x82d8, 0x8250, 0x81c9, 0x8145, 0x80c2,
    0x8040, 0xff02, 0xfd0e, 0xfb25, 0xf947, 0xf773, 0xf5aa, 0xf3ea, 0xf234,
    0xf087, 0xeee3, 0xed47, 0xebb3, 0xea27, 0xe8a3, 0xe727, 0xe5b2, 0xe443,
    0xe2dc, 0xe17a, 0xe020, 0xdecb, 0xdd7d, 0xdc34, 0xdaf1, 0xd9b3, 0xd87b,
    0xd748, 0xd61a, 0xd4f1, 0xd3cd, 0xd2ad, 0xd192, 0xd07b, 0xcf69, 0xce5b,
    0xcd51, 0xcc4a, 0xcb48, 0xca4a, 0xc94f, 0xc858, 0xc764, 0xc674, 0xc587,
    0xc49d, 0xc3b7, 0xc2d4, 0xc1f4, 0xc116, 0xc03c, 0xbf65, 0xbe90, 0xbdbe,
    0xbcef, 0xbc23, 0xbb59, 0xba91, 0xb9cc, 0xb90a, 0xb84a, 0xb78c, 0xb6d0,
    0xb617, 0xb560,
};

/* returns a*b*2^-32 - e, with error 0 <= e < 1.  */
static inline uint32_t mul32(uint32_t a, uint32_t b) {
  return (uint64_t)a * b >> 32;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
float sqrtf(float x) {
  uint32_t ix, m, m1, m0, even, ey;

  ix = *(uint32_t *)&x;
  if (ix - 0x00800000 >= 0x7f800000 - 0x00800000) {
    /* x < 0x1p-126 or inf or nan.  */
    if (ix * 2 == 0)
      return x;
    if (ix == 0x7f800000)
      return x;
    if (ix > 0x7f800000)
      return (x - x) / (x - x);
    /* x is subnormal, normalize it.  */
    x *= 0x1p23f;
    ix = *(uint32_t *)&x;
    ix -= 23 << 23;
  }

  /* x = 4^e m; with int e and m in [1, 4).  */
  even = ix & 0x00800000;
  m1 = (ix << 8) | 0x80000000;
  m0 = (ix << 7) & 0x7fffffff;
  m = even ? m0 : m1;

  /* 2^e is the exponent part of the return value.  */
  ey = ix >> 1;
  ey += 0x3f800000 >> 1;
  ey &= 0x7f800000;

  /* compute r ~ 1/sqrt(m), s ~ sqrt(m) with 2 goldschmidt iterations.  */
  static const uint32_t three = 0xc0000000;
  uint32_t r, s, d, u, i;
  i = (ix >> 17) % 128;
  r = (uint32_t)__rsqrt_tab[i] << 16;
  /* |r*sqrt(m) - 1| < 0x1p-8 */
  s = mul32(m, r);
  /* |s/sqrt(m) - 1| < 0x1p-8 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r*sqrt(m) - 1| < 0x1.7bp-16 */
  s = mul32(s, u) << 1;
  /* |s/sqrt(m) - 1| < 0x1.7bp-16 */
  d = mul32(s, r);
  u = three - d;
  s = mul32(s, u);
  /* -0x1.03p-28 < s/sqrt(m) - 1 < 0x1.fp-31 */
  s = (s - 1) >> 6;
  /* s < sqrt(m) < s + 0x1.08p-23 */

  /* compute nearest rounded result.  */
  uint32_t d0, d1, d2;
  float y, t;
  d0 = (m << 16) - s * s;
  d1 = s - d0;
  d2 = d1 + s + 1;
  s += d1 >> 31;
  s &= 0x007fffff;
  s |= ey;
  y = *(float *)&s;
  /* handle rounding and inexact exception. */
  uint32_t tiny = d2 == 0 ? 0 : 0x01000000;
  tiny |= (d1 ^ d2) & 0x80000000;
  t = *(float *)&tiny;
  y = y + t;
  return y;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
/* returns a*b*2^-64 - e, with error 0 <= e < 3.  */
static inline uint64_t mul64(uint64_t a, uint64_t b) {
  uint64_t ahi = a >> 32;
  uint64_t alo = a & 0xffffffff;
  uint64_t bhi = b >> 32;
  uint64_t blo = b & 0xffffffff;
  return ahi * bhi + (ahi * blo >> 32) + (alo * bhi >> 32);
}

double sqrt(double x) {
  uint64_t ix, top, m;

  /* special case handling.  */
  ix = *(uint64_t *)&x;
  top = ix >> 52;
  if (top - 0x001 >= 0x7ff - 0x001) {
    /* x < 0x1p-1022 or inf or nan.  */
    if (ix * 2 == 0)
      return x;
    if (ix == 0x7ff0000000000000)
      return x;
    if (ix > 0x7ff0000000000000)
      return (x - x) / (x - x);
    /* x is subnormal, normalize it.  */
    x *= 0x1p52;
    ix = *(uint64_t *)&x;
    top = ix >> 52;
    top -= 52;
  }

  /* argument reduction:
     x = 4^e m; with integer e, and m in [1, 4)
     m: fixed point representation [2.62]
     2^e is the exponent part of the result.  */
  int even = top & 1;
  m = (ix << 11) | 0x8000000000000000;
  if (even)
    m >>= 1;
  top = (top + 0x3ff) >> 1;

  /* approximate r ~ 1/sqrt(m) and s ~ sqrt(m) when m in [1,4)

     initial estimate:
     7bit table lookup (1bit exponent and 6bit significand).

     iterative approximation:
     using 2 goldschmidt iterations with 32bit int arithmetics
     and a final iteration with 64bit int arithmetics.

     details:

     the relative error (e = r0 sqrt(m)-1) of a linear estimate
     (r0 = a m + b) is |e| < 0.085955 ~ 0x1.6p-4 at best,
     a table lookup is faster and needs one less iteration
     6 bit lookup table (128b) gives |e| < 0x1.f9p-8
     7 bit lookup table (256b) gives |e| < 0x1.fdp-9
     for single and double prec 6bit is enough but for quad
     prec 7bit is needed (or modified iterations). to avoid
     one more iteration >=13bit table would be needed (16k).

     a newton-raphson iteration for r is
       w = r*r
       u = 3 - m*w
       r = r*u/2
     can use a goldschmidt iteration for s at the end or
       s = m*r

     first goldschmidt iteration is
       s = m*r
       u = 3 - s*r
       r = r*u/2
       s = s*u/2
     next goldschmidt iteration is
       u = 3 - s*r
       r = r*u/2
       s = s*u/2
     and at the end r is not computed only s.

     they use the same amount of operations and converge at the
     same quadratic rate, i.e. if
       r1 sqrt(m) - 1 = e, then
       r2 sqrt(m) - 1 = -3/2 e^2 - 1/2 e^3
     the advantage of goldschmidt is that the mul for s and r
     are independent (computed in parallel), however it is not
     "self synchronizing": it only uses the input m in the
     first iteration so rounding errors accumulate. at the end
     or when switching to larger precision arithmetics rounding
     errors dominate so the first iteration should be used.

     the fixed point representations are
       m: 2.30 r: 0.32, s: 2.30, d: 2.30, u: 2.30, three: 2.30
     and after switching to 64 bit
       m: 2.62 r: 0.64, s: 2.62, d: 2.62, u: 2.62, three: 2.62  */

  static const uint64_t three = 0xc0000000;
  uint64_t r, s, d, u, i;

  i = (ix >> 46) % 128;
  r = (uint32_t)__rsqrt_tab[i] << 16;
  /* |r sqrt(m) - 1| < 0x1.fdp-9 */
  s = mul32(m >> 32, r);
  /* |s/sqrt(m) - 1| < 0x1.fdp-9 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r sqrt(m) - 1| < 0x1.7bp-16 */
  s = mul32(s, u) << 1;
  /* |s/sqrt(m) - 1| < 0x1.7bp-16 */
  d = mul32(s, r);
  u = three - d;
  r = mul32(r, u) << 1;
  /* |r sqrt(m) - 1| < 0x1.3704p-29 (measured worst-case) */
  r = r << 32;
  s = mul64(m, r);
  d = mul64(s, r);
  u = (three << 32) - d;
  s = mul64(s, u); /* repr: 3.61 */
  /* -0x1p-57 < s - sqrt(m) < 0x1.8001p-61 */
  s = (s - 2) >> 9; /* repr: 12.52 */
  /* -0x1.09p-52 < s - sqrt(m) < -0x1.fffcp-63 */

  /* s < sqrt(m) < s + 0x1.09p-52,
     compute nearest rounded result:
     the nearest result to 52 bits is either s or s+0x1p-52,
     we can decide by comparing (2^52 s + 0.5)^2 to 2^104 m.  */
  uint64_t d0, d1, d2;
  double y, t;
  d0 = (m << 42) - s * s;
  d1 = s - d0;
  d2 = d1 + s + 1;
  s += d1 >> 63;
  s &= 0x000fffffffffffff;
  s |= top << 52;
  y = *(double *)&s;
  return y;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
double fabs(double x) {
  union {
    double f;
    uint64_t i;
  } u = {x};
  u.i &= 0x7fffffffffffffffULL;
  return u.f;
}
#endif

#ifdef MOONBIT_NATIVE_NO_SYS_HEADER
float fabsf(float x) {
  union {
    float f;
    uint32_t i;
  } u = {x};
  u.i &= 0x7fffffff;
  return u.f;
}
#endif

#ifdef _MSC_VER
/* MSVC treats syntactic division by zero as fatal error,
   even for float point numbers,
   so we have to use a constant variable to work around this */
static const int MOONBIT_ZERO = 0;
#else
#define MOONBIT_ZERO 0
#endif

#ifdef __cplusplus
}
#endif
struct _M0DTPC16result6ResultGbRP28bikallem4blit33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTPC15error5Error85bikallem_2fblit_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB6Logger;

struct _M0R88_24bikallem_2fblit_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c424;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16result6ResultGbRP28bikallem4blit33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TPC13ref3RefGiE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPC16result6ResultGbRP28bikallem4blit33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error85bikallem_2fblit_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TWssbEu {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  
};

struct _M0TUsiE {
  int32_t $1;
  moonbit_string_t $0;
  
};

struct _M0TPB13SourceLocRepr {
  int32_t $0_1;
  int32_t $0_2;
  int32_t $1_1;
  int32_t $1_2;
  int32_t $2_1;
  int32_t $2_2;
  int32_t $3_1;
  int32_t $3_2;
  int32_t $4_1;
  int32_t $4_2;
  int32_t $5_1;
  int32_t $5_2;
  moonbit_string_t $0_0;
  moonbit_string_t $1_0;
  moonbit_string_t $2_0;
  moonbit_string_t $3_0;
  moonbit_string_t $4_0;
  moonbit_string_t $5_0;
  
};

struct _M0TUsRPB6LoggerE {
  moonbit_string_t $0;
  struct _M0BTPB6Logger* $1_0;
  void* $1_1;
  
};

struct _M0TPB13StringBuilder {
  int32_t $1;
  moonbit_bytes_t $0;
  
};

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  void* $1;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0TPB5ArrayGUsiEE {
  int32_t $1;
  struct _M0TUsiE** $0;
  
};

struct _M0TWRPC15error5ErrorEs {
  moonbit_string_t(* code)(struct _M0TWRPC15error5ErrorEs*, void*);
  
};

struct _M0BTPB6Logger {
  int32_t(* $method_0)(void*, moonbit_string_t);
  int32_t(* $method_1)(void*, moonbit_string_t, int32_t, int32_t);
  int32_t(* $method_2)(void*, struct _M0TPC16string10StringView);
  int32_t(* $method_3)(void*, int32_t);
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0TPC16string10StringView {
  int32_t $1;
  int32_t $2;
  moonbit_string_t $0;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0R88_24bikallem_2fblit_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c424 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGbRP28bikallem4blit33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE {
  int32_t $1;
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** $0;
  
};

struct _M0TPC13ref3RefGiE {
  int32_t $0;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

int32_t _M0FP28bikallem4blit44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP28bikallem4blit44moonbit__test__driver__internal__do__executeN17error__to__stringS433(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP28bikallem4blit44moonbit__test__driver__internal__do__executeN14handle__resultS424(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGUsiEE* _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS402(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS397(
  int32_t
);

moonbit_string_t _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS390(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S384(
  int32_t,
  moonbit_string_t
);

#define _M0FP28bikallem4blit52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit41MoonBit__Test__Driver__Internal__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP28bikallem4blit28MoonBit__Async__Test__Driver17run__async__testsGRP28bikallem4blit34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr*,
  struct _M0TPB6Logger
);

int32_t _M0FPB7printlnGsE(moonbit_string_t);

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  struct _M0TUsiE*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsiEE(struct _M0TPB5ArrayGUsiEE*);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(int32_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t,
  int64_t,
  int64_t
);

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE*,
  int32_t,
  int32_t
);

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t);

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t);

int32_t _M0IPC14byte4BytePB3Sub3sub(int32_t, int32_t);

int32_t _M0IPC14byte4BytePB3Mod3mod(int32_t, int32_t);

int32_t _M0IPC14byte4BytePB3Div3div(int32_t, int32_t);

int32_t _M0IPC14byte4BytePB3Add3add(int32_t, int32_t);

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t,
  int32_t,
  int32_t
);

moonbit_string_t _M0MPC13int3Int18to__string_2einner(int32_t, int32_t);

int32_t _M0FPB14radix__count32(uint32_t, int32_t);

int32_t _M0FPB12hex__count32(uint32_t);

int32_t _M0FPB12dec__count32(uint32_t);

int32_t _M0FPB20int__to__string__dec(uint16_t*, uint32_t, int32_t, int32_t);

int32_t _M0FPB24int__to__string__generic(
  uint16_t*,
  uint32_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0FPB20int__to__string__hex(uint16_t*, uint32_t, int32_t, int32_t);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(int32_t);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t
);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void*
);

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView6length(struct _M0TPC16string10StringView);

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView
);

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder*,
  moonbit_string_t,
  int32_t,
  int32_t
);

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder*,
  moonbit_string_t
);

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t,
  int32_t,
  moonbit_string_t,
  int32_t,
  int32_t
);

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(moonbit_string_t);

moonbit_string_t _M0MPC15array5Array2atGsE(struct _M0TPB5ArrayGsE*, int32_t);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
);

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t);

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t);

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder*,
  int32_t
);

int32_t _M0IPC14byte4BytePB7Default7default();

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t,
  int32_t,
  int32_t
);

int32_t _M0MPC14uint4UInt8to__byte(uint32_t);

uint32_t _M0MPC14char4Char8to__uint(int32_t);

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder*
);

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

#define _M0FPB19unsafe__sub__string moonbit_unsafe_bytes_sub_string

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(int32_t);

int32_t _M0MPC14byte4Byte8to__char(int32_t);

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE**,
  int32_t,
  struct _M0TUsiE**,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t,
  int32_t,
  moonbit_bytes_t,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t*,
  int32_t,
  moonbit_string_t*,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE**,
  int32_t,
  struct _M0TUsiE**,
  int32_t,
  int32_t
);

int32_t _M0FPB5abortGiE(moonbit_string_t, moonbit_string_t);

int32_t _M0FPB5abortGuE(moonbit_string_t, moonbit_string_t);

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0IPB7FailurePB4Show6output(void*, struct _M0TPB6Logger);

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger,
  moonbit_string_t
);

int32_t _M0FPC15abort5abortGiE(moonbit_string_t);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

moonbit_string_t _M0FP15Error10to__string(void*);

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  int32_t
);

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  struct _M0TPC16string10StringView
);

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void*,
  moonbit_string_t
);

struct { int32_t rc; uint32_t meta; uint16_t const data[82]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 81), 
    98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 46, 
    77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 
    118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 
    112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 
    101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 
    110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[42]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 41), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 98, 105, 
    107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 34, 44, 32, 34, 
    102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_18 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[80]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 79), 
    98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 46, 
    77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 
    118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 
    114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_25 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_23 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    34, 44, 32, 34, 116, 101, 115, 116, 95, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_7 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_6 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    44, 32, 34, 109, 101, 115, 115, 97, 103, 101, 34, 58, 32, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP28bikallem4blit44moonbit__test__driver__internal__do__executeN17error__to__stringS433$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem4blit44moonbit__test__driver__internal__do__executeN17error__to__stringS433
  };

struct { int32_t rc; uint32_t meta; struct _M0BTPB6Logger data; 
} _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6Logger) >> 2, 0, 0),
    {.$method_0 = _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger,
       .$method_1 = _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE,
       .$method_2 = _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger,
       .$method_3 = _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger}
  };

struct _M0BTPB6Logger* _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id =
  &_M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id$object.data;

int32_t _M0FP28bikallem4blit44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS454,
  moonbit_string_t _M0L8filenameS429,
  int32_t _M0L5indexS432
) {
  struct _M0R88_24bikallem_2fblit_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c424* _closure_1139;
  struct _M0TWssbEu* _M0L14handle__resultS424;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS433;
  void* _M0L11_2atry__errS448;
  struct moonbit_result_0 _tmp_1141;
  int32_t _handle__error__result_1142;
  int32_t _M0L6_2atmpS1012;
  void* _M0L3errS449;
  moonbit_string_t _M0L4nameS451;
  struct _M0DTPC15error5Error87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS452;
  moonbit_string_t _M0L8_2afieldS1024;
  int32_t _M0L6_2acntS1108;
  moonbit_string_t _M0L7_2anameS453;
  #line 483 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS429);
  _closure_1139
  = (struct _M0R88_24bikallem_2fblit_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c424*)moonbit_malloc(sizeof(struct _M0R88_24bikallem_2fblit_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c424));
  Moonbit_object_header(_closure_1139)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R88_24bikallem_2fblit_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c424, $1) >> 2, 1, 0);
  _closure_1139->code
  = &_M0FP28bikallem4blit44moonbit__test__driver__internal__do__executeN14handle__resultS424;
  _closure_1139->$0 = _M0L5indexS432;
  _closure_1139->$1 = _M0L8filenameS429;
  _M0L14handle__resultS424 = (struct _M0TWssbEu*)_closure_1139;
  _M0L17error__to__stringS433
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP28bikallem4blit44moonbit__test__driver__internal__do__executeN17error__to__stringS433$closure.data;
  moonbit_incref(_M0L12async__testsS454);
  moonbit_incref(_M0L17error__to__stringS433);
  moonbit_incref(_M0L8filenameS429);
  moonbit_incref(_M0L14handle__resultS424);
  #line 517 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _tmp_1141
  = _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit41MoonBit__Test__Driver__Internal__No__ArgsE(_M0L12async__testsS454, _M0L8filenameS429, _M0L5indexS432, _M0L14handle__resultS424, _M0L17error__to__stringS433);
  if (_tmp_1141.tag) {
    int32_t const _M0L5_2aokS1021 = _tmp_1141.data.ok;
    _handle__error__result_1142 = _M0L5_2aokS1021;
  } else {
    void* const _M0L6_2aerrS1022 = _tmp_1141.data.err;
    moonbit_decref(_M0L12async__testsS454);
    moonbit_decref(_M0L17error__to__stringS433);
    moonbit_decref(_M0L8filenameS429);
    _M0L11_2atry__errS448 = _M0L6_2aerrS1022;
    goto join_447;
  }
  if (_handle__error__result_1142) {
    moonbit_decref(_M0L12async__testsS454);
    moonbit_decref(_M0L17error__to__stringS433);
    moonbit_decref(_M0L8filenameS429);
    _M0L6_2atmpS1012 = 1;
  } else {
    struct moonbit_result_0 _tmp_1143;
    int32_t _handle__error__result_1144;
    moonbit_incref(_M0L12async__testsS454);
    moonbit_incref(_M0L17error__to__stringS433);
    moonbit_incref(_M0L8filenameS429);
    moonbit_incref(_M0L14handle__resultS424);
    #line 520 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
    _tmp_1143
    = _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS454, _M0L8filenameS429, _M0L5indexS432, _M0L14handle__resultS424, _M0L17error__to__stringS433);
    if (_tmp_1143.tag) {
      int32_t const _M0L5_2aokS1019 = _tmp_1143.data.ok;
      _handle__error__result_1144 = _M0L5_2aokS1019;
    } else {
      void* const _M0L6_2aerrS1020 = _tmp_1143.data.err;
      moonbit_decref(_M0L12async__testsS454);
      moonbit_decref(_M0L17error__to__stringS433);
      moonbit_decref(_M0L8filenameS429);
      _M0L11_2atry__errS448 = _M0L6_2aerrS1020;
      goto join_447;
    }
    if (_handle__error__result_1144) {
      moonbit_decref(_M0L12async__testsS454);
      moonbit_decref(_M0L17error__to__stringS433);
      moonbit_decref(_M0L8filenameS429);
      _M0L6_2atmpS1012 = 1;
    } else {
      struct moonbit_result_0 _tmp_1145;
      int32_t _handle__error__result_1146;
      moonbit_incref(_M0L12async__testsS454);
      moonbit_incref(_M0L17error__to__stringS433);
      moonbit_incref(_M0L8filenameS429);
      moonbit_incref(_M0L14handle__resultS424);
      #line 523 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _tmp_1145
      = _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS454, _M0L8filenameS429, _M0L5indexS432, _M0L14handle__resultS424, _M0L17error__to__stringS433);
      if (_tmp_1145.tag) {
        int32_t const _M0L5_2aokS1017 = _tmp_1145.data.ok;
        _handle__error__result_1146 = _M0L5_2aokS1017;
      } else {
        void* const _M0L6_2aerrS1018 = _tmp_1145.data.err;
        moonbit_decref(_M0L12async__testsS454);
        moonbit_decref(_M0L17error__to__stringS433);
        moonbit_decref(_M0L8filenameS429);
        _M0L11_2atry__errS448 = _M0L6_2aerrS1018;
        goto join_447;
      }
      if (_handle__error__result_1146) {
        moonbit_decref(_M0L12async__testsS454);
        moonbit_decref(_M0L17error__to__stringS433);
        moonbit_decref(_M0L8filenameS429);
        _M0L6_2atmpS1012 = 1;
      } else {
        struct moonbit_result_0 _tmp_1147;
        int32_t _handle__error__result_1148;
        moonbit_incref(_M0L12async__testsS454);
        moonbit_incref(_M0L17error__to__stringS433);
        moonbit_incref(_M0L8filenameS429);
        moonbit_incref(_M0L14handle__resultS424);
        #line 526 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        _tmp_1147
        = _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS454, _M0L8filenameS429, _M0L5indexS432, _M0L14handle__resultS424, _M0L17error__to__stringS433);
        if (_tmp_1147.tag) {
          int32_t const _M0L5_2aokS1015 = _tmp_1147.data.ok;
          _handle__error__result_1148 = _M0L5_2aokS1015;
        } else {
          void* const _M0L6_2aerrS1016 = _tmp_1147.data.err;
          moonbit_decref(_M0L12async__testsS454);
          moonbit_decref(_M0L17error__to__stringS433);
          moonbit_decref(_M0L8filenameS429);
          _M0L11_2atry__errS448 = _M0L6_2aerrS1016;
          goto join_447;
        }
        if (_handle__error__result_1148) {
          moonbit_decref(_M0L12async__testsS454);
          moonbit_decref(_M0L17error__to__stringS433);
          moonbit_decref(_M0L8filenameS429);
          _M0L6_2atmpS1012 = 1;
        } else {
          struct moonbit_result_0 _tmp_1149;
          moonbit_incref(_M0L14handle__resultS424);
          #line 529 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
          _tmp_1149
          = _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS454, _M0L8filenameS429, _M0L5indexS432, _M0L14handle__resultS424, _M0L17error__to__stringS433);
          if (_tmp_1149.tag) {
            int32_t const _M0L5_2aokS1013 = _tmp_1149.data.ok;
            _M0L6_2atmpS1012 = _M0L5_2aokS1013;
          } else {
            void* const _M0L6_2aerrS1014 = _tmp_1149.data.err;
            _M0L11_2atry__errS448 = _M0L6_2aerrS1014;
            goto join_447;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS1012) {
    void* _M0L87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1023 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1023)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1023)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS448
    = _M0L87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1023;
    goto join_447;
  } else {
    moonbit_decref(_M0L14handle__resultS424);
  }
  goto joinlet_1140;
  join_447:;
  _M0L3errS449 = _M0L11_2atry__errS448;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS452
  = (struct _M0DTPC15error5Error87bikallem_2fblit_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS449;
  _M0L8_2afieldS1024 = _M0L36_2aMoonBitTestDriverInternalSkipTestS452->$0;
  _M0L6_2acntS1108
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS452)->rc;
  if (_M0L6_2acntS1108 > 1) {
    int32_t _M0L11_2anew__cntS1109 = _M0L6_2acntS1108 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS452)->rc
    = _M0L11_2anew__cntS1109;
    moonbit_incref(_M0L8_2afieldS1024);
  } else if (_M0L6_2acntS1108 == 1) {
    #line 536 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS452);
  }
  _M0L7_2anameS453 = _M0L8_2afieldS1024;
  _M0L4nameS451 = _M0L7_2anameS453;
  goto join_450;
  goto joinlet_1150;
  join_450:;
  #line 537 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0FP28bikallem4blit44moonbit__test__driver__internal__do__executeN14handle__resultS424(_M0L14handle__resultS424, _M0L4nameS451, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_1150:;
  joinlet_1140:;
  return 0;
}

moonbit_string_t _M0FP28bikallem4blit44moonbit__test__driver__internal__do__executeN17error__to__stringS433(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS1011,
  void* _M0L3errS434
) {
  void* _M0L1eS436;
  moonbit_string_t _M0L1eS438;
  #line 506 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS1011);
  switch (Moonbit_object_tag(_M0L3errS434)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS439 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS434;
      moonbit_string_t _M0L8_2afieldS1025 = _M0L10_2aFailureS439->$0;
      int32_t _M0L6_2acntS1110 =
        Moonbit_object_header(_M0L10_2aFailureS439)->rc;
      moonbit_string_t _M0L4_2aeS440;
      if (_M0L6_2acntS1110 > 1) {
        int32_t _M0L11_2anew__cntS1111 = _M0L6_2acntS1110 - 1;
        Moonbit_object_header(_M0L10_2aFailureS439)->rc
        = _M0L11_2anew__cntS1111;
        moonbit_incref(_M0L8_2afieldS1025);
      } else if (_M0L6_2acntS1110 == 1) {
        #line 507 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS439);
      }
      _M0L4_2aeS440 = _M0L8_2afieldS1025;
      _M0L1eS438 = _M0L4_2aeS440;
      goto join_437;
      break;
    }
    
    case 2: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS441 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS434;
      moonbit_string_t _M0L8_2afieldS1026 = _M0L15_2aInspectErrorS441->$0;
      int32_t _M0L6_2acntS1112 =
        Moonbit_object_header(_M0L15_2aInspectErrorS441)->rc;
      moonbit_string_t _M0L4_2aeS442;
      if (_M0L6_2acntS1112 > 1) {
        int32_t _M0L11_2anew__cntS1113 = _M0L6_2acntS1112 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS441)->rc
        = _M0L11_2anew__cntS1113;
        moonbit_incref(_M0L8_2afieldS1026);
      } else if (_M0L6_2acntS1112 == 1) {
        #line 507 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS441);
      }
      _M0L4_2aeS442 = _M0L8_2afieldS1026;
      _M0L1eS438 = _M0L4_2aeS442;
      goto join_437;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS443 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS434;
      moonbit_string_t _M0L8_2afieldS1027 = _M0L16_2aSnapshotErrorS443->$0;
      int32_t _M0L6_2acntS1114 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS443)->rc;
      moonbit_string_t _M0L4_2aeS444;
      if (_M0L6_2acntS1114 > 1) {
        int32_t _M0L11_2anew__cntS1115 = _M0L6_2acntS1114 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS443)->rc
        = _M0L11_2anew__cntS1115;
        moonbit_incref(_M0L8_2afieldS1027);
      } else if (_M0L6_2acntS1114 == 1) {
        #line 507 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS443);
      }
      _M0L4_2aeS444 = _M0L8_2afieldS1027;
      _M0L1eS438 = _M0L4_2aeS444;
      goto join_437;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error85bikallem_2fblit_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS445 =
        (struct _M0DTPC15error5Error85bikallem_2fblit_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS434;
      moonbit_string_t _M0L8_2afieldS1028 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS445->$0;
      int32_t _M0L6_2acntS1116 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS445)->rc;
      moonbit_string_t _M0L4_2aeS446;
      if (_M0L6_2acntS1116 > 1) {
        int32_t _M0L11_2anew__cntS1117 = _M0L6_2acntS1116 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS445)->rc
        = _M0L11_2anew__cntS1117;
        moonbit_incref(_M0L8_2afieldS1028);
      } else if (_M0L6_2acntS1116 == 1) {
        #line 507 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS445);
      }
      _M0L4_2aeS446 = _M0L8_2afieldS1028;
      _M0L1eS438 = _M0L4_2aeS446;
      goto join_437;
      break;
    }
    default: {
      _M0L1eS436 = _M0L3errS434;
      goto join_435;
      break;
    }
  }
  join_437:;
  return _M0L1eS438;
  join_435:;
  #line 512 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS436);
}

int32_t _M0FP28bikallem4blit44moonbit__test__driver__internal__do__executeN14handle__resultS424(
  struct _M0TWssbEu* _M0L6_2aenvS997,
  moonbit_string_t _M0L8testnameS425,
  moonbit_string_t _M0L7messageS426,
  int32_t _M0L7skippedS427
) {
  struct _M0R88_24bikallem_2fblit_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c424* _M0L14_2acasted__envS998;
  moonbit_string_t _M0L8_2afieldS1038;
  moonbit_string_t _M0L8filenameS429;
  int32_t _M0L8_2afieldS1037;
  int32_t _M0L6_2acntS1118;
  int32_t _M0L5indexS432;
  int32_t _if__result_1153;
  moonbit_string_t _M0L10file__nameS428;
  moonbit_string_t _M0L10test__nameS430;
  moonbit_string_t _M0L7messageS431;
  moonbit_string_t _M0L6_2atmpS1010;
  moonbit_string_t _M0L6_2atmpS1036;
  moonbit_string_t _M0L6_2atmpS1009;
  moonbit_string_t _M0L6_2atmpS1035;
  moonbit_string_t _M0L6_2atmpS1007;
  moonbit_string_t _M0L6_2atmpS1008;
  moonbit_string_t _M0L6_2atmpS1034;
  moonbit_string_t _M0L6_2atmpS1006;
  moonbit_string_t _M0L6_2atmpS1033;
  moonbit_string_t _M0L6_2atmpS1004;
  moonbit_string_t _M0L6_2atmpS1005;
  moonbit_string_t _M0L6_2atmpS1032;
  moonbit_string_t _M0L6_2atmpS1003;
  moonbit_string_t _M0L6_2atmpS1031;
  moonbit_string_t _M0L6_2atmpS1001;
  moonbit_string_t _M0L6_2atmpS1002;
  moonbit_string_t _M0L6_2atmpS1030;
  moonbit_string_t _M0L6_2atmpS1000;
  moonbit_string_t _M0L6_2atmpS1029;
  moonbit_string_t _M0L6_2atmpS999;
  #line 490 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS998
  = (struct _M0R88_24bikallem_2fblit_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c424*)_M0L6_2aenvS997;
  _M0L8_2afieldS1038 = _M0L14_2acasted__envS998->$1;
  _M0L8filenameS429 = _M0L8_2afieldS1038;
  _M0L8_2afieldS1037 = _M0L14_2acasted__envS998->$0;
  _M0L6_2acntS1118 = Moonbit_object_header(_M0L14_2acasted__envS998)->rc;
  if (_M0L6_2acntS1118 > 1) {
    int32_t _M0L11_2anew__cntS1119 = _M0L6_2acntS1118 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS998)->rc
    = _M0L11_2anew__cntS1119;
    moonbit_incref(_M0L8filenameS429);
  } else if (_M0L6_2acntS1118 == 1) {
    #line 490 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS998);
  }
  _M0L5indexS432 = _M0L8_2afieldS1037;
  if (!_M0L7skippedS427) {
    _if__result_1153 = 1;
  } else {
    _if__result_1153 = 0;
  }
  if (_if__result_1153) {
    
  }
  #line 496 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS428 = _M0MPC16string6String6escape(_M0L8filenameS429);
  #line 497 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS430 = _M0MPC16string6String6escape(_M0L8testnameS425);
  #line 498 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L7messageS431 = _M0MPC16string6String6escape(_M0L7messageS426);
  #line 499 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 501 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1010
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS428);
  #line 500 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1036
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS1010);
  moonbit_decref(_M0L6_2atmpS1010);
  _M0L6_2atmpS1009 = _M0L6_2atmpS1036;
  #line 500 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1035
  = moonbit_add_string(_M0L6_2atmpS1009, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS1009);
  _M0L6_2atmpS1007 = _M0L6_2atmpS1035;
  #line 501 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1008
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS432);
  #line 500 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1034 = moonbit_add_string(_M0L6_2atmpS1007, _M0L6_2atmpS1008);
  moonbit_decref(_M0L6_2atmpS1007);
  moonbit_decref(_M0L6_2atmpS1008);
  _M0L6_2atmpS1006 = _M0L6_2atmpS1034;
  #line 500 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1033
  = moonbit_add_string(_M0L6_2atmpS1006, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS1006);
  _M0L6_2atmpS1004 = _M0L6_2atmpS1033;
  #line 501 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1005
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS430);
  #line 500 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1032 = moonbit_add_string(_M0L6_2atmpS1004, _M0L6_2atmpS1005);
  moonbit_decref(_M0L6_2atmpS1004);
  moonbit_decref(_M0L6_2atmpS1005);
  _M0L6_2atmpS1003 = _M0L6_2atmpS1032;
  #line 500 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1031
  = moonbit_add_string(_M0L6_2atmpS1003, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS1003);
  _M0L6_2atmpS1001 = _M0L6_2atmpS1031;
  #line 501 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1002
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS431);
  #line 500 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1030 = moonbit_add_string(_M0L6_2atmpS1001, _M0L6_2atmpS1002);
  moonbit_decref(_M0L6_2atmpS1001);
  moonbit_decref(_M0L6_2atmpS1002);
  _M0L6_2atmpS1000 = _M0L6_2atmpS1030;
  #line 500 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1029
  = moonbit_add_string(_M0L6_2atmpS1000, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1000);
  _M0L6_2atmpS999 = _M0L6_2atmpS1029;
  #line 500 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS999);
  #line 503 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S384;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS390;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS397;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS402;
  struct _M0TUsiE** _M0L6_2atmpS996;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS409;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS410;
  moonbit_string_t _M0L6_2atmpS995;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS411;
  int32_t _M0L7_2abindS412;
  int32_t _M0L2__S413;
  #line 193 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S384 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS390 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS397
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS390;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS402 = 0;
  _M0L6_2atmpS996 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS409
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS409)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS409->$0 = _M0L6_2atmpS996;
  _M0L16file__and__indexS409->$1 = 0;
  #line 282 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS410
  = _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS397(_M0L57moonbit__test__driver__internal__get__cli__args__internalS397);
  #line 284 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS995 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS410, 1);
  #line 283 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS411
  = _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS402(_M0L51moonbit__test__driver__internal__split__mbt__stringS402, _M0L6_2atmpS995, 47);
  _M0L7_2abindS412 = _M0L10test__argsS411->$1;
  _M0L2__S413 = 0;
  while (1) {
    if (_M0L2__S413 < _M0L7_2abindS412) {
      moonbit_string_t* _M0L8_2afieldS1040 = _M0L10test__argsS411->$0;
      moonbit_string_t* _M0L3bufS994 = _M0L8_2afieldS1040;
      moonbit_string_t _M0L6_2atmpS1039 =
        (moonbit_string_t)_M0L3bufS994[_M0L2__S413];
      moonbit_string_t _M0L3argS414 = _M0L6_2atmpS1039;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS415;
      moonbit_string_t _M0L4fileS416;
      moonbit_string_t _M0L5rangeS417;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS418;
      moonbit_string_t _M0L6_2atmpS992;
      int32_t _M0L5startS419;
      moonbit_string_t _M0L6_2atmpS991;
      int32_t _M0L3endS420;
      int32_t _M0L1iS421;
      int32_t _M0L6_2atmpS993;
      moonbit_incref(_M0L3argS414);
      #line 288 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS415
      = _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS402(_M0L51moonbit__test__driver__internal__split__mbt__stringS402, _M0L3argS414, 58);
      moonbit_incref(_M0L16file__and__rangeS415);
      #line 289 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0L4fileS416
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS415, 0);
      #line 290 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0L5rangeS417
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS415, 1);
      #line 291 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS418
      = _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS402(_M0L51moonbit__test__driver__internal__split__mbt__stringS402, _M0L5rangeS417, 45);
      moonbit_incref(_M0L15start__and__endS418);
      #line 294 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS992
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS418, 0);
      #line 294 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0L5startS419
      = _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S384(_M0L45moonbit__test__driver__internal__parse__int__S384, _M0L6_2atmpS992);
      #line 295 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS991
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS418, 1);
      #line 295 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0L3endS420
      = _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S384(_M0L45moonbit__test__driver__internal__parse__int__S384, _M0L6_2atmpS991);
      _M0L1iS421 = _M0L5startS419;
      while (1) {
        if (_M0L1iS421 < _M0L3endS420) {
          struct _M0TUsiE* _M0L8_2atupleS989;
          int32_t _M0L6_2atmpS990;
          moonbit_incref(_M0L4fileS416);
          _M0L8_2atupleS989
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS989)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS989->$0 = _M0L4fileS416;
          _M0L8_2atupleS989->$1 = _M0L1iS421;
          moonbit_incref(_M0L16file__and__indexS409);
          #line 297 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS409, _M0L8_2atupleS989);
          _M0L6_2atmpS990 = _M0L1iS421 + 1;
          _M0L1iS421 = _M0L6_2atmpS990;
          continue;
        } else {
          moonbit_decref(_M0L4fileS416);
        }
        break;
      }
      _M0L6_2atmpS993 = _M0L2__S413 + 1;
      _M0L2__S413 = _M0L6_2atmpS993;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS411);
    }
    break;
  }
  return _M0L16file__and__indexS409;
}

struct _M0TPB5ArrayGsE* _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS402(
  int32_t _M0L6_2aenvS970,
  moonbit_string_t _M0L1sS403,
  int32_t _M0L3sepS404
) {
  moonbit_string_t* _M0L6_2atmpS988;
  struct _M0TPB5ArrayGsE* _M0L3resS405;
  struct _M0TPC13ref3RefGiE* _M0L1iS406;
  struct _M0TPC13ref3RefGiE* _M0L5startS407;
  int32_t _M0L3valS983;
  int32_t _M0L6_2atmpS984;
  #line 261 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS988 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS405
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS405)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS405->$0 = _M0L6_2atmpS988;
  _M0L3resS405->$1 = 0;
  _M0L1iS406
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS406)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS406->$0 = 0;
  _M0L5startS407
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS407)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS407->$0 = 0;
  while (1) {
    int32_t _M0L3valS971 = _M0L1iS406->$0;
    int32_t _M0L6_2atmpS972 = Moonbit_array_length(_M0L1sS403);
    if (_M0L3valS971 < _M0L6_2atmpS972) {
      int32_t _M0L3valS975 = _M0L1iS406->$0;
      int32_t _M0L6_2atmpS974;
      int32_t _M0L6_2atmpS973;
      int32_t _M0L3valS982;
      int32_t _M0L6_2atmpS981;
      if (
        _M0L3valS975 < 0 || _M0L3valS975 >= Moonbit_array_length(_M0L1sS403)
      ) {
        #line 269 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS974 = _M0L1sS403[_M0L3valS975];
      _M0L6_2atmpS973 = _M0L6_2atmpS974;
      if (_M0L6_2atmpS973 == _M0L3sepS404) {
        int32_t _M0L3valS977 = _M0L5startS407->$0;
        int32_t _M0L3valS978 = _M0L1iS406->$0;
        moonbit_string_t _M0L6_2atmpS976;
        int32_t _M0L3valS980;
        int32_t _M0L6_2atmpS979;
        moonbit_incref(_M0L1sS403);
        #line 270 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS976
        = _M0MPC16string6String17unsafe__substring(_M0L1sS403, _M0L3valS977, _M0L3valS978);
        moonbit_incref(_M0L3resS405);
        #line 270 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS405, _M0L6_2atmpS976);
        _M0L3valS980 = _M0L1iS406->$0;
        _M0L6_2atmpS979 = _M0L3valS980 + 1;
        _M0L5startS407->$0 = _M0L6_2atmpS979;
      }
      _M0L3valS982 = _M0L1iS406->$0;
      _M0L6_2atmpS981 = _M0L3valS982 + 1;
      _M0L1iS406->$0 = _M0L6_2atmpS981;
      continue;
    } else {
      moonbit_decref(_M0L1iS406);
    }
    break;
  }
  _M0L3valS983 = _M0L5startS407->$0;
  _M0L6_2atmpS984 = Moonbit_array_length(_M0L1sS403);
  if (_M0L3valS983 < _M0L6_2atmpS984) {
    int32_t _M0L8_2afieldS1041 = _M0L5startS407->$0;
    int32_t _M0L3valS986;
    int32_t _M0L6_2atmpS987;
    moonbit_string_t _M0L6_2atmpS985;
    moonbit_decref(_M0L5startS407);
    _M0L3valS986 = _M0L8_2afieldS1041;
    _M0L6_2atmpS987 = Moonbit_array_length(_M0L1sS403);
    #line 276 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS985
    = _M0MPC16string6String17unsafe__substring(_M0L1sS403, _M0L3valS986, _M0L6_2atmpS987);
    moonbit_incref(_M0L3resS405);
    #line 276 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS405, _M0L6_2atmpS985);
  } else {
    moonbit_decref(_M0L5startS407);
    moonbit_decref(_M0L1sS403);
  }
  return _M0L3resS405;
}

struct _M0TPB5ArrayGsE* _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS397(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS390
) {
  moonbit_bytes_t* _M0L3tmpS398;
  int32_t _M0L6_2atmpS969;
  struct _M0TPB5ArrayGsE* _M0L3resS399;
  int32_t _M0L1iS400;
  #line 250 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  #line 253 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L3tmpS398
  = _M0FP28bikallem4blit52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS969 = Moonbit_array_length(_M0L3tmpS398);
  #line 254 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L3resS399 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS969);
  _M0L1iS400 = 0;
  while (1) {
    int32_t _M0L6_2atmpS965 = Moonbit_array_length(_M0L3tmpS398);
    if (_M0L1iS400 < _M0L6_2atmpS965) {
      moonbit_bytes_t _M0L6_2atmpS1042;
      moonbit_bytes_t _M0L6_2atmpS967;
      moonbit_string_t _M0L6_2atmpS966;
      int32_t _M0L6_2atmpS968;
      if (_M0L1iS400 < 0 || _M0L1iS400 >= Moonbit_array_length(_M0L3tmpS398)) {
        #line 256 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1042 = (moonbit_bytes_t)_M0L3tmpS398[_M0L1iS400];
      _M0L6_2atmpS967 = _M0L6_2atmpS1042;
      moonbit_incref(_M0L6_2atmpS967);
      #line 256 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS966
      = _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS390(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS390, _M0L6_2atmpS967);
      moonbit_incref(_M0L3resS399);
      #line 256 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS399, _M0L6_2atmpS966);
      _M0L6_2atmpS968 = _M0L1iS400 + 1;
      _M0L1iS400 = _M0L6_2atmpS968;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS398);
    }
    break;
  }
  return _M0L3resS399;
}

moonbit_string_t _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS390(
  int32_t _M0L6_2aenvS879,
  moonbit_bytes_t _M0L5bytesS391
) {
  struct _M0TPB13StringBuilder* _M0L3resS392;
  int32_t _M0L3lenS393;
  struct _M0TPC13ref3RefGiE* _M0L1iS394;
  #line 206 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  #line 209 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L3resS392 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS393 = Moonbit_array_length(_M0L5bytesS391);
  _M0L1iS394
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS394)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS394->$0 = 0;
  while (1) {
    int32_t _M0L3valS880 = _M0L1iS394->$0;
    if (_M0L3valS880 < _M0L3lenS393) {
      int32_t _M0L3valS964 = _M0L1iS394->$0;
      int32_t _M0L6_2atmpS963;
      int32_t _M0L6_2atmpS962;
      struct _M0TPC13ref3RefGiE* _M0L1cS395;
      int32_t _M0L3valS881;
      if (
        _M0L3valS964 < 0
        || _M0L3valS964 >= Moonbit_array_length(_M0L5bytesS391)
      ) {
        #line 213 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS963 = _M0L5bytesS391[_M0L3valS964];
      _M0L6_2atmpS962 = (int32_t)_M0L6_2atmpS963;
      _M0L1cS395
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS395)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS395->$0 = _M0L6_2atmpS962;
      _M0L3valS881 = _M0L1cS395->$0;
      if (_M0L3valS881 < 128) {
        int32_t _M0L8_2afieldS1043 = _M0L1cS395->$0;
        int32_t _M0L3valS883;
        int32_t _M0L6_2atmpS882;
        int32_t _M0L3valS885;
        int32_t _M0L6_2atmpS884;
        moonbit_decref(_M0L1cS395);
        _M0L3valS883 = _M0L8_2afieldS1043;
        _M0L6_2atmpS882 = _M0L3valS883;
        moonbit_incref(_M0L3resS392);
        #line 215 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS392, _M0L6_2atmpS882);
        _M0L3valS885 = _M0L1iS394->$0;
        _M0L6_2atmpS884 = _M0L3valS885 + 1;
        _M0L1iS394->$0 = _M0L6_2atmpS884;
      } else {
        int32_t _M0L3valS886 = _M0L1cS395->$0;
        if (_M0L3valS886 < 224) {
          int32_t _M0L3valS888 = _M0L1iS394->$0;
          int32_t _M0L6_2atmpS887 = _M0L3valS888 + 1;
          int32_t _M0L3valS897;
          int32_t _M0L6_2atmpS896;
          int32_t _M0L6_2atmpS890;
          int32_t _M0L3valS895;
          int32_t _M0L6_2atmpS894;
          int32_t _M0L6_2atmpS893;
          int32_t _M0L6_2atmpS892;
          int32_t _M0L6_2atmpS891;
          int32_t _M0L6_2atmpS889;
          int32_t _M0L8_2afieldS1044;
          int32_t _M0L3valS899;
          int32_t _M0L6_2atmpS898;
          int32_t _M0L3valS901;
          int32_t _M0L6_2atmpS900;
          if (_M0L6_2atmpS887 >= _M0L3lenS393) {
            moonbit_decref(_M0L1cS395);
            moonbit_decref(_M0L1iS394);
            moonbit_decref(_M0L5bytesS391);
            break;
          }
          _M0L3valS897 = _M0L1cS395->$0;
          _M0L6_2atmpS896 = _M0L3valS897 & 31;
          _M0L6_2atmpS890 = _M0L6_2atmpS896 << 6;
          _M0L3valS895 = _M0L1iS394->$0;
          _M0L6_2atmpS894 = _M0L3valS895 + 1;
          if (
            _M0L6_2atmpS894 < 0
            || _M0L6_2atmpS894 >= Moonbit_array_length(_M0L5bytesS391)
          ) {
            #line 221 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS893 = _M0L5bytesS391[_M0L6_2atmpS894];
          _M0L6_2atmpS892 = (int32_t)_M0L6_2atmpS893;
          _M0L6_2atmpS891 = _M0L6_2atmpS892 & 63;
          _M0L6_2atmpS889 = _M0L6_2atmpS890 | _M0L6_2atmpS891;
          _M0L1cS395->$0 = _M0L6_2atmpS889;
          _M0L8_2afieldS1044 = _M0L1cS395->$0;
          moonbit_decref(_M0L1cS395);
          _M0L3valS899 = _M0L8_2afieldS1044;
          _M0L6_2atmpS898 = _M0L3valS899;
          moonbit_incref(_M0L3resS392);
          #line 222 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS392, _M0L6_2atmpS898);
          _M0L3valS901 = _M0L1iS394->$0;
          _M0L6_2atmpS900 = _M0L3valS901 + 2;
          _M0L1iS394->$0 = _M0L6_2atmpS900;
        } else {
          int32_t _M0L3valS902 = _M0L1cS395->$0;
          if (_M0L3valS902 < 240) {
            int32_t _M0L3valS904 = _M0L1iS394->$0;
            int32_t _M0L6_2atmpS903 = _M0L3valS904 + 2;
            int32_t _M0L3valS920;
            int32_t _M0L6_2atmpS919;
            int32_t _M0L6_2atmpS912;
            int32_t _M0L3valS918;
            int32_t _M0L6_2atmpS917;
            int32_t _M0L6_2atmpS916;
            int32_t _M0L6_2atmpS915;
            int32_t _M0L6_2atmpS914;
            int32_t _M0L6_2atmpS913;
            int32_t _M0L6_2atmpS906;
            int32_t _M0L3valS911;
            int32_t _M0L6_2atmpS910;
            int32_t _M0L6_2atmpS909;
            int32_t _M0L6_2atmpS908;
            int32_t _M0L6_2atmpS907;
            int32_t _M0L6_2atmpS905;
            int32_t _M0L8_2afieldS1045;
            int32_t _M0L3valS922;
            int32_t _M0L6_2atmpS921;
            int32_t _M0L3valS924;
            int32_t _M0L6_2atmpS923;
            if (_M0L6_2atmpS903 >= _M0L3lenS393) {
              moonbit_decref(_M0L1cS395);
              moonbit_decref(_M0L1iS394);
              moonbit_decref(_M0L5bytesS391);
              break;
            }
            _M0L3valS920 = _M0L1cS395->$0;
            _M0L6_2atmpS919 = _M0L3valS920 & 15;
            _M0L6_2atmpS912 = _M0L6_2atmpS919 << 12;
            _M0L3valS918 = _M0L1iS394->$0;
            _M0L6_2atmpS917 = _M0L3valS918 + 1;
            if (
              _M0L6_2atmpS917 < 0
              || _M0L6_2atmpS917 >= Moonbit_array_length(_M0L5bytesS391)
            ) {
              #line 229 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS916 = _M0L5bytesS391[_M0L6_2atmpS917];
            _M0L6_2atmpS915 = (int32_t)_M0L6_2atmpS916;
            _M0L6_2atmpS914 = _M0L6_2atmpS915 & 63;
            _M0L6_2atmpS913 = _M0L6_2atmpS914 << 6;
            _M0L6_2atmpS906 = _M0L6_2atmpS912 | _M0L6_2atmpS913;
            _M0L3valS911 = _M0L1iS394->$0;
            _M0L6_2atmpS910 = _M0L3valS911 + 2;
            if (
              _M0L6_2atmpS910 < 0
              || _M0L6_2atmpS910 >= Moonbit_array_length(_M0L5bytesS391)
            ) {
              #line 230 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS909 = _M0L5bytesS391[_M0L6_2atmpS910];
            _M0L6_2atmpS908 = (int32_t)_M0L6_2atmpS909;
            _M0L6_2atmpS907 = _M0L6_2atmpS908 & 63;
            _M0L6_2atmpS905 = _M0L6_2atmpS906 | _M0L6_2atmpS907;
            _M0L1cS395->$0 = _M0L6_2atmpS905;
            _M0L8_2afieldS1045 = _M0L1cS395->$0;
            moonbit_decref(_M0L1cS395);
            _M0L3valS922 = _M0L8_2afieldS1045;
            _M0L6_2atmpS921 = _M0L3valS922;
            moonbit_incref(_M0L3resS392);
            #line 231 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS392, _M0L6_2atmpS921);
            _M0L3valS924 = _M0L1iS394->$0;
            _M0L6_2atmpS923 = _M0L3valS924 + 3;
            _M0L1iS394->$0 = _M0L6_2atmpS923;
          } else {
            int32_t _M0L3valS926 = _M0L1iS394->$0;
            int32_t _M0L6_2atmpS925 = _M0L3valS926 + 3;
            int32_t _M0L3valS949;
            int32_t _M0L6_2atmpS948;
            int32_t _M0L6_2atmpS941;
            int32_t _M0L3valS947;
            int32_t _M0L6_2atmpS946;
            int32_t _M0L6_2atmpS945;
            int32_t _M0L6_2atmpS944;
            int32_t _M0L6_2atmpS943;
            int32_t _M0L6_2atmpS942;
            int32_t _M0L6_2atmpS934;
            int32_t _M0L3valS940;
            int32_t _M0L6_2atmpS939;
            int32_t _M0L6_2atmpS938;
            int32_t _M0L6_2atmpS937;
            int32_t _M0L6_2atmpS936;
            int32_t _M0L6_2atmpS935;
            int32_t _M0L6_2atmpS928;
            int32_t _M0L3valS933;
            int32_t _M0L6_2atmpS932;
            int32_t _M0L6_2atmpS931;
            int32_t _M0L6_2atmpS930;
            int32_t _M0L6_2atmpS929;
            int32_t _M0L6_2atmpS927;
            int32_t _M0L3valS951;
            int32_t _M0L6_2atmpS950;
            int32_t _M0L3valS955;
            int32_t _M0L6_2atmpS954;
            int32_t _M0L6_2atmpS953;
            int32_t _M0L6_2atmpS952;
            int32_t _M0L8_2afieldS1046;
            int32_t _M0L3valS959;
            int32_t _M0L6_2atmpS958;
            int32_t _M0L6_2atmpS957;
            int32_t _M0L6_2atmpS956;
            int32_t _M0L3valS961;
            int32_t _M0L6_2atmpS960;
            if (_M0L6_2atmpS925 >= _M0L3lenS393) {
              moonbit_decref(_M0L1cS395);
              moonbit_decref(_M0L1iS394);
              moonbit_decref(_M0L5bytesS391);
              break;
            }
            _M0L3valS949 = _M0L1cS395->$0;
            _M0L6_2atmpS948 = _M0L3valS949 & 7;
            _M0L6_2atmpS941 = _M0L6_2atmpS948 << 18;
            _M0L3valS947 = _M0L1iS394->$0;
            _M0L6_2atmpS946 = _M0L3valS947 + 1;
            if (
              _M0L6_2atmpS946 < 0
              || _M0L6_2atmpS946 >= Moonbit_array_length(_M0L5bytesS391)
            ) {
              #line 238 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS945 = _M0L5bytesS391[_M0L6_2atmpS946];
            _M0L6_2atmpS944 = (int32_t)_M0L6_2atmpS945;
            _M0L6_2atmpS943 = _M0L6_2atmpS944 & 63;
            _M0L6_2atmpS942 = _M0L6_2atmpS943 << 12;
            _M0L6_2atmpS934 = _M0L6_2atmpS941 | _M0L6_2atmpS942;
            _M0L3valS940 = _M0L1iS394->$0;
            _M0L6_2atmpS939 = _M0L3valS940 + 2;
            if (
              _M0L6_2atmpS939 < 0
              || _M0L6_2atmpS939 >= Moonbit_array_length(_M0L5bytesS391)
            ) {
              #line 239 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS938 = _M0L5bytesS391[_M0L6_2atmpS939];
            _M0L6_2atmpS937 = (int32_t)_M0L6_2atmpS938;
            _M0L6_2atmpS936 = _M0L6_2atmpS937 & 63;
            _M0L6_2atmpS935 = _M0L6_2atmpS936 << 6;
            _M0L6_2atmpS928 = _M0L6_2atmpS934 | _M0L6_2atmpS935;
            _M0L3valS933 = _M0L1iS394->$0;
            _M0L6_2atmpS932 = _M0L3valS933 + 3;
            if (
              _M0L6_2atmpS932 < 0
              || _M0L6_2atmpS932 >= Moonbit_array_length(_M0L5bytesS391)
            ) {
              #line 240 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS931 = _M0L5bytesS391[_M0L6_2atmpS932];
            _M0L6_2atmpS930 = (int32_t)_M0L6_2atmpS931;
            _M0L6_2atmpS929 = _M0L6_2atmpS930 & 63;
            _M0L6_2atmpS927 = _M0L6_2atmpS928 | _M0L6_2atmpS929;
            _M0L1cS395->$0 = _M0L6_2atmpS927;
            _M0L3valS951 = _M0L1cS395->$0;
            _M0L6_2atmpS950 = _M0L3valS951 - 65536;
            _M0L1cS395->$0 = _M0L6_2atmpS950;
            _M0L3valS955 = _M0L1cS395->$0;
            _M0L6_2atmpS954 = _M0L3valS955 >> 10;
            _M0L6_2atmpS953 = _M0L6_2atmpS954 + 55296;
            _M0L6_2atmpS952 = _M0L6_2atmpS953;
            moonbit_incref(_M0L3resS392);
            #line 242 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS392, _M0L6_2atmpS952);
            _M0L8_2afieldS1046 = _M0L1cS395->$0;
            moonbit_decref(_M0L1cS395);
            _M0L3valS959 = _M0L8_2afieldS1046;
            _M0L6_2atmpS958 = _M0L3valS959 & 1023;
            _M0L6_2atmpS957 = _M0L6_2atmpS958 + 56320;
            _M0L6_2atmpS956 = _M0L6_2atmpS957;
            moonbit_incref(_M0L3resS392);
            #line 243 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS392, _M0L6_2atmpS956);
            _M0L3valS961 = _M0L1iS394->$0;
            _M0L6_2atmpS960 = _M0L3valS961 + 4;
            _M0L1iS394->$0 = _M0L6_2atmpS960;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS394);
      moonbit_decref(_M0L5bytesS391);
    }
    break;
  }
  #line 247 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS392);
}

int32_t _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S384(
  int32_t _M0L6_2aenvS872,
  moonbit_string_t _M0L1sS385
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS386;
  int32_t _M0L3lenS387;
  int32_t _M0L1iS388;
  int32_t _M0L8_2afieldS1047;
  #line 197 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L3resS386
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS386)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS386->$0 = 0;
  _M0L3lenS387 = Moonbit_array_length(_M0L1sS385);
  _M0L1iS388 = 0;
  while (1) {
    if (_M0L1iS388 < _M0L3lenS387) {
      int32_t _M0L3valS877 = _M0L3resS386->$0;
      int32_t _M0L6_2atmpS874 = _M0L3valS877 * 10;
      int32_t _M0L6_2atmpS876;
      int32_t _M0L6_2atmpS875;
      int32_t _M0L6_2atmpS873;
      int32_t _M0L6_2atmpS878;
      if (_M0L1iS388 < 0 || _M0L1iS388 >= Moonbit_array_length(_M0L1sS385)) {
        #line 201 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS876 = _M0L1sS385[_M0L1iS388];
      _M0L6_2atmpS875 = _M0L6_2atmpS876 - 48;
      _M0L6_2atmpS873 = _M0L6_2atmpS874 + _M0L6_2atmpS875;
      _M0L3resS386->$0 = _M0L6_2atmpS873;
      _M0L6_2atmpS878 = _M0L1iS388 + 1;
      _M0L1iS388 = _M0L6_2atmpS878;
      continue;
    } else {
      moonbit_decref(_M0L1sS385);
    }
    break;
  }
  _M0L8_2afieldS1047 = _M0L3resS386->$0;
  moonbit_decref(_M0L3resS386);
  return _M0L8_2afieldS1047;
}

struct moonbit_result_0 _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit41MoonBit__Test__Driver__Internal__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S359,
  moonbit_string_t _M0L12_2adiscard__S360,
  int32_t _M0L12_2adiscard__S361,
  struct _M0TWssbEu* _M0L12_2adiscard__S362,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S363
) {
  struct moonbit_result_0 _result_1160;
  #line 34 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S363);
  moonbit_decref(_M0L12_2adiscard__S362);
  moonbit_decref(_M0L12_2adiscard__S360);
  moonbit_decref(_M0L12_2adiscard__S359);
  _result_1160.tag = 1;
  _result_1160.data.ok = 0;
  return _result_1160;
}

struct moonbit_result_0 _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S364,
  moonbit_string_t _M0L12_2adiscard__S365,
  int32_t _M0L12_2adiscard__S366,
  struct _M0TWssbEu* _M0L12_2adiscard__S367,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S368
) {
  struct moonbit_result_0 _result_1161;
  #line 34 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S368);
  moonbit_decref(_M0L12_2adiscard__S367);
  moonbit_decref(_M0L12_2adiscard__S365);
  moonbit_decref(_M0L12_2adiscard__S364);
  _result_1161.tag = 1;
  _result_1161.data.ok = 0;
  return _result_1161;
}

struct moonbit_result_0 _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S369,
  moonbit_string_t _M0L12_2adiscard__S370,
  int32_t _M0L12_2adiscard__S371,
  struct _M0TWssbEu* _M0L12_2adiscard__S372,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S373
) {
  struct moonbit_result_0 _result_1162;
  #line 34 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S373);
  moonbit_decref(_M0L12_2adiscard__S372);
  moonbit_decref(_M0L12_2adiscard__S370);
  moonbit_decref(_M0L12_2adiscard__S369);
  _result_1162.tag = 1;
  _result_1162.data.ok = 0;
  return _result_1162;
}

struct moonbit_result_0 _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S374,
  moonbit_string_t _M0L12_2adiscard__S375,
  int32_t _M0L12_2adiscard__S376,
  struct _M0TWssbEu* _M0L12_2adiscard__S377,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S378
) {
  struct moonbit_result_0 _result_1163;
  #line 34 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S378);
  moonbit_decref(_M0L12_2adiscard__S377);
  moonbit_decref(_M0L12_2adiscard__S375);
  moonbit_decref(_M0L12_2adiscard__S374);
  _result_1163.tag = 1;
  _result_1163.data.ok = 0;
  return _result_1163;
}

struct moonbit_result_0 _M0IP016_24default__implP28bikallem4blit21MoonBit__Test__Driver9run__testGRP28bikallem4blit50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S379,
  moonbit_string_t _M0L12_2adiscard__S380,
  int32_t _M0L12_2adiscard__S381,
  struct _M0TWssbEu* _M0L12_2adiscard__S382,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S383
) {
  struct moonbit_result_0 _result_1164;
  #line 34 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S383);
  moonbit_decref(_M0L12_2adiscard__S382);
  moonbit_decref(_M0L12_2adiscard__S380);
  moonbit_decref(_M0L12_2adiscard__S379);
  _result_1164.tag = 1;
  _result_1164.data.ok = 0;
  return _result_1164;
}

int32_t _M0IP016_24default__implP28bikallem4blit28MoonBit__Async__Test__Driver17run__async__testsGRP28bikallem4blit34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S358
) {
  #line 12 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S358);
  return 0;
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS356,
  struct _M0TPB6Logger _M0L6loggerS357
) {
  moonbit_string_t _M0L6_2atmpS871;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS870;
  #line 43 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS871 = _M0L4selfS356;
  #line 44 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS870 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS871);
  #line 44 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS870, _M0L6loggerS357);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS333,
  struct _M0TPB6Logger _M0L6loggerS355
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS1056;
  struct _M0TPC16string10StringView _M0L3pkgS332;
  moonbit_string_t _M0L7_2adataS334;
  int32_t _M0L8_2astartS335;
  int32_t _M0L6_2atmpS869;
  int32_t _M0L6_2aendS336;
  int32_t _M0Lm9_2acursorS337;
  int32_t _M0Lm13accept__stateS338;
  int32_t _M0Lm10match__endS339;
  int32_t _M0Lm20match__tag__saver__0S340;
  int32_t _M0Lm6tag__0S341;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS342;
  struct _M0TPC16string10StringView _M0L8_2afieldS1055;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS351;
  void* _M0L8_2afieldS1054;
  int32_t _M0L6_2acntS1120;
  void* _M0L16_2apackage__nameS352;
  struct _M0TPC16string10StringView _M0L8_2afieldS1052;
  struct _M0TPC16string10StringView _M0L8filenameS846;
  struct _M0TPC16string10StringView _M0L8_2afieldS1051;
  struct _M0TPC16string10StringView _M0L11start__lineS847;
  struct _M0TPC16string10StringView _M0L8_2afieldS1050;
  struct _M0TPC16string10StringView _M0L13start__columnS848;
  struct _M0TPC16string10StringView _M0L8_2afieldS1049;
  struct _M0TPC16string10StringView _M0L9end__lineS849;
  struct _M0TPC16string10StringView _M0L8_2afieldS1048;
  int32_t _M0L6_2acntS1124;
  struct _M0TPC16string10StringView _M0L11end__columnS850;
  #line 58 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2afieldS1056
  = (struct _M0TPC16string10StringView){
    _M0L4selfS333->$0_1, _M0L4selfS333->$0_2, _M0L4selfS333->$0_0
  };
  _M0L3pkgS332 = _M0L8_2afieldS1056;
  moonbit_incref(_M0L3pkgS332.$0);
  moonbit_incref(_M0L3pkgS332.$0);
  #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L7_2adataS334 = _M0MPC16string10StringView4data(_M0L3pkgS332);
  moonbit_incref(_M0L3pkgS332.$0);
  #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2astartS335 = _M0MPC16string10StringView13start__offset(_M0L3pkgS332);
  moonbit_incref(_M0L3pkgS332.$0);
  #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS869 = _M0MPC16string10StringView6length(_M0L3pkgS332);
  _M0L6_2aendS336 = _M0L8_2astartS335 + _M0L6_2atmpS869;
  _M0Lm9_2acursorS337 = _M0L8_2astartS335;
  _M0Lm13accept__stateS338 = -1;
  _M0Lm10match__endS339 = -1;
  _M0Lm20match__tag__saver__0S340 = -1;
  _M0Lm6tag__0S341 = -1;
  while (1) {
    int32_t _M0L6_2atmpS861 = _M0Lm9_2acursorS337;
    if (_M0L6_2atmpS861 < _M0L6_2aendS336) {
      int32_t _M0L6_2atmpS868 = _M0Lm9_2acursorS337;
      int32_t _M0L10next__charS346;
      int32_t _M0L6_2atmpS862;
      moonbit_incref(_M0L7_2adataS334);
      #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L10next__charS346
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS334, _M0L6_2atmpS868);
      _M0L6_2atmpS862 = _M0Lm9_2acursorS337;
      _M0Lm9_2acursorS337 = _M0L6_2atmpS862 + 1;
      if (_M0L10next__charS346 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS863;
          _M0Lm6tag__0S341 = _M0Lm9_2acursorS337;
          _M0L6_2atmpS863 = _M0Lm9_2acursorS337;
          if (_M0L6_2atmpS863 < _M0L6_2aendS336) {
            int32_t _M0L6_2atmpS867 = _M0Lm9_2acursorS337;
            int32_t _M0L10next__charS347;
            int32_t _M0L6_2atmpS864;
            moonbit_incref(_M0L7_2adataS334);
            #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS347
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS334, _M0L6_2atmpS867);
            _M0L6_2atmpS864 = _M0Lm9_2acursorS337;
            _M0Lm9_2acursorS337 = _M0L6_2atmpS864 + 1;
            if (_M0L10next__charS347 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS865 = _M0Lm9_2acursorS337;
                if (_M0L6_2atmpS865 < _M0L6_2aendS336) {
                  int32_t _M0L6_2atmpS866 = _M0Lm9_2acursorS337;
                  _M0Lm9_2acursorS337 = _M0L6_2atmpS866 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S340 = _M0Lm6tag__0S341;
                  _M0Lm13accept__stateS338 = 0;
                  _M0Lm10match__endS339 = _M0Lm9_2acursorS337;
                  goto join_343;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_343;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_343;
    }
    break;
  }
  goto joinlet_1165;
  join_343:;
  switch (_M0Lm13accept__stateS338) {
    case 0: {
      int32_t _M0L6_2atmpS859;
      int32_t _M0L6_2atmpS858;
      int64_t _M0L6_2atmpS855;
      int32_t _M0L6_2atmpS857;
      int64_t _M0L6_2atmpS856;
      struct _M0TPC16string10StringView _M0L13package__nameS344;
      int64_t _M0L6_2atmpS852;
      int32_t _M0L6_2atmpS854;
      int64_t _M0L6_2atmpS853;
      struct _M0TPC16string10StringView _M0L12module__nameS345;
      void* _M0L4SomeS851;
      moonbit_decref(_M0L3pkgS332.$0);
      _M0L6_2atmpS859 = _M0Lm20match__tag__saver__0S340;
      _M0L6_2atmpS858 = _M0L6_2atmpS859 + 1;
      _M0L6_2atmpS855 = (int64_t)_M0L6_2atmpS858;
      _M0L6_2atmpS857 = _M0Lm10match__endS339;
      _M0L6_2atmpS856 = (int64_t)_M0L6_2atmpS857;
      moonbit_incref(_M0L7_2adataS334);
      #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L13package__nameS344
      = _M0MPC16string6String4view(_M0L7_2adataS334, _M0L6_2atmpS855, _M0L6_2atmpS856);
      _M0L6_2atmpS852 = (int64_t)_M0L8_2astartS335;
      _M0L6_2atmpS854 = _M0Lm20match__tag__saver__0S340;
      _M0L6_2atmpS853 = (int64_t)_M0L6_2atmpS854;
      #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L12module__nameS345
      = _M0MPC16string6String4view(_M0L7_2adataS334, _M0L6_2atmpS852, _M0L6_2atmpS853);
      _M0L4SomeS851
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS851)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS851)->$0_0
      = _M0L13package__nameS344.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS851)->$0_1
      = _M0L13package__nameS344.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS851)->$0_2
      = _M0L13package__nameS344.$2;
      _M0L7_2abindS342
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS342)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS342->$0_0 = _M0L12module__nameS345.$0;
      _M0L7_2abindS342->$0_1 = _M0L12module__nameS345.$1;
      _M0L7_2abindS342->$0_2 = _M0L12module__nameS345.$2;
      _M0L7_2abindS342->$1 = _M0L4SomeS851;
      break;
    }
    default: {
      void* _M0L4NoneS860;
      moonbit_decref(_M0L7_2adataS334);
      _M0L4NoneS860
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS342
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS342)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS342->$0_0 = _M0L3pkgS332.$0;
      _M0L7_2abindS342->$0_1 = _M0L3pkgS332.$1;
      _M0L7_2abindS342->$0_2 = _M0L3pkgS332.$2;
      _M0L7_2abindS342->$1 = _M0L4NoneS860;
      break;
    }
  }
  joinlet_1165:;
  _M0L8_2afieldS1055
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS342->$0_1, _M0L7_2abindS342->$0_2, _M0L7_2abindS342->$0_0
  };
  _M0L15_2amodule__nameS351 = _M0L8_2afieldS1055;
  _M0L8_2afieldS1054 = _M0L7_2abindS342->$1;
  _M0L6_2acntS1120 = Moonbit_object_header(_M0L7_2abindS342)->rc;
  if (_M0L6_2acntS1120 > 1) {
    int32_t _M0L11_2anew__cntS1121 = _M0L6_2acntS1120 - 1;
    Moonbit_object_header(_M0L7_2abindS342)->rc = _M0L11_2anew__cntS1121;
    moonbit_incref(_M0L8_2afieldS1054);
    moonbit_incref(_M0L15_2amodule__nameS351.$0);
  } else if (_M0L6_2acntS1120 == 1) {
    #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L7_2abindS342);
  }
  _M0L16_2apackage__nameS352 = _M0L8_2afieldS1054;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS352)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS353 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS352;
      struct _M0TPC16string10StringView _M0L8_2afieldS1053 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS353->$0_1,
                                              _M0L7_2aSomeS353->$0_2,
                                              _M0L7_2aSomeS353->$0_0};
      int32_t _M0L6_2acntS1122 = Moonbit_object_header(_M0L7_2aSomeS353)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS354;
      if (_M0L6_2acntS1122 > 1) {
        int32_t _M0L11_2anew__cntS1123 = _M0L6_2acntS1122 - 1;
        Moonbit_object_header(_M0L7_2aSomeS353)->rc = _M0L11_2anew__cntS1123;
        moonbit_incref(_M0L8_2afieldS1053.$0);
      } else if (_M0L6_2acntS1122 == 1) {
        #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS353);
      }
      _M0L12_2apkg__nameS354 = _M0L8_2afieldS1053;
      if (_M0L6loggerS355.$1) {
        moonbit_incref(_M0L6loggerS355.$1);
      }
      #line 66 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L6loggerS355.$0->$method_2(_M0L6loggerS355.$1, _M0L12_2apkg__nameS354);
      if (_M0L6loggerS355.$1) {
        moonbit_incref(_M0L6loggerS355.$1);
      }
      #line 67 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L6loggerS355.$0->$method_3(_M0L6loggerS355.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS352);
      break;
    }
  }
  _M0L8_2afieldS1052
  = (struct _M0TPC16string10StringView){
    _M0L4selfS333->$1_1, _M0L4selfS333->$1_2, _M0L4selfS333->$1_0
  };
  _M0L8filenameS846 = _M0L8_2afieldS1052;
  moonbit_incref(_M0L8filenameS846.$0);
  if (_M0L6loggerS355.$1) {
    moonbit_incref(_M0L6loggerS355.$1);
  }
  #line 69 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_2(_M0L6loggerS355.$1, _M0L8filenameS846);
  if (_M0L6loggerS355.$1) {
    moonbit_incref(_M0L6loggerS355.$1);
  }
  #line 70 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_3(_M0L6loggerS355.$1, 58);
  _M0L8_2afieldS1051
  = (struct _M0TPC16string10StringView){
    _M0L4selfS333->$2_1, _M0L4selfS333->$2_2, _M0L4selfS333->$2_0
  };
  _M0L11start__lineS847 = _M0L8_2afieldS1051;
  moonbit_incref(_M0L11start__lineS847.$0);
  if (_M0L6loggerS355.$1) {
    moonbit_incref(_M0L6loggerS355.$1);
  }
  #line 71 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_2(_M0L6loggerS355.$1, _M0L11start__lineS847);
  if (_M0L6loggerS355.$1) {
    moonbit_incref(_M0L6loggerS355.$1);
  }
  #line 72 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_3(_M0L6loggerS355.$1, 58);
  _M0L8_2afieldS1050
  = (struct _M0TPC16string10StringView){
    _M0L4selfS333->$3_1, _M0L4selfS333->$3_2, _M0L4selfS333->$3_0
  };
  _M0L13start__columnS848 = _M0L8_2afieldS1050;
  moonbit_incref(_M0L13start__columnS848.$0);
  if (_M0L6loggerS355.$1) {
    moonbit_incref(_M0L6loggerS355.$1);
  }
  #line 73 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_2(_M0L6loggerS355.$1, _M0L13start__columnS848);
  if (_M0L6loggerS355.$1) {
    moonbit_incref(_M0L6loggerS355.$1);
  }
  #line 74 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_3(_M0L6loggerS355.$1, 45);
  _M0L8_2afieldS1049
  = (struct _M0TPC16string10StringView){
    _M0L4selfS333->$4_1, _M0L4selfS333->$4_2, _M0L4selfS333->$4_0
  };
  _M0L9end__lineS849 = _M0L8_2afieldS1049;
  moonbit_incref(_M0L9end__lineS849.$0);
  if (_M0L6loggerS355.$1) {
    moonbit_incref(_M0L6loggerS355.$1);
  }
  #line 75 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_2(_M0L6loggerS355.$1, _M0L9end__lineS849);
  if (_M0L6loggerS355.$1) {
    moonbit_incref(_M0L6loggerS355.$1);
  }
  #line 76 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_3(_M0L6loggerS355.$1, 58);
  _M0L8_2afieldS1048
  = (struct _M0TPC16string10StringView){
    _M0L4selfS333->$5_1, _M0L4selfS333->$5_2, _M0L4selfS333->$5_0
  };
  _M0L6_2acntS1124 = Moonbit_object_header(_M0L4selfS333)->rc;
  if (_M0L6_2acntS1124 > 1) {
    int32_t _M0L11_2anew__cntS1130 = _M0L6_2acntS1124 - 1;
    Moonbit_object_header(_M0L4selfS333)->rc = _M0L11_2anew__cntS1130;
    moonbit_incref(_M0L8_2afieldS1048.$0);
  } else if (_M0L6_2acntS1124 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS1129 =
      (struct _M0TPC16string10StringView){_M0L4selfS333->$4_1,
                                            _M0L4selfS333->$4_2,
                                            _M0L4selfS333->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS1128;
    struct _M0TPC16string10StringView _M0L8_2afieldS1127;
    struct _M0TPC16string10StringView _M0L8_2afieldS1126;
    struct _M0TPC16string10StringView _M0L8_2afieldS1125;
    moonbit_decref(_M0L8_2afieldS1129.$0);
    _M0L8_2afieldS1128
    = (struct _M0TPC16string10StringView){
      _M0L4selfS333->$3_1, _M0L4selfS333->$3_2, _M0L4selfS333->$3_0
    };
    moonbit_decref(_M0L8_2afieldS1128.$0);
    _M0L8_2afieldS1127
    = (struct _M0TPC16string10StringView){
      _M0L4selfS333->$2_1, _M0L4selfS333->$2_2, _M0L4selfS333->$2_0
    };
    moonbit_decref(_M0L8_2afieldS1127.$0);
    _M0L8_2afieldS1126
    = (struct _M0TPC16string10StringView){
      _M0L4selfS333->$1_1, _M0L4selfS333->$1_2, _M0L4selfS333->$1_0
    };
    moonbit_decref(_M0L8_2afieldS1126.$0);
    _M0L8_2afieldS1125
    = (struct _M0TPC16string10StringView){
      _M0L4selfS333->$0_1, _M0L4selfS333->$0_2, _M0L4selfS333->$0_0
    };
    moonbit_decref(_M0L8_2afieldS1125.$0);
    #line 77 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L4selfS333);
  }
  _M0L11end__columnS850 = _M0L8_2afieldS1048;
  if (_M0L6loggerS355.$1) {
    moonbit_incref(_M0L6loggerS355.$1);
  }
  #line 77 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_2(_M0L6loggerS355.$1, _M0L11end__columnS850);
  if (_M0L6loggerS355.$1) {
    moonbit_incref(_M0L6loggerS355.$1);
  }
  #line 78 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_3(_M0L6loggerS355.$1, 64);
  #line 79 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS355.$0->$method_2(_M0L6loggerS355.$1, _M0L15_2amodule__nameS351);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS331) {
  moonbit_string_t _M0L6_2atmpS845;
  #line 37 "/home/blem/.moon/lib/core/builtin/console.mbt"
  #line 38 "/home/blem/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS845 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS331);
  #line 38 "/home/blem/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS845);
  moonbit_decref(_M0L6_2atmpS845);
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS329,
  int32_t _M0L3idxS330
) {
  int32_t _M0L6_2atmpS1057;
  #line 1794 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1057 = _M0L4selfS329[_M0L3idxS330];
  moonbit_decref(_M0L4selfS329);
  return _M0L6_2atmpS1057;
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS328
) {
  #line 168 "/home/blem/.moon/lib/core/builtin/show.mbt"
  return _M0L4selfS328;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS327,
  struct _M0TPB6Logger _M0L6loggerS326
) {
  moonbit_string_t _M0L6_2atmpS844;
  #line 30 "/home/blem/.moon/lib/core/builtin/show.mbt"
  #line 31 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS844 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS327, 10);
  #line 31 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS326.$0->$method_0(_M0L6loggerS326.$1, _M0L6_2atmpS844);
  return 0;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS320,
  moonbit_string_t _M0L5valueS322
) {
  int32_t _M0L3lenS834;
  moonbit_string_t* _M0L6_2atmpS836;
  int32_t _M0L6_2atmpS1060;
  int32_t _M0L6_2atmpS835;
  int32_t _M0L6lengthS321;
  moonbit_string_t* _M0L8_2afieldS1059;
  moonbit_string_t* _M0L3bufS837;
  moonbit_string_t _M0L6_2aoldS1058;
  int32_t _M0L6_2atmpS838;
  #line 242 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS834 = _M0L4selfS320->$1;
  moonbit_incref(_M0L4selfS320);
  #line 243 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS836 = _M0MPC15array5Array6bufferGsE(_M0L4selfS320);
  _M0L6_2atmpS1060 = Moonbit_array_length(_M0L6_2atmpS836);
  moonbit_decref(_M0L6_2atmpS836);
  _M0L6_2atmpS835 = _M0L6_2atmpS1060;
  if (_M0L3lenS834 == _M0L6_2atmpS835) {
    moonbit_incref(_M0L4selfS320);
    #line 244 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS320);
  }
  _M0L6lengthS321 = _M0L4selfS320->$1;
  _M0L8_2afieldS1059 = _M0L4selfS320->$0;
  _M0L3bufS837 = _M0L8_2afieldS1059;
  _M0L6_2aoldS1058 = (moonbit_string_t)_M0L3bufS837[_M0L6lengthS321];
  moonbit_decref(_M0L6_2aoldS1058);
  _M0L3bufS837[_M0L6lengthS321] = _M0L5valueS322;
  _M0L6_2atmpS838 = _M0L6lengthS321 + 1;
  _M0L4selfS320->$1 = _M0L6_2atmpS838;
  moonbit_decref(_M0L4selfS320);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS323,
  struct _M0TUsiE* _M0L5valueS325
) {
  int32_t _M0L3lenS839;
  struct _M0TUsiE** _M0L6_2atmpS841;
  int32_t _M0L6_2atmpS1063;
  int32_t _M0L6_2atmpS840;
  int32_t _M0L6lengthS324;
  struct _M0TUsiE** _M0L8_2afieldS1062;
  struct _M0TUsiE** _M0L3bufS842;
  struct _M0TUsiE* _M0L6_2aoldS1061;
  int32_t _M0L6_2atmpS843;
  #line 242 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS839 = _M0L4selfS323->$1;
  moonbit_incref(_M0L4selfS323);
  #line 243 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS841 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS323);
  _M0L6_2atmpS1063 = Moonbit_array_length(_M0L6_2atmpS841);
  moonbit_decref(_M0L6_2atmpS841);
  _M0L6_2atmpS840 = _M0L6_2atmpS1063;
  if (_M0L3lenS839 == _M0L6_2atmpS840) {
    moonbit_incref(_M0L4selfS323);
    #line 244 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS323);
  }
  _M0L6lengthS324 = _M0L4selfS323->$1;
  _M0L8_2afieldS1062 = _M0L4selfS323->$0;
  _M0L3bufS842 = _M0L8_2afieldS1062;
  _M0L6_2aoldS1061 = (struct _M0TUsiE*)_M0L3bufS842[_M0L6lengthS324];
  if (_M0L6_2aoldS1061) {
    moonbit_decref(_M0L6_2aoldS1061);
  }
  _M0L3bufS842[_M0L6lengthS324] = _M0L5valueS325;
  _M0L6_2atmpS843 = _M0L6lengthS324 + 1;
  _M0L4selfS323->$1 = _M0L6_2atmpS843;
  moonbit_decref(_M0L4selfS323);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS315) {
  int32_t _M0L8old__capS314;
  int32_t _M0L8new__capS316;
  #line 182 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS314 = _M0L4selfS315->$1;
  if (_M0L8old__capS314 == 0) {
    _M0L8new__capS316 = 8;
  } else {
    _M0L8new__capS316 = _M0L8old__capS314 * 2;
  }
  #line 185 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS315, _M0L8new__capS316);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS318
) {
  int32_t _M0L8old__capS317;
  int32_t _M0L8new__capS319;
  #line 182 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS317 = _M0L4selfS318->$1;
  if (_M0L8old__capS317 == 0) {
    _M0L8new__capS319 = 8;
  } else {
    _M0L8new__capS319 = _M0L8old__capS317 * 2;
  }
  #line 185 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS318, _M0L8new__capS319);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS305,
  int32_t _M0L13new__capacityS303
) {
  moonbit_string_t* _M0L8new__bufS302;
  moonbit_string_t* _M0L8_2afieldS1065;
  moonbit_string_t* _M0L8old__bufS304;
  int32_t _M0L8old__capS306;
  int32_t _M0L9copy__lenS307;
  moonbit_string_t* _M0L6_2aoldS1064;
  #line 129 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS302
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS303, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS1065 = _M0L4selfS305->$0;
  _M0L8old__bufS304 = _M0L8_2afieldS1065;
  _M0L8old__capS306 = Moonbit_array_length(_M0L8old__bufS304);
  if (_M0L8old__capS306 < _M0L13new__capacityS303) {
    _M0L9copy__lenS307 = _M0L8old__capS306;
  } else {
    _M0L9copy__lenS307 = _M0L13new__capacityS303;
  }
  moonbit_incref(_M0L8old__bufS304);
  moonbit_incref(_M0L8new__bufS302);
  #line 134 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS302, 0, _M0L8old__bufS304, 0, _M0L9copy__lenS307);
  _M0L6_2aoldS1064 = _M0L4selfS305->$0;
  moonbit_decref(_M0L6_2aoldS1064);
  _M0L4selfS305->$0 = _M0L8new__bufS302;
  moonbit_decref(_M0L4selfS305);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS311,
  int32_t _M0L13new__capacityS309
) {
  struct _M0TUsiE** _M0L8new__bufS308;
  struct _M0TUsiE** _M0L8_2afieldS1067;
  struct _M0TUsiE** _M0L8old__bufS310;
  int32_t _M0L8old__capS312;
  int32_t _M0L9copy__lenS313;
  struct _M0TUsiE** _M0L6_2aoldS1066;
  #line 129 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS308
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS309, 0);
  _M0L8_2afieldS1067 = _M0L4selfS311->$0;
  _M0L8old__bufS310 = _M0L8_2afieldS1067;
  _M0L8old__capS312 = Moonbit_array_length(_M0L8old__bufS310);
  if (_M0L8old__capS312 < _M0L13new__capacityS309) {
    _M0L9copy__lenS313 = _M0L8old__capS312;
  } else {
    _M0L9copy__lenS313 = _M0L13new__capacityS309;
  }
  moonbit_incref(_M0L8old__bufS310);
  moonbit_incref(_M0L8new__bufS308);
  #line 134 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS308, 0, _M0L8old__bufS310, 0, _M0L9copy__lenS313);
  _M0L6_2aoldS1066 = _M0L4selfS311->$0;
  moonbit_decref(_M0L6_2aoldS1066);
  _M0L4selfS311->$0 = _M0L8new__bufS308;
  moonbit_decref(_M0L4selfS311);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS301
) {
  #line 53 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  if (_M0L8capacityS301 == 0) {
    moonbit_string_t* _M0L6_2atmpS832 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_1169 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_1169)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_1169->$0 = _M0L6_2atmpS832;
    _block_1169->$1 = 0;
    return _block_1169;
  } else {
    moonbit_string_t* _M0L6_2atmpS833 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS301, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_1170 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_1170)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_1170->$0 = _M0L6_2atmpS833;
    _block_1170->$1 = 0;
    return _block_1170;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS299,
  struct _M0TPC16string10StringView _M0L3strS300
) {
  int32_t _M0L3lenS820;
  int32_t _M0L6_2atmpS822;
  int32_t _M0L6_2atmpS821;
  int32_t _M0L6_2atmpS819;
  moonbit_bytes_t _M0L8_2afieldS1068;
  moonbit_bytes_t _M0L4dataS823;
  int32_t _M0L3lenS824;
  moonbit_string_t _M0L6_2atmpS825;
  int32_t _M0L6_2atmpS826;
  int32_t _M0L6_2atmpS827;
  int32_t _M0L3lenS829;
  int32_t _M0L6_2atmpS831;
  int32_t _M0L6_2atmpS830;
  int32_t _M0L6_2atmpS828;
  #line 99 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS820 = _M0L4selfS299->$1;
  moonbit_incref(_M0L3strS300.$0);
  #line 103 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS822 = _M0MPC16string10StringView6length(_M0L3strS300);
  _M0L6_2atmpS821 = _M0L6_2atmpS822 * 2;
  _M0L6_2atmpS819 = _M0L3lenS820 + _M0L6_2atmpS821;
  moonbit_incref(_M0L4selfS299);
  #line 103 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS299, _M0L6_2atmpS819);
  _M0L8_2afieldS1068 = _M0L4selfS299->$0;
  _M0L4dataS823 = _M0L8_2afieldS1068;
  _M0L3lenS824 = _M0L4selfS299->$1;
  moonbit_incref(_M0L4dataS823);
  moonbit_incref(_M0L3strS300.$0);
  #line 106 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS825 = _M0MPC16string10StringView4data(_M0L3strS300);
  moonbit_incref(_M0L3strS300.$0);
  #line 107 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS826 = _M0MPC16string10StringView13start__offset(_M0L3strS300);
  moonbit_incref(_M0L3strS300.$0);
  #line 108 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS827 = _M0MPC16string10StringView6length(_M0L3strS300);
  #line 104 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS823, _M0L3lenS824, _M0L6_2atmpS825, _M0L6_2atmpS826, _M0L6_2atmpS827);
  _M0L3lenS829 = _M0L4selfS299->$1;
  #line 110 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS831 = _M0MPC16string10StringView6length(_M0L3strS300);
  _M0L6_2atmpS830 = _M0L6_2atmpS831 * 2;
  _M0L6_2atmpS828 = _M0L3lenS829 + _M0L6_2atmpS830;
  _M0L4selfS299->$1 = _M0L6_2atmpS828;
  moonbit_decref(_M0L4selfS299);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS297,
  int64_t _M0L19start__offset_2eoptS295,
  int64_t _M0L11end__offsetS298
) {
  int32_t _M0L13start__offsetS294;
  if (_M0L19start__offset_2eoptS295 == 4294967296ll) {
    _M0L13start__offsetS294 = 0;
  } else {
    int64_t _M0L7_2aSomeS296 = _M0L19start__offset_2eoptS295;
    _M0L13start__offsetS294 = (int32_t)_M0L7_2aSomeS296;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS297, _M0L13start__offsetS294, _M0L11end__offsetS298);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS292,
  int32_t _M0L13start__offsetS293,
  int64_t _M0L11end__offsetS290
) {
  int32_t _M0L11end__offsetS289;
  int32_t _if__result_1171;
  #line 390 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS290 == 4294967296ll) {
    _M0L11end__offsetS289 = Moonbit_array_length(_M0L4selfS292);
  } else {
    int64_t _M0L7_2aSomeS291 = _M0L11end__offsetS290;
    _M0L11end__offsetS289 = (int32_t)_M0L7_2aSomeS291;
  }
  if (_M0L13start__offsetS293 >= 0) {
    if (_M0L13start__offsetS293 <= _M0L11end__offsetS289) {
      int32_t _M0L6_2atmpS818 = Moonbit_array_length(_M0L4selfS292);
      _if__result_1171 = _M0L11end__offsetS289 <= _M0L6_2atmpS818;
    } else {
      _if__result_1171 = 0;
    }
  } else {
    _if__result_1171 = 0;
  }
  if (_if__result_1171) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS293,
                                                 _M0L11end__offsetS289,
                                                 _M0L4selfS292};
  } else {
    moonbit_decref(_M0L4selfS292);
    #line 399 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_9.data, (moonbit_string_t)moonbit_string_literal_10.data);
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS281,
  struct _M0TPB6Logger _M0L6loggerS279
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS280;
  int32_t _M0L3lenS282;
  int32_t _M0L1iS283;
  int32_t _M0L3segS284;
  #line 88 "/home/blem/.moon/lib/core/builtin/show.mbt"
  if (_M0L6loggerS279.$1) {
    moonbit_incref(_M0L6loggerS279.$1);
  }
  #line 89 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS279.$0->$method_3(_M0L6loggerS279.$1, 34);
  moonbit_incref(_M0L4selfS281);
  if (_M0L6loggerS279.$1) {
    moonbit_incref(_M0L6loggerS279.$1);
  }
  _M0L6_2aenvS280
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS280)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS280->$0 = _M0L4selfS281;
  _M0L6_2aenvS280->$1_0 = _M0L6loggerS279.$0;
  _M0L6_2aenvS280->$1_1 = _M0L6loggerS279.$1;
  _M0L3lenS282 = Moonbit_array_length(_M0L4selfS281);
  _M0L1iS283 = 0;
  _M0L3segS284 = 0;
  _2afor_285:;
  while (1) {
    int32_t _M0L4codeS286;
    int32_t _M0L1cS288;
    int32_t _M0L6_2atmpS802;
    int32_t _M0L6_2atmpS803;
    int32_t _M0L6_2atmpS804;
    int32_t _tmp_1175;
    int32_t _tmp_1176;
    if (_M0L1iS283 >= _M0L3lenS282) {
      moonbit_decref(_M0L4selfS281);
      #line 105 "/home/blem/.moon/lib/core/builtin/show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS280, _M0L3segS284, _M0L1iS283);
      break;
    }
    _M0L4codeS286 = _M0L4selfS281[_M0L1iS283];
    switch (_M0L4codeS286) {
      case 34: {
        _M0L1cS288 = _M0L4codeS286;
        goto join_287;
        break;
      }
      
      case 92: {
        _M0L1cS288 = _M0L4codeS286;
        goto join_287;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS805;
        int32_t _M0L6_2atmpS806;
        moonbit_incref(_M0L6_2aenvS280);
        #line 118 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS280, _M0L3segS284, _M0L1iS283);
        if (_M0L6loggerS279.$1) {
          moonbit_incref(_M0L6loggerS279.$1);
        }
        #line 119 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS279.$0->$method_0(_M0L6loggerS279.$1, (moonbit_string_t)moonbit_string_literal_11.data);
        _M0L6_2atmpS805 = _M0L1iS283 + 1;
        _M0L6_2atmpS806 = _M0L1iS283 + 1;
        _M0L1iS283 = _M0L6_2atmpS805;
        _M0L3segS284 = _M0L6_2atmpS806;
        goto _2afor_285;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS807;
        int32_t _M0L6_2atmpS808;
        moonbit_incref(_M0L6_2aenvS280);
        #line 123 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS280, _M0L3segS284, _M0L1iS283);
        if (_M0L6loggerS279.$1) {
          moonbit_incref(_M0L6loggerS279.$1);
        }
        #line 124 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS279.$0->$method_0(_M0L6loggerS279.$1, (moonbit_string_t)moonbit_string_literal_12.data);
        _M0L6_2atmpS807 = _M0L1iS283 + 1;
        _M0L6_2atmpS808 = _M0L1iS283 + 1;
        _M0L1iS283 = _M0L6_2atmpS807;
        _M0L3segS284 = _M0L6_2atmpS808;
        goto _2afor_285;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS809;
        int32_t _M0L6_2atmpS810;
        moonbit_incref(_M0L6_2aenvS280);
        #line 128 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS280, _M0L3segS284, _M0L1iS283);
        if (_M0L6loggerS279.$1) {
          moonbit_incref(_M0L6loggerS279.$1);
        }
        #line 129 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS279.$0->$method_0(_M0L6loggerS279.$1, (moonbit_string_t)moonbit_string_literal_13.data);
        _M0L6_2atmpS809 = _M0L1iS283 + 1;
        _M0L6_2atmpS810 = _M0L1iS283 + 1;
        _M0L1iS283 = _M0L6_2atmpS809;
        _M0L3segS284 = _M0L6_2atmpS810;
        goto _2afor_285;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS811;
        int32_t _M0L6_2atmpS812;
        moonbit_incref(_M0L6_2aenvS280);
        #line 133 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS280, _M0L3segS284, _M0L1iS283);
        if (_M0L6loggerS279.$1) {
          moonbit_incref(_M0L6loggerS279.$1);
        }
        #line 134 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS279.$0->$method_0(_M0L6loggerS279.$1, (moonbit_string_t)moonbit_string_literal_14.data);
        _M0L6_2atmpS811 = _M0L1iS283 + 1;
        _M0L6_2atmpS812 = _M0L1iS283 + 1;
        _M0L1iS283 = _M0L6_2atmpS811;
        _M0L3segS284 = _M0L6_2atmpS812;
        goto _2afor_285;
        break;
      }
      default: {
        if (_M0L4codeS286 < 32) {
          int32_t _M0L6_2atmpS814;
          moonbit_string_t _M0L6_2atmpS813;
          int32_t _M0L6_2atmpS815;
          int32_t _M0L6_2atmpS816;
          moonbit_incref(_M0L6_2aenvS280);
          #line 140 "/home/blem/.moon/lib/core/builtin/show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS280, _M0L3segS284, _M0L1iS283);
          if (_M0L6loggerS279.$1) {
            moonbit_incref(_M0L6loggerS279.$1);
          }
          #line 141 "/home/blem/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS279.$0->$method_0(_M0L6loggerS279.$1, (moonbit_string_t)moonbit_string_literal_15.data);
          _M0L6_2atmpS814 = _M0L4codeS286 & 0xff;
          #line 142 "/home/blem/.moon/lib/core/builtin/show.mbt"
          _M0L6_2atmpS813 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS814);
          if (_M0L6loggerS279.$1) {
            moonbit_incref(_M0L6loggerS279.$1);
          }
          #line 142 "/home/blem/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS279.$0->$method_0(_M0L6loggerS279.$1, _M0L6_2atmpS813);
          if (_M0L6loggerS279.$1) {
            moonbit_incref(_M0L6loggerS279.$1);
          }
          #line 143 "/home/blem/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS279.$0->$method_3(_M0L6loggerS279.$1, 125);
          _M0L6_2atmpS815 = _M0L1iS283 + 1;
          _M0L6_2atmpS816 = _M0L1iS283 + 1;
          _M0L1iS283 = _M0L6_2atmpS815;
          _M0L3segS284 = _M0L6_2atmpS816;
          goto _2afor_285;
        } else {
          int32_t _M0L6_2atmpS817 = _M0L1iS283 + 1;
          int32_t _tmp_1174 = _M0L3segS284;
          _M0L1iS283 = _M0L6_2atmpS817;
          _M0L3segS284 = _tmp_1174;
          goto _2afor_285;
        }
        break;
      }
    }
    goto joinlet_1173;
    join_287:;
    moonbit_incref(_M0L6_2aenvS280);
    #line 111 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS280, _M0L3segS284, _M0L1iS283);
    if (_M0L6loggerS279.$1) {
      moonbit_incref(_M0L6loggerS279.$1);
    }
    #line 112 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS279.$0->$method_3(_M0L6loggerS279.$1, 92);
    #line 113 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS802 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS288);
    if (_M0L6loggerS279.$1) {
      moonbit_incref(_M0L6loggerS279.$1);
    }
    #line 113 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS279.$0->$method_3(_M0L6loggerS279.$1, _M0L6_2atmpS802);
    _M0L6_2atmpS803 = _M0L1iS283 + 1;
    _M0L6_2atmpS804 = _M0L1iS283 + 1;
    _M0L1iS283 = _M0L6_2atmpS803;
    _M0L3segS284 = _M0L6_2atmpS804;
    continue;
    joinlet_1173:;
    _tmp_1175 = _M0L1iS283;
    _tmp_1176 = _M0L3segS284;
    _M0L1iS283 = _tmp_1175;
    _M0L3segS284 = _tmp_1176;
    continue;
    break;
  }
  #line 151 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS279.$0->$method_3(_M0L6loggerS279.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS275,
  int32_t _M0L3segS278,
  int32_t _M0L1iS277
) {
  struct _M0TPB6Logger _M0L8_2afieldS1070;
  struct _M0TPB6Logger _M0L6loggerS274;
  moonbit_string_t _M0L8_2afieldS1069;
  int32_t _M0L6_2acntS1131;
  moonbit_string_t _M0L4selfS276;
  #line 90 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L8_2afieldS1070
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS275->$1_0, _M0L6_2aenvS275->$1_1
  };
  _M0L6loggerS274 = _M0L8_2afieldS1070;
  _M0L8_2afieldS1069 = _M0L6_2aenvS275->$0;
  _M0L6_2acntS1131 = Moonbit_object_header(_M0L6_2aenvS275)->rc;
  if (_M0L6_2acntS1131 > 1) {
    int32_t _M0L11_2anew__cntS1132 = _M0L6_2acntS1131 - 1;
    Moonbit_object_header(_M0L6_2aenvS275)->rc = _M0L11_2anew__cntS1132;
    if (_M0L6loggerS274.$1) {
      moonbit_incref(_M0L6loggerS274.$1);
    }
    moonbit_incref(_M0L8_2afieldS1069);
  } else if (_M0L6_2acntS1131 == 1) {
    #line 90 "/home/blem/.moon/lib/core/builtin/show.mbt"
    moonbit_free(_M0L6_2aenvS275);
  }
  _M0L4selfS276 = _M0L8_2afieldS1069;
  if (_M0L1iS277 > _M0L3segS278) {
    int32_t _M0L6_2atmpS801 = _M0L1iS277 - _M0L3segS278;
    #line 92 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS274.$0->$method_1(_M0L6loggerS274.$1, _M0L4selfS276, _M0L3segS278, _M0L6_2atmpS801);
  } else {
    moonbit_decref(_M0L4selfS276);
    if (_M0L6loggerS274.$1) {
      moonbit_decref(_M0L6loggerS274.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS273) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS272;
  int32_t _M0L6_2atmpS798;
  int32_t _M0L6_2atmpS797;
  int32_t _M0L6_2atmpS800;
  int32_t _M0L6_2atmpS799;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS796;
  #line 69 "/home/blem/.moon/lib/core/builtin/show.mbt"
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L7_2aselfS272 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS798 = _M0IPC14byte4BytePB3Div3div(_M0L1bS273, 16);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS797
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS798);
  moonbit_incref(_M0L7_2aselfS272);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS272, _M0L6_2atmpS797);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS800 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS273, 16);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS799
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS800);
  moonbit_incref(_M0L7_2aselfS272);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS272, _M0L6_2atmpS799);
  _M0L6_2atmpS796 = _M0L7_2aselfS272;
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS796);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS271) {
  #line 70 "/home/blem/.moon/lib/core/builtin/show.mbt"
  if (_M0L1iS271 < 10) {
    int32_t _M0L6_2atmpS793;
    #line 72 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS793 = _M0IPC14byte4BytePB3Add3add(_M0L1iS271, 48);
    #line 72 "/home/blem/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS793);
  } else {
    int32_t _M0L6_2atmpS795;
    int32_t _M0L6_2atmpS794;
    #line 74 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS795 = _M0IPC14byte4BytePB3Add3add(_M0L1iS271, 97);
    #line 74 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS794 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS795, 10);
    #line 74 "/home/blem/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS794);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS269,
  int32_t _M0L4thatS270
) {
  int32_t _M0L6_2atmpS791;
  int32_t _M0L6_2atmpS792;
  int32_t _M0L6_2atmpS790;
  #line 120 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS791 = (int32_t)_M0L4selfS269;
  _M0L6_2atmpS792 = (int32_t)_M0L4thatS270;
  _M0L6_2atmpS790 = _M0L6_2atmpS791 - _M0L6_2atmpS792;
  return _M0L6_2atmpS790 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS267,
  int32_t _M0L4thatS268
) {
  int32_t _M0L6_2atmpS788;
  int32_t _M0L6_2atmpS789;
  int32_t _M0L6_2atmpS787;
  #line 67 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS788 = (int32_t)_M0L4selfS267;
  _M0L6_2atmpS789 = (int32_t)_M0L4thatS268;
  _M0L6_2atmpS787 = _M0L6_2atmpS788 % _M0L6_2atmpS789;
  return _M0L6_2atmpS787 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS265,
  int32_t _M0L4thatS266
) {
  int32_t _M0L6_2atmpS785;
  int32_t _M0L6_2atmpS786;
  int32_t _M0L6_2atmpS784;
  #line 62 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS785 = (int32_t)_M0L4selfS265;
  _M0L6_2atmpS786 = (int32_t)_M0L4thatS266;
  _M0L6_2atmpS784 = _M0L6_2atmpS785 / _M0L6_2atmpS786;
  return _M0L6_2atmpS784 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS263,
  int32_t _M0L4thatS264
) {
  int32_t _M0L6_2atmpS782;
  int32_t _M0L6_2atmpS783;
  int32_t _M0L6_2atmpS781;
  #line 106 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS782 = (int32_t)_M0L4selfS263;
  _M0L6_2atmpS783 = (int32_t)_M0L4thatS264;
  _M0L6_2atmpS781 = _M0L6_2atmpS782 + _M0L6_2atmpS783;
  return _M0L6_2atmpS781 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS260,
  int32_t _M0L5startS258,
  int32_t _M0L3endS259
) {
  int32_t _if__result_1177;
  int32_t _M0L3lenS261;
  int32_t _M0L6_2atmpS779;
  int32_t _M0L6_2atmpS780;
  moonbit_bytes_t _M0L5bytesS262;
  moonbit_bytes_t _M0L6_2atmpS778;
  #line 91 "/home/blem/.moon/lib/core/builtin/string.mbt"
  if (_M0L5startS258 == 0) {
    int32_t _M0L6_2atmpS777 = Moonbit_array_length(_M0L3strS260);
    _if__result_1177 = _M0L3endS259 == _M0L6_2atmpS777;
  } else {
    _if__result_1177 = 0;
  }
  if (_if__result_1177) {
    return _M0L3strS260;
  }
  _M0L3lenS261 = _M0L3endS259 - _M0L5startS258;
  _M0L6_2atmpS779 = _M0L3lenS261 * 2;
  #line 101 "/home/blem/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS780 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS262
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS779, _M0L6_2atmpS780);
  moonbit_incref(_M0L5bytesS262);
  #line 102 "/home/blem/.moon/lib/core/builtin/string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS262, 0, _M0L3strS260, _M0L5startS258, _M0L3lenS261);
  _M0L6_2atmpS778 = _M0L5bytesS262;
  #line 103 "/home/blem/.moon/lib/core/builtin/string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS778, 0, 4294967296ll);
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS242,
  int32_t _M0L5radixS241
) {
  int32_t _if__result_1178;
  int32_t _M0L12is__negativeS243;
  uint32_t _M0L3numS244;
  uint16_t* _M0L6bufferS245;
  #line 220 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS241 < 2) {
    _if__result_1178 = 1;
  } else {
    _if__result_1178 = _M0L5radixS241 > 36;
  }
  if (_if__result_1178) {
    #line 224 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_16.data, (moonbit_string_t)moonbit_string_literal_17.data);
  }
  if (_M0L4selfS242 == 0) {
    return (moonbit_string_t)moonbit_string_literal_18.data;
  }
  _M0L12is__negativeS243 = _M0L4selfS242 < 0;
  if (_M0L12is__negativeS243) {
    int32_t _M0L6_2atmpS776 = -_M0L4selfS242;
    _M0L3numS244 = *(uint32_t*)&_M0L6_2atmpS776;
  } else {
    _M0L3numS244 = *(uint32_t*)&_M0L4selfS242;
  }
  switch (_M0L5radixS241) {
    case 10: {
      int32_t _M0L10digit__lenS246;
      int32_t _M0L6_2atmpS773;
      int32_t _M0L10total__lenS247;
      uint16_t* _M0L6bufferS248;
      int32_t _M0L12digit__startS249;
      #line 246 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS246 = _M0FPB12dec__count32(_M0L3numS244);
      if (_M0L12is__negativeS243) {
        _M0L6_2atmpS773 = 1;
      } else {
        _M0L6_2atmpS773 = 0;
      }
      _M0L10total__lenS247 = _M0L10digit__lenS246 + _M0L6_2atmpS773;
      _M0L6bufferS248
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS247, 0);
      if (_M0L12is__negativeS243) {
        _M0L12digit__startS249 = 1;
      } else {
        _M0L12digit__startS249 = 0;
      }
      moonbit_incref(_M0L6bufferS248);
      #line 250 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS248, _M0L3numS244, _M0L12digit__startS249, _M0L10total__lenS247);
      _M0L6bufferS245 = _M0L6bufferS248;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS250;
      int32_t _M0L6_2atmpS774;
      int32_t _M0L10total__lenS251;
      uint16_t* _M0L6bufferS252;
      int32_t _M0L12digit__startS253;
      #line 254 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS250 = _M0FPB12hex__count32(_M0L3numS244);
      if (_M0L12is__negativeS243) {
        _M0L6_2atmpS774 = 1;
      } else {
        _M0L6_2atmpS774 = 0;
      }
      _M0L10total__lenS251 = _M0L10digit__lenS250 + _M0L6_2atmpS774;
      _M0L6bufferS252
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS251, 0);
      if (_M0L12is__negativeS243) {
        _M0L12digit__startS253 = 1;
      } else {
        _M0L12digit__startS253 = 0;
      }
      moonbit_incref(_M0L6bufferS252);
      #line 258 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS252, _M0L3numS244, _M0L12digit__startS253, _M0L10total__lenS251);
      _M0L6bufferS245 = _M0L6bufferS252;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS254;
      int32_t _M0L6_2atmpS775;
      int32_t _M0L10total__lenS255;
      uint16_t* _M0L6bufferS256;
      int32_t _M0L12digit__startS257;
      #line 262 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS254
      = _M0FPB14radix__count32(_M0L3numS244, _M0L5radixS241);
      if (_M0L12is__negativeS243) {
        _M0L6_2atmpS775 = 1;
      } else {
        _M0L6_2atmpS775 = 0;
      }
      _M0L10total__lenS255 = _M0L10digit__lenS254 + _M0L6_2atmpS775;
      _M0L6bufferS256
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS255, 0);
      if (_M0L12is__negativeS243) {
        _M0L12digit__startS257 = 1;
      } else {
        _M0L12digit__startS257 = 0;
      }
      moonbit_incref(_M0L6bufferS256);
      #line 266 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS256, _M0L3numS244, _M0L12digit__startS257, _M0L10total__lenS255, _M0L5radixS241);
      _M0L6bufferS245 = _M0L6bufferS256;
      break;
    }
  }
  if (_M0L12is__negativeS243) {
    _M0L6bufferS245[0] = 45;
  }
  return _M0L6bufferS245;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS235,
  int32_t _M0L5radixS238
) {
  uint32_t _M0Lm3numS236;
  uint32_t _M0L4baseS237;
  int32_t _M0Lm5countS239;
  #line 198 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS235 == 0u) {
    return 1;
  }
  _M0Lm3numS236 = _M0L5valueS235;
  _M0L4baseS237 = *(uint32_t*)&_M0L5radixS238;
  _M0Lm5countS239 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS770 = _M0Lm3numS236;
    if (_M0L6_2atmpS770 > 0u) {
      int32_t _M0L6_2atmpS771 = _M0Lm5countS239;
      uint32_t _M0L6_2atmpS772;
      _M0Lm5countS239 = _M0L6_2atmpS771 + 1;
      _M0L6_2atmpS772 = _M0Lm3numS236;
      _M0Lm3numS236 = _M0L6_2atmpS772 / _M0L4baseS237;
      continue;
    }
    break;
  }
  return _M0Lm5countS239;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS233) {
  #line 186 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS233 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS234;
    int32_t _M0L6_2atmpS769;
    int32_t _M0L6_2atmpS768;
    #line 191 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS234 = moonbit_clz32(_M0L5valueS233);
    _M0L6_2atmpS769 = 31 - _M0L14leading__zerosS234;
    _M0L6_2atmpS768 = _M0L6_2atmpS769 / 4;
    return _M0L6_2atmpS768 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS232) {
  #line 152 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS232 >= 100000u) {
    if (_M0L5valueS232 >= 10000000u) {
      if (_M0L5valueS232 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS232 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS232 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS232 >= 1000u) {
    if (_M0L5valueS232 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS232 >= 100u) {
    return 3;
  } else if (_M0L5valueS232 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS222,
  uint32_t _M0L3numS210,
  int32_t _M0L12digit__startS213,
  int32_t _M0L10total__lenS212
) {
  uint32_t _M0Lm3numS209;
  int32_t _M0Lm6offsetS211;
  uint32_t _M0L6_2atmpS767;
  int32_t _M0Lm9remainingS224;
  int32_t _M0L6_2atmpS748;
  #line 94 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  _M0Lm3numS209 = _M0L3numS210;
  _M0Lm6offsetS211 = _M0L10total__lenS212 - _M0L12digit__startS213;
  while (1) {
    uint32_t _M0L6_2atmpS711 = _M0Lm3numS209;
    if (_M0L6_2atmpS711 >= 10000u) {
      uint32_t _M0L6_2atmpS734 = _M0Lm3numS209;
      uint32_t _M0L1tS214 = _M0L6_2atmpS734 / 10000u;
      uint32_t _M0L6_2atmpS733 = _M0Lm3numS209;
      uint32_t _M0L6_2atmpS732 = _M0L6_2atmpS733 % 10000u;
      int32_t _M0L1rS215 = *(int32_t*)&_M0L6_2atmpS732;
      int32_t _M0L2d1S216;
      int32_t _M0L2d2S217;
      int32_t _M0L6_2atmpS712;
      int32_t _M0L6_2atmpS731;
      int32_t _M0L6_2atmpS730;
      int32_t _M0L6d1__hiS218;
      int32_t _M0L6_2atmpS729;
      int32_t _M0L6_2atmpS728;
      int32_t _M0L6d1__loS219;
      int32_t _M0L6_2atmpS727;
      int32_t _M0L6_2atmpS726;
      int32_t _M0L6d2__hiS220;
      int32_t _M0L6_2atmpS725;
      int32_t _M0L6_2atmpS724;
      int32_t _M0L6d2__loS221;
      int32_t _M0L6_2atmpS714;
      int32_t _M0L6_2atmpS713;
      int32_t _M0L6_2atmpS717;
      int32_t _M0L6_2atmpS716;
      int32_t _M0L6_2atmpS715;
      int32_t _M0L6_2atmpS720;
      int32_t _M0L6_2atmpS719;
      int32_t _M0L6_2atmpS718;
      int32_t _M0L6_2atmpS723;
      int32_t _M0L6_2atmpS722;
      int32_t _M0L6_2atmpS721;
      _M0Lm3numS209 = _M0L1tS214;
      _M0L2d1S216 = _M0L1rS215 / 100;
      _M0L2d2S217 = _M0L1rS215 % 100;
      _M0L6_2atmpS712 = _M0Lm6offsetS211;
      _M0Lm6offsetS211 = _M0L6_2atmpS712 - 4;
      _M0L6_2atmpS731 = _M0L2d1S216 / 10;
      _M0L6_2atmpS730 = 48 + _M0L6_2atmpS731;
      _M0L6d1__hiS218 = (uint16_t)_M0L6_2atmpS730;
      _M0L6_2atmpS729 = _M0L2d1S216 % 10;
      _M0L6_2atmpS728 = 48 + _M0L6_2atmpS729;
      _M0L6d1__loS219 = (uint16_t)_M0L6_2atmpS728;
      _M0L6_2atmpS727 = _M0L2d2S217 / 10;
      _M0L6_2atmpS726 = 48 + _M0L6_2atmpS727;
      _M0L6d2__hiS220 = (uint16_t)_M0L6_2atmpS726;
      _M0L6_2atmpS725 = _M0L2d2S217 % 10;
      _M0L6_2atmpS724 = 48 + _M0L6_2atmpS725;
      _M0L6d2__loS221 = (uint16_t)_M0L6_2atmpS724;
      _M0L6_2atmpS714 = _M0Lm6offsetS211;
      _M0L6_2atmpS713 = _M0L12digit__startS213 + _M0L6_2atmpS714;
      _M0L6bufferS222[_M0L6_2atmpS713] = _M0L6d1__hiS218;
      _M0L6_2atmpS717 = _M0Lm6offsetS211;
      _M0L6_2atmpS716 = _M0L12digit__startS213 + _M0L6_2atmpS717;
      _M0L6_2atmpS715 = _M0L6_2atmpS716 + 1;
      _M0L6bufferS222[_M0L6_2atmpS715] = _M0L6d1__loS219;
      _M0L6_2atmpS720 = _M0Lm6offsetS211;
      _M0L6_2atmpS719 = _M0L12digit__startS213 + _M0L6_2atmpS720;
      _M0L6_2atmpS718 = _M0L6_2atmpS719 + 2;
      _M0L6bufferS222[_M0L6_2atmpS718] = _M0L6d2__hiS220;
      _M0L6_2atmpS723 = _M0Lm6offsetS211;
      _M0L6_2atmpS722 = _M0L12digit__startS213 + _M0L6_2atmpS723;
      _M0L6_2atmpS721 = _M0L6_2atmpS722 + 3;
      _M0L6bufferS222[_M0L6_2atmpS721] = _M0L6d2__loS221;
      continue;
    }
    break;
  }
  _M0L6_2atmpS767 = _M0Lm3numS209;
  _M0Lm9remainingS224 = *(int32_t*)&_M0L6_2atmpS767;
  while (1) {
    int32_t _M0L6_2atmpS735 = _M0Lm9remainingS224;
    if (_M0L6_2atmpS735 >= 100) {
      int32_t _M0L6_2atmpS747 = _M0Lm9remainingS224;
      int32_t _M0L1tS225 = _M0L6_2atmpS747 / 100;
      int32_t _M0L6_2atmpS746 = _M0Lm9remainingS224;
      int32_t _M0L1dS226 = _M0L6_2atmpS746 % 100;
      int32_t _M0L6_2atmpS736;
      int32_t _M0L6_2atmpS745;
      int32_t _M0L6_2atmpS744;
      int32_t _M0L5d__hiS227;
      int32_t _M0L6_2atmpS743;
      int32_t _M0L6_2atmpS742;
      int32_t _M0L5d__loS228;
      int32_t _M0L6_2atmpS738;
      int32_t _M0L6_2atmpS737;
      int32_t _M0L6_2atmpS741;
      int32_t _M0L6_2atmpS740;
      int32_t _M0L6_2atmpS739;
      _M0Lm9remainingS224 = _M0L1tS225;
      _M0L6_2atmpS736 = _M0Lm6offsetS211;
      _M0Lm6offsetS211 = _M0L6_2atmpS736 - 2;
      _M0L6_2atmpS745 = _M0L1dS226 / 10;
      _M0L6_2atmpS744 = 48 + _M0L6_2atmpS745;
      _M0L5d__hiS227 = (uint16_t)_M0L6_2atmpS744;
      _M0L6_2atmpS743 = _M0L1dS226 % 10;
      _M0L6_2atmpS742 = 48 + _M0L6_2atmpS743;
      _M0L5d__loS228 = (uint16_t)_M0L6_2atmpS742;
      _M0L6_2atmpS738 = _M0Lm6offsetS211;
      _M0L6_2atmpS737 = _M0L12digit__startS213 + _M0L6_2atmpS738;
      _M0L6bufferS222[_M0L6_2atmpS737] = _M0L5d__hiS227;
      _M0L6_2atmpS741 = _M0Lm6offsetS211;
      _M0L6_2atmpS740 = _M0L12digit__startS213 + _M0L6_2atmpS741;
      _M0L6_2atmpS739 = _M0L6_2atmpS740 + 1;
      _M0L6bufferS222[_M0L6_2atmpS739] = _M0L5d__loS228;
      continue;
    }
    break;
  }
  _M0L6_2atmpS748 = _M0Lm9remainingS224;
  if (_M0L6_2atmpS748 >= 10) {
    int32_t _M0L6_2atmpS749 = _M0Lm6offsetS211;
    int32_t _M0L6_2atmpS760;
    int32_t _M0L6_2atmpS759;
    int32_t _M0L6_2atmpS758;
    int32_t _M0L5d__hiS230;
    int32_t _M0L6_2atmpS757;
    int32_t _M0L6_2atmpS756;
    int32_t _M0L6_2atmpS755;
    int32_t _M0L5d__loS231;
    int32_t _M0L6_2atmpS751;
    int32_t _M0L6_2atmpS750;
    int32_t _M0L6_2atmpS754;
    int32_t _M0L6_2atmpS753;
    int32_t _M0L6_2atmpS752;
    _M0Lm6offsetS211 = _M0L6_2atmpS749 - 2;
    _M0L6_2atmpS760 = _M0Lm9remainingS224;
    _M0L6_2atmpS759 = _M0L6_2atmpS760 / 10;
    _M0L6_2atmpS758 = 48 + _M0L6_2atmpS759;
    _M0L5d__hiS230 = (uint16_t)_M0L6_2atmpS758;
    _M0L6_2atmpS757 = _M0Lm9remainingS224;
    _M0L6_2atmpS756 = _M0L6_2atmpS757 % 10;
    _M0L6_2atmpS755 = 48 + _M0L6_2atmpS756;
    _M0L5d__loS231 = (uint16_t)_M0L6_2atmpS755;
    _M0L6_2atmpS751 = _M0Lm6offsetS211;
    _M0L6_2atmpS750 = _M0L12digit__startS213 + _M0L6_2atmpS751;
    _M0L6bufferS222[_M0L6_2atmpS750] = _M0L5d__hiS230;
    _M0L6_2atmpS754 = _M0Lm6offsetS211;
    _M0L6_2atmpS753 = _M0L12digit__startS213 + _M0L6_2atmpS754;
    _M0L6_2atmpS752 = _M0L6_2atmpS753 + 1;
    _M0L6bufferS222[_M0L6_2atmpS752] = _M0L5d__loS231;
    moonbit_decref(_M0L6bufferS222);
  } else {
    int32_t _M0L6_2atmpS761 = _M0Lm6offsetS211;
    int32_t _M0L6_2atmpS766;
    int32_t _M0L6_2atmpS762;
    int32_t _M0L6_2atmpS765;
    int32_t _M0L6_2atmpS764;
    int32_t _M0L6_2atmpS763;
    _M0Lm6offsetS211 = _M0L6_2atmpS761 - 1;
    _M0L6_2atmpS766 = _M0Lm6offsetS211;
    _M0L6_2atmpS762 = _M0L12digit__startS213 + _M0L6_2atmpS766;
    _M0L6_2atmpS765 = _M0Lm9remainingS224;
    _M0L6_2atmpS764 = 48 + _M0L6_2atmpS765;
    _M0L6_2atmpS763 = (uint16_t)_M0L6_2atmpS764;
    _M0L6bufferS222[_M0L6_2atmpS762] = _M0L6_2atmpS763;
    moonbit_decref(_M0L6bufferS222);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS204,
  uint32_t _M0L3numS198,
  int32_t _M0L12digit__startS196,
  int32_t _M0L10total__lenS195,
  int32_t _M0L5radixS200
) {
  int32_t _M0Lm6offsetS194;
  uint32_t _M0Lm1nS197;
  uint32_t _M0L4baseS199;
  int32_t _M0L6_2atmpS693;
  int32_t _M0L6_2atmpS692;
  #line 59 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  _M0Lm6offsetS194 = _M0L10total__lenS195 - _M0L12digit__startS196;
  _M0Lm1nS197 = _M0L3numS198;
  _M0L4baseS199 = *(uint32_t*)&_M0L5radixS200;
  _M0L6_2atmpS693 = _M0L5radixS200 - 1;
  _M0L6_2atmpS692 = _M0L5radixS200 & _M0L6_2atmpS693;
  if (_M0L6_2atmpS692 == 0) {
    int32_t _M0L5shiftS201;
    uint32_t _M0L4maskS202;
    #line 72 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS201 = moonbit_ctz32(_M0L5radixS200);
    _M0L4maskS202 = _M0L4baseS199 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS694 = _M0Lm1nS197;
      if (_M0L6_2atmpS694 > 0u) {
        int32_t _M0L6_2atmpS695 = _M0Lm6offsetS194;
        uint32_t _M0L6_2atmpS701;
        uint32_t _M0L6_2atmpS700;
        int32_t _M0L5digitS203;
        int32_t _M0L6_2atmpS698;
        int32_t _M0L6_2atmpS696;
        int32_t _M0L6_2atmpS697;
        uint32_t _M0L6_2atmpS699;
        _M0Lm6offsetS194 = _M0L6_2atmpS695 - 1;
        _M0L6_2atmpS701 = _M0Lm1nS197;
        _M0L6_2atmpS700 = _M0L6_2atmpS701 & _M0L4maskS202;
        _M0L5digitS203 = *(int32_t*)&_M0L6_2atmpS700;
        _M0L6_2atmpS698 = _M0Lm6offsetS194;
        _M0L6_2atmpS696 = _M0L12digit__startS196 + _M0L6_2atmpS698;
        _M0L6_2atmpS697
        = ((moonbit_string_t)moonbit_string_literal_19.data)[
          _M0L5digitS203
        ];
        _M0L6bufferS204[_M0L6_2atmpS696] = _M0L6_2atmpS697;
        _M0L6_2atmpS699 = _M0Lm1nS197;
        _M0Lm1nS197 = _M0L6_2atmpS699 >> (_M0L5shiftS201 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS204);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS702 = _M0Lm1nS197;
      if (_M0L6_2atmpS702 > 0u) {
        int32_t _M0L6_2atmpS703 = _M0Lm6offsetS194;
        uint32_t _M0L6_2atmpS710;
        uint32_t _M0L1qS206;
        uint32_t _M0L6_2atmpS708;
        uint32_t _M0L6_2atmpS709;
        uint32_t _M0L6_2atmpS707;
        int32_t _M0L5digitS207;
        int32_t _M0L6_2atmpS706;
        int32_t _M0L6_2atmpS704;
        int32_t _M0L6_2atmpS705;
        _M0Lm6offsetS194 = _M0L6_2atmpS703 - 1;
        _M0L6_2atmpS710 = _M0Lm1nS197;
        _M0L1qS206 = _M0L6_2atmpS710 / _M0L4baseS199;
        _M0L6_2atmpS708 = _M0Lm1nS197;
        _M0L6_2atmpS709 = _M0L1qS206 * _M0L4baseS199;
        _M0L6_2atmpS707 = _M0L6_2atmpS708 - _M0L6_2atmpS709;
        _M0L5digitS207 = *(int32_t*)&_M0L6_2atmpS707;
        _M0L6_2atmpS706 = _M0Lm6offsetS194;
        _M0L6_2atmpS704 = _M0L12digit__startS196 + _M0L6_2atmpS706;
        _M0L6_2atmpS705
        = ((moonbit_string_t)moonbit_string_literal_19.data)[
          _M0L5digitS207
        ];
        _M0L6bufferS204[_M0L6_2atmpS704] = _M0L6_2atmpS705;
        _M0Lm1nS197 = _M0L1qS206;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS204);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS191,
  uint32_t _M0L3numS187,
  int32_t _M0L12digit__startS185,
  int32_t _M0L10total__lenS184
) {
  int32_t _M0Lm6offsetS183;
  uint32_t _M0Lm1nS186;
  int32_t _M0L6_2atmpS688;
  #line 29 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  _M0Lm6offsetS183 = _M0L10total__lenS184 - _M0L12digit__startS185;
  _M0Lm1nS186 = _M0L3numS187;
  while (1) {
    int32_t _M0L6_2atmpS676 = _M0Lm6offsetS183;
    if (_M0L6_2atmpS676 >= 2) {
      int32_t _M0L6_2atmpS677 = _M0Lm6offsetS183;
      uint32_t _M0L6_2atmpS687;
      uint32_t _M0L6_2atmpS686;
      int32_t _M0L9byte__valS188;
      int32_t _M0L2hiS189;
      int32_t _M0L2loS190;
      int32_t _M0L6_2atmpS680;
      int32_t _M0L6_2atmpS678;
      int32_t _M0L6_2atmpS679;
      int32_t _M0L6_2atmpS684;
      int32_t _M0L6_2atmpS683;
      int32_t _M0L6_2atmpS681;
      int32_t _M0L6_2atmpS682;
      uint32_t _M0L6_2atmpS685;
      _M0Lm6offsetS183 = _M0L6_2atmpS677 - 2;
      _M0L6_2atmpS687 = _M0Lm1nS186;
      _M0L6_2atmpS686 = _M0L6_2atmpS687 & 255u;
      _M0L9byte__valS188 = *(int32_t*)&_M0L6_2atmpS686;
      _M0L2hiS189 = _M0L9byte__valS188 / 16;
      _M0L2loS190 = _M0L9byte__valS188 % 16;
      _M0L6_2atmpS680 = _M0Lm6offsetS183;
      _M0L6_2atmpS678 = _M0L12digit__startS185 + _M0L6_2atmpS680;
      _M0L6_2atmpS679
      = ((moonbit_string_t)moonbit_string_literal_19.data)[
        _M0L2hiS189
      ];
      _M0L6bufferS191[_M0L6_2atmpS678] = _M0L6_2atmpS679;
      _M0L6_2atmpS684 = _M0Lm6offsetS183;
      _M0L6_2atmpS683 = _M0L12digit__startS185 + _M0L6_2atmpS684;
      _M0L6_2atmpS681 = _M0L6_2atmpS683 + 1;
      _M0L6_2atmpS682
      = ((moonbit_string_t)moonbit_string_literal_19.data)[
        _M0L2loS190
      ];
      _M0L6bufferS191[_M0L6_2atmpS681] = _M0L6_2atmpS682;
      _M0L6_2atmpS685 = _M0Lm1nS186;
      _M0Lm1nS186 = _M0L6_2atmpS685 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS688 = _M0Lm6offsetS183;
  if (_M0L6_2atmpS688 == 1) {
    uint32_t _M0L6_2atmpS691 = _M0Lm1nS186;
    uint32_t _M0L6_2atmpS690 = _M0L6_2atmpS691 & 15u;
    int32_t _M0L6nibbleS193 = *(int32_t*)&_M0L6_2atmpS690;
    int32_t _M0L6_2atmpS689 =
      ((moonbit_string_t)moonbit_string_literal_19.data)[_M0L6nibbleS193];
    _M0L6bufferS191[_M0L12digit__startS185] = _M0L6_2atmpS689;
    moonbit_decref(_M0L6bufferS191);
  } else {
    moonbit_decref(_M0L6bufferS191);
  }
  return 0;
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS178
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS177;
  struct _M0TPB6Logger _M0L6_2atmpS673;
  #line 142 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 143 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS177 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS177);
  _M0L6_2atmpS673
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS177
  };
  #line 144 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS178, _M0L6_2atmpS673);
  #line 145 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS177);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS180
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS179;
  struct _M0TPB6Logger _M0L6_2atmpS674;
  #line 142 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 143 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS179 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS179);
  _M0L6_2atmpS674
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS179
  };
  #line 144 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS180, _M0L6_2atmpS674);
  #line 145 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS179);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS182
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS181;
  struct _M0TPB6Logger _M0L6_2atmpS675;
  #line 142 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 143 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS181 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS181);
  _M0L6_2atmpS675
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS181
  };
  #line 144 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS182, _M0L6_2atmpS675);
  #line 145 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS181);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS176
) {
  int32_t _M0L8_2afieldS1071;
  #line 98 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS1071 = _M0L4selfS176.$1;
  moonbit_decref(_M0L4selfS176.$0);
  return _M0L8_2afieldS1071;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS175
) {
  int32_t _M0L3endS671;
  int32_t _M0L8_2afieldS1072;
  int32_t _M0L5startS672;
  #line 49 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L3endS671 = _M0L4selfS175.$2;
  _M0L8_2afieldS1072 = _M0L4selfS175.$1;
  moonbit_decref(_M0L4selfS175.$0);
  _M0L5startS672 = _M0L8_2afieldS1072;
  return _M0L3endS671 - _M0L5startS672;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS174
) {
  moonbit_string_t _M0L8_2afieldS1073;
  #line 91 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS1073 = _M0L4selfS174.$0;
  return _M0L8_2afieldS1073;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS170,
  moonbit_string_t _M0L5valueS171,
  int32_t _M0L5startS172,
  int32_t _M0L3lenS173
) {
  int32_t _M0L6_2atmpS670;
  int64_t _M0L6_2atmpS669;
  struct _M0TPC16string10StringView _M0L6_2atmpS668;
  #line 104 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS670 = _M0L5startS172 + _M0L3lenS173;
  _M0L6_2atmpS669 = (int64_t)_M0L6_2atmpS670;
  #line 105 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS668
  = _M0MPC16string6String11sub_2einner(_M0L5valueS171, _M0L5startS172, _M0L6_2atmpS669);
  #line 105 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS170, _M0L6_2atmpS668);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS163,
  int32_t _M0L5startS169,
  int64_t _M0L3endS165
) {
  int32_t _M0L3lenS162;
  int32_t _M0L3endS164;
  int32_t _M0L5startS168;
  int32_t _if__result_1185;
  #line 458 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS162 = Moonbit_array_length(_M0L4selfS163);
  if (_M0L3endS165 == 4294967296ll) {
    _M0L3endS164 = _M0L3lenS162;
  } else {
    int64_t _M0L7_2aSomeS166 = _M0L3endS165;
    int32_t _M0L6_2aendS167 = (int32_t)_M0L7_2aSomeS166;
    if (_M0L6_2aendS167 < 0) {
      _M0L3endS164 = _M0L3lenS162 + _M0L6_2aendS167;
    } else {
      _M0L3endS164 = _M0L6_2aendS167;
    }
  }
  if (_M0L5startS169 < 0) {
    _M0L5startS168 = _M0L3lenS162 + _M0L5startS169;
  } else {
    _M0L5startS168 = _M0L5startS169;
  }
  if (_M0L5startS168 >= 0) {
    if (_M0L5startS168 <= _M0L3endS164) {
      _if__result_1185 = _M0L3endS164 <= _M0L3lenS162;
    } else {
      _if__result_1185 = 0;
    }
  } else {
    _if__result_1185 = 0;
  }
  if (_if__result_1185) {
    if (_M0L5startS168 < _M0L3lenS162) {
      int32_t _M0L6_2atmpS665 = _M0L4selfS163[_M0L5startS168];
      int32_t _M0L6_2atmpS664;
      #line 468 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS664
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS665);
      if (!_M0L6_2atmpS664) {
        
      } else {
        #line 468 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS164 < _M0L3lenS162) {
      int32_t _M0L6_2atmpS667 = _M0L4selfS163[_M0L3endS164];
      int32_t _M0L6_2atmpS666;
      #line 471 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS666
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS667);
      if (!_M0L6_2atmpS666) {
        
      } else {
        #line 471 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS168,
                                                 _M0L3endS164,
                                                 _M0L4selfS163};
  } else {
    moonbit_decref(_M0L4selfS163);
    #line 466 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS160,
  moonbit_string_t _M0L3strS161
) {
  int32_t _M0L3lenS654;
  int32_t _M0L6_2atmpS656;
  int32_t _M0L6_2atmpS655;
  int32_t _M0L6_2atmpS653;
  moonbit_bytes_t _M0L8_2afieldS1075;
  moonbit_bytes_t _M0L4dataS657;
  int32_t _M0L3lenS658;
  int32_t _M0L6_2atmpS659;
  int32_t _M0L3lenS661;
  int32_t _M0L6_2atmpS1074;
  int32_t _M0L6_2atmpS663;
  int32_t _M0L6_2atmpS662;
  int32_t _M0L6_2atmpS660;
  #line 66 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS654 = _M0L4selfS160->$1;
  _M0L6_2atmpS656 = Moonbit_array_length(_M0L3strS161);
  _M0L6_2atmpS655 = _M0L6_2atmpS656 * 2;
  _M0L6_2atmpS653 = _M0L3lenS654 + _M0L6_2atmpS655;
  moonbit_incref(_M0L4selfS160);
  #line 67 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS160, _M0L6_2atmpS653);
  _M0L8_2afieldS1075 = _M0L4selfS160->$0;
  _M0L4dataS657 = _M0L8_2afieldS1075;
  _M0L3lenS658 = _M0L4selfS160->$1;
  _M0L6_2atmpS659 = Moonbit_array_length(_M0L3strS161);
  moonbit_incref(_M0L4dataS657);
  moonbit_incref(_M0L3strS161);
  #line 68 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS657, _M0L3lenS658, _M0L3strS161, 0, _M0L6_2atmpS659);
  _M0L3lenS661 = _M0L4selfS160->$1;
  _M0L6_2atmpS1074 = Moonbit_array_length(_M0L3strS161);
  moonbit_decref(_M0L3strS161);
  _M0L6_2atmpS663 = _M0L6_2atmpS1074;
  _M0L6_2atmpS662 = _M0L6_2atmpS663 * 2;
  _M0L6_2atmpS660 = _M0L3lenS661 + _M0L6_2atmpS662;
  _M0L4selfS160->$1 = _M0L6_2atmpS660;
  moonbit_decref(_M0L4selfS160);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS152,
  int32_t _M0L13bytes__offsetS147,
  moonbit_string_t _M0L3strS154,
  int32_t _M0L11str__offsetS150,
  int32_t _M0L6lengthS148
) {
  int32_t _M0L6_2atmpS652;
  int32_t _M0L6_2atmpS651;
  int32_t _M0L2e1S146;
  int32_t _M0L6_2atmpS650;
  int32_t _M0L2e2S149;
  int32_t _M0L4len1S151;
  int32_t _M0L4len2S153;
  int32_t _if__result_1186;
  #line 124 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
  _M0L6_2atmpS652 = _M0L6lengthS148 * 2;
  _M0L6_2atmpS651 = _M0L13bytes__offsetS147 + _M0L6_2atmpS652;
  _M0L2e1S146 = _M0L6_2atmpS651 - 1;
  _M0L6_2atmpS650 = _M0L11str__offsetS150 + _M0L6lengthS148;
  _M0L2e2S149 = _M0L6_2atmpS650 - 1;
  _M0L4len1S151 = Moonbit_array_length(_M0L4selfS152);
  _M0L4len2S153 = Moonbit_array_length(_M0L3strS154);
  if (_M0L6lengthS148 >= 0) {
    if (_M0L13bytes__offsetS147 >= 0) {
      if (_M0L2e1S146 < _M0L4len1S151) {
        if (_M0L11str__offsetS150 >= 0) {
          _if__result_1186 = _M0L2e2S149 < _M0L4len2S153;
        } else {
          _if__result_1186 = 0;
        }
      } else {
        _if__result_1186 = 0;
      }
    } else {
      _if__result_1186 = 0;
    }
  } else {
    _if__result_1186 = 0;
  }
  if (_if__result_1186) {
    int32_t _M0L16end__str__offsetS155 =
      _M0L11str__offsetS150 + _M0L6lengthS148;
    int32_t _M0L1iS156 = _M0L11str__offsetS150;
    int32_t _M0L1jS157 = _M0L13bytes__offsetS147;
    while (1) {
      if (_M0L1iS156 < _M0L16end__str__offsetS155) {
        int32_t _M0L6_2atmpS647 = _M0L3strS154[_M0L1iS156];
        int32_t _M0L6_2atmpS646 = (int32_t)_M0L6_2atmpS647;
        uint32_t _M0L1cS158 = *(uint32_t*)&_M0L6_2atmpS646;
        uint32_t _M0L6_2atmpS642 = _M0L1cS158 & 255u;
        int32_t _M0L6_2atmpS641;
        int32_t _M0L6_2atmpS643;
        uint32_t _M0L6_2atmpS645;
        int32_t _M0L6_2atmpS644;
        int32_t _M0L6_2atmpS648;
        int32_t _M0L6_2atmpS649;
        #line 141 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS641 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS642);
        if (
          _M0L1jS157 < 0 || _M0L1jS157 >= Moonbit_array_length(_M0L4selfS152)
        ) {
          #line 141 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS152[_M0L1jS157] = _M0L6_2atmpS641;
        _M0L6_2atmpS643 = _M0L1jS157 + 1;
        _M0L6_2atmpS645 = _M0L1cS158 >> 8;
        #line 142 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS644 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS645);
        if (
          _M0L6_2atmpS643 < 0
          || _M0L6_2atmpS643 >= Moonbit_array_length(_M0L4selfS152)
        ) {
          #line 142 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS152[_M0L6_2atmpS643] = _M0L6_2atmpS644;
        _M0L6_2atmpS648 = _M0L1iS156 + 1;
        _M0L6_2atmpS649 = _M0L1jS157 + 2;
        _M0L1iS156 = _M0L6_2atmpS648;
        _M0L1jS157 = _M0L6_2atmpS649;
        continue;
      } else {
        moonbit_decref(_M0L3strS154);
        moonbit_decref(_M0L4selfS152);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS154);
    moonbit_decref(_M0L4selfS152);
    #line 137 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS92
) {
  int32_t _M0L6_2atmpS640;
  struct _M0TPC16string10StringView _M0L7_2abindS91;
  moonbit_string_t _M0L7_2adataS93;
  int32_t _M0L8_2astartS94;
  int32_t _M0L6_2atmpS639;
  int32_t _M0L6_2aendS95;
  int32_t _M0Lm9_2acursorS96;
  int32_t _M0Lm13accept__stateS97;
  int32_t _M0Lm10match__endS98;
  int32_t _M0Lm20match__tag__saver__0S99;
  int32_t _M0Lm20match__tag__saver__1S100;
  int32_t _M0Lm20match__tag__saver__2S101;
  int32_t _M0Lm20match__tag__saver__3S102;
  int32_t _M0Lm20match__tag__saver__4S103;
  int32_t _M0Lm6tag__0S104;
  int32_t _M0Lm6tag__1S105;
  int32_t _M0Lm9tag__1__1S106;
  int32_t _M0Lm9tag__1__2S107;
  int32_t _M0Lm6tag__3S108;
  int32_t _M0Lm6tag__2S109;
  int32_t _M0Lm9tag__2__1S110;
  int32_t _M0Lm6tag__4S111;
  int32_t _M0L6_2atmpS597;
  #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS640 = Moonbit_array_length(_M0L4reprS92);
  _M0L7_2abindS91
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS640, _M0L4reprS92
  };
  moonbit_incref(_M0L7_2abindS91.$0);
  #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L7_2adataS93 = _M0MPC16string10StringView4data(_M0L7_2abindS91);
  moonbit_incref(_M0L7_2abindS91.$0);
  #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2astartS94
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS91);
  #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS639 = _M0MPC16string10StringView6length(_M0L7_2abindS91);
  _M0L6_2aendS95 = _M0L8_2astartS94 + _M0L6_2atmpS639;
  _M0Lm9_2acursorS96 = _M0L8_2astartS94;
  _M0Lm13accept__stateS97 = -1;
  _M0Lm10match__endS98 = -1;
  _M0Lm20match__tag__saver__0S99 = -1;
  _M0Lm20match__tag__saver__1S100 = -1;
  _M0Lm20match__tag__saver__2S101 = -1;
  _M0Lm20match__tag__saver__3S102 = -1;
  _M0Lm20match__tag__saver__4S103 = -1;
  _M0Lm6tag__0S104 = -1;
  _M0Lm6tag__1S105 = -1;
  _M0Lm9tag__1__1S106 = -1;
  _M0Lm9tag__1__2S107 = -1;
  _M0Lm6tag__3S108 = -1;
  _M0Lm6tag__2S109 = -1;
  _M0Lm9tag__2__1S110 = -1;
  _M0Lm6tag__4S111 = -1;
  _M0L6_2atmpS597 = _M0Lm9_2acursorS96;
  if (_M0L6_2atmpS597 < _M0L6_2aendS95) {
    int32_t _M0L6_2atmpS599 = _M0Lm9_2acursorS96;
    int32_t _M0L6_2atmpS598;
    moonbit_incref(_M0L7_2adataS93);
    #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
    _M0L6_2atmpS598
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS599);
    if (_M0L6_2atmpS598 == 64) {
      int32_t _M0L6_2atmpS600 = _M0Lm9_2acursorS96;
      _M0Lm9_2acursorS96 = _M0L6_2atmpS600 + 1;
      while (1) {
        int32_t _M0L6_2atmpS601;
        _M0Lm6tag__0S104 = _M0Lm9_2acursorS96;
        _M0L6_2atmpS601 = _M0Lm9_2acursorS96;
        if (_M0L6_2atmpS601 < _M0L6_2aendS95) {
          int32_t _M0L6_2atmpS638 = _M0Lm9_2acursorS96;
          int32_t _M0L10next__charS119;
          int32_t _M0L6_2atmpS602;
          moonbit_incref(_M0L7_2adataS93);
          #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
          _M0L10next__charS119
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS638);
          _M0L6_2atmpS602 = _M0Lm9_2acursorS96;
          _M0Lm9_2acursorS96 = _M0L6_2atmpS602 + 1;
          if (_M0L10next__charS119 == 58) {
            int32_t _M0L6_2atmpS603 = _M0Lm9_2acursorS96;
            if (_M0L6_2atmpS603 < _M0L6_2aendS95) {
              int32_t _M0L6_2atmpS604 = _M0Lm9_2acursorS96;
              int32_t _M0L12dispatch__15S120;
              _M0Lm9_2acursorS96 = _M0L6_2atmpS604 + 1;
              _M0L12dispatch__15S120 = 0;
              loop__label__15_123:;
              while (1) {
                int32_t _M0L6_2atmpS605;
                switch (_M0L12dispatch__15S120) {
                  case 3: {
                    int32_t _M0L6_2atmpS608;
                    _M0Lm9tag__1__2S107 = _M0Lm9tag__1__1S106;
                    _M0Lm9tag__1__1S106 = _M0Lm6tag__1S105;
                    _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                    _M0L6_2atmpS608 = _M0Lm9_2acursorS96;
                    if (_M0L6_2atmpS608 < _M0L6_2aendS95) {
                      int32_t _M0L6_2atmpS613 = _M0Lm9_2acursorS96;
                      int32_t _M0L10next__charS127;
                      int32_t _M0L6_2atmpS609;
                      moonbit_incref(_M0L7_2adataS93);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS127
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS613);
                      _M0L6_2atmpS609 = _M0Lm9_2acursorS96;
                      _M0Lm9_2acursorS96 = _M0L6_2atmpS609 + 1;
                      if (_M0L10next__charS127 < 58) {
                        if (_M0L10next__charS127 < 48) {
                          goto join_126;
                        } else {
                          int32_t _M0L6_2atmpS610;
                          _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                          _M0Lm9tag__2__1S110 = _M0Lm6tag__2S109;
                          _M0Lm6tag__2S109 = _M0Lm9_2acursorS96;
                          _M0Lm6tag__3S108 = _M0Lm9_2acursorS96;
                          _M0L6_2atmpS610 = _M0Lm9_2acursorS96;
                          if (_M0L6_2atmpS610 < _M0L6_2aendS95) {
                            int32_t _M0L6_2atmpS612 = _M0Lm9_2acursorS96;
                            int32_t _M0L10next__charS129;
                            int32_t _M0L6_2atmpS611;
                            moonbit_incref(_M0L7_2adataS93);
                            #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                            _M0L10next__charS129
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS612);
                            _M0L6_2atmpS611 = _M0Lm9_2acursorS96;
                            _M0Lm9_2acursorS96 = _M0L6_2atmpS611 + 1;
                            if (_M0L10next__charS129 < 48) {
                              if (_M0L10next__charS129 == 45) {
                                goto join_121;
                              } else {
                                goto join_128;
                              }
                            } else if (_M0L10next__charS129 > 57) {
                              if (_M0L10next__charS129 < 59) {
                                _M0L12dispatch__15S120 = 3;
                                goto loop__label__15_123;
                              } else {
                                goto join_128;
                              }
                            } else {
                              _M0L12dispatch__15S120 = 6;
                              goto loop__label__15_123;
                            }
                            join_128:;
                            _M0L12dispatch__15S120 = 0;
                            goto loop__label__15_123;
                          } else {
                            goto join_112;
                          }
                        }
                      } else if (_M0L10next__charS127 > 58) {
                        goto join_126;
                      } else {
                        _M0L12dispatch__15S120 = 1;
                        goto loop__label__15_123;
                      }
                      join_126:;
                      _M0L12dispatch__15S120 = 0;
                      goto loop__label__15_123;
                    } else {
                      goto join_112;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS614;
                    _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                    _M0Lm6tag__2S109 = _M0Lm9_2acursorS96;
                    _M0L6_2atmpS614 = _M0Lm9_2acursorS96;
                    if (_M0L6_2atmpS614 < _M0L6_2aendS95) {
                      int32_t _M0L6_2atmpS616 = _M0Lm9_2acursorS96;
                      int32_t _M0L10next__charS131;
                      int32_t _M0L6_2atmpS615;
                      moonbit_incref(_M0L7_2adataS93);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS131
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS616);
                      _M0L6_2atmpS615 = _M0Lm9_2acursorS96;
                      _M0Lm9_2acursorS96 = _M0L6_2atmpS615 + 1;
                      if (_M0L10next__charS131 < 58) {
                        if (_M0L10next__charS131 < 48) {
                          goto join_130;
                        } else {
                          _M0L12dispatch__15S120 = 2;
                          goto loop__label__15_123;
                        }
                      } else if (_M0L10next__charS131 > 58) {
                        goto join_130;
                      } else {
                        _M0L12dispatch__15S120 = 3;
                        goto loop__label__15_123;
                      }
                      join_130:;
                      _M0L12dispatch__15S120 = 0;
                      goto loop__label__15_123;
                    } else {
                      goto join_112;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS617;
                    _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                    _M0L6_2atmpS617 = _M0Lm9_2acursorS96;
                    if (_M0L6_2atmpS617 < _M0L6_2aendS95) {
                      int32_t _M0L6_2atmpS619 = _M0Lm9_2acursorS96;
                      int32_t _M0L10next__charS132;
                      int32_t _M0L6_2atmpS618;
                      moonbit_incref(_M0L7_2adataS93);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS132
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS619);
                      _M0L6_2atmpS618 = _M0Lm9_2acursorS96;
                      _M0Lm9_2acursorS96 = _M0L6_2atmpS618 + 1;
                      if (_M0L10next__charS132 == 58) {
                        _M0L12dispatch__15S120 = 1;
                        goto loop__label__15_123;
                      } else {
                        _M0L12dispatch__15S120 = 0;
                        goto loop__label__15_123;
                      }
                    } else {
                      goto join_112;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS620;
                    _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                    _M0Lm6tag__4S111 = _M0Lm9_2acursorS96;
                    _M0L6_2atmpS620 = _M0Lm9_2acursorS96;
                    if (_M0L6_2atmpS620 < _M0L6_2aendS95) {
                      int32_t _M0L6_2atmpS628 = _M0Lm9_2acursorS96;
                      int32_t _M0L10next__charS134;
                      int32_t _M0L6_2atmpS621;
                      moonbit_incref(_M0L7_2adataS93);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS134
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS628);
                      _M0L6_2atmpS621 = _M0Lm9_2acursorS96;
                      _M0Lm9_2acursorS96 = _M0L6_2atmpS621 + 1;
                      if (_M0L10next__charS134 < 58) {
                        if (_M0L10next__charS134 < 48) {
                          goto join_133;
                        } else {
                          _M0L12dispatch__15S120 = 4;
                          goto loop__label__15_123;
                        }
                      } else if (_M0L10next__charS134 > 58) {
                        goto join_133;
                      } else {
                        int32_t _M0L6_2atmpS622;
                        _M0Lm9tag__1__2S107 = _M0Lm9tag__1__1S106;
                        _M0Lm9tag__1__1S106 = _M0Lm6tag__1S105;
                        _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                        _M0L6_2atmpS622 = _M0Lm9_2acursorS96;
                        if (_M0L6_2atmpS622 < _M0L6_2aendS95) {
                          int32_t _M0L6_2atmpS627 = _M0Lm9_2acursorS96;
                          int32_t _M0L10next__charS136;
                          int32_t _M0L6_2atmpS623;
                          moonbit_incref(_M0L7_2adataS93);
                          #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                          _M0L10next__charS136
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS627);
                          _M0L6_2atmpS623 = _M0Lm9_2acursorS96;
                          _M0Lm9_2acursorS96 = _M0L6_2atmpS623 + 1;
                          if (_M0L10next__charS136 < 58) {
                            if (_M0L10next__charS136 < 48) {
                              goto join_135;
                            } else {
                              int32_t _M0L6_2atmpS624;
                              _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                              _M0Lm9tag__2__1S110 = _M0Lm6tag__2S109;
                              _M0Lm6tag__2S109 = _M0Lm9_2acursorS96;
                              _M0L6_2atmpS624 = _M0Lm9_2acursorS96;
                              if (_M0L6_2atmpS624 < _M0L6_2aendS95) {
                                int32_t _M0L6_2atmpS626 = _M0Lm9_2acursorS96;
                                int32_t _M0L10next__charS138;
                                int32_t _M0L6_2atmpS625;
                                moonbit_incref(_M0L7_2adataS93);
                                #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                                _M0L10next__charS138
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS626);
                                _M0L6_2atmpS625 = _M0Lm9_2acursorS96;
                                _M0Lm9_2acursorS96 = _M0L6_2atmpS625 + 1;
                                if (_M0L10next__charS138 < 58) {
                                  if (_M0L10next__charS138 < 48) {
                                    goto join_137;
                                  } else {
                                    _M0L12dispatch__15S120 = 5;
                                    goto loop__label__15_123;
                                  }
                                } else if (_M0L10next__charS138 > 58) {
                                  goto join_137;
                                } else {
                                  _M0L12dispatch__15S120 = 3;
                                  goto loop__label__15_123;
                                }
                                join_137:;
                                _M0L12dispatch__15S120 = 0;
                                goto loop__label__15_123;
                              } else {
                                goto join_125;
                              }
                            }
                          } else if (_M0L10next__charS136 > 58) {
                            goto join_135;
                          } else {
                            _M0L12dispatch__15S120 = 1;
                            goto loop__label__15_123;
                          }
                          join_135:;
                          _M0L12dispatch__15S120 = 0;
                          goto loop__label__15_123;
                        } else {
                          goto join_112;
                        }
                      }
                      join_133:;
                      _M0L12dispatch__15S120 = 0;
                      goto loop__label__15_123;
                    } else {
                      goto join_112;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS629;
                    _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                    _M0Lm6tag__2S109 = _M0Lm9_2acursorS96;
                    _M0L6_2atmpS629 = _M0Lm9_2acursorS96;
                    if (_M0L6_2atmpS629 < _M0L6_2aendS95) {
                      int32_t _M0L6_2atmpS631 = _M0Lm9_2acursorS96;
                      int32_t _M0L10next__charS140;
                      int32_t _M0L6_2atmpS630;
                      moonbit_incref(_M0L7_2adataS93);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS140
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS631);
                      _M0L6_2atmpS630 = _M0Lm9_2acursorS96;
                      _M0Lm9_2acursorS96 = _M0L6_2atmpS630 + 1;
                      if (_M0L10next__charS140 < 58) {
                        if (_M0L10next__charS140 < 48) {
                          goto join_139;
                        } else {
                          _M0L12dispatch__15S120 = 5;
                          goto loop__label__15_123;
                        }
                      } else if (_M0L10next__charS140 > 58) {
                        goto join_139;
                      } else {
                        _M0L12dispatch__15S120 = 3;
                        goto loop__label__15_123;
                      }
                      join_139:;
                      _M0L12dispatch__15S120 = 0;
                      goto loop__label__15_123;
                    } else {
                      goto join_125;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS632;
                    _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                    _M0Lm6tag__2S109 = _M0Lm9_2acursorS96;
                    _M0Lm6tag__3S108 = _M0Lm9_2acursorS96;
                    _M0L6_2atmpS632 = _M0Lm9_2acursorS96;
                    if (_M0L6_2atmpS632 < _M0L6_2aendS95) {
                      int32_t _M0L6_2atmpS634 = _M0Lm9_2acursorS96;
                      int32_t _M0L10next__charS142;
                      int32_t _M0L6_2atmpS633;
                      moonbit_incref(_M0L7_2adataS93);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS142
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS634);
                      _M0L6_2atmpS633 = _M0Lm9_2acursorS96;
                      _M0Lm9_2acursorS96 = _M0L6_2atmpS633 + 1;
                      if (_M0L10next__charS142 < 48) {
                        if (_M0L10next__charS142 == 45) {
                          goto join_121;
                        } else {
                          goto join_141;
                        }
                      } else if (_M0L10next__charS142 > 57) {
                        if (_M0L10next__charS142 < 59) {
                          _M0L12dispatch__15S120 = 3;
                          goto loop__label__15_123;
                        } else {
                          goto join_141;
                        }
                      } else {
                        _M0L12dispatch__15S120 = 6;
                        goto loop__label__15_123;
                      }
                      join_141:;
                      _M0L12dispatch__15S120 = 0;
                      goto loop__label__15_123;
                    } else {
                      goto join_112;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS635;
                    _M0Lm9tag__1__1S106 = _M0Lm6tag__1S105;
                    _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                    _M0L6_2atmpS635 = _M0Lm9_2acursorS96;
                    if (_M0L6_2atmpS635 < _M0L6_2aendS95) {
                      int32_t _M0L6_2atmpS637 = _M0Lm9_2acursorS96;
                      int32_t _M0L10next__charS144;
                      int32_t _M0L6_2atmpS636;
                      moonbit_incref(_M0L7_2adataS93);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS144
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS637);
                      _M0L6_2atmpS636 = _M0Lm9_2acursorS96;
                      _M0Lm9_2acursorS96 = _M0L6_2atmpS636 + 1;
                      if (_M0L10next__charS144 < 58) {
                        if (_M0L10next__charS144 < 48) {
                          goto join_143;
                        } else {
                          _M0L12dispatch__15S120 = 2;
                          goto loop__label__15_123;
                        }
                      } else if (_M0L10next__charS144 > 58) {
                        goto join_143;
                      } else {
                        _M0L12dispatch__15S120 = 1;
                        goto loop__label__15_123;
                      }
                      join_143:;
                      _M0L12dispatch__15S120 = 0;
                      goto loop__label__15_123;
                    } else {
                      goto join_112;
                    }
                    break;
                  }
                  default: {
                    goto join_112;
                    break;
                  }
                }
                join_125:;
                _M0Lm6tag__1S105 = _M0Lm9tag__1__2S107;
                _M0Lm6tag__2S109 = _M0Lm9tag__2__1S110;
                _M0Lm20match__tag__saver__0S99 = _M0Lm6tag__0S104;
                _M0Lm20match__tag__saver__1S100 = _M0Lm6tag__1S105;
                _M0Lm20match__tag__saver__2S101 = _M0Lm6tag__2S109;
                _M0Lm20match__tag__saver__3S102 = _M0Lm6tag__3S108;
                _M0Lm20match__tag__saver__4S103 = _M0Lm6tag__4S111;
                _M0Lm13accept__stateS97 = 0;
                _M0Lm10match__endS98 = _M0Lm9_2acursorS96;
                goto join_112;
                join_121:;
                _M0Lm9tag__1__1S106 = _M0Lm9tag__1__2S107;
                _M0Lm6tag__1S105 = _M0Lm9_2acursorS96;
                _M0Lm6tag__2S109 = _M0Lm9tag__2__1S110;
                _M0L6_2atmpS605 = _M0Lm9_2acursorS96;
                if (_M0L6_2atmpS605 < _M0L6_2aendS95) {
                  int32_t _M0L6_2atmpS607 = _M0Lm9_2acursorS96;
                  int32_t _M0L10next__charS124;
                  int32_t _M0L6_2atmpS606;
                  moonbit_incref(_M0L7_2adataS93);
                  #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                  _M0L10next__charS124
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS93, _M0L6_2atmpS607);
                  _M0L6_2atmpS606 = _M0Lm9_2acursorS96;
                  _M0Lm9_2acursorS96 = _M0L6_2atmpS606 + 1;
                  if (_M0L10next__charS124 < 58) {
                    if (_M0L10next__charS124 < 48) {
                      goto join_122;
                    } else {
                      _M0L12dispatch__15S120 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS124 > 58) {
                    goto join_122;
                  } else {
                    _M0L12dispatch__15S120 = 1;
                    continue;
                  }
                  join_122:;
                  _M0L12dispatch__15S120 = 0;
                  continue;
                } else {
                  goto join_112;
                }
                break;
              }
            } else {
              goto join_112;
            }
          } else {
            continue;
          }
        } else {
          goto join_112;
        }
        break;
      }
    } else {
      goto join_112;
    }
  } else {
    goto join_112;
  }
  join_112:;
  switch (_M0Lm13accept__stateS97) {
    case 0: {
      int32_t _M0L6_2atmpS596 = _M0Lm20match__tag__saver__1S100;
      int32_t _M0L6_2atmpS595 = _M0L6_2atmpS596 + 1;
      int64_t _M0L6_2atmpS592 = (int64_t)_M0L6_2atmpS595;
      int32_t _M0L6_2atmpS594 = _M0Lm20match__tag__saver__2S101;
      int64_t _M0L6_2atmpS593 = (int64_t)_M0L6_2atmpS594;
      struct _M0TPC16string10StringView _M0L11start__lineS113;
      int32_t _M0L6_2atmpS591;
      int32_t _M0L6_2atmpS590;
      int64_t _M0L6_2atmpS587;
      int32_t _M0L6_2atmpS589;
      int64_t _M0L6_2atmpS588;
      struct _M0TPC16string10StringView _M0L13start__columnS114;
      int32_t _M0L6_2atmpS586;
      int64_t _M0L6_2atmpS583;
      int32_t _M0L6_2atmpS585;
      int64_t _M0L6_2atmpS584;
      struct _M0TPC16string10StringView _M0L3pkgS115;
      int32_t _M0L6_2atmpS582;
      int32_t _M0L6_2atmpS581;
      int64_t _M0L6_2atmpS578;
      int32_t _M0L6_2atmpS580;
      int64_t _M0L6_2atmpS579;
      struct _M0TPC16string10StringView _M0L8filenameS116;
      int32_t _M0L6_2atmpS577;
      int32_t _M0L6_2atmpS576;
      int64_t _M0L6_2atmpS573;
      int32_t _M0L6_2atmpS575;
      int64_t _M0L6_2atmpS574;
      struct _M0TPC16string10StringView _M0L9end__lineS117;
      int32_t _M0L6_2atmpS572;
      int32_t _M0L6_2atmpS571;
      int64_t _M0L6_2atmpS568;
      int32_t _M0L6_2atmpS570;
      int64_t _M0L6_2atmpS569;
      struct _M0TPC16string10StringView _M0L11end__columnS118;
      struct _M0TPB13SourceLocRepr* _block_1203;
      moonbit_incref(_M0L7_2adataS93);
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11start__lineS113
      = _M0MPC16string6String4view(_M0L7_2adataS93, _M0L6_2atmpS592, _M0L6_2atmpS593);
      _M0L6_2atmpS591 = _M0Lm20match__tag__saver__2S101;
      _M0L6_2atmpS590 = _M0L6_2atmpS591 + 1;
      _M0L6_2atmpS587 = (int64_t)_M0L6_2atmpS590;
      _M0L6_2atmpS589 = _M0Lm20match__tag__saver__3S102;
      _M0L6_2atmpS588 = (int64_t)_M0L6_2atmpS589;
      moonbit_incref(_M0L7_2adataS93);
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L13start__columnS114
      = _M0MPC16string6String4view(_M0L7_2adataS93, _M0L6_2atmpS587, _M0L6_2atmpS588);
      _M0L6_2atmpS586 = _M0L8_2astartS94 + 1;
      _M0L6_2atmpS583 = (int64_t)_M0L6_2atmpS586;
      _M0L6_2atmpS585 = _M0Lm20match__tag__saver__0S99;
      _M0L6_2atmpS584 = (int64_t)_M0L6_2atmpS585;
      moonbit_incref(_M0L7_2adataS93);
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L3pkgS115
      = _M0MPC16string6String4view(_M0L7_2adataS93, _M0L6_2atmpS583, _M0L6_2atmpS584);
      _M0L6_2atmpS582 = _M0Lm20match__tag__saver__0S99;
      _M0L6_2atmpS581 = _M0L6_2atmpS582 + 1;
      _M0L6_2atmpS578 = (int64_t)_M0L6_2atmpS581;
      _M0L6_2atmpS580 = _M0Lm20match__tag__saver__1S100;
      _M0L6_2atmpS579 = (int64_t)_M0L6_2atmpS580;
      moonbit_incref(_M0L7_2adataS93);
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L8filenameS116
      = _M0MPC16string6String4view(_M0L7_2adataS93, _M0L6_2atmpS578, _M0L6_2atmpS579);
      _M0L6_2atmpS577 = _M0Lm20match__tag__saver__3S102;
      _M0L6_2atmpS576 = _M0L6_2atmpS577 + 1;
      _M0L6_2atmpS573 = (int64_t)_M0L6_2atmpS576;
      _M0L6_2atmpS575 = _M0Lm20match__tag__saver__4S103;
      _M0L6_2atmpS574 = (int64_t)_M0L6_2atmpS575;
      moonbit_incref(_M0L7_2adataS93);
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L9end__lineS117
      = _M0MPC16string6String4view(_M0L7_2adataS93, _M0L6_2atmpS573, _M0L6_2atmpS574);
      _M0L6_2atmpS572 = _M0Lm20match__tag__saver__4S103;
      _M0L6_2atmpS571 = _M0L6_2atmpS572 + 1;
      _M0L6_2atmpS568 = (int64_t)_M0L6_2atmpS571;
      _M0L6_2atmpS570 = _M0Lm10match__endS98;
      _M0L6_2atmpS569 = (int64_t)_M0L6_2atmpS570;
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11end__columnS118
      = _M0MPC16string6String4view(_M0L7_2adataS93, _M0L6_2atmpS568, _M0L6_2atmpS569);
      _block_1203
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_1203)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_1203->$0_0 = _M0L3pkgS115.$0;
      _block_1203->$0_1 = _M0L3pkgS115.$1;
      _block_1203->$0_2 = _M0L3pkgS115.$2;
      _block_1203->$1_0 = _M0L8filenameS116.$0;
      _block_1203->$1_1 = _M0L8filenameS116.$1;
      _block_1203->$1_2 = _M0L8filenameS116.$2;
      _block_1203->$2_0 = _M0L11start__lineS113.$0;
      _block_1203->$2_1 = _M0L11start__lineS113.$1;
      _block_1203->$2_2 = _M0L11start__lineS113.$2;
      _block_1203->$3_0 = _M0L13start__columnS114.$0;
      _block_1203->$3_1 = _M0L13start__columnS114.$1;
      _block_1203->$3_2 = _M0L13start__columnS114.$2;
      _block_1203->$4_0 = _M0L9end__lineS117.$0;
      _block_1203->$4_1 = _M0L9end__lineS117.$1;
      _block_1203->$4_2 = _M0L9end__lineS117.$2;
      _block_1203->$5_0 = _M0L11end__columnS118.$0;
      _block_1203->$5_1 = _M0L11end__columnS118.$1;
      _block_1203->$5_2 = _M0L11end__columnS118.$2;
      return _block_1203;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS93);
      #line 99 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS89,
  int32_t _M0L5indexS90
) {
  int32_t _M0L3lenS88;
  int32_t _if__result_1204;
  #line 183 "/home/blem/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS88 = _M0L4selfS89->$1;
  if (_M0L5indexS90 >= 0) {
    _if__result_1204 = _M0L5indexS90 < _M0L3lenS88;
  } else {
    _if__result_1204 = 0;
  }
  if (_if__result_1204) {
    moonbit_string_t* _M0L6_2atmpS567;
    moonbit_string_t _M0L6_2atmpS1076;
    #line 188 "/home/blem/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS567 = _M0MPC15array5Array6bufferGsE(_M0L4selfS89);
    if (
      _M0L5indexS90 < 0
      || _M0L5indexS90 >= Moonbit_array_length(_M0L6_2atmpS567)
    ) {
      #line 188 "/home/blem/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1076 = (moonbit_string_t)_M0L6_2atmpS567[_M0L5indexS90];
    moonbit_incref(_M0L6_2atmpS1076);
    moonbit_decref(_M0L6_2atmpS567);
    return _M0L6_2atmpS1076;
  } else {
    moonbit_decref(_M0L4selfS89);
    #line 187 "/home/blem/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS86
) {
  moonbit_string_t* _M0L8_2afieldS1077;
  int32_t _M0L6_2acntS1133;
  #line 124 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS1077 = _M0L4selfS86->$0;
  _M0L6_2acntS1133 = Moonbit_object_header(_M0L4selfS86)->rc;
  if (_M0L6_2acntS1133 > 1) {
    int32_t _M0L11_2anew__cntS1134 = _M0L6_2acntS1133 - 1;
    Moonbit_object_header(_M0L4selfS86)->rc = _M0L11_2anew__cntS1134;
    moonbit_incref(_M0L8_2afieldS1077);
  } else if (_M0L6_2acntS1133 == 1) {
    #line 125 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS86);
  }
  return _M0L8_2afieldS1077;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS87
) {
  struct _M0TUsiE** _M0L8_2afieldS1078;
  int32_t _M0L6_2acntS1135;
  #line 124 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS1078 = _M0L4selfS87->$0;
  _M0L6_2acntS1135 = Moonbit_object_header(_M0L4selfS87)->rc;
  if (_M0L6_2acntS1135 > 1) {
    int32_t _M0L11_2anew__cntS1136 = _M0L6_2acntS1135 - 1;
    Moonbit_object_header(_M0L4selfS87)->rc = _M0L11_2anew__cntS1136;
    moonbit_incref(_M0L8_2afieldS1078);
  } else if (_M0L6_2acntS1135 == 1) {
    #line 125 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS87);
  }
  return _M0L8_2afieldS1078;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS85) {
  struct _M0TPB13StringBuilder* _M0L3bufS84;
  struct _M0TPB6Logger _M0L6_2atmpS566;
  #line 184 "/home/blem/.moon/lib/core/builtin/show.mbt"
  #line 185 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L3bufS84 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS84);
  _M0L6_2atmpS566
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS84
  };
  #line 186 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS85, _M0L6_2atmpS566);
  #line 187 "/home/blem/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS84);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS83) {
  int32_t _M0L6_2atmpS565;
  #line 68 "/home/blem/.moon/lib/core/builtin/uint16_char.mbt"
  _M0L6_2atmpS565 = (int32_t)_M0L4selfS83;
  return _M0L6_2atmpS565;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS82) {
  #line 45 "/home/blem/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS82 >= 56320) {
    return _M0L4selfS82 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS79,
  int32_t _M0L2chS81
) {
  int32_t _M0L3lenS560;
  int32_t _M0L6_2atmpS559;
  moonbit_bytes_t _M0L8_2afieldS1079;
  moonbit_bytes_t _M0L4dataS563;
  int32_t _M0L3lenS564;
  int32_t _M0L3incS80;
  int32_t _M0L3lenS562;
  int32_t _M0L6_2atmpS561;
  #line 74 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS560 = _M0L4selfS79->$1;
  _M0L6_2atmpS559 = _M0L3lenS560 + 4;
  moonbit_incref(_M0L4selfS79);
  #line 75 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS79, _M0L6_2atmpS559);
  _M0L8_2afieldS1079 = _M0L4selfS79->$0;
  _M0L4dataS563 = _M0L8_2afieldS1079;
  _M0L3lenS564 = _M0L4selfS79->$1;
  moonbit_incref(_M0L4dataS563);
  #line 76 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3incS80
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS563, _M0L3lenS564, _M0L2chS81);
  _M0L3lenS562 = _M0L4selfS79->$1;
  _M0L6_2atmpS561 = _M0L3lenS562 + _M0L3incS80;
  _M0L4selfS79->$1 = _M0L6_2atmpS561;
  moonbit_decref(_M0L4selfS79);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS74,
  int32_t _M0L8requiredS75
) {
  moonbit_bytes_t _M0L8_2afieldS1083;
  moonbit_bytes_t _M0L4dataS558;
  int32_t _M0L6_2atmpS1082;
  int32_t _M0L12current__lenS73;
  int32_t _M0Lm13enough__spaceS76;
  int32_t _M0L6_2atmpS556;
  int32_t _M0L6_2atmpS557;
  moonbit_bytes_t _M0L9new__dataS78;
  moonbit_bytes_t _M0L8_2afieldS1081;
  moonbit_bytes_t _M0L4dataS554;
  int32_t _M0L3lenS555;
  moonbit_bytes_t _M0L6_2aoldS1080;
  #line 45 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8_2afieldS1083 = _M0L4selfS74->$0;
  _M0L4dataS558 = _M0L8_2afieldS1083;
  _M0L6_2atmpS1082 = Moonbit_array_length(_M0L4dataS558);
  _M0L12current__lenS73 = _M0L6_2atmpS1082;
  if (_M0L8requiredS75 <= _M0L12current__lenS73) {
    moonbit_decref(_M0L4selfS74);
    return 0;
  }
  _M0Lm13enough__spaceS76 = _M0L12current__lenS73;
  while (1) {
    int32_t _M0L6_2atmpS552 = _M0Lm13enough__spaceS76;
    if (_M0L6_2atmpS552 < _M0L8requiredS75) {
      int32_t _M0L6_2atmpS553 = _M0Lm13enough__spaceS76;
      _M0Lm13enough__spaceS76 = _M0L6_2atmpS553 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS556 = _M0Lm13enough__spaceS76;
  #line 59 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS557 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS78
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS556, _M0L6_2atmpS557);
  _M0L8_2afieldS1081 = _M0L4selfS74->$0;
  _M0L4dataS554 = _M0L8_2afieldS1081;
  _M0L3lenS555 = _M0L4selfS74->$1;
  moonbit_incref(_M0L4dataS554);
  moonbit_incref(_M0L9new__dataS78);
  #line 60 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS78, 0, _M0L4dataS554, 0, _M0L3lenS555);
  _M0L6_2aoldS1080 = _M0L4selfS74->$0;
  moonbit_decref(_M0L6_2aoldS1080);
  _M0L4selfS74->$0 = _M0L9new__dataS78;
  moonbit_decref(_M0L4selfS74);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS68,
  int32_t _M0L6offsetS69,
  int32_t _M0L5valueS67
) {
  uint32_t _M0L4codeS66;
  #line 278 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
  #line 283 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
  _M0L4codeS66 = _M0MPC14char4Char8to__uint(_M0L5valueS67);
  if (_M0L4codeS66 < 65536u) {
    uint32_t _M0L6_2atmpS535 = _M0L4codeS66 & 255u;
    int32_t _M0L6_2atmpS534;
    int32_t _M0L6_2atmpS536;
    uint32_t _M0L6_2atmpS538;
    int32_t _M0L6_2atmpS537;
    #line 285 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS534 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS535);
    if (
      _M0L6offsetS69 < 0
      || _M0L6offsetS69 >= Moonbit_array_length(_M0L4selfS68)
    ) {
      #line 285 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS68[_M0L6offsetS69] = _M0L6_2atmpS534;
    _M0L6_2atmpS536 = _M0L6offsetS69 + 1;
    _M0L6_2atmpS538 = _M0L4codeS66 >> 8;
    #line 286 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS537 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS538);
    if (
      _M0L6_2atmpS536 < 0
      || _M0L6_2atmpS536 >= Moonbit_array_length(_M0L4selfS68)
    ) {
      #line 286 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS68[_M0L6_2atmpS536] = _M0L6_2atmpS537;
    moonbit_decref(_M0L4selfS68);
    return 2;
  } else if (_M0L4codeS66 < 1114112u) {
    uint32_t _M0L2hiS70 = _M0L4codeS66 - 65536u;
    uint32_t _M0L6_2atmpS551 = _M0L2hiS70 >> 10;
    uint32_t _M0L2loS71 = _M0L6_2atmpS551 | 55296u;
    uint32_t _M0L6_2atmpS550 = _M0L2hiS70 & 1023u;
    uint32_t _M0L2hiS72 = _M0L6_2atmpS550 | 56320u;
    uint32_t _M0L6_2atmpS540 = _M0L2loS71 & 255u;
    int32_t _M0L6_2atmpS539;
    int32_t _M0L6_2atmpS541;
    uint32_t _M0L6_2atmpS543;
    int32_t _M0L6_2atmpS542;
    int32_t _M0L6_2atmpS544;
    uint32_t _M0L6_2atmpS546;
    int32_t _M0L6_2atmpS545;
    int32_t _M0L6_2atmpS547;
    uint32_t _M0L6_2atmpS549;
    int32_t _M0L6_2atmpS548;
    #line 292 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS539 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS540);
    if (
      _M0L6offsetS69 < 0
      || _M0L6offsetS69 >= Moonbit_array_length(_M0L4selfS68)
    ) {
      #line 292 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS68[_M0L6offsetS69] = _M0L6_2atmpS539;
    _M0L6_2atmpS541 = _M0L6offsetS69 + 1;
    _M0L6_2atmpS543 = _M0L2loS71 >> 8;
    #line 293 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS542 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS543);
    if (
      _M0L6_2atmpS541 < 0
      || _M0L6_2atmpS541 >= Moonbit_array_length(_M0L4selfS68)
    ) {
      #line 293 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS68[_M0L6_2atmpS541] = _M0L6_2atmpS542;
    _M0L6_2atmpS544 = _M0L6offsetS69 + 2;
    _M0L6_2atmpS546 = _M0L2hiS72 & 255u;
    #line 294 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS545 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS546);
    if (
      _M0L6_2atmpS544 < 0
      || _M0L6_2atmpS544 >= Moonbit_array_length(_M0L4selfS68)
    ) {
      #line 294 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS68[_M0L6_2atmpS544] = _M0L6_2atmpS545;
    _M0L6_2atmpS547 = _M0L6offsetS69 + 3;
    _M0L6_2atmpS549 = _M0L2hiS72 >> 8;
    #line 295 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS548 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS549);
    if (
      _M0L6_2atmpS547 < 0
      || _M0L6_2atmpS547 >= Moonbit_array_length(_M0L4selfS68)
    ) {
      #line 295 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS68[_M0L6_2atmpS547] = _M0L6_2atmpS548;
    moonbit_decref(_M0L4selfS68);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS68);
    #line 298 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS65) {
  int32_t _M0L6_2atmpS533;
  #line 2554 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS533 = *(int32_t*)&_M0L4selfS65;
  return _M0L6_2atmpS533 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS64) {
  int32_t _M0L6_2atmpS532;
  #line 1270 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS532 = _M0L4selfS64;
  return *(uint32_t*)&_M0L6_2atmpS532;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS63
) {
  moonbit_bytes_t _M0L8_2afieldS1085;
  moonbit_bytes_t _M0L4dataS531;
  moonbit_bytes_t _M0L6_2atmpS528;
  int32_t _M0L8_2afieldS1084;
  int32_t _M0L3lenS530;
  int64_t _M0L6_2atmpS529;
  #line 115 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8_2afieldS1085 = _M0L4selfS63->$0;
  _M0L4dataS531 = _M0L8_2afieldS1085;
  moonbit_incref(_M0L4dataS531);
  _M0L6_2atmpS528 = _M0L4dataS531;
  _M0L8_2afieldS1084 = _M0L4selfS63->$1;
  moonbit_decref(_M0L4selfS63);
  _M0L3lenS530 = _M0L8_2afieldS1084;
  _M0L6_2atmpS529 = (int64_t)_M0L3lenS530;
  #line 116 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS528, 0, _M0L6_2atmpS529);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS58,
  int32_t _M0L6offsetS62,
  int64_t _M0L6lengthS60
) {
  int32_t _M0L3lenS57;
  int32_t _M0L6lengthS59;
  int32_t _if__result_1206;
  #line 76 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
  _M0L3lenS57 = Moonbit_array_length(_M0L4selfS58);
  if (_M0L6lengthS60 == 4294967296ll) {
    _M0L6lengthS59 = _M0L3lenS57 - _M0L6offsetS62;
  } else {
    int64_t _M0L7_2aSomeS61 = _M0L6lengthS60;
    _M0L6lengthS59 = (int32_t)_M0L7_2aSomeS61;
  }
  if (_M0L6offsetS62 >= 0) {
    if (_M0L6lengthS59 >= 0) {
      int32_t _M0L6_2atmpS527 = _M0L6offsetS62 + _M0L6lengthS59;
      _if__result_1206 = _M0L6_2atmpS527 <= _M0L3lenS57;
    } else {
      _if__result_1206 = 0;
    }
  } else {
    _if__result_1206 = 0;
  }
  if (_if__result_1206) {
    #line 84 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS58, _M0L6offsetS62, _M0L6lengthS59);
  } else {
    moonbit_decref(_M0L4selfS58);
    #line 83 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS55
) {
  int32_t _M0L7initialS54;
  moonbit_bytes_t _M0L4dataS56;
  struct _M0TPB13StringBuilder* _block_1207;
  #line 32 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS55 < 1) {
    _M0L7initialS54 = 1;
  } else {
    _M0L7initialS54 = _M0L10size__hintS55;
  }
  _M0L4dataS56 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS54, 0);
  _block_1207
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_1207)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_1207->$0 = _M0L4dataS56;
  _block_1207->$1 = 0;
  return _block_1207;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS53) {
  int32_t _M0L6_2atmpS526;
  #line 1903 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS526 = (int32_t)_M0L4selfS53;
  return _M0L6_2atmpS526;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS43,
  int32_t _M0L11dst__offsetS44,
  moonbit_string_t* _M0L3srcS45,
  int32_t _M0L11src__offsetS46,
  int32_t _M0L3lenS47
) {
  #line 104 "/home/blem/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/home/blem/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS43, _M0L11dst__offsetS44, _M0L3srcS45, _M0L11src__offsetS46, _M0L3lenS47);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS48,
  int32_t _M0L11dst__offsetS49,
  struct _M0TUsiE** _M0L3srcS50,
  int32_t _M0L11src__offsetS51,
  int32_t _M0L3lenS52
) {
  #line 104 "/home/blem/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/home/blem/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS48, _M0L11dst__offsetS49, _M0L3srcS50, _M0L11src__offsetS51, _M0L3lenS52);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS16,
  int32_t _M0L11dst__offsetS18,
  moonbit_bytes_t _M0L3srcS17,
  int32_t _M0L11src__offsetS19,
  int32_t _M0L3lenS21
) {
  int32_t _if__result_1208;
  #line 38 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS16 == _M0L3srcS17) {
    _if__result_1208 = _M0L11dst__offsetS18 < _M0L11src__offsetS19;
  } else {
    _if__result_1208 = 0;
  }
  if (_if__result_1208) {
    int32_t _M0L1iS20 = 0;
    while (1) {
      if (_M0L1iS20 < _M0L3lenS21) {
        int32_t _M0L6_2atmpS499 = _M0L11dst__offsetS18 + _M0L1iS20;
        int32_t _M0L6_2atmpS501 = _M0L11src__offsetS19 + _M0L1iS20;
        int32_t _M0L6_2atmpS500;
        int32_t _M0L6_2atmpS502;
        if (
          _M0L6_2atmpS501 < 0
          || _M0L6_2atmpS501 >= Moonbit_array_length(_M0L3srcS17)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS500 = (int32_t)_M0L3srcS17[_M0L6_2atmpS501];
        if (
          _M0L6_2atmpS499 < 0
          || _M0L6_2atmpS499 >= Moonbit_array_length(_M0L3dstS16)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS16[_M0L6_2atmpS499] = _M0L6_2atmpS500;
        _M0L6_2atmpS502 = _M0L1iS20 + 1;
        _M0L1iS20 = _M0L6_2atmpS502;
        continue;
      } else {
        moonbit_decref(_M0L3srcS17);
        moonbit_decref(_M0L3dstS16);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS507 = _M0L3lenS21 - 1;
    int32_t _M0L1iS23 = _M0L6_2atmpS507;
    while (1) {
      if (_M0L1iS23 >= 0) {
        int32_t _M0L6_2atmpS503 = _M0L11dst__offsetS18 + _M0L1iS23;
        int32_t _M0L6_2atmpS505 = _M0L11src__offsetS19 + _M0L1iS23;
        int32_t _M0L6_2atmpS504;
        int32_t _M0L6_2atmpS506;
        if (
          _M0L6_2atmpS505 < 0
          || _M0L6_2atmpS505 >= Moonbit_array_length(_M0L3srcS17)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS504 = (int32_t)_M0L3srcS17[_M0L6_2atmpS505];
        if (
          _M0L6_2atmpS503 < 0
          || _M0L6_2atmpS503 >= Moonbit_array_length(_M0L3dstS16)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS16[_M0L6_2atmpS503] = _M0L6_2atmpS504;
        _M0L6_2atmpS506 = _M0L1iS23 - 1;
        _M0L1iS23 = _M0L6_2atmpS506;
        continue;
      } else {
        moonbit_decref(_M0L3srcS17);
        moonbit_decref(_M0L3dstS16);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS25,
  int32_t _M0L11dst__offsetS27,
  moonbit_string_t* _M0L3srcS26,
  int32_t _M0L11src__offsetS28,
  int32_t _M0L3lenS30
) {
  int32_t _if__result_1211;
  #line 38 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS25 == _M0L3srcS26) {
    _if__result_1211 = _M0L11dst__offsetS27 < _M0L11src__offsetS28;
  } else {
    _if__result_1211 = 0;
  }
  if (_if__result_1211) {
    int32_t _M0L1iS29 = 0;
    while (1) {
      if (_M0L1iS29 < _M0L3lenS30) {
        int32_t _M0L6_2atmpS508 = _M0L11dst__offsetS27 + _M0L1iS29;
        int32_t _M0L6_2atmpS510 = _M0L11src__offsetS28 + _M0L1iS29;
        moonbit_string_t _M0L6_2atmpS1087;
        moonbit_string_t _M0L6_2atmpS509;
        moonbit_string_t _M0L6_2aoldS1086;
        int32_t _M0L6_2atmpS511;
        if (
          _M0L6_2atmpS510 < 0
          || _M0L6_2atmpS510 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1087 = (moonbit_string_t)_M0L3srcS26[_M0L6_2atmpS510];
        _M0L6_2atmpS509 = _M0L6_2atmpS1087;
        if (
          _M0L6_2atmpS508 < 0
          || _M0L6_2atmpS508 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1086 = (moonbit_string_t)_M0L3dstS25[_M0L6_2atmpS508];
        moonbit_incref(_M0L6_2atmpS509);
        moonbit_decref(_M0L6_2aoldS1086);
        _M0L3dstS25[_M0L6_2atmpS508] = _M0L6_2atmpS509;
        _M0L6_2atmpS511 = _M0L1iS29 + 1;
        _M0L1iS29 = _M0L6_2atmpS511;
        continue;
      } else {
        moonbit_decref(_M0L3srcS26);
        moonbit_decref(_M0L3dstS25);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS516 = _M0L3lenS30 - 1;
    int32_t _M0L1iS32 = _M0L6_2atmpS516;
    while (1) {
      if (_M0L1iS32 >= 0) {
        int32_t _M0L6_2atmpS512 = _M0L11dst__offsetS27 + _M0L1iS32;
        int32_t _M0L6_2atmpS514 = _M0L11src__offsetS28 + _M0L1iS32;
        moonbit_string_t _M0L6_2atmpS1089;
        moonbit_string_t _M0L6_2atmpS513;
        moonbit_string_t _M0L6_2aoldS1088;
        int32_t _M0L6_2atmpS515;
        if (
          _M0L6_2atmpS514 < 0
          || _M0L6_2atmpS514 >= Moonbit_array_length(_M0L3srcS26)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1089 = (moonbit_string_t)_M0L3srcS26[_M0L6_2atmpS514];
        _M0L6_2atmpS513 = _M0L6_2atmpS1089;
        if (
          _M0L6_2atmpS512 < 0
          || _M0L6_2atmpS512 >= Moonbit_array_length(_M0L3dstS25)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1088 = (moonbit_string_t)_M0L3dstS25[_M0L6_2atmpS512];
        moonbit_incref(_M0L6_2atmpS513);
        moonbit_decref(_M0L6_2aoldS1088);
        _M0L3dstS25[_M0L6_2atmpS512] = _M0L6_2atmpS513;
        _M0L6_2atmpS515 = _M0L1iS32 - 1;
        _M0L1iS32 = _M0L6_2atmpS515;
        continue;
      } else {
        moonbit_decref(_M0L3srcS26);
        moonbit_decref(_M0L3dstS25);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS34,
  int32_t _M0L11dst__offsetS36,
  struct _M0TUsiE** _M0L3srcS35,
  int32_t _M0L11src__offsetS37,
  int32_t _M0L3lenS39
) {
  int32_t _if__result_1214;
  #line 38 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS34 == _M0L3srcS35) {
    _if__result_1214 = _M0L11dst__offsetS36 < _M0L11src__offsetS37;
  } else {
    _if__result_1214 = 0;
  }
  if (_if__result_1214) {
    int32_t _M0L1iS38 = 0;
    while (1) {
      if (_M0L1iS38 < _M0L3lenS39) {
        int32_t _M0L6_2atmpS517 = _M0L11dst__offsetS36 + _M0L1iS38;
        int32_t _M0L6_2atmpS519 = _M0L11src__offsetS37 + _M0L1iS38;
        struct _M0TUsiE* _M0L6_2atmpS1091;
        struct _M0TUsiE* _M0L6_2atmpS518;
        struct _M0TUsiE* _M0L6_2aoldS1090;
        int32_t _M0L6_2atmpS520;
        if (
          _M0L6_2atmpS519 < 0
          || _M0L6_2atmpS519 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1091 = (struct _M0TUsiE*)_M0L3srcS35[_M0L6_2atmpS519];
        _M0L6_2atmpS518 = _M0L6_2atmpS1091;
        if (
          _M0L6_2atmpS517 < 0
          || _M0L6_2atmpS517 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1090 = (struct _M0TUsiE*)_M0L3dstS34[_M0L6_2atmpS517];
        if (_M0L6_2atmpS518) {
          moonbit_incref(_M0L6_2atmpS518);
        }
        if (_M0L6_2aoldS1090) {
          moonbit_decref(_M0L6_2aoldS1090);
        }
        _M0L3dstS34[_M0L6_2atmpS517] = _M0L6_2atmpS518;
        _M0L6_2atmpS520 = _M0L1iS38 + 1;
        _M0L1iS38 = _M0L6_2atmpS520;
        continue;
      } else {
        moonbit_decref(_M0L3srcS35);
        moonbit_decref(_M0L3dstS34);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS525 = _M0L3lenS39 - 1;
    int32_t _M0L1iS41 = _M0L6_2atmpS525;
    while (1) {
      if (_M0L1iS41 >= 0) {
        int32_t _M0L6_2atmpS521 = _M0L11dst__offsetS36 + _M0L1iS41;
        int32_t _M0L6_2atmpS523 = _M0L11src__offsetS37 + _M0L1iS41;
        struct _M0TUsiE* _M0L6_2atmpS1093;
        struct _M0TUsiE* _M0L6_2atmpS522;
        struct _M0TUsiE* _M0L6_2aoldS1092;
        int32_t _M0L6_2atmpS524;
        if (
          _M0L6_2atmpS523 < 0
          || _M0L6_2atmpS523 >= Moonbit_array_length(_M0L3srcS35)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1093 = (struct _M0TUsiE*)_M0L3srcS35[_M0L6_2atmpS523];
        _M0L6_2atmpS522 = _M0L6_2atmpS1093;
        if (
          _M0L6_2atmpS521 < 0
          || _M0L6_2atmpS521 >= Moonbit_array_length(_M0L3dstS34)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1092 = (struct _M0TUsiE*)_M0L3dstS34[_M0L6_2atmpS521];
        if (_M0L6_2atmpS522) {
          moonbit_incref(_M0L6_2atmpS522);
        }
        if (_M0L6_2aoldS1092) {
          moonbit_decref(_M0L6_2aoldS1092);
        }
        _M0L3dstS34[_M0L6_2atmpS521] = _M0L6_2atmpS522;
        _M0L6_2atmpS524 = _M0L1iS41 - 1;
        _M0L1iS41 = _M0L6_2atmpS524;
        continue;
      } else {
        moonbit_decref(_M0L3srcS35);
        moonbit_decref(_M0L3dstS34);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS10,
  moonbit_string_t _M0L3locS11
) {
  moonbit_string_t _M0L6_2atmpS488;
  moonbit_string_t _M0L6_2atmpS1096;
  moonbit_string_t _M0L6_2atmpS486;
  moonbit_string_t _M0L6_2atmpS487;
  moonbit_string_t _M0L6_2atmpS1095;
  moonbit_string_t _M0L6_2atmpS485;
  moonbit_string_t _M0L6_2atmpS1094;
  moonbit_string_t _M0L6_2atmpS484;
  #line 69 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  #line 73 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS488 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS10);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1096
  = moonbit_add_string(_M0L6_2atmpS488, (moonbit_string_t)moonbit_string_literal_22.data);
  moonbit_decref(_M0L6_2atmpS488);
  _M0L6_2atmpS486 = _M0L6_2atmpS1096;
  #line 74 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS487
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS11);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1095 = moonbit_add_string(_M0L6_2atmpS486, _M0L6_2atmpS487);
  moonbit_decref(_M0L6_2atmpS486);
  moonbit_decref(_M0L6_2atmpS487);
  _M0L6_2atmpS485 = _M0L6_2atmpS1095;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1094
  = moonbit_add_string(_M0L6_2atmpS485, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS485);
  _M0L6_2atmpS484 = _M0L6_2atmpS1094;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS484);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS12,
  moonbit_string_t _M0L3locS13
) {
  moonbit_string_t _M0L6_2atmpS493;
  moonbit_string_t _M0L6_2atmpS1099;
  moonbit_string_t _M0L6_2atmpS491;
  moonbit_string_t _M0L6_2atmpS492;
  moonbit_string_t _M0L6_2atmpS1098;
  moonbit_string_t _M0L6_2atmpS490;
  moonbit_string_t _M0L6_2atmpS1097;
  moonbit_string_t _M0L6_2atmpS489;
  #line 69 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  #line 73 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS493 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS12);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1099
  = moonbit_add_string(_M0L6_2atmpS493, (moonbit_string_t)moonbit_string_literal_22.data);
  moonbit_decref(_M0L6_2atmpS493);
  _M0L6_2atmpS491 = _M0L6_2atmpS1099;
  #line 74 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS492
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS13);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1098 = moonbit_add_string(_M0L6_2atmpS491, _M0L6_2atmpS492);
  moonbit_decref(_M0L6_2atmpS491);
  moonbit_decref(_M0L6_2atmpS492);
  _M0L6_2atmpS490 = _M0L6_2atmpS1098;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1097
  = moonbit_add_string(_M0L6_2atmpS490, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS490);
  _M0L6_2atmpS489 = _M0L6_2atmpS1097;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS489);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS14,
  moonbit_string_t _M0L3locS15
) {
  moonbit_string_t _M0L6_2atmpS498;
  moonbit_string_t _M0L6_2atmpS1102;
  moonbit_string_t _M0L6_2atmpS496;
  moonbit_string_t _M0L6_2atmpS497;
  moonbit_string_t _M0L6_2atmpS1101;
  moonbit_string_t _M0L6_2atmpS495;
  moonbit_string_t _M0L6_2atmpS1100;
  moonbit_string_t _M0L6_2atmpS494;
  #line 69 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  #line 73 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS498 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS14);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1102
  = moonbit_add_string(_M0L6_2atmpS498, (moonbit_string_t)moonbit_string_literal_22.data);
  moonbit_decref(_M0L6_2atmpS498);
  _M0L6_2atmpS496 = _M0L6_2atmpS1102;
  #line 74 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS497
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS15);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1101 = moonbit_add_string(_M0L6_2atmpS496, _M0L6_2atmpS497);
  moonbit_decref(_M0L6_2atmpS496);
  moonbit_decref(_M0L6_2atmpS497);
  _M0L6_2atmpS495 = _M0L6_2atmpS1101;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1100
  = moonbit_add_string(_M0L6_2atmpS495, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS495);
  _M0L6_2atmpS494 = _M0L6_2atmpS1100;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS494);
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S6,
  struct _M0TPB6Logger _M0L10_2ax__4934S9
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS7;
  moonbit_string_t _M0L8_2afieldS1103;
  int32_t _M0L6_2acntS1137;
  moonbit_string_t _M0L15_2a_2aarg__4935S8;
  #line 37 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2aFailureS7
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S6;
  _M0L8_2afieldS1103 = _M0L10_2aFailureS7->$0;
  _M0L6_2acntS1137 = Moonbit_object_header(_M0L10_2aFailureS7)->rc;
  if (_M0L6_2acntS1137 > 1) {
    int32_t _M0L11_2anew__cntS1138 = _M0L6_2acntS1137 - 1;
    Moonbit_object_header(_M0L10_2aFailureS7)->rc = _M0L11_2anew__cntS1138;
    moonbit_incref(_M0L8_2afieldS1103);
  } else if (_M0L6_2acntS1137 == 1) {
    #line 37 "/home/blem/.moon/lib/core/builtin/failure.mbt"
    moonbit_free(_M0L10_2aFailureS7);
  }
  _M0L15_2a_2aarg__4935S8 = _M0L8_2afieldS1103;
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_24.data);
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S9, _M0L15_2a_2aarg__4935S8);
  #line 37 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_25.data);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS5,
  moonbit_string_t _M0L3objS4
) {
  #line 150 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 151 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS4, _M0L4selfS5);
  return 0;
}

int32_t _M0FPC15abort5abortGiE(moonbit_string_t _M0L3msgS1) {
  #line 47 "/home/blem/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS1);
  moonbit_decref(_M0L3msgS1);
  #line 50 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

int32_t _M0FPC15abort5abortGuE(moonbit_string_t _M0L3msgS2) {
  #line 47 "/home/blem/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS2);
  moonbit_decref(_M0L3msgS2);
  #line 50 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
  return 0;
}

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS3
) {
  #line 47 "/home/blem/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS3);
  moonbit_decref(_M0L3msgS3);
  #line 50 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS461) {
  switch (Moonbit_object_tag(_M0L4_2aeS461)) {
    case 1: {
      moonbit_decref(_M0L4_2aeS461);
      return (moonbit_string_t)moonbit_string_literal_26.data;
      break;
    }
    
    case 3: {
      moonbit_decref(_M0L4_2aeS461);
      return (moonbit_string_t)moonbit_string_literal_27.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS461);
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS461);
      return (moonbit_string_t)moonbit_string_literal_28.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS461);
      return (moonbit_string_t)moonbit_string_literal_29.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS478,
  int32_t _M0L8_2aparamS477
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS476 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS478;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS476, _M0L8_2aparamS477);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS475,
  struct _M0TPC16string10StringView _M0L8_2aparamS474
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS473 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS475;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS473, _M0L8_2aparamS474);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS472,
  moonbit_string_t _M0L8_2aparamS469,
  int32_t _M0L8_2aparamS470,
  int32_t _M0L8_2aparamS471
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS468 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS472;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS468, _M0L8_2aparamS469, _M0L8_2aparamS470, _M0L8_2aparamS471);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS467,
  moonbit_string_t _M0L8_2aparamS466
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS465 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS467;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS465, _M0L8_2aparamS466);
  return 0;
}

void moonbit_init() {
  
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS483;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS455;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS456;
  int32_t _M0L7_2abindS457;
  int32_t _M0L2__S458;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS483
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS455
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS455)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS455->$0 = _M0L6_2atmpS483;
  _M0L12async__testsS455->$1 = 0;
  #line 397 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS456
  = _M0FP28bikallem4blit52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS457 = _M0L7_2abindS456->$1;
  _M0L2__S458 = 0;
  while (1) {
    if (_M0L2__S458 < _M0L7_2abindS457) {
      struct _M0TUsiE** _M0L8_2afieldS1107 = _M0L7_2abindS456->$0;
      struct _M0TUsiE** _M0L3bufS482 = _M0L8_2afieldS1107;
      struct _M0TUsiE* _M0L6_2atmpS1106 =
        (struct _M0TUsiE*)_M0L3bufS482[_M0L2__S458];
      struct _M0TUsiE* _M0L3argS459 = _M0L6_2atmpS1106;
      moonbit_string_t _M0L8_2afieldS1105 = _M0L3argS459->$0;
      moonbit_string_t _M0L6_2atmpS479 = _M0L8_2afieldS1105;
      int32_t _M0L8_2afieldS1104 = _M0L3argS459->$1;
      int32_t _M0L6_2atmpS480 = _M0L8_2afieldS1104;
      int32_t _M0L6_2atmpS481;
      moonbit_incref(_M0L6_2atmpS479);
      moonbit_incref(_M0L12async__testsS455);
      #line 398 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
      _M0FP28bikallem4blit44moonbit__test__driver__internal__do__execute(_M0L12async__testsS455, _M0L6_2atmpS479, _M0L6_2atmpS480);
      _M0L6_2atmpS481 = _M0L2__S458 + 1;
      _M0L2__S458 = _M0L6_2atmpS481;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS456);
    }
    break;
  }
  #line 400 "/home/blem/projects/blit/src/__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP28bikallem4blit28MoonBit__Async__Test__Driver17run__async__testsGRP28bikallem4blit34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS455);
  return 0;
}