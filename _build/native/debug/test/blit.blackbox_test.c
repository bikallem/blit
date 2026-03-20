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
struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0R57ArrayView_3a_3aiter_7c_5bByte_5d_7c_2eanon__u1571__l570__;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0DTPC16result6ResultGbRP28bikallem20blit__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TWEOy;

struct _M0DTPC15error5Error103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB6Logger;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0DTPC15error5Error101bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPB4Show;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0DTPC16result6ResultGbRP28bikallem20blit__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TPC15bytes9BytesView;

struct _M0BTPB4Show;

struct _M0R104_24bikallem_2fblit__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c836;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2022__l441__;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1559__l570__;

struct _M0TPB5ArrayGsE;

struct _M0TPB9ArrayViewGyE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0KTPB4ShowTAy;

struct _M0DTPC16result6ResultGuRPB7FailureE3Err;

struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2018__l442__;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $1;
  struct _M0TUWEuQRPC15error5ErrorNsE* $5;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
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

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $5;
  
};

struct _M0R57ArrayView_3a_3aiter_7c_5bByte_5d_7c_2eanon__u1571__l570__ {
  int32_t(* code)(struct _M0TWEOy*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_bytes_t $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
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

struct _M0TPB5ArrayGORPB9SourceLocE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $1;
  moonbit_string_t $4;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0TPB5ArrayGUsiEE {
  int32_t $1;
  struct _M0TUsiE** $0;
  
};

struct _M0DTPC16result6ResultGbRP28bikallem20blit__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0TWEOy {
  int32_t(* code)(struct _M0TWEOy*);
  
};

struct _M0DTPC15error5Error103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok {
  int32_t $0;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0DTPC15error5Error101bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TPB4Show {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
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

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** $0;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0DTPC16result6ResultGbRP28bikallem20blit__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0TUiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  struct _M0TUWEuQRPC15error5ErrorNsE* $1;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPC15bytes9BytesView {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
};

struct _M0BTPB4Show {
  int32_t(* $method_0)(void*, struct _M0TPB6Logger);
  moonbit_string_t(* $method_1)(void*);
  
};

struct _M0R104_24bikallem_2fblit__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c836 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
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

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  
};

struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2022__l441__ {
  int32_t(* code)(struct _M0TWEOy*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1559__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TPB5ArrayGsE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TPB9ArrayViewGyE {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
};

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0KTPB4ShowTAy {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGuRPB7FailureE3Err {
  void* $0;
  
};

struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2018__l442__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok {
  int32_t $0;
  
};

struct _M0TPC13ref3RefGiE {
  int32_t $0;
  
};

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE {
  int32_t $1;
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** $0;
  
};

struct _M0TUWEuQRPC15error5ErrorNsE {
  struct _M0TWEuQRPC15error5Error* $0;
  moonbit_string_t* $1;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__11_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__7_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__5_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__6_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__12_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__9_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__8_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__10_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS845(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS836(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP28bikallem20blit__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP28bikallem20blit__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testC2022l441(
  struct _M0TWEOy*
);

int32_t _M0IP28bikallem20blit__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testC2018l442(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP28bikallem20blit__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOy*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS768(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS763(
  int32_t
);

moonbit_string_t _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS756(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S750(
  int32_t,
  moonbit_string_t
);

#define _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP28bikallem20blit__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP28bikallem20blit__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test40____test__626c69745f746573742e6d6274__12(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test40____test__626c69745f746573742e6d6274__11(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test40____test__626c69745f746573742e6d6274__10(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__9(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__8(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__7(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__6(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__5(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__4(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__3(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__2(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__0(
  
);

int32_t _M0FP28bikallem4blit15blit__bytesview(
  moonbit_bytes_t,
  int32_t,
  struct _M0TPC15bytes9BytesView,
  int32_t,
  int32_t
);

#define _M0FP28bikallem4blit12make__uninit bikallem_blit_make_uninit

#define _M0FP28bikallem4blit11fill__bytes bikallem_blit_fill_bytes

#define _M0FP28bikallem4blit11blit__bytes bikallem_blit_blit_fixed_array

#define _M0FP28bikallem4blit18blit__fixed__array bikallem_blit_blit_fixed_array

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t,
  struct _M0TPB6Logger
);

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr*,
  struct _M0TPB6Logger
);

moonbit_bytes_t _M0MPC15bytes9BytesView4data(struct _M0TPC15bytes9BytesView);

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

int32_t _M0FPB7printlnGsE(moonbit_string_t);

int32_t _M0IPC13int3IntPB4Hash13hash__combine(int32_t, struct _M0TPB6Hasher*);

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t,
  struct _M0TPB6Hasher*
);

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher*,
  moonbit_string_t
);

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t,
  int32_t
);

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE
);

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TUWEuQRPC15error5ErrorNsE*
);

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  moonbit_string_t,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TUWEuQRPC15error5ErrorNsE*,
  int32_t
);

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  int32_t,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*,
  int32_t
);

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*,
  int32_t,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*,
  int32_t,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t
);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

int32_t _M0IPC15array10FixedArrayPB4Show6outputGyE(
  moonbit_bytes_t,
  struct _M0TPB6Logger
);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOy* _M0MPC15array10FixedArray4iterGyE(moonbit_bytes_t);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

struct _M0TWEOy* _M0MPC15array9ArrayView4iterGyE(struct _M0TPB9ArrayViewGyE);

int32_t _M0MPC15array9ArrayView4iterGyEC1571l570(struct _M0TWEOy*);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1559l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC14byte4BytePB4Show6output(int32_t, struct _M0TPB6Logger);

moonbit_string_t _M0MPC14byte4Byte10to__string(int32_t);

moonbit_string_t _M0FPB8alphabet(int32_t);

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

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t,
  int32_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE
);

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE
);

int32_t _M0MPC15array9ArrayView6lengthGsE(struct _M0TPB9ArrayViewGsE);

int32_t _M0MPC15array9ArrayView6lengthGyE(struct _M0TPB9ArrayViewGyE);

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

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView
);

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView,
  struct _M0TPB6Logger
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

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs*);

struct _M0TWEOy* _M0MPB4Iter3newGyE(struct _M0TWEOy*);

struct moonbit_result_0 _M0FPB10assert__eqGiE(
  int32_t,
  int32_t,
  moonbit_string_t,
  moonbit_string_t
);

struct moonbit_result_0 _M0FPB4failGuE(moonbit_string_t, moonbit_string_t);

moonbit_string_t _M0FPB13debug__stringGiE(int32_t);

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

int32_t _M0MPB6Logger19write__iter_2einnerGyE(
  struct _M0TPB6Logger,
  struct _M0TWEOy*,
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs*);

int32_t _M0MPB4Iter4nextGyE(struct _M0TWEOy*);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(int32_t);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t
);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGAyE(
  moonbit_bytes_t
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

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t);

int32_t _M0IP016_24default__implPB4Hash4hashGsE(moonbit_string_t);

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t);

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t);

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher*);

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher*);

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MPB6Hasher7combineGiE(struct _M0TPB6Hasher*, int32_t);

int32_t _M0MPB6Hasher7combineGsE(struct _M0TPB6Hasher*, moonbit_string_t);

int32_t _M0MPB6Hasher12combine__int(struct _M0TPB6Hasher*, int32_t);

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show,
  moonbit_string_t,
  moonbit_string_t,
  struct _M0TPB5ArrayGORPB9SourceLocE*
);

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE*
);

moonbit_string_t _M0MPB9SourceLoc16to__json__string(moonbit_string_t);

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr*
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

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(moonbit_string_t);

moonbit_string_t _M0MPC15array5Array2atGsE(struct _M0TPB5ArrayGsE*, int32_t);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
);

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t);

moonbit_string_t _M0FPB33base64__encode__string__codepoint(moonbit_string_t);

int32_t _M0MPC16string6String16unsafe__char__at(moonbit_string_t, int32_t);

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t);

int32_t _M0FPB32code__point__of__surrogate__pair(int32_t, int32_t);

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t,
  int32_t,
  int64_t
);

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t);

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t);

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t);

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

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0FPB5abortGsE(moonbit_string_t, moonbit_string_t);

int32_t _M0MPB6Hasher13combine__uint(struct _M0TPB6Hasher*, uint32_t);

int32_t _M0MPB6Hasher8consume4(struct _M0TPB6Hasher*, uint32_t);

uint32_t _M0FPB4rotl(uint32_t, int32_t);

int32_t _M0IPB7FailurePB4Show6output(void*, struct _M0TPB6Logger);

int32_t _M0MPB6Logger13write__objectGyE(struct _M0TPB6Logger, int32_t);

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger,
  moonbit_string_t
);

int32_t _M0FPC15abort5abortGiE(moonbit_string_t);

int32_t _M0FPC15abort5abortGuE(moonbit_string_t);

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
  moonbit_string_t
);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

moonbit_string_t _M0FPC15abort5abortGsE(moonbit_string_t);

moonbit_string_t _M0FP15Error10to__string(void*);

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGAyE(
  void*
);

int32_t _M0IPC15array10FixedArrayPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGyE(
  void*,
  struct _M0TPB6Logger
);

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

int32_t bikallem_blit_fill_bytes(moonbit_bytes_t, int32_t, int32_t, int32_t);

moonbit_bytes_t bikallem_blit_make_uninit(int32_t);

int32_t bikallem_blit_blit_fixed_array(
  moonbit_bytes_t,
  int32_t,
  moonbit_bytes_t,
  int32_t,
  int32_t
);

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_66 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 55, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_131 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[40]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 39), 
    98, 108, 105, 116, 95, 102, 105, 120, 101, 100, 95, 97, 114, 114, 
    97, 121, 32, 99, 111, 112, 105, 101, 115, 32, 98, 121, 116, 101, 
    115, 32, 99, 111, 114, 114, 101, 99, 116, 108, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
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
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 50, 58, 49, 49, 45, 50, 50, 58, 49, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_60 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_59 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[46]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 45), 
    91, 98, 39, 92, 120, 54, 49, 39, 44, 32, 98, 39, 92, 120, 54, 49, 
    39, 44, 32, 98, 39, 92, 120, 54, 50, 39, 44, 32, 98, 39, 92, 120, 
    54, 51, 39, 44, 32, 98, 39, 92, 120, 54, 53, 39, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    55, 51, 58, 50, 52, 45, 55, 51, 58, 55, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    56, 49, 58, 49, 49, 45, 56, 49, 58, 49, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_113 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_63 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    57, 55, 58, 49, 49, 45, 57, 55, 58, 49, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_121 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    98, 108, 105, 116, 95, 102, 105, 120, 101, 100, 95, 97, 114, 114, 
    97, 121, 32, 111, 118, 101, 114, 108, 97, 112, 112, 105, 110, 103, 
    32, 102, 111, 114, 119, 97, 114, 100, 32, 40, 115, 97, 109, 101, 
    32, 97, 114, 114, 97, 121, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 56, 
    48, 58, 53, 45, 49, 56, 48, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_132 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    98, 108, 105, 116, 95, 110, 97, 116, 105, 118, 101, 46, 109, 98, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_69 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 65, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[38]; 
} const moonbit_string_literal_120 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 37), 
    98, 108, 105, 116, 95, 102, 105, 120, 101, 100, 95, 97, 114, 114, 
    97, 121, 32, 122, 101, 114, 111, 32, 108, 101, 110, 103, 116, 104, 
    32, 105, 115, 32, 110, 111, 45, 111, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 57, 58, 49, 49, 45, 50, 57, 58, 49, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[28]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 27), 
    91, 98, 39, 92, 120, 54, 51, 39, 44, 32, 98, 39, 92, 120, 54, 52, 
    39, 44, 32, 98, 39, 92, 120, 54, 53, 39, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 55, 48, 58, 53, 45, 55, 
    48, 58, 54, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 52, 58, 51, 45, 49, 52, 58, 55, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 58, 50, 52, 45, 54, 58, 55, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[44]; 
} const moonbit_string_literal_125 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 43), 
    109, 97, 107, 101, 95, 117, 110, 105, 110, 105, 116, 32, 114, 101, 
    116, 117, 114, 110, 115, 32, 97, 114, 114, 97, 121, 32, 111, 102, 
    32, 99, 111, 114, 114, 101, 99, 116, 32, 108, 101, 110, 103, 116, 
    104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[28]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 27), 
    91, 98, 39, 92, 120, 48, 48, 39, 44, 32, 98, 39, 92, 120, 48, 48, 
    39, 44, 32, 98, 39, 92, 120, 48, 48, 39, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[28]; 
} const moonbit_string_literal_128 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 27), 
    98, 108, 105, 116, 95, 98, 121, 116, 101, 115, 118, 105, 101, 119, 
    32, 119, 105, 116, 104, 32, 111, 102, 102, 115, 101, 116, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 54, 58, 49, 49, 45, 51, 54, 58, 49, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    56, 49, 58, 50, 52, 45, 56, 49, 58, 55, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_64 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 54, 58, 51, 45, 51, 54, 58, 55, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    109, 97, 107, 101, 95, 117, 110, 105, 110, 105, 116, 32, 122, 101, 
    114, 111, 32, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    91, 98, 39, 92, 120, 48, 48, 39, 44, 32, 98, 39, 92, 120, 48, 48, 
    39, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[32]; 
} const moonbit_string_literal_124 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 31), 
    102, 105, 108, 108, 95, 98, 121, 116, 101, 115, 32, 122, 101, 114, 
    111, 32, 108, 101, 110, 103, 116, 104, 32, 105, 115, 32, 110, 111, 
    45, 111, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 58, 49, 49, 45, 54, 58, 49, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 52, 58, 53, 45, 52, 52, 58, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    91, 98, 39, 92, 120, 48, 48, 39, 44, 32, 98, 39, 92, 120, 70, 70, 
    39, 44, 32, 98, 39, 92, 120, 70, 70, 39, 44, 32, 98, 39, 92, 120, 
    70, 70, 39, 44, 32, 98, 39, 92, 120, 70, 70, 39, 44, 32, 98, 39, 
    92, 120, 48, 48, 39, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_74 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 70, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_62 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 53, 58, 51, 45, 54, 53, 58, 50, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_123 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    102, 105, 108, 108, 95, 98, 121, 116, 101, 115, 32, 102, 105, 108, 
    108, 115, 32, 114, 101, 103, 105, 111, 110, 32, 119, 105, 116, 104, 
    32, 118, 97, 108, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 52, 58, 49, 49, 45, 49, 52, 58, 49, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    56, 57, 58, 50, 52, 45, 56, 57, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    32, 33, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[38]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 37), 
    98, 108, 105, 116, 95, 98, 121, 116, 101, 115, 118, 105, 101, 119, 
    32, 99, 111, 112, 105, 101, 115, 32, 98, 121, 116, 101, 115, 32, 
    99, 111, 114, 114, 101, 99, 116, 108, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_58 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 39, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 50, 58, 50, 52, 45, 50, 50, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[36]; 
} const moonbit_string_literal_130 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 35), 
    98, 108, 105, 116, 95, 98, 121, 116, 101, 115, 118, 105, 101, 119, 
    32, 122, 101, 114, 111, 32, 108, 101, 110, 103, 116, 104, 32, 105, 
    115, 32, 110, 111, 45, 111, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    98, 39, 92, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    51, 54, 58, 50, 52, 45, 51, 54, 58, 55, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[46]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 45), 
    91, 98, 39, 92, 120, 54, 49, 39, 44, 32, 98, 39, 92, 120, 54, 50, 
    39, 44, 32, 98, 39, 92, 120, 54, 51, 39, 44, 32, 98, 39, 92, 120, 
    54, 52, 39, 44, 32, 98, 39, 92, 120, 54, 53, 39, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 46, 109, 98, 116, 58, 49, 56, 48, 58, 49, 48, 45, 49, 56, 
    48, 58, 50, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_61 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    54, 58, 51, 45, 54, 58, 55, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    32, 70, 65, 73, 76, 69, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[96]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 95), 
    98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 95, 
    98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 
    84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 57, 58, 51, 45, 53, 57, 58, 51, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[46]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 45), 
    91, 98, 39, 92, 120, 48, 48, 39, 44, 32, 98, 39, 92, 120, 54, 51, 
    39, 44, 32, 98, 39, 92, 120, 54, 52, 39, 44, 32, 98, 39, 92, 120, 
    54, 53, 39, 44, 32, 98, 39, 92, 120, 48, 48, 39, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 51, 58, 51, 45, 53, 51, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_73 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 69, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_65 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 54, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_54 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 91, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_111 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    49, 52, 58, 50, 52, 45, 49, 52, 58, 55, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 53, 58, 49, 51, 45, 52, 53, 58, 55, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_133 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    98, 108, 105, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_112 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_129 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 40), 
    98, 108, 105, 116, 95, 98, 121, 116, 101, 115, 118, 105, 101, 119, 
    32, 119, 105, 116, 104, 32, 115, 114, 99, 95, 111, 102, 102, 115, 
    101, 116, 32, 105, 110, 116, 111, 32, 118, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    56, 49, 58, 51, 45, 56, 49, 58, 55, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    57, 55, 58, 50, 52, 45, 57, 55, 58, 52, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    55, 51, 58, 49, 49, 45, 55, 51, 58, 49, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    57, 55, 58, 51, 45, 57, 55, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_68 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 57, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[46]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 45), 
    91, 98, 39, 92, 120, 54, 51, 39, 44, 32, 98, 39, 92, 120, 54, 52, 
    39, 44, 32, 98, 39, 92, 120, 54, 53, 39, 44, 32, 98, 39, 92, 120, 
    54, 52, 39, 44, 32, 98, 39, 92, 120, 54, 53, 39, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[30]; 
} const moonbit_string_literal_119 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 29), 
    98, 108, 105, 116, 95, 102, 105, 120, 101, 100, 95, 97, 114, 114, 
    97, 121, 32, 119, 105, 116, 104, 32, 111, 102, 102, 115, 101, 116, 
    115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    56, 57, 58, 49, 49, 45, 56, 57, 58, 49, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_85 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 96, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[94]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 93), 
    98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 95, 
    98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 
    114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 
    116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 
    108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_72 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 68, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    105, 109, 112, 111, 115, 115, 105, 98, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 50, 58, 51, 45, 50, 50, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_122 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    98, 108, 105, 116, 95, 102, 105, 120, 101, 100, 95, 97, 114, 114, 
    97, 121, 32, 111, 118, 101, 114, 108, 97, 112, 112, 105, 110, 103, 
    32, 98, 97, 99, 107, 119, 97, 114, 100, 32, 40, 115, 97, 109, 101, 
    32, 97, 114, 114, 97, 121, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 51, 58, 50, 52, 45, 53, 51, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    53, 51, 58, 49, 49, 45, 53, 51, 58, 49, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 57, 58, 51, 45, 50, 57, 58, 55, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_71 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 67, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_55 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 93, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    55, 51, 58, 51, 45, 55, 51, 58, 55, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_91 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_67 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 56, 49, 58, 57, 45, 56, 
    49, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    56, 57, 58, 51, 45, 56, 57, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    52, 51, 58, 51, 45, 52, 54, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_70 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 66, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[55]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 54), 
    64, 98, 105, 107, 97, 108, 108, 101, 109, 47, 98, 108, 105, 116, 
    95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 
    98, 108, 105, 116, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 
    50, 57, 58, 50, 52, 45, 50, 57, 58, 55, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint8_t const data[3]; 
} const moonbit_bytes_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 2), 
    97, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint8_t const data[6]; 
} const moonbit_bytes_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 5), 
    97, 98, 99, 100, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint8_t const data[65]; 
} const moonbit_bytes_literal_0 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 64), 
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 
    82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98, 99, 100, 101, 102, 103, 
    104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 
    117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 
    57, 43, 47, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__4_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__4_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__3_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__3_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__2_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__11_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__11_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__6_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__6_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__8_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__8_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__12_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__12_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__10_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__10_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__7_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__7_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__9_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__9_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__5_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__5_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS845$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS845
  };

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test46____test__626c69745f746573742e6d6274__10_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__10_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__8_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__8_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__9_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__9_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test46____test__626c69745f746573742e6d6274__12_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__12_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__6_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__6_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__5_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__5_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__3_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__3_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__7_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__7_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__4_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__4_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test46____test__626c69745f746573742e6d6274__11_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__11_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__1_2edyncall$closure.data;

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

struct { int32_t rc; uint32_t meta; struct _M0BTPB4Show data; 
} _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC15array10FixedArrayPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGyE,
       .$method_1 = _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGAyE}
  };

struct _M0BTPB4Show* _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

moonbit_bytes_t _M0FPB14base64__encodeN6base64S1657 =
  (moonbit_bytes_t)moonbit_bytes_literal_0.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP28bikallem20blit__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2065
) {
  return _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__1();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__11_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2064
) {
  return _M0FP28bikallem20blit__blackbox__test40____test__626c69745f746573742e6d6274__11();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2063
) {
  return _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__4();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__7_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2062
) {
  return _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__7();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2061
) {
  return _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__3();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__5_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2060
) {
  return _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__5();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__6_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2059
) {
  return _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__6();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__12_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2058
) {
  return _M0FP28bikallem20blit__blackbox__test40____test__626c69745f746573742e6d6274__12();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__9_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2057
) {
  return _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__9();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__8_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2056
) {
  return _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__8();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2055
) {
  return _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__2();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test50____test__626c69745f746573742e6d6274__10_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2054
) {
  return _M0FP28bikallem20blit__blackbox__test40____test__626c69745f746573742e6d6274__10();
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test49____test__626c69745f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2053
) {
  return _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__0();
}

int32_t _M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS866,
  moonbit_string_t _M0L8filenameS841,
  int32_t _M0L5indexS844
) {
  struct _M0R104_24bikallem_2fblit__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c836* _closure_2359;
  struct _M0TWssbEu* _M0L14handle__resultS836;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS845;
  void* _M0L11_2atry__errS860;
  struct moonbit_result_0 _tmp_2361;
  int32_t _handle__error__result_2362;
  int32_t _M0L6_2atmpS2041;
  void* _M0L3errS861;
  moonbit_string_t _M0L4nameS863;
  struct _M0DTPC15error5Error103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS864;
  moonbit_string_t _M0L8_2afieldS2066;
  int32_t _M0L6_2acntS2293;
  moonbit_string_t _M0L7_2anameS865;
  #line 540 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS841);
  _closure_2359
  = (struct _M0R104_24bikallem_2fblit__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c836*)moonbit_malloc(sizeof(struct _M0R104_24bikallem_2fblit__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c836));
  Moonbit_object_header(_closure_2359)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R104_24bikallem_2fblit__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c836, $1) >> 2, 1, 0);
  _closure_2359->code
  = &_M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS836;
  _closure_2359->$0 = _M0L5indexS844;
  _closure_2359->$1 = _M0L8filenameS841;
  _M0L14handle__resultS836 = (struct _M0TWssbEu*)_closure_2359;
  _M0L17error__to__stringS845
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS845$closure.data;
  moonbit_incref(_M0L12async__testsS866);
  moonbit_incref(_M0L17error__to__stringS845);
  moonbit_incref(_M0L8filenameS841);
  moonbit_incref(_M0L14handle__resultS836);
  #line 574 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _tmp_2361
  = _M0IP28bikallem20blit__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS866, _M0L8filenameS841, _M0L5indexS844, _M0L14handle__resultS836, _M0L17error__to__stringS845);
  if (_tmp_2361.tag) {
    int32_t const _M0L5_2aokS2050 = _tmp_2361.data.ok;
    _handle__error__result_2362 = _M0L5_2aokS2050;
  } else {
    void* const _M0L6_2aerrS2051 = _tmp_2361.data.err;
    moonbit_decref(_M0L12async__testsS866);
    moonbit_decref(_M0L17error__to__stringS845);
    moonbit_decref(_M0L8filenameS841);
    _M0L11_2atry__errS860 = _M0L6_2aerrS2051;
    goto join_859;
  }
  if (_handle__error__result_2362) {
    moonbit_decref(_M0L12async__testsS866);
    moonbit_decref(_M0L17error__to__stringS845);
    moonbit_decref(_M0L8filenameS841);
    _M0L6_2atmpS2041 = 1;
  } else {
    struct moonbit_result_0 _tmp_2363;
    int32_t _handle__error__result_2364;
    moonbit_incref(_M0L12async__testsS866);
    moonbit_incref(_M0L17error__to__stringS845);
    moonbit_incref(_M0L8filenameS841);
    moonbit_incref(_M0L14handle__resultS836);
    #line 577 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
    _tmp_2363
    = _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS866, _M0L8filenameS841, _M0L5indexS844, _M0L14handle__resultS836, _M0L17error__to__stringS845);
    if (_tmp_2363.tag) {
      int32_t const _M0L5_2aokS2048 = _tmp_2363.data.ok;
      _handle__error__result_2364 = _M0L5_2aokS2048;
    } else {
      void* const _M0L6_2aerrS2049 = _tmp_2363.data.err;
      moonbit_decref(_M0L12async__testsS866);
      moonbit_decref(_M0L17error__to__stringS845);
      moonbit_decref(_M0L8filenameS841);
      _M0L11_2atry__errS860 = _M0L6_2aerrS2049;
      goto join_859;
    }
    if (_handle__error__result_2364) {
      moonbit_decref(_M0L12async__testsS866);
      moonbit_decref(_M0L17error__to__stringS845);
      moonbit_decref(_M0L8filenameS841);
      _M0L6_2atmpS2041 = 1;
    } else {
      struct moonbit_result_0 _tmp_2365;
      int32_t _handle__error__result_2366;
      moonbit_incref(_M0L12async__testsS866);
      moonbit_incref(_M0L17error__to__stringS845);
      moonbit_incref(_M0L8filenameS841);
      moonbit_incref(_M0L14handle__resultS836);
      #line 580 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _tmp_2365
      = _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS866, _M0L8filenameS841, _M0L5indexS844, _M0L14handle__resultS836, _M0L17error__to__stringS845);
      if (_tmp_2365.tag) {
        int32_t const _M0L5_2aokS2046 = _tmp_2365.data.ok;
        _handle__error__result_2366 = _M0L5_2aokS2046;
      } else {
        void* const _M0L6_2aerrS2047 = _tmp_2365.data.err;
        moonbit_decref(_M0L12async__testsS866);
        moonbit_decref(_M0L17error__to__stringS845);
        moonbit_decref(_M0L8filenameS841);
        _M0L11_2atry__errS860 = _M0L6_2aerrS2047;
        goto join_859;
      }
      if (_handle__error__result_2366) {
        moonbit_decref(_M0L12async__testsS866);
        moonbit_decref(_M0L17error__to__stringS845);
        moonbit_decref(_M0L8filenameS841);
        _M0L6_2atmpS2041 = 1;
      } else {
        struct moonbit_result_0 _tmp_2367;
        int32_t _handle__error__result_2368;
        moonbit_incref(_M0L12async__testsS866);
        moonbit_incref(_M0L17error__to__stringS845);
        moonbit_incref(_M0L8filenameS841);
        moonbit_incref(_M0L14handle__resultS836);
        #line 583 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        _tmp_2367
        = _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS866, _M0L8filenameS841, _M0L5indexS844, _M0L14handle__resultS836, _M0L17error__to__stringS845);
        if (_tmp_2367.tag) {
          int32_t const _M0L5_2aokS2044 = _tmp_2367.data.ok;
          _handle__error__result_2368 = _M0L5_2aokS2044;
        } else {
          void* const _M0L6_2aerrS2045 = _tmp_2367.data.err;
          moonbit_decref(_M0L12async__testsS866);
          moonbit_decref(_M0L17error__to__stringS845);
          moonbit_decref(_M0L8filenameS841);
          _M0L11_2atry__errS860 = _M0L6_2aerrS2045;
          goto join_859;
        }
        if (_handle__error__result_2368) {
          moonbit_decref(_M0L12async__testsS866);
          moonbit_decref(_M0L17error__to__stringS845);
          moonbit_decref(_M0L8filenameS841);
          _M0L6_2atmpS2041 = 1;
        } else {
          struct moonbit_result_0 _tmp_2369;
          moonbit_incref(_M0L14handle__resultS836);
          #line 586 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
          _tmp_2369
          = _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS866, _M0L8filenameS841, _M0L5indexS844, _M0L14handle__resultS836, _M0L17error__to__stringS845);
          if (_tmp_2369.tag) {
            int32_t const _M0L5_2aokS2042 = _tmp_2369.data.ok;
            _M0L6_2atmpS2041 = _M0L5_2aokS2042;
          } else {
            void* const _M0L6_2aerrS2043 = _tmp_2369.data.err;
            _M0L11_2atry__errS860 = _M0L6_2aerrS2043;
            goto join_859;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS2041) {
    void* _M0L103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2052 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2052)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2052)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS860
    = _M0L103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2052;
    goto join_859;
  } else {
    moonbit_decref(_M0L14handle__resultS836);
  }
  goto joinlet_2360;
  join_859:;
  _M0L3errS861 = _M0L11_2atry__errS860;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS864
  = (struct _M0DTPC15error5Error103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS861;
  _M0L8_2afieldS2066 = _M0L36_2aMoonBitTestDriverInternalSkipTestS864->$0;
  _M0L6_2acntS2293
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS864)->rc;
  if (_M0L6_2acntS2293 > 1) {
    int32_t _M0L11_2anew__cntS2294 = _M0L6_2acntS2293 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS864)->rc
    = _M0L11_2anew__cntS2294;
    moonbit_incref(_M0L8_2afieldS2066);
  } else if (_M0L6_2acntS2293 == 1) {
    #line 593 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS864);
  }
  _M0L7_2anameS865 = _M0L8_2afieldS2066;
  _M0L4nameS863 = _M0L7_2anameS865;
  goto join_862;
  goto joinlet_2370;
  join_862:;
  #line 594 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS836(_M0L14handle__resultS836, _M0L4nameS863, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_2370:;
  joinlet_2360:;
  return 0;
}

moonbit_string_t _M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS845(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS2040,
  void* _M0L3errS846
) {
  void* _M0L1eS848;
  moonbit_string_t _M0L1eS850;
  #line 563 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS2040);
  switch (Moonbit_object_tag(_M0L3errS846)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS851 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS846;
      moonbit_string_t _M0L8_2afieldS2067 = _M0L10_2aFailureS851->$0;
      int32_t _M0L6_2acntS2295 =
        Moonbit_object_header(_M0L10_2aFailureS851)->rc;
      moonbit_string_t _M0L4_2aeS852;
      if (_M0L6_2acntS2295 > 1) {
        int32_t _M0L11_2anew__cntS2296 = _M0L6_2acntS2295 - 1;
        Moonbit_object_header(_M0L10_2aFailureS851)->rc
        = _M0L11_2anew__cntS2296;
        moonbit_incref(_M0L8_2afieldS2067);
      } else if (_M0L6_2acntS2295 == 1) {
        #line 564 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS851);
      }
      _M0L4_2aeS852 = _M0L8_2afieldS2067;
      _M0L1eS850 = _M0L4_2aeS852;
      goto join_849;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS853 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS846;
      moonbit_string_t _M0L8_2afieldS2068 = _M0L15_2aInspectErrorS853->$0;
      int32_t _M0L6_2acntS2297 =
        Moonbit_object_header(_M0L15_2aInspectErrorS853)->rc;
      moonbit_string_t _M0L4_2aeS854;
      if (_M0L6_2acntS2297 > 1) {
        int32_t _M0L11_2anew__cntS2298 = _M0L6_2acntS2297 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS853)->rc
        = _M0L11_2anew__cntS2298;
        moonbit_incref(_M0L8_2afieldS2068);
      } else if (_M0L6_2acntS2297 == 1) {
        #line 564 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS853);
      }
      _M0L4_2aeS854 = _M0L8_2afieldS2068;
      _M0L1eS850 = _M0L4_2aeS854;
      goto join_849;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS855 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS846;
      moonbit_string_t _M0L8_2afieldS2069 = _M0L16_2aSnapshotErrorS855->$0;
      int32_t _M0L6_2acntS2299 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS855)->rc;
      moonbit_string_t _M0L4_2aeS856;
      if (_M0L6_2acntS2299 > 1) {
        int32_t _M0L11_2anew__cntS2300 = _M0L6_2acntS2299 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS855)->rc
        = _M0L11_2anew__cntS2300;
        moonbit_incref(_M0L8_2afieldS2069);
      } else if (_M0L6_2acntS2299 == 1) {
        #line 564 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS855);
      }
      _M0L4_2aeS856 = _M0L8_2afieldS2069;
      _M0L1eS850 = _M0L4_2aeS856;
      goto join_849;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error101bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS857 =
        (struct _M0DTPC15error5Error101bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS846;
      moonbit_string_t _M0L8_2afieldS2070 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS857->$0;
      int32_t _M0L6_2acntS2301 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS857)->rc;
      moonbit_string_t _M0L4_2aeS858;
      if (_M0L6_2acntS2301 > 1) {
        int32_t _M0L11_2anew__cntS2302 = _M0L6_2acntS2301 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS857)->rc
        = _M0L11_2anew__cntS2302;
        moonbit_incref(_M0L8_2afieldS2070);
      } else if (_M0L6_2acntS2301 == 1) {
        #line 564 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS857);
      }
      _M0L4_2aeS858 = _M0L8_2afieldS2070;
      _M0L1eS850 = _M0L4_2aeS858;
      goto join_849;
      break;
    }
    default: {
      _M0L1eS848 = _M0L3errS846;
      goto join_847;
      break;
    }
  }
  join_849:;
  return _M0L1eS850;
  join_847:;
  #line 569 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS848);
}

int32_t _M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS836(
  struct _M0TWssbEu* _M0L6_2aenvS2026,
  moonbit_string_t _M0L8testnameS837,
  moonbit_string_t _M0L7messageS838,
  int32_t _M0L7skippedS839
) {
  struct _M0R104_24bikallem_2fblit__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c836* _M0L14_2acasted__envS2027;
  moonbit_string_t _M0L8_2afieldS2080;
  moonbit_string_t _M0L8filenameS841;
  int32_t _M0L8_2afieldS2079;
  int32_t _M0L6_2acntS2303;
  int32_t _M0L5indexS844;
  int32_t _if__result_2373;
  moonbit_string_t _M0L10file__nameS840;
  moonbit_string_t _M0L10test__nameS842;
  moonbit_string_t _M0L7messageS843;
  moonbit_string_t _M0L6_2atmpS2039;
  moonbit_string_t _M0L6_2atmpS2078;
  moonbit_string_t _M0L6_2atmpS2038;
  moonbit_string_t _M0L6_2atmpS2077;
  moonbit_string_t _M0L6_2atmpS2036;
  moonbit_string_t _M0L6_2atmpS2037;
  moonbit_string_t _M0L6_2atmpS2076;
  moonbit_string_t _M0L6_2atmpS2035;
  moonbit_string_t _M0L6_2atmpS2075;
  moonbit_string_t _M0L6_2atmpS2033;
  moonbit_string_t _M0L6_2atmpS2034;
  moonbit_string_t _M0L6_2atmpS2074;
  moonbit_string_t _M0L6_2atmpS2032;
  moonbit_string_t _M0L6_2atmpS2073;
  moonbit_string_t _M0L6_2atmpS2030;
  moonbit_string_t _M0L6_2atmpS2031;
  moonbit_string_t _M0L6_2atmpS2072;
  moonbit_string_t _M0L6_2atmpS2029;
  moonbit_string_t _M0L6_2atmpS2071;
  moonbit_string_t _M0L6_2atmpS2028;
  #line 547 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2027
  = (struct _M0R104_24bikallem_2fblit__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c836*)_M0L6_2aenvS2026;
  _M0L8_2afieldS2080 = _M0L14_2acasted__envS2027->$1;
  _M0L8filenameS841 = _M0L8_2afieldS2080;
  _M0L8_2afieldS2079 = _M0L14_2acasted__envS2027->$0;
  _M0L6_2acntS2303 = Moonbit_object_header(_M0L14_2acasted__envS2027)->rc;
  if (_M0L6_2acntS2303 > 1) {
    int32_t _M0L11_2anew__cntS2304 = _M0L6_2acntS2303 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2027)->rc
    = _M0L11_2anew__cntS2304;
    moonbit_incref(_M0L8filenameS841);
  } else if (_M0L6_2acntS2303 == 1) {
    #line 547 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2027);
  }
  _M0L5indexS844 = _M0L8_2afieldS2079;
  if (!_M0L7skippedS839) {
    _if__result_2373 = 1;
  } else {
    _if__result_2373 = 0;
  }
  if (_if__result_2373) {
    
  }
  #line 553 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS840 = _M0MPC16string6String6escape(_M0L8filenameS841);
  #line 554 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS842 = _M0MPC16string6String6escape(_M0L8testnameS837);
  #line 555 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS843 = _M0MPC16string6String6escape(_M0L7messageS838);
  #line 556 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 558 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2039
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS840);
  #line 557 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2078
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS2039);
  moonbit_decref(_M0L6_2atmpS2039);
  _M0L6_2atmpS2038 = _M0L6_2atmpS2078;
  #line 557 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2077
  = moonbit_add_string(_M0L6_2atmpS2038, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS2038);
  _M0L6_2atmpS2036 = _M0L6_2atmpS2077;
  #line 558 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2037
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS844);
  #line 557 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2076 = moonbit_add_string(_M0L6_2atmpS2036, _M0L6_2atmpS2037);
  moonbit_decref(_M0L6_2atmpS2036);
  moonbit_decref(_M0L6_2atmpS2037);
  _M0L6_2atmpS2035 = _M0L6_2atmpS2076;
  #line 557 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2075
  = moonbit_add_string(_M0L6_2atmpS2035, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS2035);
  _M0L6_2atmpS2033 = _M0L6_2atmpS2075;
  #line 558 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2034
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS842);
  #line 557 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2074 = moonbit_add_string(_M0L6_2atmpS2033, _M0L6_2atmpS2034);
  moonbit_decref(_M0L6_2atmpS2033);
  moonbit_decref(_M0L6_2atmpS2034);
  _M0L6_2atmpS2032 = _M0L6_2atmpS2074;
  #line 557 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2073
  = moonbit_add_string(_M0L6_2atmpS2032, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS2032);
  _M0L6_2atmpS2030 = _M0L6_2atmpS2073;
  #line 558 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2031
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS843);
  #line 557 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2072 = moonbit_add_string(_M0L6_2atmpS2030, _M0L6_2atmpS2031);
  moonbit_decref(_M0L6_2atmpS2030);
  moonbit_decref(_M0L6_2atmpS2031);
  _M0L6_2atmpS2029 = _M0L6_2atmpS2072;
  #line 557 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2071
  = moonbit_add_string(_M0L6_2atmpS2029, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS2029);
  _M0L6_2atmpS2028 = _M0L6_2atmpS2071;
  #line 557 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2028);
  #line 560 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP28bikallem20blit__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S835,
  moonbit_string_t _M0L8filenameS832,
  int32_t _M0L5indexS826,
  struct _M0TWssbEu* _M0L14handle__resultS822,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS824
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS802;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS831;
  struct _M0TWEuQRPC15error5Error* _M0L1fS804;
  moonbit_string_t* _M0L5attrsS805;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS825;
  moonbit_string_t _M0L4nameS808;
  moonbit_string_t _M0L4nameS806;
  int32_t _M0L6_2atmpS2025;
  struct _M0TWEOs* _M0L5_2aitS810;
  struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2022__l441__* _closure_2382;
  struct _M0TWEOy* _M0L6_2atmpS2016;
  struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2018__l442__* _closure_2383;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS2017;
  struct moonbit_result_0 _result_2384;
  #line 421 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S835);
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 428 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS831
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP28bikallem20blit__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS832);
  if (_M0L7_2abindS831 == 0) {
    struct moonbit_result_0 _result_2375;
    if (_M0L7_2abindS831) {
      moonbit_decref(_M0L7_2abindS831);
    }
    moonbit_decref(_M0L17error__to__stringS824);
    moonbit_decref(_M0L14handle__resultS822);
    _result_2375.tag = 1;
    _result_2375.data.ok = 0;
    return _result_2375;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS833 =
      _M0L7_2abindS831;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS834 =
      _M0L7_2aSomeS833;
    _M0L10index__mapS802 = _M0L13_2aindex__mapS834;
    goto join_801;
  }
  join_801:;
  #line 430 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS825
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS802, _M0L5indexS826);
  if (_M0L7_2abindS825 == 0) {
    struct moonbit_result_0 _result_2377;
    if (_M0L7_2abindS825) {
      moonbit_decref(_M0L7_2abindS825);
    }
    moonbit_decref(_M0L17error__to__stringS824);
    moonbit_decref(_M0L14handle__resultS822);
    _result_2377.tag = 1;
    _result_2377.data.ok = 0;
    return _result_2377;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS827 = _M0L7_2abindS825;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS828 = _M0L7_2aSomeS827;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS2084 = _M0L4_2axS828->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS829 = _M0L8_2afieldS2084;
    moonbit_string_t* _M0L8_2afieldS2083 = _M0L4_2axS828->$1;
    int32_t _M0L6_2acntS2305 = Moonbit_object_header(_M0L4_2axS828)->rc;
    moonbit_string_t* _M0L8_2aattrsS830;
    if (_M0L6_2acntS2305 > 1) {
      int32_t _M0L11_2anew__cntS2306 = _M0L6_2acntS2305 - 1;
      Moonbit_object_header(_M0L4_2axS828)->rc = _M0L11_2anew__cntS2306;
      moonbit_incref(_M0L8_2afieldS2083);
      moonbit_incref(_M0L4_2afS829);
    } else if (_M0L6_2acntS2305 == 1) {
      #line 428 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS828);
    }
    _M0L8_2aattrsS830 = _M0L8_2afieldS2083;
    _M0L1fS804 = _M0L4_2afS829;
    _M0L5attrsS805 = _M0L8_2aattrsS830;
    goto join_803;
  }
  join_803:;
  _M0L6_2atmpS2025 = Moonbit_array_length(_M0L5attrsS805);
  if (_M0L6_2atmpS2025 >= 1) {
    moonbit_string_t _M0L6_2atmpS2082 = (moonbit_string_t)_M0L5attrsS805[0];
    moonbit_string_t _M0L7_2anameS809 = _M0L6_2atmpS2082;
    moonbit_incref(_M0L7_2anameS809);
    _M0L4nameS808 = _M0L7_2anameS809;
    goto join_807;
  } else {
    _M0L4nameS806 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_2378;
  join_807:;
  _M0L4nameS806 = _M0L4nameS808;
  joinlet_2378:;
  #line 431 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS810 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS805);
  while (1) {
    moonbit_string_t _M0L4attrS812;
    moonbit_string_t _M0L7_2abindS819;
    int32_t _M0L6_2atmpS2009;
    int64_t _M0L6_2atmpS2008;
    moonbit_incref(_M0L5_2aitS810);
    #line 433 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS819 = _M0MPB4Iter4nextGsE(_M0L5_2aitS810);
    if (_M0L7_2abindS819 == 0) {
      if (_M0L7_2abindS819) {
        moonbit_decref(_M0L7_2abindS819);
      }
      moonbit_decref(_M0L5_2aitS810);
    } else {
      moonbit_string_t _M0L7_2aSomeS820 = _M0L7_2abindS819;
      moonbit_string_t _M0L7_2aattrS821 = _M0L7_2aSomeS820;
      _M0L4attrS812 = _M0L7_2aattrS821;
      goto join_811;
    }
    goto joinlet_2380;
    join_811:;
    _M0L6_2atmpS2009 = Moonbit_array_length(_M0L4attrS812);
    _M0L6_2atmpS2008 = (int64_t)_M0L6_2atmpS2009;
    moonbit_incref(_M0L4attrS812);
    #line 434 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS812, 5, 0, _M0L6_2atmpS2008)
    ) {
      int32_t _M0L6_2atmpS2015 = _M0L4attrS812[0];
      int32_t _M0L4_2axS813 = _M0L6_2atmpS2015;
      if (_M0L4_2axS813 == 112) {
        int32_t _M0L6_2atmpS2014 = _M0L4attrS812[1];
        int32_t _M0L4_2axS814 = _M0L6_2atmpS2014;
        if (_M0L4_2axS814 == 97) {
          int32_t _M0L6_2atmpS2013 = _M0L4attrS812[2];
          int32_t _M0L4_2axS815 = _M0L6_2atmpS2013;
          if (_M0L4_2axS815 == 110) {
            int32_t _M0L6_2atmpS2012 = _M0L4attrS812[3];
            int32_t _M0L4_2axS816 = _M0L6_2atmpS2012;
            if (_M0L4_2axS816 == 105) {
              int32_t _M0L6_2atmpS2081 = _M0L4attrS812[4];
              int32_t _M0L6_2atmpS2011;
              int32_t _M0L4_2axS817;
              moonbit_decref(_M0L4attrS812);
              _M0L6_2atmpS2011 = _M0L6_2atmpS2081;
              _M0L4_2axS817 = _M0L6_2atmpS2011;
              if (_M0L4_2axS817 == 99) {
                void* _M0L103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2010;
                struct moonbit_result_0 _result_2381;
                moonbit_decref(_M0L17error__to__stringS824);
                moonbit_decref(_M0L14handle__resultS822);
                moonbit_decref(_M0L5_2aitS810);
                moonbit_decref(_M0L1fS804);
                _M0L103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2010
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2010)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2010)->$0
                = _M0L4nameS806;
                _result_2381.tag = 0;
                _result_2381.data.err
                = _M0L103bikallem_2fblit__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2010;
                return _result_2381;
              }
            } else {
              moonbit_decref(_M0L4attrS812);
            }
          } else {
            moonbit_decref(_M0L4attrS812);
          }
        } else {
          moonbit_decref(_M0L4attrS812);
        }
      } else {
        moonbit_decref(_M0L4attrS812);
      }
    } else {
      moonbit_decref(_M0L4attrS812);
    }
    continue;
    joinlet_2380:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS822);
  moonbit_incref(_M0L4nameS806);
  _closure_2382
  = (struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2022__l441__*)moonbit_malloc(sizeof(struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2022__l441__));
  Moonbit_object_header(_closure_2382)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2022__l441__, $0) >> 2, 2, 0);
  _closure_2382->code
  = &_M0IP28bikallem20blit__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testC2022l441;
  _closure_2382->$0 = _M0L14handle__resultS822;
  _closure_2382->$1 = _M0L4nameS806;
  _M0L6_2atmpS2016 = (struct _M0TWEOy*)_closure_2382;
  _closure_2383
  = (struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2018__l442__*)moonbit_malloc(sizeof(struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2018__l442__));
  Moonbit_object_header(_closure_2383)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2018__l442__, $0) >> 2, 3, 0);
  _closure_2383->code
  = &_M0IP28bikallem20blit__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testC2018l442;
  _closure_2383->$0 = _M0L17error__to__stringS824;
  _closure_2383->$1 = _M0L14handle__resultS822;
  _closure_2383->$2 = _M0L4nameS806;
  _M0L6_2atmpS2017 = (struct _M0TWRPC15error5ErrorEu*)_closure_2383;
  #line 439 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0FP28bikallem20blit__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS804, _M0L6_2atmpS2016, _M0L6_2atmpS2017);
  _result_2384.tag = 1;
  _result_2384.data.ok = 1;
  return _result_2384;
}

int32_t _M0IP28bikallem20blit__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testC2022l441(
  struct _M0TWEOy* _M0L6_2aenvS2023
) {
  struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2022__l441__* _M0L14_2acasted__envS2024;
  moonbit_string_t _M0L8_2afieldS2086;
  moonbit_string_t _M0L4nameS806;
  struct _M0TWssbEu* _M0L8_2afieldS2085;
  int32_t _M0L6_2acntS2307;
  struct _M0TWssbEu* _M0L14handle__resultS822;
  #line 441 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2024
  = (struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2022__l441__*)_M0L6_2aenvS2023;
  _M0L8_2afieldS2086 = _M0L14_2acasted__envS2024->$1;
  _M0L4nameS806 = _M0L8_2afieldS2086;
  _M0L8_2afieldS2085 = _M0L14_2acasted__envS2024->$0;
  _M0L6_2acntS2307 = Moonbit_object_header(_M0L14_2acasted__envS2024)->rc;
  if (_M0L6_2acntS2307 > 1) {
    int32_t _M0L11_2anew__cntS2308 = _M0L6_2acntS2307 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2024)->rc
    = _M0L11_2anew__cntS2308;
    moonbit_incref(_M0L4nameS806);
    moonbit_incref(_M0L8_2afieldS2085);
  } else if (_M0L6_2acntS2307 == 1) {
    #line 441 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2024);
  }
  _M0L14handle__resultS822 = _M0L8_2afieldS2085;
  #line 441 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS822->code(_M0L14handle__resultS822, _M0L4nameS806, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP28bikallem20blit__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testC2018l442(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS2019,
  void* _M0L3errS823
) {
  struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2018__l442__* _M0L14_2acasted__envS2020;
  moonbit_string_t _M0L8_2afieldS2089;
  moonbit_string_t _M0L4nameS806;
  struct _M0TWssbEu* _M0L8_2afieldS2088;
  struct _M0TWssbEu* _M0L14handle__resultS822;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS2087;
  int32_t _M0L6_2acntS2309;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS824;
  moonbit_string_t _M0L6_2atmpS2021;
  #line 442 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2020
  = (struct _M0R179_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40bikallem_2fblit__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2018__l442__*)_M0L6_2aenvS2019;
  _M0L8_2afieldS2089 = _M0L14_2acasted__envS2020->$2;
  _M0L4nameS806 = _M0L8_2afieldS2089;
  _M0L8_2afieldS2088 = _M0L14_2acasted__envS2020->$1;
  _M0L14handle__resultS822 = _M0L8_2afieldS2088;
  _M0L8_2afieldS2087 = _M0L14_2acasted__envS2020->$0;
  _M0L6_2acntS2309 = Moonbit_object_header(_M0L14_2acasted__envS2020)->rc;
  if (_M0L6_2acntS2309 > 1) {
    int32_t _M0L11_2anew__cntS2310 = _M0L6_2acntS2309 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2020)->rc
    = _M0L11_2anew__cntS2310;
    moonbit_incref(_M0L4nameS806);
    moonbit_incref(_M0L14handle__resultS822);
    moonbit_incref(_M0L8_2afieldS2087);
  } else if (_M0L6_2acntS2309 == 1) {
    #line 442 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2020);
  }
  _M0L17error__to__stringS824 = _M0L8_2afieldS2087;
  #line 442 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2021
  = _M0L17error__to__stringS824->code(_M0L17error__to__stringS824, _M0L3errS823);
  #line 442 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS822->code(_M0L14handle__resultS822, _M0L4nameS806, _M0L6_2atmpS2021, 0);
  return 0;
}

int32_t _M0FP28bikallem20blit__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS795,
  struct _M0TWEOy* _M0L6on__okS796,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS793
) {
  void* _M0L11_2atry__errS791;
  struct moonbit_result_0 _tmp_2386;
  void* _M0L3errS792;
  #line 375 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  #line 382 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _tmp_2386 = _M0L1fS795->code(_M0L1fS795);
  if (_tmp_2386.tag) {
    int32_t const _M0L5_2aokS2006 = _tmp_2386.data.ok;
    moonbit_decref(_M0L7on__errS793);
  } else {
    void* const _M0L6_2aerrS2007 = _tmp_2386.data.err;
    moonbit_decref(_M0L6on__okS796);
    _M0L11_2atry__errS791 = _M0L6_2aerrS2007;
    goto join_790;
  }
  #line 382 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS796->code(_M0L6on__okS796);
  goto joinlet_2385;
  join_790:;
  _M0L3errS792 = _M0L11_2atry__errS791;
  #line 383 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS793->code(_M0L7on__errS793, _M0L3errS792);
  joinlet_2385:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S750;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS756;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS763;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS768;
  struct _M0TUsiE** _M0L6_2atmpS2005;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS775;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS776;
  moonbit_string_t _M0L6_2atmpS2004;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS777;
  int32_t _M0L7_2abindS778;
  int32_t _M0L2__S779;
  #line 193 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S750 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS756 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS763
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS756;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS768 = 0;
  _M0L6_2atmpS2005 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS775
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS775)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS775->$0 = _M0L6_2atmpS2005;
  _M0L16file__and__indexS775->$1 = 0;
  #line 282 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS776
  = _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS763(_M0L57moonbit__test__driver__internal__get__cli__args__internalS763);
  #line 284 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2004 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS776, 1);
  #line 283 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS777
  = _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS768(_M0L51moonbit__test__driver__internal__split__mbt__stringS768, _M0L6_2atmpS2004, 47);
  _M0L7_2abindS778 = _M0L10test__argsS777->$1;
  _M0L2__S779 = 0;
  while (1) {
    if (_M0L2__S779 < _M0L7_2abindS778) {
      moonbit_string_t* _M0L8_2afieldS2091 = _M0L10test__argsS777->$0;
      moonbit_string_t* _M0L3bufS2003 = _M0L8_2afieldS2091;
      moonbit_string_t _M0L6_2atmpS2090 =
        (moonbit_string_t)_M0L3bufS2003[_M0L2__S779];
      moonbit_string_t _M0L3argS780 = _M0L6_2atmpS2090;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS781;
      moonbit_string_t _M0L4fileS782;
      moonbit_string_t _M0L5rangeS783;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS784;
      moonbit_string_t _M0L6_2atmpS2001;
      int32_t _M0L5startS785;
      moonbit_string_t _M0L6_2atmpS2000;
      int32_t _M0L3endS786;
      int32_t _M0L1iS787;
      int32_t _M0L6_2atmpS2002;
      moonbit_incref(_M0L3argS780);
      #line 288 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS781
      = _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS768(_M0L51moonbit__test__driver__internal__split__mbt__stringS768, _M0L3argS780, 58);
      moonbit_incref(_M0L16file__and__rangeS781);
      #line 289 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS782
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS781, 0);
      #line 290 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS783
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS781, 1);
      #line 291 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS784
      = _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS768(_M0L51moonbit__test__driver__internal__split__mbt__stringS768, _M0L5rangeS783, 45);
      moonbit_incref(_M0L15start__and__endS784);
      #line 294 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2001
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS784, 0);
      #line 294 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0L5startS785
      = _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S750(_M0L45moonbit__test__driver__internal__parse__int__S750, _M0L6_2atmpS2001);
      #line 295 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2000
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS784, 1);
      #line 295 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0L3endS786
      = _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S750(_M0L45moonbit__test__driver__internal__parse__int__S750, _M0L6_2atmpS2000);
      _M0L1iS787 = _M0L5startS785;
      while (1) {
        if (_M0L1iS787 < _M0L3endS786) {
          struct _M0TUsiE* _M0L8_2atupleS1998;
          int32_t _M0L6_2atmpS1999;
          moonbit_incref(_M0L4fileS782);
          _M0L8_2atupleS1998
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS1998)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS1998->$0 = _M0L4fileS782;
          _M0L8_2atupleS1998->$1 = _M0L1iS787;
          moonbit_incref(_M0L16file__and__indexS775);
          #line 297 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS775, _M0L8_2atupleS1998);
          _M0L6_2atmpS1999 = _M0L1iS787 + 1;
          _M0L1iS787 = _M0L6_2atmpS1999;
          continue;
        } else {
          moonbit_decref(_M0L4fileS782);
        }
        break;
      }
      _M0L6_2atmpS2002 = _M0L2__S779 + 1;
      _M0L2__S779 = _M0L6_2atmpS2002;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS777);
    }
    break;
  }
  return _M0L16file__and__indexS775;
}

struct _M0TPB5ArrayGsE* _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS768(
  int32_t _M0L6_2aenvS1979,
  moonbit_string_t _M0L1sS769,
  int32_t _M0L3sepS770
) {
  moonbit_string_t* _M0L6_2atmpS1997;
  struct _M0TPB5ArrayGsE* _M0L3resS771;
  struct _M0TPC13ref3RefGiE* _M0L1iS772;
  struct _M0TPC13ref3RefGiE* _M0L5startS773;
  int32_t _M0L3valS1992;
  int32_t _M0L6_2atmpS1993;
  #line 261 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1997 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS771
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS771)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS771->$0 = _M0L6_2atmpS1997;
  _M0L3resS771->$1 = 0;
  _M0L1iS772
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS772)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS772->$0 = 0;
  _M0L5startS773
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS773)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS773->$0 = 0;
  while (1) {
    int32_t _M0L3valS1980 = _M0L1iS772->$0;
    int32_t _M0L6_2atmpS1981 = Moonbit_array_length(_M0L1sS769);
    if (_M0L3valS1980 < _M0L6_2atmpS1981) {
      int32_t _M0L3valS1984 = _M0L1iS772->$0;
      int32_t _M0L6_2atmpS1983;
      int32_t _M0L6_2atmpS1982;
      int32_t _M0L3valS1991;
      int32_t _M0L6_2atmpS1990;
      if (
        _M0L3valS1984 < 0
        || _M0L3valS1984 >= Moonbit_array_length(_M0L1sS769)
      ) {
        #line 269 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1983 = _M0L1sS769[_M0L3valS1984];
      _M0L6_2atmpS1982 = _M0L6_2atmpS1983;
      if (_M0L6_2atmpS1982 == _M0L3sepS770) {
        int32_t _M0L3valS1986 = _M0L5startS773->$0;
        int32_t _M0L3valS1987 = _M0L1iS772->$0;
        moonbit_string_t _M0L6_2atmpS1985;
        int32_t _M0L3valS1989;
        int32_t _M0L6_2atmpS1988;
        moonbit_incref(_M0L1sS769);
        #line 270 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS1985
        = _M0MPC16string6String17unsafe__substring(_M0L1sS769, _M0L3valS1986, _M0L3valS1987);
        moonbit_incref(_M0L3resS771);
        #line 270 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS771, _M0L6_2atmpS1985);
        _M0L3valS1989 = _M0L1iS772->$0;
        _M0L6_2atmpS1988 = _M0L3valS1989 + 1;
        _M0L5startS773->$0 = _M0L6_2atmpS1988;
      }
      _M0L3valS1991 = _M0L1iS772->$0;
      _M0L6_2atmpS1990 = _M0L3valS1991 + 1;
      _M0L1iS772->$0 = _M0L6_2atmpS1990;
      continue;
    } else {
      moonbit_decref(_M0L1iS772);
    }
    break;
  }
  _M0L3valS1992 = _M0L5startS773->$0;
  _M0L6_2atmpS1993 = Moonbit_array_length(_M0L1sS769);
  if (_M0L3valS1992 < _M0L6_2atmpS1993) {
    int32_t _M0L8_2afieldS2092 = _M0L5startS773->$0;
    int32_t _M0L3valS1995;
    int32_t _M0L6_2atmpS1996;
    moonbit_string_t _M0L6_2atmpS1994;
    moonbit_decref(_M0L5startS773);
    _M0L3valS1995 = _M0L8_2afieldS2092;
    _M0L6_2atmpS1996 = Moonbit_array_length(_M0L1sS769);
    #line 276 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS1994
    = _M0MPC16string6String17unsafe__substring(_M0L1sS769, _M0L3valS1995, _M0L6_2atmpS1996);
    moonbit_incref(_M0L3resS771);
    #line 276 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS771, _M0L6_2atmpS1994);
  } else {
    moonbit_decref(_M0L5startS773);
    moonbit_decref(_M0L1sS769);
  }
  return _M0L3resS771;
}

struct _M0TPB5ArrayGsE* _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS763(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS756
) {
  moonbit_bytes_t* _M0L3tmpS764;
  int32_t _M0L6_2atmpS1978;
  struct _M0TPB5ArrayGsE* _M0L3resS765;
  int32_t _M0L1iS766;
  #line 250 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  #line 253 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS764
  = _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS1978 = Moonbit_array_length(_M0L3tmpS764);
  #line 254 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS765 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS1978);
  _M0L1iS766 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1974 = Moonbit_array_length(_M0L3tmpS764);
    if (_M0L1iS766 < _M0L6_2atmpS1974) {
      moonbit_bytes_t _M0L6_2atmpS2093;
      moonbit_bytes_t _M0L6_2atmpS1976;
      moonbit_string_t _M0L6_2atmpS1975;
      int32_t _M0L6_2atmpS1977;
      if (_M0L1iS766 < 0 || _M0L1iS766 >= Moonbit_array_length(_M0L3tmpS764)) {
        #line 256 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2093 = (moonbit_bytes_t)_M0L3tmpS764[_M0L1iS766];
      _M0L6_2atmpS1976 = _M0L6_2atmpS2093;
      moonbit_incref(_M0L6_2atmpS1976);
      #line 256 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS1975
      = _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS756(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS756, _M0L6_2atmpS1976);
      moonbit_incref(_M0L3resS765);
      #line 256 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS765, _M0L6_2atmpS1975);
      _M0L6_2atmpS1977 = _M0L1iS766 + 1;
      _M0L1iS766 = _M0L6_2atmpS1977;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS764);
    }
    break;
  }
  return _M0L3resS765;
}

moonbit_string_t _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS756(
  int32_t _M0L6_2aenvS1888,
  moonbit_bytes_t _M0L5bytesS757
) {
  struct _M0TPB13StringBuilder* _M0L3resS758;
  int32_t _M0L3lenS759;
  struct _M0TPC13ref3RefGiE* _M0L1iS760;
  #line 206 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  #line 209 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS758 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS759 = Moonbit_array_length(_M0L5bytesS757);
  _M0L1iS760
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS760)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS760->$0 = 0;
  while (1) {
    int32_t _M0L3valS1889 = _M0L1iS760->$0;
    if (_M0L3valS1889 < _M0L3lenS759) {
      int32_t _M0L3valS1973 = _M0L1iS760->$0;
      int32_t _M0L6_2atmpS1972;
      int32_t _M0L6_2atmpS1971;
      struct _M0TPC13ref3RefGiE* _M0L1cS761;
      int32_t _M0L3valS1890;
      if (
        _M0L3valS1973 < 0
        || _M0L3valS1973 >= Moonbit_array_length(_M0L5bytesS757)
      ) {
        #line 213 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1972 = _M0L5bytesS757[_M0L3valS1973];
      _M0L6_2atmpS1971 = (int32_t)_M0L6_2atmpS1972;
      _M0L1cS761
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS761)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS761->$0 = _M0L6_2atmpS1971;
      _M0L3valS1890 = _M0L1cS761->$0;
      if (_M0L3valS1890 < 128) {
        int32_t _M0L8_2afieldS2094 = _M0L1cS761->$0;
        int32_t _M0L3valS1892;
        int32_t _M0L6_2atmpS1891;
        int32_t _M0L3valS1894;
        int32_t _M0L6_2atmpS1893;
        moonbit_decref(_M0L1cS761);
        _M0L3valS1892 = _M0L8_2afieldS2094;
        _M0L6_2atmpS1891 = _M0L3valS1892;
        moonbit_incref(_M0L3resS758);
        #line 215 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS758, _M0L6_2atmpS1891);
        _M0L3valS1894 = _M0L1iS760->$0;
        _M0L6_2atmpS1893 = _M0L3valS1894 + 1;
        _M0L1iS760->$0 = _M0L6_2atmpS1893;
      } else {
        int32_t _M0L3valS1895 = _M0L1cS761->$0;
        if (_M0L3valS1895 < 224) {
          int32_t _M0L3valS1897 = _M0L1iS760->$0;
          int32_t _M0L6_2atmpS1896 = _M0L3valS1897 + 1;
          int32_t _M0L3valS1906;
          int32_t _M0L6_2atmpS1905;
          int32_t _M0L6_2atmpS1899;
          int32_t _M0L3valS1904;
          int32_t _M0L6_2atmpS1903;
          int32_t _M0L6_2atmpS1902;
          int32_t _M0L6_2atmpS1901;
          int32_t _M0L6_2atmpS1900;
          int32_t _M0L6_2atmpS1898;
          int32_t _M0L8_2afieldS2095;
          int32_t _M0L3valS1908;
          int32_t _M0L6_2atmpS1907;
          int32_t _M0L3valS1910;
          int32_t _M0L6_2atmpS1909;
          if (_M0L6_2atmpS1896 >= _M0L3lenS759) {
            moonbit_decref(_M0L1cS761);
            moonbit_decref(_M0L1iS760);
            moonbit_decref(_M0L5bytesS757);
            break;
          }
          _M0L3valS1906 = _M0L1cS761->$0;
          _M0L6_2atmpS1905 = _M0L3valS1906 & 31;
          _M0L6_2atmpS1899 = _M0L6_2atmpS1905 << 6;
          _M0L3valS1904 = _M0L1iS760->$0;
          _M0L6_2atmpS1903 = _M0L3valS1904 + 1;
          if (
            _M0L6_2atmpS1903 < 0
            || _M0L6_2atmpS1903 >= Moonbit_array_length(_M0L5bytesS757)
          ) {
            #line 221 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1902 = _M0L5bytesS757[_M0L6_2atmpS1903];
          _M0L6_2atmpS1901 = (int32_t)_M0L6_2atmpS1902;
          _M0L6_2atmpS1900 = _M0L6_2atmpS1901 & 63;
          _M0L6_2atmpS1898 = _M0L6_2atmpS1899 | _M0L6_2atmpS1900;
          _M0L1cS761->$0 = _M0L6_2atmpS1898;
          _M0L8_2afieldS2095 = _M0L1cS761->$0;
          moonbit_decref(_M0L1cS761);
          _M0L3valS1908 = _M0L8_2afieldS2095;
          _M0L6_2atmpS1907 = _M0L3valS1908;
          moonbit_incref(_M0L3resS758);
          #line 222 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS758, _M0L6_2atmpS1907);
          _M0L3valS1910 = _M0L1iS760->$0;
          _M0L6_2atmpS1909 = _M0L3valS1910 + 2;
          _M0L1iS760->$0 = _M0L6_2atmpS1909;
        } else {
          int32_t _M0L3valS1911 = _M0L1cS761->$0;
          if (_M0L3valS1911 < 240) {
            int32_t _M0L3valS1913 = _M0L1iS760->$0;
            int32_t _M0L6_2atmpS1912 = _M0L3valS1913 + 2;
            int32_t _M0L3valS1929;
            int32_t _M0L6_2atmpS1928;
            int32_t _M0L6_2atmpS1921;
            int32_t _M0L3valS1927;
            int32_t _M0L6_2atmpS1926;
            int32_t _M0L6_2atmpS1925;
            int32_t _M0L6_2atmpS1924;
            int32_t _M0L6_2atmpS1923;
            int32_t _M0L6_2atmpS1922;
            int32_t _M0L6_2atmpS1915;
            int32_t _M0L3valS1920;
            int32_t _M0L6_2atmpS1919;
            int32_t _M0L6_2atmpS1918;
            int32_t _M0L6_2atmpS1917;
            int32_t _M0L6_2atmpS1916;
            int32_t _M0L6_2atmpS1914;
            int32_t _M0L8_2afieldS2096;
            int32_t _M0L3valS1931;
            int32_t _M0L6_2atmpS1930;
            int32_t _M0L3valS1933;
            int32_t _M0L6_2atmpS1932;
            if (_M0L6_2atmpS1912 >= _M0L3lenS759) {
              moonbit_decref(_M0L1cS761);
              moonbit_decref(_M0L1iS760);
              moonbit_decref(_M0L5bytesS757);
              break;
            }
            _M0L3valS1929 = _M0L1cS761->$0;
            _M0L6_2atmpS1928 = _M0L3valS1929 & 15;
            _M0L6_2atmpS1921 = _M0L6_2atmpS1928 << 12;
            _M0L3valS1927 = _M0L1iS760->$0;
            _M0L6_2atmpS1926 = _M0L3valS1927 + 1;
            if (
              _M0L6_2atmpS1926 < 0
              || _M0L6_2atmpS1926 >= Moonbit_array_length(_M0L5bytesS757)
            ) {
              #line 229 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1925 = _M0L5bytesS757[_M0L6_2atmpS1926];
            _M0L6_2atmpS1924 = (int32_t)_M0L6_2atmpS1925;
            _M0L6_2atmpS1923 = _M0L6_2atmpS1924 & 63;
            _M0L6_2atmpS1922 = _M0L6_2atmpS1923 << 6;
            _M0L6_2atmpS1915 = _M0L6_2atmpS1921 | _M0L6_2atmpS1922;
            _M0L3valS1920 = _M0L1iS760->$0;
            _M0L6_2atmpS1919 = _M0L3valS1920 + 2;
            if (
              _M0L6_2atmpS1919 < 0
              || _M0L6_2atmpS1919 >= Moonbit_array_length(_M0L5bytesS757)
            ) {
              #line 230 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1918 = _M0L5bytesS757[_M0L6_2atmpS1919];
            _M0L6_2atmpS1917 = (int32_t)_M0L6_2atmpS1918;
            _M0L6_2atmpS1916 = _M0L6_2atmpS1917 & 63;
            _M0L6_2atmpS1914 = _M0L6_2atmpS1915 | _M0L6_2atmpS1916;
            _M0L1cS761->$0 = _M0L6_2atmpS1914;
            _M0L8_2afieldS2096 = _M0L1cS761->$0;
            moonbit_decref(_M0L1cS761);
            _M0L3valS1931 = _M0L8_2afieldS2096;
            _M0L6_2atmpS1930 = _M0L3valS1931;
            moonbit_incref(_M0L3resS758);
            #line 231 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS758, _M0L6_2atmpS1930);
            _M0L3valS1933 = _M0L1iS760->$0;
            _M0L6_2atmpS1932 = _M0L3valS1933 + 3;
            _M0L1iS760->$0 = _M0L6_2atmpS1932;
          } else {
            int32_t _M0L3valS1935 = _M0L1iS760->$0;
            int32_t _M0L6_2atmpS1934 = _M0L3valS1935 + 3;
            int32_t _M0L3valS1958;
            int32_t _M0L6_2atmpS1957;
            int32_t _M0L6_2atmpS1950;
            int32_t _M0L3valS1956;
            int32_t _M0L6_2atmpS1955;
            int32_t _M0L6_2atmpS1954;
            int32_t _M0L6_2atmpS1953;
            int32_t _M0L6_2atmpS1952;
            int32_t _M0L6_2atmpS1951;
            int32_t _M0L6_2atmpS1943;
            int32_t _M0L3valS1949;
            int32_t _M0L6_2atmpS1948;
            int32_t _M0L6_2atmpS1947;
            int32_t _M0L6_2atmpS1946;
            int32_t _M0L6_2atmpS1945;
            int32_t _M0L6_2atmpS1944;
            int32_t _M0L6_2atmpS1937;
            int32_t _M0L3valS1942;
            int32_t _M0L6_2atmpS1941;
            int32_t _M0L6_2atmpS1940;
            int32_t _M0L6_2atmpS1939;
            int32_t _M0L6_2atmpS1938;
            int32_t _M0L6_2atmpS1936;
            int32_t _M0L3valS1960;
            int32_t _M0L6_2atmpS1959;
            int32_t _M0L3valS1964;
            int32_t _M0L6_2atmpS1963;
            int32_t _M0L6_2atmpS1962;
            int32_t _M0L6_2atmpS1961;
            int32_t _M0L8_2afieldS2097;
            int32_t _M0L3valS1968;
            int32_t _M0L6_2atmpS1967;
            int32_t _M0L6_2atmpS1966;
            int32_t _M0L6_2atmpS1965;
            int32_t _M0L3valS1970;
            int32_t _M0L6_2atmpS1969;
            if (_M0L6_2atmpS1934 >= _M0L3lenS759) {
              moonbit_decref(_M0L1cS761);
              moonbit_decref(_M0L1iS760);
              moonbit_decref(_M0L5bytesS757);
              break;
            }
            _M0L3valS1958 = _M0L1cS761->$0;
            _M0L6_2atmpS1957 = _M0L3valS1958 & 7;
            _M0L6_2atmpS1950 = _M0L6_2atmpS1957 << 18;
            _M0L3valS1956 = _M0L1iS760->$0;
            _M0L6_2atmpS1955 = _M0L3valS1956 + 1;
            if (
              _M0L6_2atmpS1955 < 0
              || _M0L6_2atmpS1955 >= Moonbit_array_length(_M0L5bytesS757)
            ) {
              #line 238 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1954 = _M0L5bytesS757[_M0L6_2atmpS1955];
            _M0L6_2atmpS1953 = (int32_t)_M0L6_2atmpS1954;
            _M0L6_2atmpS1952 = _M0L6_2atmpS1953 & 63;
            _M0L6_2atmpS1951 = _M0L6_2atmpS1952 << 12;
            _M0L6_2atmpS1943 = _M0L6_2atmpS1950 | _M0L6_2atmpS1951;
            _M0L3valS1949 = _M0L1iS760->$0;
            _M0L6_2atmpS1948 = _M0L3valS1949 + 2;
            if (
              _M0L6_2atmpS1948 < 0
              || _M0L6_2atmpS1948 >= Moonbit_array_length(_M0L5bytesS757)
            ) {
              #line 239 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1947 = _M0L5bytesS757[_M0L6_2atmpS1948];
            _M0L6_2atmpS1946 = (int32_t)_M0L6_2atmpS1947;
            _M0L6_2atmpS1945 = _M0L6_2atmpS1946 & 63;
            _M0L6_2atmpS1944 = _M0L6_2atmpS1945 << 6;
            _M0L6_2atmpS1937 = _M0L6_2atmpS1943 | _M0L6_2atmpS1944;
            _M0L3valS1942 = _M0L1iS760->$0;
            _M0L6_2atmpS1941 = _M0L3valS1942 + 3;
            if (
              _M0L6_2atmpS1941 < 0
              || _M0L6_2atmpS1941 >= Moonbit_array_length(_M0L5bytesS757)
            ) {
              #line 240 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1940 = _M0L5bytesS757[_M0L6_2atmpS1941];
            _M0L6_2atmpS1939 = (int32_t)_M0L6_2atmpS1940;
            _M0L6_2atmpS1938 = _M0L6_2atmpS1939 & 63;
            _M0L6_2atmpS1936 = _M0L6_2atmpS1937 | _M0L6_2atmpS1938;
            _M0L1cS761->$0 = _M0L6_2atmpS1936;
            _M0L3valS1960 = _M0L1cS761->$0;
            _M0L6_2atmpS1959 = _M0L3valS1960 - 65536;
            _M0L1cS761->$0 = _M0L6_2atmpS1959;
            _M0L3valS1964 = _M0L1cS761->$0;
            _M0L6_2atmpS1963 = _M0L3valS1964 >> 10;
            _M0L6_2atmpS1962 = _M0L6_2atmpS1963 + 55296;
            _M0L6_2atmpS1961 = _M0L6_2atmpS1962;
            moonbit_incref(_M0L3resS758);
            #line 242 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS758, _M0L6_2atmpS1961);
            _M0L8_2afieldS2097 = _M0L1cS761->$0;
            moonbit_decref(_M0L1cS761);
            _M0L3valS1968 = _M0L8_2afieldS2097;
            _M0L6_2atmpS1967 = _M0L3valS1968 & 1023;
            _M0L6_2atmpS1966 = _M0L6_2atmpS1967 + 56320;
            _M0L6_2atmpS1965 = _M0L6_2atmpS1966;
            moonbit_incref(_M0L3resS758);
            #line 243 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS758, _M0L6_2atmpS1965);
            _M0L3valS1970 = _M0L1iS760->$0;
            _M0L6_2atmpS1969 = _M0L3valS1970 + 4;
            _M0L1iS760->$0 = _M0L6_2atmpS1969;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS760);
      moonbit_decref(_M0L5bytesS757);
    }
    break;
  }
  #line 247 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS758);
}

int32_t _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S750(
  int32_t _M0L6_2aenvS1881,
  moonbit_string_t _M0L1sS751
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS752;
  int32_t _M0L3lenS753;
  int32_t _M0L1iS754;
  int32_t _M0L8_2afieldS2098;
  #line 197 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L3resS752
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS752)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS752->$0 = 0;
  _M0L3lenS753 = Moonbit_array_length(_M0L1sS751);
  _M0L1iS754 = 0;
  while (1) {
    if (_M0L1iS754 < _M0L3lenS753) {
      int32_t _M0L3valS1886 = _M0L3resS752->$0;
      int32_t _M0L6_2atmpS1883 = _M0L3valS1886 * 10;
      int32_t _M0L6_2atmpS1885;
      int32_t _M0L6_2atmpS1884;
      int32_t _M0L6_2atmpS1882;
      int32_t _M0L6_2atmpS1887;
      if (_M0L1iS754 < 0 || _M0L1iS754 >= Moonbit_array_length(_M0L1sS751)) {
        #line 201 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1885 = _M0L1sS751[_M0L1iS754];
      _M0L6_2atmpS1884 = _M0L6_2atmpS1885 - 48;
      _M0L6_2atmpS1882 = _M0L6_2atmpS1883 + _M0L6_2atmpS1884;
      _M0L3resS752->$0 = _M0L6_2atmpS1882;
      _M0L6_2atmpS1887 = _M0L1iS754 + 1;
      _M0L1iS754 = _M0L6_2atmpS1887;
      continue;
    } else {
      moonbit_decref(_M0L1sS751);
    }
    break;
  }
  _M0L8_2afieldS2098 = _M0L3resS752->$0;
  moonbit_decref(_M0L3resS752);
  return _M0L8_2afieldS2098;
}

struct moonbit_result_0 _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S730,
  moonbit_string_t _M0L12_2adiscard__S731,
  int32_t _M0L12_2adiscard__S732,
  struct _M0TWssbEu* _M0L12_2adiscard__S733,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S734
) {
  struct moonbit_result_0 _result_2393;
  #line 34 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S734);
  moonbit_decref(_M0L12_2adiscard__S733);
  moonbit_decref(_M0L12_2adiscard__S731);
  moonbit_decref(_M0L12_2adiscard__S730);
  _result_2393.tag = 1;
  _result_2393.data.ok = 0;
  return _result_2393;
}

struct moonbit_result_0 _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S735,
  moonbit_string_t _M0L12_2adiscard__S736,
  int32_t _M0L12_2adiscard__S737,
  struct _M0TWssbEu* _M0L12_2adiscard__S738,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S739
) {
  struct moonbit_result_0 _result_2394;
  #line 34 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S739);
  moonbit_decref(_M0L12_2adiscard__S738);
  moonbit_decref(_M0L12_2adiscard__S736);
  moonbit_decref(_M0L12_2adiscard__S735);
  _result_2394.tag = 1;
  _result_2394.data.ok = 0;
  return _result_2394;
}

struct moonbit_result_0 _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S740,
  moonbit_string_t _M0L12_2adiscard__S741,
  int32_t _M0L12_2adiscard__S742,
  struct _M0TWssbEu* _M0L12_2adiscard__S743,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S744
) {
  struct moonbit_result_0 _result_2395;
  #line 34 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S744);
  moonbit_decref(_M0L12_2adiscard__S743);
  moonbit_decref(_M0L12_2adiscard__S741);
  moonbit_decref(_M0L12_2adiscard__S740);
  _result_2395.tag = 1;
  _result_2395.data.ok = 0;
  return _result_2395;
}

struct moonbit_result_0 _M0IP016_24default__implP28bikallem20blit__blackbox__test21MoonBit__Test__Driver9run__testGRP28bikallem20blit__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S745,
  moonbit_string_t _M0L12_2adiscard__S746,
  int32_t _M0L12_2adiscard__S747,
  struct _M0TWssbEu* _M0L12_2adiscard__S748,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S749
) {
  struct moonbit_result_0 _result_2396;
  #line 34 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S749);
  moonbit_decref(_M0L12_2adiscard__S748);
  moonbit_decref(_M0L12_2adiscard__S746);
  moonbit_decref(_M0L12_2adiscard__S745);
  _result_2396.tag = 1;
  _result_2396.data.ok = 0;
  return _result_2396;
}

int32_t _M0IP016_24default__implP28bikallem20blit__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP28bikallem20blit__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S729
) {
  #line 12 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S729);
  return 0;
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test40____test__626c69745f746573742e6d6274__12(
  
) {
  moonbit_bytes_t _M0L3srcS727;
  moonbit_bytes_t _M0L3dstS728;
  int32_t _M0L6_2atmpS1873;
  int64_t _M0L6_2atmpS1872;
  struct _M0TPC15bytes9BytesView _M0L6_2atmpS1871;
  struct _M0TPB4Show _M0L6_2atmpS1874;
  moonbit_string_t _M0L6_2atmpS1877;
  moonbit_string_t _M0L6_2atmpS1878;
  moonbit_string_t _M0L6_2atmpS1879;
  moonbit_string_t _M0L6_2atmpS1880;
  moonbit_string_t* _M0L6_2atmpS1876;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1875;
  #line 93 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3srcS727 = (moonbit_bytes_t)moonbit_bytes_literal_1.data;
  _M0L3dstS728 = (moonbit_bytes_t)moonbit_make_bytes(2, 0);
  _M0L6_2atmpS1873 = Moonbit_array_length(_M0L3srcS727);
  _M0L6_2atmpS1872 = (int64_t)_M0L6_2atmpS1873;
  #line 96 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L6_2atmpS1871
  = _M0MPC15bytes5Bytes12view_2einner(_M0L3srcS727, 0, _M0L6_2atmpS1872);
  moonbit_incref(_M0L3dstS728);
  #line 96 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit15blit__bytesview(_M0L3dstS728, 0, _M0L6_2atmpS1871, 0, 0);
  _M0L6_2atmpS1874
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3dstS728
  };
  _M0L6_2atmpS1877 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS1878 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS1879 = 0;
  _M0L6_2atmpS1880 = 0;
  _M0L6_2atmpS1876 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1876[0] = _M0L6_2atmpS1877;
  _M0L6_2atmpS1876[1] = _M0L6_2atmpS1878;
  _M0L6_2atmpS1876[2] = _M0L6_2atmpS1879;
  _M0L6_2atmpS1876[3] = _M0L6_2atmpS1880;
  _M0L6_2atmpS1875
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1875)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1875->$0 = _M0L6_2atmpS1876;
  _M0L6_2atmpS1875->$1 = 4;
  #line 97 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1874, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS1875);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test40____test__626c69745f746573742e6d6274__11(
  
) {
  moonbit_bytes_t _M0L3srcS725;
  moonbit_bytes_t _M0L3dstS726;
  int32_t _M0L6_2atmpS1863;
  int64_t _M0L6_2atmpS1862;
  struct _M0TPC15bytes9BytesView _M0L6_2atmpS1861;
  struct _M0TPB4Show _M0L6_2atmpS1864;
  moonbit_string_t _M0L6_2atmpS1867;
  moonbit_string_t _M0L6_2atmpS1868;
  moonbit_string_t _M0L6_2atmpS1869;
  moonbit_string_t _M0L6_2atmpS1870;
  moonbit_string_t* _M0L6_2atmpS1866;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1865;
  #line 85 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3srcS725 = (moonbit_bytes_t)moonbit_bytes_literal_2.data;
  _M0L3dstS726 = (moonbit_bytes_t)moonbit_make_bytes(3, 0);
  _M0L6_2atmpS1863 = Moonbit_array_length(_M0L3srcS725);
  _M0L6_2atmpS1862 = (int64_t)_M0L6_2atmpS1863;
  #line 88 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L6_2atmpS1861
  = _M0MPC15bytes5Bytes12view_2einner(_M0L3srcS725, 0, _M0L6_2atmpS1862);
  moonbit_incref(_M0L3dstS726);
  #line 88 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit15blit__bytesview(_M0L3dstS726, 0, _M0L6_2atmpS1861, 2, 3);
  _M0L6_2atmpS1864
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3dstS726
  };
  _M0L6_2atmpS1867 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS1868 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS1869 = 0;
  _M0L6_2atmpS1870 = 0;
  _M0L6_2atmpS1866 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1866[0] = _M0L6_2atmpS1867;
  _M0L6_2atmpS1866[1] = _M0L6_2atmpS1868;
  _M0L6_2atmpS1866[2] = _M0L6_2atmpS1869;
  _M0L6_2atmpS1866[3] = _M0L6_2atmpS1870;
  _M0L6_2atmpS1865
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1865)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1865->$0 = _M0L6_2atmpS1866;
  _M0L6_2atmpS1865->$1 = 4;
  #line 89 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1864, (moonbit_string_t)moonbit_string_literal_15.data, (moonbit_string_t)moonbit_string_literal_16.data, _M0L6_2atmpS1865);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test40____test__626c69745f746573742e6d6274__10(
  
) {
  moonbit_bytes_t _M0L3srcS723;
  moonbit_bytes_t _M0L3dstS724;
  int32_t _M0L6_2atmpS1853;
  int64_t _M0L6_2atmpS1852;
  struct _M0TPC15bytes9BytesView _M0L6_2atmpS1851;
  struct _M0TPB4Show _M0L6_2atmpS1854;
  moonbit_string_t _M0L6_2atmpS1857;
  moonbit_string_t _M0L6_2atmpS1858;
  moonbit_string_t _M0L6_2atmpS1859;
  moonbit_string_t _M0L6_2atmpS1860;
  moonbit_string_t* _M0L6_2atmpS1856;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1855;
  #line 77 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3srcS723 = (moonbit_bytes_t)moonbit_bytes_literal_2.data;
  _M0L3dstS724 = (moonbit_bytes_t)moonbit_make_bytes(5, 0);
  _M0L6_2atmpS1853 = Moonbit_array_length(_M0L3srcS723);
  _M0L6_2atmpS1852 = (int64_t)_M0L6_2atmpS1853;
  #line 80 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L6_2atmpS1851
  = _M0MPC15bytes5Bytes12view_2einner(_M0L3srcS723, 0, _M0L6_2atmpS1852);
  moonbit_incref(_M0L3dstS724);
  #line 80 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit15blit__bytesview(_M0L3dstS724, 1, _M0L6_2atmpS1851, 2, 3);
  _M0L6_2atmpS1854
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3dstS724
  };
  _M0L6_2atmpS1857 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS1858 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS1859 = 0;
  _M0L6_2atmpS1860 = 0;
  _M0L6_2atmpS1856 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1856[0] = _M0L6_2atmpS1857;
  _M0L6_2atmpS1856[1] = _M0L6_2atmpS1858;
  _M0L6_2atmpS1856[2] = _M0L6_2atmpS1859;
  _M0L6_2atmpS1856[3] = _M0L6_2atmpS1860;
  _M0L6_2atmpS1855
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1855)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1855->$0 = _M0L6_2atmpS1856;
  _M0L6_2atmpS1855->$1 = 4;
  #line 81 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1854, (moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_20.data, _M0L6_2atmpS1855);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__9(
  
) {
  moonbit_bytes_t _M0L3srcS721;
  moonbit_bytes_t _M0L3dstS722;
  int32_t _M0L6_2atmpS1843;
  int64_t _M0L6_2atmpS1842;
  struct _M0TPC15bytes9BytesView _M0L6_2atmpS1841;
  struct _M0TPB4Show _M0L6_2atmpS1844;
  moonbit_string_t _M0L6_2atmpS1847;
  moonbit_string_t _M0L6_2atmpS1848;
  moonbit_string_t _M0L6_2atmpS1849;
  moonbit_string_t _M0L6_2atmpS1850;
  moonbit_string_t* _M0L6_2atmpS1846;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1845;
  #line 69 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3srcS721 = (moonbit_bytes_t)moonbit_bytes_literal_2.data;
  _M0L3dstS722 = (moonbit_bytes_t)moonbit_make_bytes(5, 0);
  _M0L6_2atmpS1843 = Moonbit_array_length(_M0L3srcS721);
  _M0L6_2atmpS1842 = (int64_t)_M0L6_2atmpS1843;
  #line 72 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L6_2atmpS1841
  = _M0MPC15bytes5Bytes12view_2einner(_M0L3srcS721, 0, _M0L6_2atmpS1842);
  moonbit_incref(_M0L3dstS722);
  #line 72 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit15blit__bytesview(_M0L3dstS722, 0, _M0L6_2atmpS1841, 0, 5);
  _M0L6_2atmpS1844
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3dstS722
  };
  _M0L6_2atmpS1847 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS1848 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS1849 = 0;
  _M0L6_2atmpS1850 = 0;
  _M0L6_2atmpS1846 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1846[0] = _M0L6_2atmpS1847;
  _M0L6_2atmpS1846[1] = _M0L6_2atmpS1848;
  _M0L6_2atmpS1846[2] = _M0L6_2atmpS1849;
  _M0L6_2atmpS1846[3] = _M0L6_2atmpS1850;
  _M0L6_2atmpS1845
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1845)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1845->$0 = _M0L6_2atmpS1846;
  _M0L6_2atmpS1845->$1 = 4;
  #line 73 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1844, (moonbit_string_t)moonbit_string_literal_23.data, (moonbit_string_t)moonbit_string_literal_24.data, _M0L6_2atmpS1845);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__8(
  
) {
  moonbit_bytes_t _M0L3arrS720;
  int32_t _M0L6_2atmpS2099;
  int32_t _M0L6_2atmpS1839;
  moonbit_string_t _M0L6_2atmpS1840;
  #line 63 "/home/blem/projects/blit/src/blit_test.mbt"
  #line 64 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3arrS720 = _M0FP28bikallem4blit12make__uninit(0);
  _M0L6_2atmpS2099 = Moonbit_array_length(_M0L3arrS720);
  moonbit_decref(_M0L3arrS720);
  _M0L6_2atmpS1839 = _M0L6_2atmpS2099;
  _M0L6_2atmpS1840 = 0;
  #line 65 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB10assert__eqGiE(_M0L6_2atmpS1839, 0, _M0L6_2atmpS1840, (moonbit_string_t)moonbit_string_literal_25.data);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__7(
  
) {
  moonbit_bytes_t _M0L3arrS719;
  int32_t _M0L6_2atmpS2100;
  int32_t _M0L6_2atmpS1837;
  moonbit_string_t _M0L6_2atmpS1838;
  #line 57 "/home/blem/projects/blit/src/blit_test.mbt"
  #line 58 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3arrS719 = _M0FP28bikallem4blit12make__uninit(10);
  _M0L6_2atmpS2100 = Moonbit_array_length(_M0L3arrS719);
  moonbit_decref(_M0L3arrS719);
  _M0L6_2atmpS1837 = _M0L6_2atmpS2100;
  _M0L6_2atmpS1838 = 0;
  #line 59 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB10assert__eqGiE(_M0L6_2atmpS1837, 10, _M0L6_2atmpS1838, (moonbit_string_t)moonbit_string_literal_26.data);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__6(
  
) {
  moonbit_bytes_t _M0L3dstS718;
  struct _M0TPB4Show _M0L6_2atmpS1830;
  moonbit_string_t _M0L6_2atmpS1833;
  moonbit_string_t _M0L6_2atmpS1834;
  moonbit_string_t _M0L6_2atmpS1835;
  moonbit_string_t _M0L6_2atmpS1836;
  moonbit_string_t* _M0L6_2atmpS1832;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1831;
  #line 50 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3dstS718 = (moonbit_bytes_t)moonbit_make_bytes(3, 0);
  #line 52 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit11fill__bytes(_M0L3dstS718, 0, 171, 0);
  _M0L6_2atmpS1830
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3dstS718
  };
  _M0L6_2atmpS1833 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS1834 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS1835 = 0;
  _M0L6_2atmpS1836 = 0;
  _M0L6_2atmpS1832 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1832[0] = _M0L6_2atmpS1833;
  _M0L6_2atmpS1832[1] = _M0L6_2atmpS1834;
  _M0L6_2atmpS1832[2] = _M0L6_2atmpS1835;
  _M0L6_2atmpS1832[3] = _M0L6_2atmpS1836;
  _M0L6_2atmpS1831
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1831)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1831->$0 = _M0L6_2atmpS1832;
  _M0L6_2atmpS1831->$1 = 4;
  #line 53 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1830, (moonbit_string_t)moonbit_string_literal_29.data, (moonbit_string_t)moonbit_string_literal_30.data, _M0L6_2atmpS1831);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__5(
  
) {
  moonbit_bytes_t _M0L3dstS717;
  struct _M0TPB4Show _M0L6_2atmpS1823;
  moonbit_string_t _M0L6_2atmpS1826;
  moonbit_string_t _M0L6_2atmpS1827;
  moonbit_string_t _M0L6_2atmpS1828;
  moonbit_string_t _M0L6_2atmpS1829;
  moonbit_string_t* _M0L6_2atmpS1825;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1824;
  #line 40 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3dstS717 = (moonbit_bytes_t)moonbit_make_bytes(6, 0);
  #line 42 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit11fill__bytes(_M0L3dstS717, 1, 255, 4);
  _M0L6_2atmpS1823
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3dstS717
  };
  _M0L6_2atmpS1826 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS1827 = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6_2atmpS1828 = 0;
  _M0L6_2atmpS1829 = 0;
  _M0L6_2atmpS1825 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1825[0] = _M0L6_2atmpS1826;
  _M0L6_2atmpS1825[1] = _M0L6_2atmpS1827;
  _M0L6_2atmpS1825[2] = _M0L6_2atmpS1828;
  _M0L6_2atmpS1825[3] = _M0L6_2atmpS1829;
  _M0L6_2atmpS1824
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1824)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1824->$0 = _M0L6_2atmpS1825;
  _M0L6_2atmpS1824->$1 = 4;
  #line 43 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1823, (moonbit_string_t)moonbit_string_literal_33.data, (moonbit_string_t)moonbit_string_literal_34.data, _M0L6_2atmpS1824);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__4(
  
) {
  moonbit_bytes_t _M0L3bufS716;
  struct _M0TPB4Show _M0L6_2atmpS1816;
  moonbit_string_t _M0L6_2atmpS1819;
  moonbit_string_t _M0L6_2atmpS1820;
  moonbit_string_t _M0L6_2atmpS1821;
  moonbit_string_t _M0L6_2atmpS1822;
  moonbit_string_t* _M0L6_2atmpS1818;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1817;
  #line 33 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3bufS716 = (moonbit_bytes_t)moonbit_make_bytes_raw(5);
  _M0L3bufS716[0] = 97;
  _M0L3bufS716[1] = 98;
  _M0L3bufS716[2] = 99;
  _M0L3bufS716[3] = 100;
  _M0L3bufS716[4] = 101;
  #line 35 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit18blit__fixed__array(_M0L3bufS716, 0, _M0L3bufS716, 2, 3);
  _M0L6_2atmpS1816
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3bufS716
  };
  _M0L6_2atmpS1819 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS1820 = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS1821 = 0;
  _M0L6_2atmpS1822 = 0;
  _M0L6_2atmpS1818 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1818[0] = _M0L6_2atmpS1819;
  _M0L6_2atmpS1818[1] = _M0L6_2atmpS1820;
  _M0L6_2atmpS1818[2] = _M0L6_2atmpS1821;
  _M0L6_2atmpS1818[3] = _M0L6_2atmpS1822;
  _M0L6_2atmpS1817
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1817)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1817->$0 = _M0L6_2atmpS1818;
  _M0L6_2atmpS1817->$1 = 4;
  #line 36 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1816, (moonbit_string_t)moonbit_string_literal_37.data, (moonbit_string_t)moonbit_string_literal_38.data, _M0L6_2atmpS1817);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__3(
  
) {
  moonbit_bytes_t _M0L3bufS715;
  struct _M0TPB4Show _M0L6_2atmpS1809;
  moonbit_string_t _M0L6_2atmpS1812;
  moonbit_string_t _M0L6_2atmpS1813;
  moonbit_string_t _M0L6_2atmpS1814;
  moonbit_string_t _M0L6_2atmpS1815;
  moonbit_string_t* _M0L6_2atmpS1811;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1810;
  #line 26 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3bufS715 = (moonbit_bytes_t)moonbit_make_bytes_raw(5);
  _M0L3bufS715[0] = 97;
  _M0L3bufS715[1] = 98;
  _M0L3bufS715[2] = 99;
  _M0L3bufS715[3] = 100;
  _M0L3bufS715[4] = 101;
  #line 28 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit18blit__fixed__array(_M0L3bufS715, 1, _M0L3bufS715, 0, 3);
  _M0L6_2atmpS1809
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3bufS715
  };
  _M0L6_2atmpS1812 = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS1813 = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS1814 = 0;
  _M0L6_2atmpS1815 = 0;
  _M0L6_2atmpS1811 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1811[0] = _M0L6_2atmpS1812;
  _M0L6_2atmpS1811[1] = _M0L6_2atmpS1813;
  _M0L6_2atmpS1811[2] = _M0L6_2atmpS1814;
  _M0L6_2atmpS1811[3] = _M0L6_2atmpS1815;
  _M0L6_2atmpS1810
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1810)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1810->$0 = _M0L6_2atmpS1811;
  _M0L6_2atmpS1810->$1 = 4;
  #line 29 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1809, (moonbit_string_t)moonbit_string_literal_41.data, (moonbit_string_t)moonbit_string_literal_42.data, _M0L6_2atmpS1810);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__2(
  
) {
  moonbit_bytes_t _M0L3srcS713;
  moonbit_bytes_t _M0L3dstS714;
  struct _M0TPB4Show _M0L6_2atmpS1802;
  moonbit_string_t _M0L6_2atmpS1805;
  moonbit_string_t _M0L6_2atmpS1806;
  moonbit_string_t _M0L6_2atmpS1807;
  moonbit_string_t _M0L6_2atmpS1808;
  moonbit_string_t* _M0L6_2atmpS1804;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1803;
  #line 18 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3srcS713 = (moonbit_bytes_t)moonbit_make_bytes_raw(2);
  _M0L3srcS713[0] = 97;
  _M0L3srcS713[1] = 98;
  _M0L3dstS714 = (moonbit_bytes_t)moonbit_make_bytes(2, 0);
  #line 21 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit18blit__fixed__array(_M0L3dstS714, 0, _M0L3srcS713, 0, 0);
  moonbit_decref(_M0L3srcS713);
  _M0L6_2atmpS1802
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3dstS714
  };
  _M0L6_2atmpS1805 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS1806 = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS1807 = 0;
  _M0L6_2atmpS1808 = 0;
  _M0L6_2atmpS1804 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1804[0] = _M0L6_2atmpS1805;
  _M0L6_2atmpS1804[1] = _M0L6_2atmpS1806;
  _M0L6_2atmpS1804[2] = _M0L6_2atmpS1807;
  _M0L6_2atmpS1804[3] = _M0L6_2atmpS1808;
  _M0L6_2atmpS1803
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1803)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1803->$0 = _M0L6_2atmpS1804;
  _M0L6_2atmpS1803->$1 = 4;
  #line 22 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1802, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_45.data, _M0L6_2atmpS1803);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__1(
  
) {
  moonbit_bytes_t _M0L3srcS711;
  moonbit_bytes_t _M0L3dstS712;
  struct _M0TPB4Show _M0L6_2atmpS1795;
  moonbit_string_t _M0L6_2atmpS1798;
  moonbit_string_t _M0L6_2atmpS1799;
  moonbit_string_t _M0L6_2atmpS1800;
  moonbit_string_t _M0L6_2atmpS1801;
  moonbit_string_t* _M0L6_2atmpS1797;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1796;
  #line 10 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3srcS711 = (moonbit_bytes_t)moonbit_make_bytes_raw(5);
  _M0L3srcS711[0] = 97;
  _M0L3srcS711[1] = 98;
  _M0L3srcS711[2] = 99;
  _M0L3srcS711[3] = 100;
  _M0L3srcS711[4] = 101;
  _M0L3dstS712 = (moonbit_bytes_t)moonbit_make_bytes(5, 0);
  #line 13 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit18blit__fixed__array(_M0L3dstS712, 1, _M0L3srcS711, 2, 3);
  moonbit_decref(_M0L3srcS711);
  _M0L6_2atmpS1795
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3dstS712
  };
  _M0L6_2atmpS1798 = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS1799 = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS1800 = 0;
  _M0L6_2atmpS1801 = 0;
  _M0L6_2atmpS1797 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1797[0] = _M0L6_2atmpS1798;
  _M0L6_2atmpS1797[1] = _M0L6_2atmpS1799;
  _M0L6_2atmpS1797[2] = _M0L6_2atmpS1800;
  _M0L6_2atmpS1797[3] = _M0L6_2atmpS1801;
  _M0L6_2atmpS1796
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1796)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1796->$0 = _M0L6_2atmpS1797;
  _M0L6_2atmpS1796->$1 = 4;
  #line 14 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1795, (moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_48.data, _M0L6_2atmpS1796);
}

struct moonbit_result_0 _M0FP28bikallem20blit__blackbox__test39____test__626c69745f746573742e6d6274__0(
  
) {
  moonbit_bytes_t _M0L3srcS709;
  moonbit_bytes_t _M0L3dstS710;
  struct _M0TPB4Show _M0L6_2atmpS1788;
  moonbit_string_t _M0L6_2atmpS1791;
  moonbit_string_t _M0L6_2atmpS1792;
  moonbit_string_t _M0L6_2atmpS1793;
  moonbit_string_t _M0L6_2atmpS1794;
  moonbit_string_t* _M0L6_2atmpS1790;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1789;
  #line 2 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0L3srcS709 = (moonbit_bytes_t)moonbit_make_bytes_raw(5);
  _M0L3srcS709[0] = 97;
  _M0L3srcS709[1] = 98;
  _M0L3srcS709[2] = 99;
  _M0L3srcS709[3] = 100;
  _M0L3srcS709[4] = 101;
  _M0L3dstS710 = (moonbit_bytes_t)moonbit_make_bytes(5, 0);
  #line 5 "/home/blem/projects/blit/src/blit_test.mbt"
  _M0FP28bikallem4blit18blit__fixed__array(_M0L3dstS710, 0, _M0L3srcS709, 0, 5);
  moonbit_decref(_M0L3srcS709);
  _M0L6_2atmpS1788
  = (struct _M0TPB4Show){
    _M0FP093FixedArray_5bByte_5d_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L3dstS710
  };
  _M0L6_2atmpS1791 = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS1792 = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS1793 = 0;
  _M0L6_2atmpS1794 = 0;
  _M0L6_2atmpS1790 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1790[0] = _M0L6_2atmpS1791;
  _M0L6_2atmpS1790[1] = _M0L6_2atmpS1792;
  _M0L6_2atmpS1790[2] = _M0L6_2atmpS1793;
  _M0L6_2atmpS1790[3] = _M0L6_2atmpS1794;
  _M0L6_2atmpS1789
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1789)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1789->$0 = _M0L6_2atmpS1790;
  _M0L6_2atmpS1789->$1 = 4;
  #line 6 "/home/blem/projects/blit/src/blit_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1788, (moonbit_string_t)moonbit_string_literal_23.data, (moonbit_string_t)moonbit_string_literal_51.data, _M0L6_2atmpS1789);
}

int32_t _M0FP28bikallem4blit15blit__bytesview(
  moonbit_bytes_t _M0L3dstS704,
  int32_t _M0L11dst__offsetS705,
  struct _M0TPC15bytes9BytesView _M0L3srcS706,
  int32_t _M0L11src__offsetS707,
  int32_t _M0L6lengthS708
) {
  moonbit_bytes_t _M0L6_2atmpS1787;
  #line 4 "/home/blem/projects/blit/src/blit.mbt"
  #line 11 "/home/blem/projects/blit/src/blit.mbt"
  _M0L6_2atmpS1787 = _M0MPC15bytes9BytesView4data(_M0L3srcS706);
  #line 11 "/home/blem/projects/blit/src/blit.mbt"
  _M0FP28bikallem4blit11blit__bytes(_M0L3dstS704, _M0L11dst__offsetS705, _M0L6_2atmpS1787, _M0L11src__offsetS707, _M0L6lengthS708);
  moonbit_decref(_M0L3dstS704);
  moonbit_decref(_M0L6_2atmpS1787);
  return 0;
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS702,
  struct _M0TPB6Logger _M0L6loggerS703
) {
  moonbit_string_t _M0L6_2atmpS1786;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1785;
  #line 43 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1786 = _M0L4selfS702;
  #line 44 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1785 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1786);
  #line 44 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS1785, _M0L6loggerS703);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS679,
  struct _M0TPB6Logger _M0L6loggerS701
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS2109;
  struct _M0TPC16string10StringView _M0L3pkgS678;
  moonbit_string_t _M0L7_2adataS680;
  int32_t _M0L8_2astartS681;
  int32_t _M0L6_2atmpS1784;
  int32_t _M0L6_2aendS682;
  int32_t _M0Lm9_2acursorS683;
  int32_t _M0Lm13accept__stateS684;
  int32_t _M0Lm10match__endS685;
  int32_t _M0Lm20match__tag__saver__0S686;
  int32_t _M0Lm6tag__0S687;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS688;
  struct _M0TPC16string10StringView _M0L8_2afieldS2108;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS697;
  void* _M0L8_2afieldS2107;
  int32_t _M0L6_2acntS2311;
  void* _M0L16_2apackage__nameS698;
  struct _M0TPC16string10StringView _M0L8_2afieldS2105;
  struct _M0TPC16string10StringView _M0L8filenameS1761;
  struct _M0TPC16string10StringView _M0L8_2afieldS2104;
  struct _M0TPC16string10StringView _M0L11start__lineS1762;
  struct _M0TPC16string10StringView _M0L8_2afieldS2103;
  struct _M0TPC16string10StringView _M0L13start__columnS1763;
  struct _M0TPC16string10StringView _M0L8_2afieldS2102;
  struct _M0TPC16string10StringView _M0L9end__lineS1764;
  struct _M0TPC16string10StringView _M0L8_2afieldS2101;
  int32_t _M0L6_2acntS2315;
  struct _M0TPC16string10StringView _M0L11end__columnS1765;
  #line 58 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2afieldS2109
  = (struct _M0TPC16string10StringView){
    _M0L4selfS679->$0_1, _M0L4selfS679->$0_2, _M0L4selfS679->$0_0
  };
  _M0L3pkgS678 = _M0L8_2afieldS2109;
  moonbit_incref(_M0L3pkgS678.$0);
  moonbit_incref(_M0L3pkgS678.$0);
  #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L7_2adataS680 = _M0MPC16string10StringView4data(_M0L3pkgS678);
  moonbit_incref(_M0L3pkgS678.$0);
  #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2astartS681 = _M0MPC16string10StringView13start__offset(_M0L3pkgS678);
  moonbit_incref(_M0L3pkgS678.$0);
  #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1784 = _M0MPC16string10StringView6length(_M0L3pkgS678);
  _M0L6_2aendS682 = _M0L8_2astartS681 + _M0L6_2atmpS1784;
  _M0Lm9_2acursorS683 = _M0L8_2astartS681;
  _M0Lm13accept__stateS684 = -1;
  _M0Lm10match__endS685 = -1;
  _M0Lm20match__tag__saver__0S686 = -1;
  _M0Lm6tag__0S687 = -1;
  while (1) {
    int32_t _M0L6_2atmpS1776 = _M0Lm9_2acursorS683;
    if (_M0L6_2atmpS1776 < _M0L6_2aendS682) {
      int32_t _M0L6_2atmpS1783 = _M0Lm9_2acursorS683;
      int32_t _M0L10next__charS692;
      int32_t _M0L6_2atmpS1777;
      moonbit_incref(_M0L7_2adataS680);
      #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L10next__charS692
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS680, _M0L6_2atmpS1783);
      _M0L6_2atmpS1777 = _M0Lm9_2acursorS683;
      _M0Lm9_2acursorS683 = _M0L6_2atmpS1777 + 1;
      if (_M0L10next__charS692 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS1778;
          _M0Lm6tag__0S687 = _M0Lm9_2acursorS683;
          _M0L6_2atmpS1778 = _M0Lm9_2acursorS683;
          if (_M0L6_2atmpS1778 < _M0L6_2aendS682) {
            int32_t _M0L6_2atmpS1782 = _M0Lm9_2acursorS683;
            int32_t _M0L10next__charS693;
            int32_t _M0L6_2atmpS1779;
            moonbit_incref(_M0L7_2adataS680);
            #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
            _M0L10next__charS693
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS680, _M0L6_2atmpS1782);
            _M0L6_2atmpS1779 = _M0Lm9_2acursorS683;
            _M0Lm9_2acursorS683 = _M0L6_2atmpS1779 + 1;
            if (_M0L10next__charS693 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS1780 = _M0Lm9_2acursorS683;
                if (_M0L6_2atmpS1780 < _M0L6_2aendS682) {
                  int32_t _M0L6_2atmpS1781 = _M0Lm9_2acursorS683;
                  _M0Lm9_2acursorS683 = _M0L6_2atmpS1781 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S686 = _M0Lm6tag__0S687;
                  _M0Lm13accept__stateS684 = 0;
                  _M0Lm10match__endS685 = _M0Lm9_2acursorS683;
                  goto join_689;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_689;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_689;
    }
    break;
  }
  goto joinlet_2397;
  join_689:;
  switch (_M0Lm13accept__stateS684) {
    case 0: {
      int32_t _M0L6_2atmpS1774;
      int32_t _M0L6_2atmpS1773;
      int64_t _M0L6_2atmpS1770;
      int32_t _M0L6_2atmpS1772;
      int64_t _M0L6_2atmpS1771;
      struct _M0TPC16string10StringView _M0L13package__nameS690;
      int64_t _M0L6_2atmpS1767;
      int32_t _M0L6_2atmpS1769;
      int64_t _M0L6_2atmpS1768;
      struct _M0TPC16string10StringView _M0L12module__nameS691;
      void* _M0L4SomeS1766;
      moonbit_decref(_M0L3pkgS678.$0);
      _M0L6_2atmpS1774 = _M0Lm20match__tag__saver__0S686;
      _M0L6_2atmpS1773 = _M0L6_2atmpS1774 + 1;
      _M0L6_2atmpS1770 = (int64_t)_M0L6_2atmpS1773;
      _M0L6_2atmpS1772 = _M0Lm10match__endS685;
      _M0L6_2atmpS1771 = (int64_t)_M0L6_2atmpS1772;
      moonbit_incref(_M0L7_2adataS680);
      #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L13package__nameS690
      = _M0MPC16string6String4view(_M0L7_2adataS680, _M0L6_2atmpS1770, _M0L6_2atmpS1771);
      _M0L6_2atmpS1767 = (int64_t)_M0L8_2astartS681;
      _M0L6_2atmpS1769 = _M0Lm20match__tag__saver__0S686;
      _M0L6_2atmpS1768 = (int64_t)_M0L6_2atmpS1769;
      #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L12module__nameS691
      = _M0MPC16string6String4view(_M0L7_2adataS680, _M0L6_2atmpS1767, _M0L6_2atmpS1768);
      _M0L4SomeS1766
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS1766)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1766)->$0_0
      = _M0L13package__nameS690.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1766)->$0_1
      = _M0L13package__nameS690.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1766)->$0_2
      = _M0L13package__nameS690.$2;
      _M0L7_2abindS688
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS688)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS688->$0_0 = _M0L12module__nameS691.$0;
      _M0L7_2abindS688->$0_1 = _M0L12module__nameS691.$1;
      _M0L7_2abindS688->$0_2 = _M0L12module__nameS691.$2;
      _M0L7_2abindS688->$1 = _M0L4SomeS1766;
      break;
    }
    default: {
      void* _M0L4NoneS1775;
      moonbit_decref(_M0L7_2adataS680);
      _M0L4NoneS1775
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS688
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS688)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS688->$0_0 = _M0L3pkgS678.$0;
      _M0L7_2abindS688->$0_1 = _M0L3pkgS678.$1;
      _M0L7_2abindS688->$0_2 = _M0L3pkgS678.$2;
      _M0L7_2abindS688->$1 = _M0L4NoneS1775;
      break;
    }
  }
  joinlet_2397:;
  _M0L8_2afieldS2108
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS688->$0_1, _M0L7_2abindS688->$0_2, _M0L7_2abindS688->$0_0
  };
  _M0L15_2amodule__nameS697 = _M0L8_2afieldS2108;
  _M0L8_2afieldS2107 = _M0L7_2abindS688->$1;
  _M0L6_2acntS2311 = Moonbit_object_header(_M0L7_2abindS688)->rc;
  if (_M0L6_2acntS2311 > 1) {
    int32_t _M0L11_2anew__cntS2312 = _M0L6_2acntS2311 - 1;
    Moonbit_object_header(_M0L7_2abindS688)->rc = _M0L11_2anew__cntS2312;
    moonbit_incref(_M0L8_2afieldS2107);
    moonbit_incref(_M0L15_2amodule__nameS697.$0);
  } else if (_M0L6_2acntS2311 == 1) {
    #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L7_2abindS688);
  }
  _M0L16_2apackage__nameS698 = _M0L8_2afieldS2107;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS698)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS699 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS698;
      struct _M0TPC16string10StringView _M0L8_2afieldS2106 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS699->$0_1,
                                              _M0L7_2aSomeS699->$0_2,
                                              _M0L7_2aSomeS699->$0_0};
      int32_t _M0L6_2acntS2313 = Moonbit_object_header(_M0L7_2aSomeS699)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS700;
      if (_M0L6_2acntS2313 > 1) {
        int32_t _M0L11_2anew__cntS2314 = _M0L6_2acntS2313 - 1;
        Moonbit_object_header(_M0L7_2aSomeS699)->rc = _M0L11_2anew__cntS2314;
        moonbit_incref(_M0L8_2afieldS2106.$0);
      } else if (_M0L6_2acntS2313 == 1) {
        #line 59 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS699);
      }
      _M0L12_2apkg__nameS700 = _M0L8_2afieldS2106;
      if (_M0L6loggerS701.$1) {
        moonbit_incref(_M0L6loggerS701.$1);
      }
      #line 66 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L6loggerS701.$0->$method_2(_M0L6loggerS701.$1, _M0L12_2apkg__nameS700);
      if (_M0L6loggerS701.$1) {
        moonbit_incref(_M0L6loggerS701.$1);
      }
      #line 67 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L6loggerS701.$0->$method_3(_M0L6loggerS701.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS698);
      break;
    }
  }
  _M0L8_2afieldS2105
  = (struct _M0TPC16string10StringView){
    _M0L4selfS679->$1_1, _M0L4selfS679->$1_2, _M0L4selfS679->$1_0
  };
  _M0L8filenameS1761 = _M0L8_2afieldS2105;
  moonbit_incref(_M0L8filenameS1761.$0);
  if (_M0L6loggerS701.$1) {
    moonbit_incref(_M0L6loggerS701.$1);
  }
  #line 69 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_2(_M0L6loggerS701.$1, _M0L8filenameS1761);
  if (_M0L6loggerS701.$1) {
    moonbit_incref(_M0L6loggerS701.$1);
  }
  #line 70 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_3(_M0L6loggerS701.$1, 58);
  _M0L8_2afieldS2104
  = (struct _M0TPC16string10StringView){
    _M0L4selfS679->$2_1, _M0L4selfS679->$2_2, _M0L4selfS679->$2_0
  };
  _M0L11start__lineS1762 = _M0L8_2afieldS2104;
  moonbit_incref(_M0L11start__lineS1762.$0);
  if (_M0L6loggerS701.$1) {
    moonbit_incref(_M0L6loggerS701.$1);
  }
  #line 71 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_2(_M0L6loggerS701.$1, _M0L11start__lineS1762);
  if (_M0L6loggerS701.$1) {
    moonbit_incref(_M0L6loggerS701.$1);
  }
  #line 72 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_3(_M0L6loggerS701.$1, 58);
  _M0L8_2afieldS2103
  = (struct _M0TPC16string10StringView){
    _M0L4selfS679->$3_1, _M0L4selfS679->$3_2, _M0L4selfS679->$3_0
  };
  _M0L13start__columnS1763 = _M0L8_2afieldS2103;
  moonbit_incref(_M0L13start__columnS1763.$0);
  if (_M0L6loggerS701.$1) {
    moonbit_incref(_M0L6loggerS701.$1);
  }
  #line 73 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_2(_M0L6loggerS701.$1, _M0L13start__columnS1763);
  if (_M0L6loggerS701.$1) {
    moonbit_incref(_M0L6loggerS701.$1);
  }
  #line 74 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_3(_M0L6loggerS701.$1, 45);
  _M0L8_2afieldS2102
  = (struct _M0TPC16string10StringView){
    _M0L4selfS679->$4_1, _M0L4selfS679->$4_2, _M0L4selfS679->$4_0
  };
  _M0L9end__lineS1764 = _M0L8_2afieldS2102;
  moonbit_incref(_M0L9end__lineS1764.$0);
  if (_M0L6loggerS701.$1) {
    moonbit_incref(_M0L6loggerS701.$1);
  }
  #line 75 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_2(_M0L6loggerS701.$1, _M0L9end__lineS1764);
  if (_M0L6loggerS701.$1) {
    moonbit_incref(_M0L6loggerS701.$1);
  }
  #line 76 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_3(_M0L6loggerS701.$1, 58);
  _M0L8_2afieldS2101
  = (struct _M0TPC16string10StringView){
    _M0L4selfS679->$5_1, _M0L4selfS679->$5_2, _M0L4selfS679->$5_0
  };
  _M0L6_2acntS2315 = Moonbit_object_header(_M0L4selfS679)->rc;
  if (_M0L6_2acntS2315 > 1) {
    int32_t _M0L11_2anew__cntS2321 = _M0L6_2acntS2315 - 1;
    Moonbit_object_header(_M0L4selfS679)->rc = _M0L11_2anew__cntS2321;
    moonbit_incref(_M0L8_2afieldS2101.$0);
  } else if (_M0L6_2acntS2315 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS2320 =
      (struct _M0TPC16string10StringView){_M0L4selfS679->$4_1,
                                            _M0L4selfS679->$4_2,
                                            _M0L4selfS679->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS2319;
    struct _M0TPC16string10StringView _M0L8_2afieldS2318;
    struct _M0TPC16string10StringView _M0L8_2afieldS2317;
    struct _M0TPC16string10StringView _M0L8_2afieldS2316;
    moonbit_decref(_M0L8_2afieldS2320.$0);
    _M0L8_2afieldS2319
    = (struct _M0TPC16string10StringView){
      _M0L4selfS679->$3_1, _M0L4selfS679->$3_2, _M0L4selfS679->$3_0
    };
    moonbit_decref(_M0L8_2afieldS2319.$0);
    _M0L8_2afieldS2318
    = (struct _M0TPC16string10StringView){
      _M0L4selfS679->$2_1, _M0L4selfS679->$2_2, _M0L4selfS679->$2_0
    };
    moonbit_decref(_M0L8_2afieldS2318.$0);
    _M0L8_2afieldS2317
    = (struct _M0TPC16string10StringView){
      _M0L4selfS679->$1_1, _M0L4selfS679->$1_2, _M0L4selfS679->$1_0
    };
    moonbit_decref(_M0L8_2afieldS2317.$0);
    _M0L8_2afieldS2316
    = (struct _M0TPC16string10StringView){
      _M0L4selfS679->$0_1, _M0L4selfS679->$0_2, _M0L4selfS679->$0_0
    };
    moonbit_decref(_M0L8_2afieldS2316.$0);
    #line 77 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L4selfS679);
  }
  _M0L11end__columnS1765 = _M0L8_2afieldS2101;
  if (_M0L6loggerS701.$1) {
    moonbit_incref(_M0L6loggerS701.$1);
  }
  #line 77 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_2(_M0L6loggerS701.$1, _M0L11end__columnS1765);
  if (_M0L6loggerS701.$1) {
    moonbit_incref(_M0L6loggerS701.$1);
  }
  #line 78 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_3(_M0L6loggerS701.$1, 64);
  #line 79 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6loggerS701.$0->$method_2(_M0L6loggerS701.$1, _M0L15_2amodule__nameS697);
  return 0;
}

moonbit_bytes_t _M0MPC15bytes9BytesView4data(
  struct _M0TPC15bytes9BytesView _M0L4selfS677
) {
  moonbit_bytes_t _M0L8_2afieldS2110;
  #line 683 "/home/blem/.moon/lib/core/builtin/bytesview.mbt"
  _M0L8_2afieldS2110 = _M0L4selfS677.$0;
  return _M0L8_2afieldS2110;
}

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t _M0L4selfS669,
  int32_t _M0L5startS675,
  int64_t _M0L3endS671
) {
  int32_t _M0L3lenS668;
  int32_t _M0L3endS670;
  int32_t _M0L5startS674;
  int32_t _if__result_2401;
  #line 170 "/home/blem/.moon/lib/core/builtin/bytesview.mbt"
  _M0L3lenS668 = Moonbit_array_length(_M0L4selfS669);
  if (_M0L3endS671 == 4294967296ll) {
    _M0L3endS670 = _M0L3lenS668;
  } else {
    int64_t _M0L7_2aSomeS672 = _M0L3endS671;
    int32_t _M0L6_2aendS673 = (int32_t)_M0L7_2aSomeS672;
    if (_M0L6_2aendS673 < 0) {
      _M0L3endS670 = _M0L3lenS668 + _M0L6_2aendS673;
    } else {
      _M0L3endS670 = _M0L6_2aendS673;
    }
  }
  if (_M0L5startS675 < 0) {
    _M0L5startS674 = _M0L3lenS668 + _M0L5startS675;
  } else {
    _M0L5startS674 = _M0L5startS675;
  }
  if (_M0L5startS674 >= 0) {
    if (_M0L5startS674 <= _M0L3endS670) {
      _if__result_2401 = _M0L3endS670 <= _M0L3lenS668;
    } else {
      _if__result_2401 = 0;
    }
  } else {
    _if__result_2401 = 0;
  }
  if (_if__result_2401) {
    int32_t _M0L7_2abindS676 = _M0L3endS670 - _M0L5startS674;
    int32_t _M0L6_2atmpS1760 = _M0L5startS674 + _M0L7_2abindS676;
    return (struct _M0TPC15bytes9BytesView){_M0L5startS674,
                                              _M0L6_2atmpS1760,
                                              _M0L4selfS669};
  } else {
    moonbit_decref(_M0L4selfS669);
    #line 180 "/home/blem/.moon/lib/core/builtin/bytesview.mbt"
    return _M0FPB5abortGRPC15bytes9BytesViewE((moonbit_string_t)moonbit_string_literal_52.data, (moonbit_string_t)moonbit_string_literal_53.data);
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS667) {
  moonbit_string_t _M0L6_2atmpS1759;
  #line 37 "/home/blem/.moon/lib/core/builtin/console.mbt"
  #line 38 "/home/blem/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS1759 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS667);
  #line 38 "/home/blem/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS1759);
  moonbit_decref(_M0L6_2atmpS1759);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS666,
  struct _M0TPB6Hasher* _M0L6hasherS665
) {
  #line 532 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  #line 533 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS665, _M0L4selfS666);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS664,
  struct _M0TPB6Hasher* _M0L6hasherS663
) {
  #line 498 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  #line 499 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS663, _M0L4selfS664);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS661,
  moonbit_string_t _M0L5valueS659
) {
  int32_t _M0L7_2abindS658;
  int32_t _M0L1iS660;
  #line 389 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0L7_2abindS658 = Moonbit_array_length(_M0L5valueS659);
  _M0L1iS660 = 0;
  while (1) {
    if (_M0L1iS660 < _M0L7_2abindS658) {
      int32_t _M0L6_2atmpS1757 = _M0L5valueS659[_M0L1iS660];
      int32_t _M0L6_2atmpS1756 = (int32_t)_M0L6_2atmpS1757;
      uint32_t _M0L6_2atmpS1755 = *(uint32_t*)&_M0L6_2atmpS1756;
      int32_t _M0L6_2atmpS1758;
      moonbit_incref(_M0L4selfS661);
      #line 391 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS661, _M0L6_2atmpS1755);
      _M0L6_2atmpS1758 = _M0L1iS660 + 1;
      _M0L1iS660 = _M0L6_2atmpS1758;
      continue;
    } else {
      moonbit_decref(_M0L4selfS661);
      moonbit_decref(_M0L5valueS659);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS656,
  int32_t _M0L3idxS657
) {
  int32_t _M0L6_2atmpS2111;
  #line 1794 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2111 = _M0L4selfS656[_M0L3idxS657];
  moonbit_decref(_M0L4selfS656);
  return _M0L6_2atmpS2111;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS643,
  int32_t _M0L3keyS639
) {
  int32_t _M0L4hashS638;
  int32_t _M0L14capacity__maskS1740;
  int32_t _M0L6_2atmpS1739;
  int32_t _M0L1iS640;
  int32_t _M0L3idxS641;
  #line 216 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 217 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS638 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS639);
  _M0L14capacity__maskS1740 = _M0L4selfS643->$3;
  _M0L6_2atmpS1739 = _M0L4hashS638 & _M0L14capacity__maskS1740;
  _M0L1iS640 = 0;
  _M0L3idxS641 = _M0L6_2atmpS1739;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2115 =
      _M0L4selfS643->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1738 =
      _M0L8_2afieldS2115;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2114;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS642;
    if (
      _M0L3idxS641 < 0
      || _M0L3idxS641 >= Moonbit_array_length(_M0L7entriesS1738)
    ) {
      #line 219 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2114
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1738[
        _M0L3idxS641
      ];
    _M0L7_2abindS642 = _M0L6_2atmpS2114;
    if (_M0L7_2abindS642 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1727;
      if (_M0L7_2abindS642) {
        moonbit_incref(_M0L7_2abindS642);
      }
      moonbit_decref(_M0L4selfS643);
      if (_M0L7_2abindS642) {
        moonbit_decref(_M0L7_2abindS642);
      }
      _M0L6_2atmpS1727 = 0;
      return _M0L6_2atmpS1727;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS644 =
        _M0L7_2abindS642;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS645 =
        _M0L7_2aSomeS644;
      int32_t _M0L4hashS1729 = _M0L8_2aentryS645->$3;
      int32_t _if__result_2404;
      int32_t _M0L8_2afieldS2112;
      int32_t _M0L3pslS1732;
      int32_t _M0L6_2atmpS1734;
      int32_t _M0L6_2atmpS1736;
      int32_t _M0L14capacity__maskS1737;
      int32_t _M0L6_2atmpS1735;
      if (_M0L4hashS1729 == _M0L4hashS638) {
        int32_t _M0L3keyS1728 = _M0L8_2aentryS645->$4;
        _if__result_2404 = _M0L3keyS1728 == _M0L3keyS639;
      } else {
        _if__result_2404 = 0;
      }
      if (_if__result_2404) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2113;
        int32_t _M0L6_2acntS2322;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1731;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1730;
        moonbit_incref(_M0L8_2aentryS645);
        moonbit_decref(_M0L4selfS643);
        _M0L8_2afieldS2113 = _M0L8_2aentryS645->$5;
        _M0L6_2acntS2322 = Moonbit_object_header(_M0L8_2aentryS645)->rc;
        if (_M0L6_2acntS2322 > 1) {
          int32_t _M0L11_2anew__cntS2324 = _M0L6_2acntS2322 - 1;
          Moonbit_object_header(_M0L8_2aentryS645)->rc
          = _M0L11_2anew__cntS2324;
          moonbit_incref(_M0L8_2afieldS2113);
        } else if (_M0L6_2acntS2322 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2323 =
            _M0L8_2aentryS645->$1;
          if (_M0L8_2afieldS2323) {
            moonbit_decref(_M0L8_2afieldS2323);
          }
          #line 221 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS645);
        }
        _M0L5valueS1731 = _M0L8_2afieldS2113;
        _M0L6_2atmpS1730 = _M0L5valueS1731;
        return _M0L6_2atmpS1730;
      } else {
        moonbit_incref(_M0L8_2aentryS645);
      }
      _M0L8_2afieldS2112 = _M0L8_2aentryS645->$2;
      moonbit_decref(_M0L8_2aentryS645);
      _M0L3pslS1732 = _M0L8_2afieldS2112;
      if (_M0L1iS640 > _M0L3pslS1732) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1733;
        moonbit_decref(_M0L4selfS643);
        _M0L6_2atmpS1733 = 0;
        return _M0L6_2atmpS1733;
      }
      _M0L6_2atmpS1734 = _M0L1iS640 + 1;
      _M0L6_2atmpS1736 = _M0L3idxS641 + 1;
      _M0L14capacity__maskS1737 = _M0L4selfS643->$3;
      _M0L6_2atmpS1735 = _M0L6_2atmpS1736 & _M0L14capacity__maskS1737;
      _M0L1iS640 = _M0L6_2atmpS1734;
      _M0L3idxS641 = _M0L6_2atmpS1735;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS652,
  moonbit_string_t _M0L3keyS648
) {
  int32_t _M0L4hashS647;
  int32_t _M0L14capacity__maskS1754;
  int32_t _M0L6_2atmpS1753;
  int32_t _M0L1iS649;
  int32_t _M0L3idxS650;
  #line 216 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS648);
  #line 217 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS647 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS648);
  _M0L14capacity__maskS1754 = _M0L4selfS652->$3;
  _M0L6_2atmpS1753 = _M0L4hashS647 & _M0L14capacity__maskS1754;
  _M0L1iS649 = 0;
  _M0L3idxS650 = _M0L6_2atmpS1753;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2121 =
      _M0L4selfS652->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1752 =
      _M0L8_2afieldS2121;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2120;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS651;
    if (
      _M0L3idxS650 < 0
      || _M0L3idxS650 >= Moonbit_array_length(_M0L7entriesS1752)
    ) {
      #line 219 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2120
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1752[
        _M0L3idxS650
      ];
    _M0L7_2abindS651 = _M0L6_2atmpS2120;
    if (_M0L7_2abindS651 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1741;
      if (_M0L7_2abindS651) {
        moonbit_incref(_M0L7_2abindS651);
      }
      moonbit_decref(_M0L4selfS652);
      if (_M0L7_2abindS651) {
        moonbit_decref(_M0L7_2abindS651);
      }
      moonbit_decref(_M0L3keyS648);
      _M0L6_2atmpS1741 = 0;
      return _M0L6_2atmpS1741;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS653 =
        _M0L7_2abindS651;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS654 =
        _M0L7_2aSomeS653;
      int32_t _M0L4hashS1743 = _M0L8_2aentryS654->$3;
      int32_t _if__result_2406;
      int32_t _M0L8_2afieldS2116;
      int32_t _M0L3pslS1746;
      int32_t _M0L6_2atmpS1748;
      int32_t _M0L6_2atmpS1750;
      int32_t _M0L14capacity__maskS1751;
      int32_t _M0L6_2atmpS1749;
      if (_M0L4hashS1743 == _M0L4hashS647) {
        moonbit_string_t _M0L8_2afieldS2119 = _M0L8_2aentryS654->$4;
        moonbit_string_t _M0L3keyS1742 = _M0L8_2afieldS2119;
        int32_t _M0L6_2atmpS2118;
        #line 220 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0L6_2atmpS2118
        = moonbit_val_array_equal(_M0L3keyS1742, _M0L3keyS648);
        _if__result_2406 = _M0L6_2atmpS2118;
      } else {
        _if__result_2406 = 0;
      }
      if (_if__result_2406) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2117;
        int32_t _M0L6_2acntS2325;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1745;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1744;
        moonbit_incref(_M0L8_2aentryS654);
        moonbit_decref(_M0L4selfS652);
        moonbit_decref(_M0L3keyS648);
        _M0L8_2afieldS2117 = _M0L8_2aentryS654->$5;
        _M0L6_2acntS2325 = Moonbit_object_header(_M0L8_2aentryS654)->rc;
        if (_M0L6_2acntS2325 > 1) {
          int32_t _M0L11_2anew__cntS2328 = _M0L6_2acntS2325 - 1;
          Moonbit_object_header(_M0L8_2aentryS654)->rc
          = _M0L11_2anew__cntS2328;
          moonbit_incref(_M0L8_2afieldS2117);
        } else if (_M0L6_2acntS2325 == 1) {
          moonbit_string_t _M0L8_2afieldS2327 = _M0L8_2aentryS654->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2326;
          moonbit_decref(_M0L8_2afieldS2327);
          _M0L8_2afieldS2326 = _M0L8_2aentryS654->$1;
          if (_M0L8_2afieldS2326) {
            moonbit_decref(_M0L8_2afieldS2326);
          }
          #line 221 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS654);
        }
        _M0L5valueS1745 = _M0L8_2afieldS2117;
        _M0L6_2atmpS1744 = _M0L5valueS1745;
        return _M0L6_2atmpS1744;
      } else {
        moonbit_incref(_M0L8_2aentryS654);
      }
      _M0L8_2afieldS2116 = _M0L8_2aentryS654->$2;
      moonbit_decref(_M0L8_2aentryS654);
      _M0L3pslS1746 = _M0L8_2afieldS2116;
      if (_M0L1iS649 > _M0L3pslS1746) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1747;
        moonbit_decref(_M0L4selfS652);
        moonbit_decref(_M0L3keyS648);
        _M0L6_2atmpS1747 = 0;
        return _M0L6_2atmpS1747;
      }
      _M0L6_2atmpS1748 = _M0L1iS649 + 1;
      _M0L6_2atmpS1750 = _M0L3idxS650 + 1;
      _M0L14capacity__maskS1751 = _M0L4selfS652->$3;
      _M0L6_2atmpS1749 = _M0L6_2atmpS1750 & _M0L14capacity__maskS1751;
      _M0L1iS649 = _M0L6_2atmpS1748;
      _M0L3idxS650 = _M0L6_2atmpS1749;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS623
) {
  int32_t _M0L6lengthS622;
  int32_t _M0Lm8capacityS624;
  int32_t _M0L6_2atmpS1704;
  int32_t _M0L6_2atmpS1703;
  int32_t _M0L6_2atmpS1714;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS625;
  int32_t _M0L3endS1712;
  int32_t _M0L5startS1713;
  int32_t _M0L7_2abindS626;
  int32_t _M0L2__S627;
  #line 72 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS623.$0);
  #line 73 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS622
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS623);
  #line 74 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS624 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS622);
  _M0L6_2atmpS1704 = _M0Lm8capacityS624;
  #line 75 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1703 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1704);
  if (_M0L6lengthS622 > _M0L6_2atmpS1703) {
    int32_t _M0L6_2atmpS1705 = _M0Lm8capacityS624;
    _M0Lm8capacityS624 = _M0L6_2atmpS1705 * 2;
  }
  _M0L6_2atmpS1714 = _M0Lm8capacityS624;
  #line 78 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS625
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1714);
  _M0L3endS1712 = _M0L3arrS623.$2;
  _M0L5startS1713 = _M0L3arrS623.$1;
  _M0L7_2abindS626 = _M0L3endS1712 - _M0L5startS1713;
  _M0L2__S627 = 0;
  while (1) {
    if (_M0L2__S627 < _M0L7_2abindS626) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2125 =
        _M0L3arrS623.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1709 =
        _M0L8_2afieldS2125;
      int32_t _M0L5startS1711 = _M0L3arrS623.$1;
      int32_t _M0L6_2atmpS1710 = _M0L5startS1711 + _M0L2__S627;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2124 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1709[
          _M0L6_2atmpS1710
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS628 =
        _M0L6_2atmpS2124;
      moonbit_string_t _M0L8_2afieldS2123 = _M0L1eS628->$0;
      moonbit_string_t _M0L6_2atmpS1706 = _M0L8_2afieldS2123;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2122 =
        _M0L1eS628->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1707 =
        _M0L8_2afieldS2122;
      int32_t _M0L6_2atmpS1708;
      moonbit_incref(_M0L6_2atmpS1707);
      moonbit_incref(_M0L6_2atmpS1706);
      moonbit_incref(_M0L1mS625);
      #line 80 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS625, _M0L6_2atmpS1706, _M0L6_2atmpS1707);
      _M0L6_2atmpS1708 = _M0L2__S627 + 1;
      _M0L2__S627 = _M0L6_2atmpS1708;
      continue;
    } else {
      moonbit_decref(_M0L3arrS623.$0);
    }
    break;
  }
  return _M0L1mS625;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS631
) {
  int32_t _M0L6lengthS630;
  int32_t _M0Lm8capacityS632;
  int32_t _M0L6_2atmpS1716;
  int32_t _M0L6_2atmpS1715;
  int32_t _M0L6_2atmpS1726;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS633;
  int32_t _M0L3endS1724;
  int32_t _M0L5startS1725;
  int32_t _M0L7_2abindS634;
  int32_t _M0L2__S635;
  #line 72 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS631.$0);
  #line 73 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6lengthS630
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS631);
  #line 74 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS632 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS630);
  _M0L6_2atmpS1716 = _M0Lm8capacityS632;
  #line 75 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1715 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1716);
  if (_M0L6lengthS630 > _M0L6_2atmpS1715) {
    int32_t _M0L6_2atmpS1717 = _M0Lm8capacityS632;
    _M0Lm8capacityS632 = _M0L6_2atmpS1717 * 2;
  }
  _M0L6_2atmpS1726 = _M0Lm8capacityS632;
  #line 78 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS633
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1726);
  _M0L3endS1724 = _M0L3arrS631.$2;
  _M0L5startS1725 = _M0L3arrS631.$1;
  _M0L7_2abindS634 = _M0L3endS1724 - _M0L5startS1725;
  _M0L2__S635 = 0;
  while (1) {
    if (_M0L2__S635 < _M0L7_2abindS634) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2128 =
        _M0L3arrS631.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1721 =
        _M0L8_2afieldS2128;
      int32_t _M0L5startS1723 = _M0L3arrS631.$1;
      int32_t _M0L6_2atmpS1722 = _M0L5startS1723 + _M0L2__S635;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2127 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1721[
          _M0L6_2atmpS1722
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS636 = _M0L6_2atmpS2127;
      int32_t _M0L6_2atmpS1718 = _M0L1eS636->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2126 =
        _M0L1eS636->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1719 =
        _M0L8_2afieldS2126;
      int32_t _M0L6_2atmpS1720;
      moonbit_incref(_M0L6_2atmpS1719);
      moonbit_incref(_M0L1mS633);
      #line 80 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS633, _M0L6_2atmpS1718, _M0L6_2atmpS1719);
      _M0L6_2atmpS1720 = _M0L2__S635 + 1;
      _M0L2__S635 = _M0L6_2atmpS1720;
      continue;
    } else {
      moonbit_decref(_M0L3arrS631.$0);
    }
    break;
  }
  return _M0L1mS633;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS616,
  moonbit_string_t _M0L3keyS617,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS618
) {
  int32_t _M0L6_2atmpS1701;
  #line 107 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS617);
  #line 109 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1701 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS617);
  #line 109 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS616, _M0L3keyS617, _M0L5valueS618, _M0L6_2atmpS1701);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS619,
  int32_t _M0L3keyS620,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS621
) {
  int32_t _M0L6_2atmpS1702;
  #line 107 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 109 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1702 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS620);
  #line 109 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS619, _M0L3keyS620, _M0L5valueS621, _M0L6_2atmpS1702);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS595
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2135;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS594;
  int32_t _M0L8capacityS1693;
  int32_t _M0L13new__capacityS596;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1688;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1687;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS2134;
  int32_t _M0L6_2atmpS1689;
  int32_t _M0L8capacityS1691;
  int32_t _M0L6_2atmpS1690;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1692;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2133;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS597;
  #line 483 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8_2afieldS2135 = _M0L4selfS595->$5;
  _M0L9old__headS594 = _M0L8_2afieldS2135;
  _M0L8capacityS1693 = _M0L4selfS595->$2;
  _M0L13new__capacityS596 = _M0L8capacityS1693 << 1;
  _M0L6_2atmpS1688 = 0;
  _M0L6_2atmpS1687
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS596, _M0L6_2atmpS1688);
  _M0L6_2aoldS2134 = _M0L4selfS595->$0;
  if (_M0L9old__headS594) {
    moonbit_incref(_M0L9old__headS594);
  }
  moonbit_decref(_M0L6_2aoldS2134);
  _M0L4selfS595->$0 = _M0L6_2atmpS1687;
  _M0L4selfS595->$2 = _M0L13new__capacityS596;
  _M0L6_2atmpS1689 = _M0L13new__capacityS596 - 1;
  _M0L4selfS595->$3 = _M0L6_2atmpS1689;
  _M0L8capacityS1691 = _M0L4selfS595->$2;
  #line 489 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1690 = _M0FPB21calc__grow__threshold(_M0L8capacityS1691);
  _M0L4selfS595->$4 = _M0L6_2atmpS1690;
  _M0L4selfS595->$1 = 0;
  _M0L6_2atmpS1692 = 0;
  _M0L6_2aoldS2133 = _M0L4selfS595->$5;
  if (_M0L6_2aoldS2133) {
    moonbit_decref(_M0L6_2aoldS2133);
  }
  _M0L4selfS595->$5 = _M0L6_2atmpS1692;
  _M0L4selfS595->$6 = -1;
  _M0L8_2aparamS597 = _M0L9old__headS594;
  while (1) {
    if (_M0L8_2aparamS597 == 0) {
      if (_M0L8_2aparamS597) {
        moonbit_decref(_M0L8_2aparamS597);
      }
      moonbit_decref(_M0L4selfS595);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS598 =
        _M0L8_2aparamS597;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS599 =
        _M0L7_2aSomeS598;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2132 =
        _M0L4_2axS599->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS600 =
        _M0L8_2afieldS2132;
      moonbit_string_t _M0L8_2afieldS2131 = _M0L4_2axS599->$4;
      moonbit_string_t _M0L6_2akeyS601 = _M0L8_2afieldS2131;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2130 =
        _M0L4_2axS599->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS602 =
        _M0L8_2afieldS2130;
      int32_t _M0L8_2afieldS2129 = _M0L4_2axS599->$3;
      int32_t _M0L6_2acntS2329 = Moonbit_object_header(_M0L4_2axS599)->rc;
      int32_t _M0L7_2ahashS603;
      if (_M0L6_2acntS2329 > 1) {
        int32_t _M0L11_2anew__cntS2330 = _M0L6_2acntS2329 - 1;
        Moonbit_object_header(_M0L4_2axS599)->rc = _M0L11_2anew__cntS2330;
        moonbit_incref(_M0L8_2avalueS602);
        moonbit_incref(_M0L6_2akeyS601);
        if (_M0L7_2anextS600) {
          moonbit_incref(_M0L7_2anextS600);
        }
      } else if (_M0L6_2acntS2329 == 1) {
        #line 493 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS599);
      }
      _M0L7_2ahashS603 = _M0L8_2afieldS2129;
      moonbit_incref(_M0L4selfS595);
      #line 495 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS595, _M0L6_2akeyS601, _M0L8_2avalueS602, _M0L7_2ahashS603);
      _M0L8_2aparamS597 = _M0L7_2anextS600;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS606
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2141;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS605;
  int32_t _M0L8capacityS1700;
  int32_t _M0L13new__capacityS607;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1695;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1694;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS2140;
  int32_t _M0L6_2atmpS1696;
  int32_t _M0L8capacityS1698;
  int32_t _M0L6_2atmpS1697;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1699;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2139;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS608;
  #line 483 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8_2afieldS2141 = _M0L4selfS606->$5;
  _M0L9old__headS605 = _M0L8_2afieldS2141;
  _M0L8capacityS1700 = _M0L4selfS606->$2;
  _M0L13new__capacityS607 = _M0L8capacityS1700 << 1;
  _M0L6_2atmpS1695 = 0;
  _M0L6_2atmpS1694
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS607, _M0L6_2atmpS1695);
  _M0L6_2aoldS2140 = _M0L4selfS606->$0;
  if (_M0L9old__headS605) {
    moonbit_incref(_M0L9old__headS605);
  }
  moonbit_decref(_M0L6_2aoldS2140);
  _M0L4selfS606->$0 = _M0L6_2atmpS1694;
  _M0L4selfS606->$2 = _M0L13new__capacityS607;
  _M0L6_2atmpS1696 = _M0L13new__capacityS607 - 1;
  _M0L4selfS606->$3 = _M0L6_2atmpS1696;
  _M0L8capacityS1698 = _M0L4selfS606->$2;
  #line 489 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1697 = _M0FPB21calc__grow__threshold(_M0L8capacityS1698);
  _M0L4selfS606->$4 = _M0L6_2atmpS1697;
  _M0L4selfS606->$1 = 0;
  _M0L6_2atmpS1699 = 0;
  _M0L6_2aoldS2139 = _M0L4selfS606->$5;
  if (_M0L6_2aoldS2139) {
    moonbit_decref(_M0L6_2aoldS2139);
  }
  _M0L4selfS606->$5 = _M0L6_2atmpS1699;
  _M0L4selfS606->$6 = -1;
  _M0L8_2aparamS608 = _M0L9old__headS605;
  while (1) {
    if (_M0L8_2aparamS608 == 0) {
      if (_M0L8_2aparamS608) {
        moonbit_decref(_M0L8_2aparamS608);
      }
      moonbit_decref(_M0L4selfS606);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS609 =
        _M0L8_2aparamS608;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS610 =
        _M0L7_2aSomeS609;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2138 =
        _M0L4_2axS610->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS611 =
        _M0L8_2afieldS2138;
      int32_t _M0L6_2akeyS612 = _M0L4_2axS610->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2137 =
        _M0L4_2axS610->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS613 =
        _M0L8_2afieldS2137;
      int32_t _M0L8_2afieldS2136 = _M0L4_2axS610->$3;
      int32_t _M0L6_2acntS2331 = Moonbit_object_header(_M0L4_2axS610)->rc;
      int32_t _M0L7_2ahashS614;
      if (_M0L6_2acntS2331 > 1) {
        int32_t _M0L11_2anew__cntS2332 = _M0L6_2acntS2331 - 1;
        Moonbit_object_header(_M0L4_2axS610)->rc = _M0L11_2anew__cntS2332;
        moonbit_incref(_M0L8_2avalueS613);
        if (_M0L7_2anextS611) {
          moonbit_incref(_M0L7_2anextS611);
        }
      } else if (_M0L6_2acntS2331 == 1) {
        #line 493 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS610);
      }
      _M0L7_2ahashS614 = _M0L8_2afieldS2136;
      moonbit_incref(_M0L4selfS606);
      #line 495 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS606, _M0L6_2akeyS612, _M0L8_2avalueS613, _M0L7_2ahashS614);
      _M0L8_2aparamS608 = _M0L7_2anextS611;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS565,
  moonbit_string_t _M0L3keyS571,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS572,
  int32_t _M0L4hashS567
) {
  int32_t _M0L14capacity__maskS1668;
  int32_t _M0L6_2atmpS1667;
  int32_t _M0L3pslS562;
  int32_t _M0L3idxS563;
  #line 113 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1668 = _M0L4selfS565->$3;
  _M0L6_2atmpS1667 = _M0L4hashS567 & _M0L14capacity__maskS1668;
  _M0L3pslS562 = 0;
  _M0L3idxS563 = _M0L6_2atmpS1667;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2146 =
      _M0L4selfS565->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1666 =
      _M0L8_2afieldS2146;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2145;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS564;
    if (
      _M0L3idxS563 < 0
      || _M0L3idxS563 >= Moonbit_array_length(_M0L7entriesS1666)
    ) {
      #line 121 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2145
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1666[
        _M0L3idxS563
      ];
    _M0L7_2abindS564 = _M0L6_2atmpS2145;
    if (_M0L7_2abindS564 == 0) {
      int32_t _M0L4sizeS1651 = _M0L4selfS565->$1;
      int32_t _M0L8grow__atS1652 = _M0L4selfS565->$4;
      int32_t _M0L7_2abindS568;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS569;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS570;
      if (_M0L4sizeS1651 >= _M0L8grow__atS1652) {
        int32_t _M0L14capacity__maskS1654;
        int32_t _M0L6_2atmpS1653;
        moonbit_incref(_M0L4selfS565);
        #line 125 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS565);
        _M0L14capacity__maskS1654 = _M0L4selfS565->$3;
        _M0L6_2atmpS1653 = _M0L4hashS567 & _M0L14capacity__maskS1654;
        _M0L3pslS562 = 0;
        _M0L3idxS563 = _M0L6_2atmpS1653;
        continue;
      }
      _M0L7_2abindS568 = _M0L4selfS565->$6;
      _M0L7_2abindS569 = 0;
      _M0L5entryS570
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS570)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS570->$0 = _M0L7_2abindS568;
      _M0L5entryS570->$1 = _M0L7_2abindS569;
      _M0L5entryS570->$2 = _M0L3pslS562;
      _M0L5entryS570->$3 = _M0L4hashS567;
      _M0L5entryS570->$4 = _M0L3keyS571;
      _M0L5entryS570->$5 = _M0L5valueS572;
      #line 130 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS565, _M0L3idxS563, _M0L5entryS570);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS573 =
        _M0L7_2abindS564;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS574 =
        _M0L7_2aSomeS573;
      int32_t _M0L4hashS1656 = _M0L14_2acurr__entryS574->$3;
      int32_t _if__result_2412;
      int32_t _M0L3pslS1657;
      int32_t _M0L6_2atmpS1662;
      int32_t _M0L6_2atmpS1664;
      int32_t _M0L14capacity__maskS1665;
      int32_t _M0L6_2atmpS1663;
      if (_M0L4hashS1656 == _M0L4hashS567) {
        moonbit_string_t _M0L8_2afieldS2144 = _M0L14_2acurr__entryS574->$4;
        moonbit_string_t _M0L3keyS1655 = _M0L8_2afieldS2144;
        int32_t _M0L6_2atmpS2143;
        #line 134 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0L6_2atmpS2143
        = moonbit_val_array_equal(_M0L3keyS1655, _M0L3keyS571);
        _if__result_2412 = _M0L6_2atmpS2143;
      } else {
        _if__result_2412 = 0;
      }
      if (_if__result_2412) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2142;
        moonbit_incref(_M0L14_2acurr__entryS574);
        moonbit_decref(_M0L3keyS571);
        moonbit_decref(_M0L4selfS565);
        _M0L6_2aoldS2142 = _M0L14_2acurr__entryS574->$5;
        moonbit_decref(_M0L6_2aoldS2142);
        _M0L14_2acurr__entryS574->$5 = _M0L5valueS572;
        moonbit_decref(_M0L14_2acurr__entryS574);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS574);
      }
      _M0L3pslS1657 = _M0L14_2acurr__entryS574->$2;
      if (_M0L3pslS562 > _M0L3pslS1657) {
        int32_t _M0L4sizeS1658 = _M0L4selfS565->$1;
        int32_t _M0L8grow__atS1659 = _M0L4selfS565->$4;
        int32_t _M0L7_2abindS575;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS576;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS577;
        if (_M0L4sizeS1658 >= _M0L8grow__atS1659) {
          int32_t _M0L14capacity__maskS1661;
          int32_t _M0L6_2atmpS1660;
          moonbit_decref(_M0L14_2acurr__entryS574);
          moonbit_incref(_M0L4selfS565);
          #line 142 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS565);
          _M0L14capacity__maskS1661 = _M0L4selfS565->$3;
          _M0L6_2atmpS1660 = _M0L4hashS567 & _M0L14capacity__maskS1661;
          _M0L3pslS562 = 0;
          _M0L3idxS563 = _M0L6_2atmpS1660;
          continue;
        }
        moonbit_incref(_M0L4selfS565);
        #line 146 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS565, _M0L3idxS563, _M0L14_2acurr__entryS574);
        _M0L7_2abindS575 = _M0L4selfS565->$6;
        _M0L7_2abindS576 = 0;
        _M0L5entryS577
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS577)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS577->$0 = _M0L7_2abindS575;
        _M0L5entryS577->$1 = _M0L7_2abindS576;
        _M0L5entryS577->$2 = _M0L3pslS562;
        _M0L5entryS577->$3 = _M0L4hashS567;
        _M0L5entryS577->$4 = _M0L3keyS571;
        _M0L5entryS577->$5 = _M0L5valueS572;
        #line 148 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS565, _M0L3idxS563, _M0L5entryS577);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS574);
      }
      _M0L6_2atmpS1662 = _M0L3pslS562 + 1;
      _M0L6_2atmpS1664 = _M0L3idxS563 + 1;
      _M0L14capacity__maskS1665 = _M0L4selfS565->$3;
      _M0L6_2atmpS1663 = _M0L6_2atmpS1664 & _M0L14capacity__maskS1665;
      _M0L3pslS562 = _M0L6_2atmpS1662;
      _M0L3idxS563 = _M0L6_2atmpS1663;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS581,
  int32_t _M0L3keyS587,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS588,
  int32_t _M0L4hashS583
) {
  int32_t _M0L14capacity__maskS1686;
  int32_t _M0L6_2atmpS1685;
  int32_t _M0L3pslS578;
  int32_t _M0L3idxS579;
  #line 113 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS1686 = _M0L4selfS581->$3;
  _M0L6_2atmpS1685 = _M0L4hashS583 & _M0L14capacity__maskS1686;
  _M0L3pslS578 = 0;
  _M0L3idxS579 = _M0L6_2atmpS1685;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2149 =
      _M0L4selfS581->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1684 =
      _M0L8_2afieldS2149;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2148;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS580;
    if (
      _M0L3idxS579 < 0
      || _M0L3idxS579 >= Moonbit_array_length(_M0L7entriesS1684)
    ) {
      #line 121 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2148
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1684[
        _M0L3idxS579
      ];
    _M0L7_2abindS580 = _M0L6_2atmpS2148;
    if (_M0L7_2abindS580 == 0) {
      int32_t _M0L4sizeS1669 = _M0L4selfS581->$1;
      int32_t _M0L8grow__atS1670 = _M0L4selfS581->$4;
      int32_t _M0L7_2abindS584;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS585;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS586;
      if (_M0L4sizeS1669 >= _M0L8grow__atS1670) {
        int32_t _M0L14capacity__maskS1672;
        int32_t _M0L6_2atmpS1671;
        moonbit_incref(_M0L4selfS581);
        #line 125 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS581);
        _M0L14capacity__maskS1672 = _M0L4selfS581->$3;
        _M0L6_2atmpS1671 = _M0L4hashS583 & _M0L14capacity__maskS1672;
        _M0L3pslS578 = 0;
        _M0L3idxS579 = _M0L6_2atmpS1671;
        continue;
      }
      _M0L7_2abindS584 = _M0L4selfS581->$6;
      _M0L7_2abindS585 = 0;
      _M0L5entryS586
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS586)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS586->$0 = _M0L7_2abindS584;
      _M0L5entryS586->$1 = _M0L7_2abindS585;
      _M0L5entryS586->$2 = _M0L3pslS578;
      _M0L5entryS586->$3 = _M0L4hashS583;
      _M0L5entryS586->$4 = _M0L3keyS587;
      _M0L5entryS586->$5 = _M0L5valueS588;
      #line 130 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS581, _M0L3idxS579, _M0L5entryS586);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS589 =
        _M0L7_2abindS580;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS590 =
        _M0L7_2aSomeS589;
      int32_t _M0L4hashS1674 = _M0L14_2acurr__entryS590->$3;
      int32_t _if__result_2414;
      int32_t _M0L3pslS1675;
      int32_t _M0L6_2atmpS1680;
      int32_t _M0L6_2atmpS1682;
      int32_t _M0L14capacity__maskS1683;
      int32_t _M0L6_2atmpS1681;
      if (_M0L4hashS1674 == _M0L4hashS583) {
        int32_t _M0L3keyS1673 = _M0L14_2acurr__entryS590->$4;
        _if__result_2414 = _M0L3keyS1673 == _M0L3keyS587;
      } else {
        _if__result_2414 = 0;
      }
      if (_if__result_2414) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS2147;
        moonbit_incref(_M0L14_2acurr__entryS590);
        moonbit_decref(_M0L4selfS581);
        _M0L6_2aoldS2147 = _M0L14_2acurr__entryS590->$5;
        moonbit_decref(_M0L6_2aoldS2147);
        _M0L14_2acurr__entryS590->$5 = _M0L5valueS588;
        moonbit_decref(_M0L14_2acurr__entryS590);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS590);
      }
      _M0L3pslS1675 = _M0L14_2acurr__entryS590->$2;
      if (_M0L3pslS578 > _M0L3pslS1675) {
        int32_t _M0L4sizeS1676 = _M0L4selfS581->$1;
        int32_t _M0L8grow__atS1677 = _M0L4selfS581->$4;
        int32_t _M0L7_2abindS591;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS592;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS593;
        if (_M0L4sizeS1676 >= _M0L8grow__atS1677) {
          int32_t _M0L14capacity__maskS1679;
          int32_t _M0L6_2atmpS1678;
          moonbit_decref(_M0L14_2acurr__entryS590);
          moonbit_incref(_M0L4selfS581);
          #line 142 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS581);
          _M0L14capacity__maskS1679 = _M0L4selfS581->$3;
          _M0L6_2atmpS1678 = _M0L4hashS583 & _M0L14capacity__maskS1679;
          _M0L3pslS578 = 0;
          _M0L3idxS579 = _M0L6_2atmpS1678;
          continue;
        }
        moonbit_incref(_M0L4selfS581);
        #line 146 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS581, _M0L3idxS579, _M0L14_2acurr__entryS590);
        _M0L7_2abindS591 = _M0L4selfS581->$6;
        _M0L7_2abindS592 = 0;
        _M0L5entryS593
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS593)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS593->$0 = _M0L7_2abindS591;
        _M0L5entryS593->$1 = _M0L7_2abindS592;
        _M0L5entryS593->$2 = _M0L3pslS578;
        _M0L5entryS593->$3 = _M0L4hashS583;
        _M0L5entryS593->$4 = _M0L3keyS587;
        _M0L5entryS593->$5 = _M0L5valueS588;
        #line 148 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS581, _M0L3idxS579, _M0L5entryS593);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS590);
      }
      _M0L6_2atmpS1680 = _M0L3pslS578 + 1;
      _M0L6_2atmpS1682 = _M0L3idxS579 + 1;
      _M0L14capacity__maskS1683 = _M0L4selfS581->$3;
      _M0L6_2atmpS1681 = _M0L6_2atmpS1682 & _M0L14capacity__maskS1683;
      _M0L3pslS578 = _M0L6_2atmpS1680;
      _M0L3idxS579 = _M0L6_2atmpS1681;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS546,
  int32_t _M0L3idxS551,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS550
) {
  int32_t _M0L3pslS1634;
  int32_t _M0L6_2atmpS1630;
  int32_t _M0L6_2atmpS1632;
  int32_t _M0L14capacity__maskS1633;
  int32_t _M0L6_2atmpS1631;
  int32_t _M0L3pslS542;
  int32_t _M0L3idxS543;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS544;
  #line 158 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1634 = _M0L5entryS550->$2;
  _M0L6_2atmpS1630 = _M0L3pslS1634 + 1;
  _M0L6_2atmpS1632 = _M0L3idxS551 + 1;
  _M0L14capacity__maskS1633 = _M0L4selfS546->$3;
  _M0L6_2atmpS1631 = _M0L6_2atmpS1632 & _M0L14capacity__maskS1633;
  _M0L3pslS542 = _M0L6_2atmpS1630;
  _M0L3idxS543 = _M0L6_2atmpS1631;
  _M0L5entryS544 = _M0L5entryS550;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2151 =
      _M0L4selfS546->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1629 =
      _M0L8_2afieldS2151;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2150;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS545;
    if (
      _M0L3idxS543 < 0
      || _M0L3idxS543 >= Moonbit_array_length(_M0L7entriesS1629)
    ) {
      #line 164 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2150
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1629[
        _M0L3idxS543
      ];
    _M0L7_2abindS545 = _M0L6_2atmpS2150;
    if (_M0L7_2abindS545 == 0) {
      _M0L5entryS544->$2 = _M0L3pslS542;
      #line 167 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS546, _M0L5entryS544, _M0L3idxS543);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS548 =
        _M0L7_2abindS545;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS549 =
        _M0L7_2aSomeS548;
      int32_t _M0L3pslS1619 = _M0L14_2acurr__entryS549->$2;
      if (_M0L3pslS542 > _M0L3pslS1619) {
        int32_t _M0L3pslS1624;
        int32_t _M0L6_2atmpS1620;
        int32_t _M0L6_2atmpS1622;
        int32_t _M0L14capacity__maskS1623;
        int32_t _M0L6_2atmpS1621;
        _M0L5entryS544->$2 = _M0L3pslS542;
        moonbit_incref(_M0L14_2acurr__entryS549);
        moonbit_incref(_M0L4selfS546);
        #line 173 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS546, _M0L5entryS544, _M0L3idxS543);
        _M0L3pslS1624 = _M0L14_2acurr__entryS549->$2;
        _M0L6_2atmpS1620 = _M0L3pslS1624 + 1;
        _M0L6_2atmpS1622 = _M0L3idxS543 + 1;
        _M0L14capacity__maskS1623 = _M0L4selfS546->$3;
        _M0L6_2atmpS1621 = _M0L6_2atmpS1622 & _M0L14capacity__maskS1623;
        _M0L3pslS542 = _M0L6_2atmpS1620;
        _M0L3idxS543 = _M0L6_2atmpS1621;
        _M0L5entryS544 = _M0L14_2acurr__entryS549;
        continue;
      } else {
        int32_t _M0L6_2atmpS1625 = _M0L3pslS542 + 1;
        int32_t _M0L6_2atmpS1627 = _M0L3idxS543 + 1;
        int32_t _M0L14capacity__maskS1628 = _M0L4selfS546->$3;
        int32_t _M0L6_2atmpS1626 =
          _M0L6_2atmpS1627 & _M0L14capacity__maskS1628;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_2416 =
          _M0L5entryS544;
        _M0L3pslS542 = _M0L6_2atmpS1625;
        _M0L3idxS543 = _M0L6_2atmpS1626;
        _M0L5entryS544 = _tmp_2416;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS556,
  int32_t _M0L3idxS561,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS560
) {
  int32_t _M0L3pslS1650;
  int32_t _M0L6_2atmpS1646;
  int32_t _M0L6_2atmpS1648;
  int32_t _M0L14capacity__maskS1649;
  int32_t _M0L6_2atmpS1647;
  int32_t _M0L3pslS552;
  int32_t _M0L3idxS553;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS554;
  #line 158 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS1650 = _M0L5entryS560->$2;
  _M0L6_2atmpS1646 = _M0L3pslS1650 + 1;
  _M0L6_2atmpS1648 = _M0L3idxS561 + 1;
  _M0L14capacity__maskS1649 = _M0L4selfS556->$3;
  _M0L6_2atmpS1647 = _M0L6_2atmpS1648 & _M0L14capacity__maskS1649;
  _M0L3pslS552 = _M0L6_2atmpS1646;
  _M0L3idxS553 = _M0L6_2atmpS1647;
  _M0L5entryS554 = _M0L5entryS560;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2153 =
      _M0L4selfS556->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1645 =
      _M0L8_2afieldS2153;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2152;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS555;
    if (
      _M0L3idxS553 < 0
      || _M0L3idxS553 >= Moonbit_array_length(_M0L7entriesS1645)
    ) {
      #line 164 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2152
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1645[
        _M0L3idxS553
      ];
    _M0L7_2abindS555 = _M0L6_2atmpS2152;
    if (_M0L7_2abindS555 == 0) {
      _M0L5entryS554->$2 = _M0L3pslS552;
      #line 167 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS556, _M0L5entryS554, _M0L3idxS553);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS558 =
        _M0L7_2abindS555;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS559 =
        _M0L7_2aSomeS558;
      int32_t _M0L3pslS1635 = _M0L14_2acurr__entryS559->$2;
      if (_M0L3pslS552 > _M0L3pslS1635) {
        int32_t _M0L3pslS1640;
        int32_t _M0L6_2atmpS1636;
        int32_t _M0L6_2atmpS1638;
        int32_t _M0L14capacity__maskS1639;
        int32_t _M0L6_2atmpS1637;
        _M0L5entryS554->$2 = _M0L3pslS552;
        moonbit_incref(_M0L14_2acurr__entryS559);
        moonbit_incref(_M0L4selfS556);
        #line 173 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS556, _M0L5entryS554, _M0L3idxS553);
        _M0L3pslS1640 = _M0L14_2acurr__entryS559->$2;
        _M0L6_2atmpS1636 = _M0L3pslS1640 + 1;
        _M0L6_2atmpS1638 = _M0L3idxS553 + 1;
        _M0L14capacity__maskS1639 = _M0L4selfS556->$3;
        _M0L6_2atmpS1637 = _M0L6_2atmpS1638 & _M0L14capacity__maskS1639;
        _M0L3pslS552 = _M0L6_2atmpS1636;
        _M0L3idxS553 = _M0L6_2atmpS1637;
        _M0L5entryS554 = _M0L14_2acurr__entryS559;
        continue;
      } else {
        int32_t _M0L6_2atmpS1641 = _M0L3pslS552 + 1;
        int32_t _M0L6_2atmpS1643 = _M0L3idxS553 + 1;
        int32_t _M0L14capacity__maskS1644 = _M0L4selfS556->$3;
        int32_t _M0L6_2atmpS1642 =
          _M0L6_2atmpS1643 & _M0L14capacity__maskS1644;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_2418 =
          _M0L5entryS554;
        _M0L3pslS552 = _M0L6_2atmpS1641;
        _M0L3idxS553 = _M0L6_2atmpS1642;
        _M0L5entryS554 = _tmp_2418;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS530,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS532,
  int32_t _M0L8new__idxS531
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2156;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1615;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1616;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2155;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2154;
  int32_t _M0L6_2acntS2333;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS533;
  #line 185 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8_2afieldS2156 = _M0L4selfS530->$0;
  _M0L7entriesS1615 = _M0L8_2afieldS2156;
  moonbit_incref(_M0L5entryS532);
  _M0L6_2atmpS1616 = _M0L5entryS532;
  if (
    _M0L8new__idxS531 < 0
    || _M0L8new__idxS531 >= Moonbit_array_length(_M0L7entriesS1615)
  ) {
    #line 190 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2155
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1615[
      _M0L8new__idxS531
    ];
  if (_M0L6_2aoldS2155) {
    moonbit_decref(_M0L6_2aoldS2155);
  }
  _M0L7entriesS1615[_M0L8new__idxS531] = _M0L6_2atmpS1616;
  _M0L8_2afieldS2154 = _M0L5entryS532->$1;
  _M0L6_2acntS2333 = Moonbit_object_header(_M0L5entryS532)->rc;
  if (_M0L6_2acntS2333 > 1) {
    int32_t _M0L11_2anew__cntS2336 = _M0L6_2acntS2333 - 1;
    Moonbit_object_header(_M0L5entryS532)->rc = _M0L11_2anew__cntS2336;
    if (_M0L8_2afieldS2154) {
      moonbit_incref(_M0L8_2afieldS2154);
    }
  } else if (_M0L6_2acntS2333 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2335 =
      _M0L5entryS532->$5;
    moonbit_string_t _M0L8_2afieldS2334;
    moonbit_decref(_M0L8_2afieldS2335);
    _M0L8_2afieldS2334 = _M0L5entryS532->$4;
    moonbit_decref(_M0L8_2afieldS2334);
    #line 191 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS532);
  }
  _M0L7_2abindS533 = _M0L8_2afieldS2154;
  if (_M0L7_2abindS533 == 0) {
    if (_M0L7_2abindS533) {
      moonbit_decref(_M0L7_2abindS533);
    }
    _M0L4selfS530->$6 = _M0L8new__idxS531;
    moonbit_decref(_M0L4selfS530);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS534;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS535;
    moonbit_decref(_M0L4selfS530);
    _M0L7_2aSomeS534 = _M0L7_2abindS533;
    _M0L7_2anextS535 = _M0L7_2aSomeS534;
    _M0L7_2anextS535->$0 = _M0L8new__idxS531;
    moonbit_decref(_M0L7_2anextS535);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS536,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS538,
  int32_t _M0L8new__idxS537
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2159;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1617;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1618;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2158;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2157;
  int32_t _M0L6_2acntS2337;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS539;
  #line 185 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8_2afieldS2159 = _M0L4selfS536->$0;
  _M0L7entriesS1617 = _M0L8_2afieldS2159;
  moonbit_incref(_M0L5entryS538);
  _M0L6_2atmpS1618 = _M0L5entryS538;
  if (
    _M0L8new__idxS537 < 0
    || _M0L8new__idxS537 >= Moonbit_array_length(_M0L7entriesS1617)
  ) {
    #line 190 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2158
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1617[
      _M0L8new__idxS537
    ];
  if (_M0L6_2aoldS2158) {
    moonbit_decref(_M0L6_2aoldS2158);
  }
  _M0L7entriesS1617[_M0L8new__idxS537] = _M0L6_2atmpS1618;
  _M0L8_2afieldS2157 = _M0L5entryS538->$1;
  _M0L6_2acntS2337 = Moonbit_object_header(_M0L5entryS538)->rc;
  if (_M0L6_2acntS2337 > 1) {
    int32_t _M0L11_2anew__cntS2339 = _M0L6_2acntS2337 - 1;
    Moonbit_object_header(_M0L5entryS538)->rc = _M0L11_2anew__cntS2339;
    if (_M0L8_2afieldS2157) {
      moonbit_incref(_M0L8_2afieldS2157);
    }
  } else if (_M0L6_2acntS2337 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2338 =
      _M0L5entryS538->$5;
    moonbit_decref(_M0L8_2afieldS2338);
    #line 191 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_free(_M0L5entryS538);
  }
  _M0L7_2abindS539 = _M0L8_2afieldS2157;
  if (_M0L7_2abindS539 == 0) {
    if (_M0L7_2abindS539) {
      moonbit_decref(_M0L7_2abindS539);
    }
    _M0L4selfS536->$6 = _M0L8new__idxS537;
    moonbit_decref(_M0L4selfS536);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS540;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS541;
    moonbit_decref(_M0L4selfS536);
    _M0L7_2aSomeS540 = _M0L7_2abindS539;
    _M0L7_2anextS541 = _M0L7_2aSomeS540;
    _M0L7_2anextS541->$0 = _M0L8new__idxS537;
    moonbit_decref(_M0L7_2anextS541);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS523,
  int32_t _M0L3idxS525,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS524
) {
  int32_t _M0L7_2abindS522;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2161;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1602;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1603;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2160;
  int32_t _M0L4sizeS1605;
  int32_t _M0L6_2atmpS1604;
  #line 443 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS522 = _M0L4selfS523->$6;
  switch (_M0L7_2abindS522) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1597;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2162;
      moonbit_incref(_M0L5entryS524);
      _M0L6_2atmpS1597 = _M0L5entryS524;
      _M0L6_2aoldS2162 = _M0L4selfS523->$5;
      if (_M0L6_2aoldS2162) {
        moonbit_decref(_M0L6_2aoldS2162);
      }
      _M0L4selfS523->$5 = _M0L6_2atmpS1597;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2165 =
        _M0L4selfS523->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1601 =
        _M0L8_2afieldS2165;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2164;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1600;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1598;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1599;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2163;
      if (
        _M0L7_2abindS522 < 0
        || _M0L7_2abindS522 >= Moonbit_array_length(_M0L7entriesS1601)
      ) {
        #line 450 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2164
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1601[
          _M0L7_2abindS522
        ];
      _M0L6_2atmpS1600 = _M0L6_2atmpS2164;
      if (_M0L6_2atmpS1600) {
        moonbit_incref(_M0L6_2atmpS1600);
      }
      #line 450 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1598
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1600);
      moonbit_incref(_M0L5entryS524);
      _M0L6_2atmpS1599 = _M0L5entryS524;
      _M0L6_2aoldS2163 = _M0L6_2atmpS1598->$1;
      if (_M0L6_2aoldS2163) {
        moonbit_decref(_M0L6_2aoldS2163);
      }
      _M0L6_2atmpS1598->$1 = _M0L6_2atmpS1599;
      moonbit_decref(_M0L6_2atmpS1598);
      break;
    }
  }
  _M0L4selfS523->$6 = _M0L3idxS525;
  _M0L8_2afieldS2161 = _M0L4selfS523->$0;
  _M0L7entriesS1602 = _M0L8_2afieldS2161;
  _M0L6_2atmpS1603 = _M0L5entryS524;
  if (
    _M0L3idxS525 < 0
    || _M0L3idxS525 >= Moonbit_array_length(_M0L7entriesS1602)
  ) {
    #line 453 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2160
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1602[
      _M0L3idxS525
    ];
  if (_M0L6_2aoldS2160) {
    moonbit_decref(_M0L6_2aoldS2160);
  }
  _M0L7entriesS1602[_M0L3idxS525] = _M0L6_2atmpS1603;
  _M0L4sizeS1605 = _M0L4selfS523->$1;
  _M0L6_2atmpS1604 = _M0L4sizeS1605 + 1;
  _M0L4selfS523->$1 = _M0L6_2atmpS1604;
  moonbit_decref(_M0L4selfS523);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS527,
  int32_t _M0L3idxS529,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS528
) {
  int32_t _M0L7_2abindS526;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2167;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1611;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1612;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2166;
  int32_t _M0L4sizeS1614;
  int32_t _M0L6_2atmpS1613;
  #line 443 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS526 = _M0L4selfS527->$6;
  switch (_M0L7_2abindS526) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1606;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2168;
      moonbit_incref(_M0L5entryS528);
      _M0L6_2atmpS1606 = _M0L5entryS528;
      _M0L6_2aoldS2168 = _M0L4selfS527->$5;
      if (_M0L6_2aoldS2168) {
        moonbit_decref(_M0L6_2aoldS2168);
      }
      _M0L4selfS527->$5 = _M0L6_2atmpS1606;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2171 =
        _M0L4selfS527->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1610 =
        _M0L8_2afieldS2171;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2170;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1609;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1607;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1608;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2169;
      if (
        _M0L7_2abindS526 < 0
        || _M0L7_2abindS526 >= Moonbit_array_length(_M0L7entriesS1610)
      ) {
        #line 450 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2170
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1610[
          _M0L7_2abindS526
        ];
      _M0L6_2atmpS1609 = _M0L6_2atmpS2170;
      if (_M0L6_2atmpS1609) {
        moonbit_incref(_M0L6_2atmpS1609);
      }
      #line 450 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS1607
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1609);
      moonbit_incref(_M0L5entryS528);
      _M0L6_2atmpS1608 = _M0L5entryS528;
      _M0L6_2aoldS2169 = _M0L6_2atmpS1607->$1;
      if (_M0L6_2aoldS2169) {
        moonbit_decref(_M0L6_2aoldS2169);
      }
      _M0L6_2atmpS1607->$1 = _M0L6_2atmpS1608;
      moonbit_decref(_M0L6_2atmpS1607);
      break;
    }
  }
  _M0L4selfS527->$6 = _M0L3idxS529;
  _M0L8_2afieldS2167 = _M0L4selfS527->$0;
  _M0L7entriesS1611 = _M0L8_2afieldS2167;
  _M0L6_2atmpS1612 = _M0L5entryS528;
  if (
    _M0L3idxS529 < 0
    || _M0L3idxS529 >= Moonbit_array_length(_M0L7entriesS1611)
  ) {
    #line 453 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2166
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1611[
      _M0L3idxS529
    ];
  if (_M0L6_2aoldS2166) {
    moonbit_decref(_M0L6_2aoldS2166);
  }
  _M0L7entriesS1611[_M0L3idxS529] = _M0L6_2atmpS1612;
  _M0L4sizeS1614 = _M0L4selfS527->$1;
  _M0L6_2atmpS1613 = _M0L4sizeS1614 + 1;
  _M0L4selfS527->$1 = _M0L6_2atmpS1613;
  moonbit_decref(_M0L4selfS527);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS511
) {
  int32_t _M0L8capacityS510;
  int32_t _M0L7_2abindS512;
  int32_t _M0L7_2abindS513;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1595;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS514;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS515;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_2419;
  #line 57 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS510
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS511);
  _M0L7_2abindS512 = _M0L8capacityS510 - 1;
  #line 63 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS513 = _M0FPB21calc__grow__threshold(_M0L8capacityS510);
  _M0L6_2atmpS1595 = 0;
  _M0L7_2abindS514
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS510, _M0L6_2atmpS1595);
  _M0L7_2abindS515 = 0;
  _block_2419
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_2419)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_2419->$0 = _M0L7_2abindS514;
  _block_2419->$1 = 0;
  _block_2419->$2 = _M0L8capacityS510;
  _block_2419->$3 = _M0L7_2abindS512;
  _block_2419->$4 = _M0L7_2abindS513;
  _block_2419->$5 = _M0L7_2abindS515;
  _block_2419->$6 = -1;
  return _block_2419;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS517
) {
  int32_t _M0L8capacityS516;
  int32_t _M0L7_2abindS518;
  int32_t _M0L7_2abindS519;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1596;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS520;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS521;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_2420;
  #line 57 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS516
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS517);
  _M0L7_2abindS518 = _M0L8capacityS516 - 1;
  #line 63 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS519 = _M0FPB21calc__grow__threshold(_M0L8capacityS516);
  _M0L6_2atmpS1596 = 0;
  _M0L7_2abindS520
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS516, _M0L6_2atmpS1596);
  _M0L7_2abindS521 = 0;
  _block_2420
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_2420)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_2420->$0 = _M0L7_2abindS520;
  _block_2420->$1 = 0;
  _block_2420->$2 = _M0L8capacityS516;
  _block_2420->$3 = _M0L7_2abindS518;
  _block_2420->$4 = _M0L7_2abindS519;
  _block_2420->$5 = _M0L7_2abindS521;
  _block_2420->$6 = -1;
  return _block_2420;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS509) {
  #line 33 "/home/blem/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS509 >= 0) {
    int32_t _M0L6_2atmpS1594;
    int32_t _M0L6_2atmpS1593;
    int32_t _M0L6_2atmpS1592;
    int32_t _M0L6_2atmpS1591;
    if (_M0L4selfS509 <= 1) {
      return 1;
    }
    if (_M0L4selfS509 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1594 = _M0L4selfS509 - 1;
    #line 44 "/home/blem/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS1593 = moonbit_clz32(_M0L6_2atmpS1594);
    _M0L6_2atmpS1592 = _M0L6_2atmpS1593 - 1;
    _M0L6_2atmpS1591 = 2147483647 >> (_M0L6_2atmpS1592 & 31);
    return _M0L6_2atmpS1591 + 1;
  } else {
    #line 34 "/home/blem/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS508) {
  int32_t _M0L6_2atmpS1590;
  #line 503 "/home/blem/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS1590 = _M0L8capacityS508 * 13;
  return _M0L6_2atmpS1590 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS504
) {
  #line 37 "/home/blem/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS504 == 0) {
    if (_M0L4selfS504) {
      moonbit_decref(_M0L4selfS504);
    }
    #line 39 "/home/blem/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS505 =
      _M0L4selfS504;
    return _M0L7_2aSomeS505;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS506
) {
  #line 37 "/home/blem/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS506 == 0) {
    if (_M0L4selfS506) {
      moonbit_decref(_M0L4selfS506);
    }
    #line 39 "/home/blem/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS507 =
      _M0L4selfS506;
    return _M0L7_2aSomeS507;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS503
) {
  moonbit_string_t* _M0L6_2atmpS1589;
  #line 165 "/home/blem/.moon/lib/core/builtin/readonlyarray.mbt"
  _M0L6_2atmpS1589 = _M0L4selfS503;
  #line 167 "/home/blem/.moon/lib/core/builtin/readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1589);
}

int32_t _M0IPC15array10FixedArrayPB4Show6outputGyE(
  moonbit_bytes_t _M0L4selfS502,
  struct _M0TPB6Logger _M0L6loggerS501
) {
  struct _M0TWEOy* _M0L6_2atmpS1588;
  #line 219 "/home/blem/.moon/lib/core/builtin/show.mbt"
  #line 220 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1588 = _M0MPC15array10FixedArray4iterGyE(_M0L4selfS502);
  #line 220 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0MPB6Logger19write__iter_2einnerGyE(_M0L6loggerS501, _M0L6_2atmpS1588, (moonbit_string_t)moonbit_string_literal_54.data, (moonbit_string_t)moonbit_string_literal_55.data, (moonbit_string_t)moonbit_string_literal_56.data, 0);
  return 0;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS499
) {
  moonbit_string_t* _M0L6_2atmpS1583;
  int32_t _M0L6_2atmpS2172;
  int32_t _M0L6_2atmpS1584;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1582;
  #line 1508 "/home/blem/.moon/lib/core/builtin/fixedarray.mbt"
  moonbit_incref(_M0L4selfS499);
  _M0L6_2atmpS1583 = _M0L4selfS499;
  _M0L6_2atmpS2172 = Moonbit_array_length(_M0L4selfS499);
  moonbit_decref(_M0L4selfS499);
  _M0L6_2atmpS1584 = _M0L6_2atmpS2172;
  _M0L6_2atmpS1582
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1584, _M0L6_2atmpS1583
  };
  #line 1510 "/home/blem/.moon/lib/core/builtin/fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1582);
}

struct _M0TWEOy* _M0MPC15array10FixedArray4iterGyE(
  moonbit_bytes_t _M0L4selfS500
) {
  moonbit_bytes_t _M0L6_2atmpS1586;
  int32_t _M0L6_2atmpS2173;
  int32_t _M0L6_2atmpS1587;
  struct _M0TPB9ArrayViewGyE _M0L6_2atmpS1585;
  #line 1508 "/home/blem/.moon/lib/core/builtin/fixedarray.mbt"
  moonbit_incref(_M0L4selfS500);
  _M0L6_2atmpS1586 = _M0L4selfS500;
  _M0L6_2atmpS2173 = Moonbit_array_length(_M0L4selfS500);
  moonbit_decref(_M0L4selfS500);
  _M0L6_2atmpS1587 = _M0L6_2atmpS2173;
  _M0L6_2atmpS1585
  = (struct _M0TPB9ArrayViewGyE){
    0, _M0L6_2atmpS1587, _M0L6_2atmpS1586
  };
  #line 1510 "/home/blem/.moon/lib/core/builtin/fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGyE(_M0L6_2atmpS1585);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS494
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS493;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1559__l570__* _closure_2421;
  struct _M0TWEOs* _M0L6_2atmpS1558;
  #line 567 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  _M0L1iS493
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS493)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS493->$0 = 0;
  _closure_2421
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1559__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1559__l570__));
  Moonbit_object_header(_closure_2421)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1559__l570__, $0_0) >> 2, 2, 0);
  _closure_2421->code = &_M0MPC15array9ArrayView4iterGsEC1559l570;
  _closure_2421->$0_0 = _M0L4selfS494.$0;
  _closure_2421->$0_1 = _M0L4selfS494.$1;
  _closure_2421->$0_2 = _M0L4selfS494.$2;
  _closure_2421->$1 = _M0L1iS493;
  _M0L6_2atmpS1558 = (struct _M0TWEOs*)_closure_2421;
  #line 570 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1558);
}

struct _M0TWEOy* _M0MPC15array9ArrayView4iterGyE(
  struct _M0TPB9ArrayViewGyE _M0L4selfS497
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS496;
  struct _M0R57ArrayView_3a_3aiter_7c_5bByte_5d_7c_2eanon__u1571__l570__* _closure_2422;
  struct _M0TWEOy* _M0L6_2atmpS1570;
  #line 567 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  _M0L1iS496
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS496)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS496->$0 = 0;
  _closure_2422
  = (struct _M0R57ArrayView_3a_3aiter_7c_5bByte_5d_7c_2eanon__u1571__l570__*)moonbit_malloc(sizeof(struct _M0R57ArrayView_3a_3aiter_7c_5bByte_5d_7c_2eanon__u1571__l570__));
  Moonbit_object_header(_closure_2422)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R57ArrayView_3a_3aiter_7c_5bByte_5d_7c_2eanon__u1571__l570__, $0_0) >> 2, 2, 0);
  _closure_2422->code = &_M0MPC15array9ArrayView4iterGyEC1571l570;
  _closure_2422->$0_0 = _M0L4selfS497.$0;
  _closure_2422->$0_1 = _M0L4selfS497.$1;
  _closure_2422->$0_2 = _M0L4selfS497.$2;
  _closure_2422->$1 = _M0L1iS496;
  _M0L6_2atmpS1570 = (struct _M0TWEOy*)_closure_2422;
  #line 570 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  return _M0MPB4Iter3newGyE(_M0L6_2atmpS1570);
}

int32_t _M0MPC15array9ArrayView4iterGyEC1571l570(
  struct _M0TWEOy* _M0L6_2aenvS1572
) {
  struct _M0R57ArrayView_3a_3aiter_7c_5bByte_5d_7c_2eanon__u1571__l570__* _M0L14_2acasted__envS1573;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2178;
  struct _M0TPC13ref3RefGiE* _M0L1iS496;
  struct _M0TPB9ArrayViewGyE _M0L8_2afieldS2177;
  int32_t _M0L6_2acntS2340;
  struct _M0TPB9ArrayViewGyE _M0L4selfS497;
  int32_t _M0L3valS1574;
  int32_t _M0L6_2atmpS1575;
  #line 570 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  _M0L14_2acasted__envS1573
  = (struct _M0R57ArrayView_3a_3aiter_7c_5bByte_5d_7c_2eanon__u1571__l570__*)_M0L6_2aenvS1572;
  _M0L8_2afieldS2178 = _M0L14_2acasted__envS1573->$1;
  _M0L1iS496 = _M0L8_2afieldS2178;
  _M0L8_2afieldS2177
  = (struct _M0TPB9ArrayViewGyE){
    _M0L14_2acasted__envS1573->$0_1,
      _M0L14_2acasted__envS1573->$0_2,
      _M0L14_2acasted__envS1573->$0_0
  };
  _M0L6_2acntS2340 = Moonbit_object_header(_M0L14_2acasted__envS1573)->rc;
  if (_M0L6_2acntS2340 > 1) {
    int32_t _M0L11_2anew__cntS2341 = _M0L6_2acntS2340 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1573)->rc
    = _M0L11_2anew__cntS2341;
    moonbit_incref(_M0L1iS496);
    moonbit_incref(_M0L8_2afieldS2177.$0);
  } else if (_M0L6_2acntS2340 == 1) {
    #line 570 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1573);
  }
  _M0L4selfS497 = _M0L8_2afieldS2177;
  _M0L3valS1574 = _M0L1iS496->$0;
  moonbit_incref(_M0L4selfS497.$0);
  #line 571 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  _M0L6_2atmpS1575 = _M0MPC15array9ArrayView6lengthGyE(_M0L4selfS497);
  if (_M0L3valS1574 < _M0L6_2atmpS1575) {
    moonbit_bytes_t _M0L8_2afieldS2176 = _M0L4selfS497.$0;
    moonbit_bytes_t _M0L3bufS1578 = _M0L8_2afieldS2176;
    int32_t _M0L8_2afieldS2175 = _M0L4selfS497.$1;
    int32_t _M0L5startS1580 = _M0L8_2afieldS2175;
    int32_t _M0L3valS1581 = _M0L1iS496->$0;
    int32_t _M0L6_2atmpS1579 = _M0L5startS1580 + _M0L3valS1581;
    int32_t _M0L6_2atmpS2174 = (int32_t)_M0L3bufS1578[_M0L6_2atmpS1579];
    int32_t _M0L4elemS498;
    int32_t _M0L3valS1577;
    int32_t _M0L6_2atmpS1576;
    moonbit_decref(_M0L3bufS1578);
    _M0L4elemS498 = _M0L6_2atmpS2174;
    _M0L3valS1577 = _M0L1iS496->$0;
    _M0L6_2atmpS1576 = _M0L3valS1577 + 1;
    _M0L1iS496->$0 = _M0L6_2atmpS1576;
    moonbit_decref(_M0L1iS496);
    return _M0L4elemS498;
  } else {
    moonbit_decref(_M0L4selfS497.$0);
    moonbit_decref(_M0L1iS496);
    return -1;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1559l570(
  struct _M0TWEOs* _M0L6_2aenvS1560
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1559__l570__* _M0L14_2acasted__envS1561;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2183;
  struct _M0TPC13ref3RefGiE* _M0L1iS493;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS2182;
  int32_t _M0L6_2acntS2342;
  struct _M0TPB9ArrayViewGsE _M0L4selfS494;
  int32_t _M0L3valS1562;
  int32_t _M0L6_2atmpS1563;
  #line 570 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  _M0L14_2acasted__envS1561
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1559__l570__*)_M0L6_2aenvS1560;
  _M0L8_2afieldS2183 = _M0L14_2acasted__envS1561->$1;
  _M0L1iS493 = _M0L8_2afieldS2183;
  _M0L8_2afieldS2182
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1561->$0_1,
      _M0L14_2acasted__envS1561->$0_2,
      _M0L14_2acasted__envS1561->$0_0
  };
  _M0L6_2acntS2342 = Moonbit_object_header(_M0L14_2acasted__envS1561)->rc;
  if (_M0L6_2acntS2342 > 1) {
    int32_t _M0L11_2anew__cntS2343 = _M0L6_2acntS2342 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1561)->rc
    = _M0L11_2anew__cntS2343;
    moonbit_incref(_M0L1iS493);
    moonbit_incref(_M0L8_2afieldS2182.$0);
  } else if (_M0L6_2acntS2342 == 1) {
    #line 570 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1561);
  }
  _M0L4selfS494 = _M0L8_2afieldS2182;
  _M0L3valS1562 = _M0L1iS493->$0;
  moonbit_incref(_M0L4selfS494.$0);
  #line 571 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  _M0L6_2atmpS1563 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS494);
  if (_M0L3valS1562 < _M0L6_2atmpS1563) {
    moonbit_string_t* _M0L8_2afieldS2181 = _M0L4selfS494.$0;
    moonbit_string_t* _M0L3bufS1566 = _M0L8_2afieldS2181;
    int32_t _M0L8_2afieldS2180 = _M0L4selfS494.$1;
    int32_t _M0L5startS1568 = _M0L8_2afieldS2180;
    int32_t _M0L3valS1569 = _M0L1iS493->$0;
    int32_t _M0L6_2atmpS1567 = _M0L5startS1568 + _M0L3valS1569;
    moonbit_string_t _M0L6_2atmpS2179 =
      (moonbit_string_t)_M0L3bufS1566[_M0L6_2atmpS1567];
    moonbit_string_t _M0L4elemS495;
    int32_t _M0L3valS1565;
    int32_t _M0L6_2atmpS1564;
    moonbit_incref(_M0L6_2atmpS2179);
    moonbit_decref(_M0L3bufS1566);
    _M0L4elemS495 = _M0L6_2atmpS2179;
    _M0L3valS1565 = _M0L1iS493->$0;
    _M0L6_2atmpS1564 = _M0L3valS1565 + 1;
    _M0L1iS493->$0 = _M0L6_2atmpS1564;
    moonbit_decref(_M0L1iS493);
    return _M0L4elemS495;
  } else {
    moonbit_decref(_M0L4selfS494.$0);
    moonbit_decref(_M0L1iS493);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS492
) {
  #line 168 "/home/blem/.moon/lib/core/builtin/show.mbt"
  return _M0L4selfS492;
}

int32_t _M0IPC14byte4BytePB4Show6output(
  int32_t _M0L4selfS491,
  struct _M0TPB6Logger _M0L6loggerS490
) {
  moonbit_string_t _M0L6_2atmpS1557;
  #line 50 "/home/blem/.moon/lib/core/builtin/show.mbt"
  #line 51 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1557 = _M0MPC14byte4Byte10to__string(_M0L4selfS491);
  #line 51 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS490.$0->$method_0(_M0L6loggerS490.$1, _M0L6_2atmpS1557);
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte10to__string(int32_t _M0L4selfS487) {
  int32_t _M0L1iS486;
  int32_t _M0L6_2atmpS1556;
  moonbit_string_t _M0L2hiS488;
  int32_t _M0L6_2atmpS1555;
  moonbit_string_t _M0L2loS489;
  moonbit_string_t _M0L6_2atmpS1554;
  moonbit_string_t _M0L6_2atmpS2186;
  moonbit_string_t _M0L6_2atmpS1552;
  moonbit_string_t _M0L6_2atmpS1553;
  moonbit_string_t _M0L6_2atmpS2185;
  moonbit_string_t _M0L6_2atmpS1551;
  moonbit_string_t _M0L6_2atmpS2184;
  #line 193 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L1iS486 = (int32_t)_M0L4selfS487;
  _M0L6_2atmpS1556 = _M0L1iS486 / 16;
  #line 195 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L2hiS488 = _M0FPB8alphabet(_M0L6_2atmpS1556);
  _M0L6_2atmpS1555 = _M0L1iS486 % 16;
  #line 196 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L2loS489 = _M0FPB8alphabet(_M0L6_2atmpS1555);
  #line 197 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1554 = _M0IPC16string6StringPB4Show10to__string(_M0L2hiS488);
  #line 196 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS2186
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_57.data, _M0L6_2atmpS1554);
  moonbit_decref(_M0L6_2atmpS1554);
  _M0L6_2atmpS1552 = _M0L6_2atmpS2186;
  #line 197 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1553 = _M0IPC16string6StringPB4Show10to__string(_M0L2loS489);
  #line 196 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS2185 = moonbit_add_string(_M0L6_2atmpS1552, _M0L6_2atmpS1553);
  moonbit_decref(_M0L6_2atmpS1552);
  moonbit_decref(_M0L6_2atmpS1553);
  _M0L6_2atmpS1551 = _M0L6_2atmpS2185;
  #line 196 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS2184
  = moonbit_add_string(_M0L6_2atmpS1551, (moonbit_string_t)moonbit_string_literal_58.data);
  moonbit_decref(_M0L6_2atmpS1551);
  return _M0L6_2atmpS2184;
}

moonbit_string_t _M0FPB8alphabet(int32_t _M0L1xS485) {
  #line 162 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  switch (_M0L1xS485) {
    case 0: {
      return (moonbit_string_t)moonbit_string_literal_59.data;
      break;
    }
    
    case 1: {
      return (moonbit_string_t)moonbit_string_literal_60.data;
      break;
    }
    
    case 2: {
      return (moonbit_string_t)moonbit_string_literal_61.data;
      break;
    }
    
    case 3: {
      return (moonbit_string_t)moonbit_string_literal_62.data;
      break;
    }
    
    case 4: {
      return (moonbit_string_t)moonbit_string_literal_63.data;
      break;
    }
    
    case 5: {
      return (moonbit_string_t)moonbit_string_literal_64.data;
      break;
    }
    
    case 6: {
      return (moonbit_string_t)moonbit_string_literal_65.data;
      break;
    }
    
    case 7: {
      return (moonbit_string_t)moonbit_string_literal_66.data;
      break;
    }
    
    case 8: {
      return (moonbit_string_t)moonbit_string_literal_67.data;
      break;
    }
    
    case 9: {
      return (moonbit_string_t)moonbit_string_literal_68.data;
      break;
    }
    
    case 10: {
      return (moonbit_string_t)moonbit_string_literal_69.data;
      break;
    }
    
    case 11: {
      return (moonbit_string_t)moonbit_string_literal_70.data;
      break;
    }
    
    case 12: {
      return (moonbit_string_t)moonbit_string_literal_71.data;
      break;
    }
    
    case 13: {
      return (moonbit_string_t)moonbit_string_literal_72.data;
      break;
    }
    
    case 14: {
      return (moonbit_string_t)moonbit_string_literal_73.data;
      break;
    }
    
    case 15: {
      return (moonbit_string_t)moonbit_string_literal_74.data;
      break;
    }
    default: {
      #line 180 "/home/blem/.moon/lib/core/builtin/byte.mbt"
      return _M0FPB5abortGsE((moonbit_string_t)moonbit_string_literal_75.data, (moonbit_string_t)moonbit_string_literal_76.data);
      break;
    }
  }
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS484,
  struct _M0TPB6Logger _M0L6loggerS483
) {
  moonbit_string_t _M0L6_2atmpS1550;
  #line 30 "/home/blem/.moon/lib/core/builtin/show.mbt"
  #line 31 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1550 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS484, 10);
  #line 31 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS483.$0->$method_0(_M0L6loggerS483.$1, _M0L6_2atmpS1550);
  return 0;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS477,
  moonbit_string_t _M0L5valueS479
) {
  int32_t _M0L3lenS1540;
  moonbit_string_t* _M0L6_2atmpS1542;
  int32_t _M0L6_2atmpS2189;
  int32_t _M0L6_2atmpS1541;
  int32_t _M0L6lengthS478;
  moonbit_string_t* _M0L8_2afieldS2188;
  moonbit_string_t* _M0L3bufS1543;
  moonbit_string_t _M0L6_2aoldS2187;
  int32_t _M0L6_2atmpS1544;
  #line 242 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1540 = _M0L4selfS477->$1;
  moonbit_incref(_M0L4selfS477);
  #line 243 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1542 = _M0MPC15array5Array6bufferGsE(_M0L4selfS477);
  _M0L6_2atmpS2189 = Moonbit_array_length(_M0L6_2atmpS1542);
  moonbit_decref(_M0L6_2atmpS1542);
  _M0L6_2atmpS1541 = _M0L6_2atmpS2189;
  if (_M0L3lenS1540 == _M0L6_2atmpS1541) {
    moonbit_incref(_M0L4selfS477);
    #line 244 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS477);
  }
  _M0L6lengthS478 = _M0L4selfS477->$1;
  _M0L8_2afieldS2188 = _M0L4selfS477->$0;
  _M0L3bufS1543 = _M0L8_2afieldS2188;
  _M0L6_2aoldS2187 = (moonbit_string_t)_M0L3bufS1543[_M0L6lengthS478];
  moonbit_decref(_M0L6_2aoldS2187);
  _M0L3bufS1543[_M0L6lengthS478] = _M0L5valueS479;
  _M0L6_2atmpS1544 = _M0L6lengthS478 + 1;
  _M0L4selfS477->$1 = _M0L6_2atmpS1544;
  moonbit_decref(_M0L4selfS477);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS480,
  struct _M0TUsiE* _M0L5valueS482
) {
  int32_t _M0L3lenS1545;
  struct _M0TUsiE** _M0L6_2atmpS1547;
  int32_t _M0L6_2atmpS2192;
  int32_t _M0L6_2atmpS1546;
  int32_t _M0L6lengthS481;
  struct _M0TUsiE** _M0L8_2afieldS2191;
  struct _M0TUsiE** _M0L3bufS1548;
  struct _M0TUsiE* _M0L6_2aoldS2190;
  int32_t _M0L6_2atmpS1549;
  #line 242 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1545 = _M0L4selfS480->$1;
  moonbit_incref(_M0L4selfS480);
  #line 243 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS1547 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS480);
  _M0L6_2atmpS2192 = Moonbit_array_length(_M0L6_2atmpS1547);
  moonbit_decref(_M0L6_2atmpS1547);
  _M0L6_2atmpS1546 = _M0L6_2atmpS2192;
  if (_M0L3lenS1545 == _M0L6_2atmpS1546) {
    moonbit_incref(_M0L4selfS480);
    #line 244 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS480);
  }
  _M0L6lengthS481 = _M0L4selfS480->$1;
  _M0L8_2afieldS2191 = _M0L4selfS480->$0;
  _M0L3bufS1548 = _M0L8_2afieldS2191;
  _M0L6_2aoldS2190 = (struct _M0TUsiE*)_M0L3bufS1548[_M0L6lengthS481];
  if (_M0L6_2aoldS2190) {
    moonbit_decref(_M0L6_2aoldS2190);
  }
  _M0L3bufS1548[_M0L6lengthS481] = _M0L5valueS482;
  _M0L6_2atmpS1549 = _M0L6lengthS481 + 1;
  _M0L4selfS480->$1 = _M0L6_2atmpS1549;
  moonbit_decref(_M0L4selfS480);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS472) {
  int32_t _M0L8old__capS471;
  int32_t _M0L8new__capS473;
  #line 182 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS471 = _M0L4selfS472->$1;
  if (_M0L8old__capS471 == 0) {
    _M0L8new__capS473 = 8;
  } else {
    _M0L8new__capS473 = _M0L8old__capS471 * 2;
  }
  #line 185 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS472, _M0L8new__capS473);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS475
) {
  int32_t _M0L8old__capS474;
  int32_t _M0L8new__capS476;
  #line 182 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS474 = _M0L4selfS475->$1;
  if (_M0L8old__capS474 == 0) {
    _M0L8new__capS476 = 8;
  } else {
    _M0L8new__capS476 = _M0L8old__capS474 * 2;
  }
  #line 185 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS475, _M0L8new__capS476);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS462,
  int32_t _M0L13new__capacityS460
) {
  moonbit_string_t* _M0L8new__bufS459;
  moonbit_string_t* _M0L8_2afieldS2194;
  moonbit_string_t* _M0L8old__bufS461;
  int32_t _M0L8old__capS463;
  int32_t _M0L9copy__lenS464;
  moonbit_string_t* _M0L6_2aoldS2193;
  #line 129 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS459
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS460, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS2194 = _M0L4selfS462->$0;
  _M0L8old__bufS461 = _M0L8_2afieldS2194;
  _M0L8old__capS463 = Moonbit_array_length(_M0L8old__bufS461);
  if (_M0L8old__capS463 < _M0L13new__capacityS460) {
    _M0L9copy__lenS464 = _M0L8old__capS463;
  } else {
    _M0L9copy__lenS464 = _M0L13new__capacityS460;
  }
  moonbit_incref(_M0L8old__bufS461);
  moonbit_incref(_M0L8new__bufS459);
  #line 134 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS459, 0, _M0L8old__bufS461, 0, _M0L9copy__lenS464);
  _M0L6_2aoldS2193 = _M0L4selfS462->$0;
  moonbit_decref(_M0L6_2aoldS2193);
  _M0L4selfS462->$0 = _M0L8new__bufS459;
  moonbit_decref(_M0L4selfS462);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS468,
  int32_t _M0L13new__capacityS466
) {
  struct _M0TUsiE** _M0L8new__bufS465;
  struct _M0TUsiE** _M0L8_2afieldS2196;
  struct _M0TUsiE** _M0L8old__bufS467;
  int32_t _M0L8old__capS469;
  int32_t _M0L9copy__lenS470;
  struct _M0TUsiE** _M0L6_2aoldS2195;
  #line 129 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS465
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS466, 0);
  _M0L8_2afieldS2196 = _M0L4selfS468->$0;
  _M0L8old__bufS467 = _M0L8_2afieldS2196;
  _M0L8old__capS469 = Moonbit_array_length(_M0L8old__bufS467);
  if (_M0L8old__capS469 < _M0L13new__capacityS466) {
    _M0L9copy__lenS470 = _M0L8old__capS469;
  } else {
    _M0L9copy__lenS470 = _M0L13new__capacityS466;
  }
  moonbit_incref(_M0L8old__bufS467);
  moonbit_incref(_M0L8new__bufS465);
  #line 134 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS465, 0, _M0L8old__bufS467, 0, _M0L9copy__lenS470);
  _M0L6_2aoldS2195 = _M0L4selfS468->$0;
  moonbit_decref(_M0L6_2aoldS2195);
  _M0L4selfS468->$0 = _M0L8new__bufS465;
  moonbit_decref(_M0L4selfS468);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS458
) {
  #line 53 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  if (_M0L8capacityS458 == 0) {
    moonbit_string_t* _M0L6_2atmpS1538 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_2423 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2423)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2423->$0 = _M0L6_2atmpS1538;
    _block_2423->$1 = 0;
    return _block_2423;
  } else {
    moonbit_string_t* _M0L6_2atmpS1539 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS458, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_2424 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2424)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2424->$0 = _M0L6_2atmpS1539;
    _block_2424->$1 = 0;
    return _block_2424;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS456,
  struct _M0TPC16string10StringView _M0L3strS457
) {
  int32_t _M0L3lenS1526;
  int32_t _M0L6_2atmpS1528;
  int32_t _M0L6_2atmpS1527;
  int32_t _M0L6_2atmpS1525;
  moonbit_bytes_t _M0L8_2afieldS2197;
  moonbit_bytes_t _M0L4dataS1529;
  int32_t _M0L3lenS1530;
  moonbit_string_t _M0L6_2atmpS1531;
  int32_t _M0L6_2atmpS1532;
  int32_t _M0L6_2atmpS1533;
  int32_t _M0L3lenS1535;
  int32_t _M0L6_2atmpS1537;
  int32_t _M0L6_2atmpS1536;
  int32_t _M0L6_2atmpS1534;
  #line 99 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS1526 = _M0L4selfS456->$1;
  moonbit_incref(_M0L3strS457.$0);
  #line 103 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1528 = _M0MPC16string10StringView6length(_M0L3strS457);
  _M0L6_2atmpS1527 = _M0L6_2atmpS1528 * 2;
  _M0L6_2atmpS1525 = _M0L3lenS1526 + _M0L6_2atmpS1527;
  moonbit_incref(_M0L4selfS456);
  #line 103 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS456, _M0L6_2atmpS1525);
  _M0L8_2afieldS2197 = _M0L4selfS456->$0;
  _M0L4dataS1529 = _M0L8_2afieldS2197;
  _M0L3lenS1530 = _M0L4selfS456->$1;
  moonbit_incref(_M0L4dataS1529);
  moonbit_incref(_M0L3strS457.$0);
  #line 106 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1531 = _M0MPC16string10StringView4data(_M0L3strS457);
  moonbit_incref(_M0L3strS457.$0);
  #line 107 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1532 = _M0MPC16string10StringView13start__offset(_M0L3strS457);
  moonbit_incref(_M0L3strS457.$0);
  #line 108 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1533 = _M0MPC16string10StringView6length(_M0L3strS457);
  #line 104 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1529, _M0L3lenS1530, _M0L6_2atmpS1531, _M0L6_2atmpS1532, _M0L6_2atmpS1533);
  _M0L3lenS1535 = _M0L4selfS456->$1;
  #line 110 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1537 = _M0MPC16string10StringView6length(_M0L3strS457);
  _M0L6_2atmpS1536 = _M0L6_2atmpS1537 * 2;
  _M0L6_2atmpS1534 = _M0L3lenS1535 + _M0L6_2atmpS1536;
  _M0L4selfS456->$1 = _M0L6_2atmpS1534;
  moonbit_decref(_M0L4selfS456);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS448,
  int32_t _M0L3lenS451,
  int32_t _M0L13start__offsetS455,
  int64_t _M0L11end__offsetS446
) {
  int32_t _M0L11end__offsetS445;
  int32_t _M0L5indexS449;
  int32_t _M0L5countS450;
  #line 438 "/home/blem/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS446 == 4294967296ll) {
    _M0L11end__offsetS445 = Moonbit_array_length(_M0L4selfS448);
  } else {
    int64_t _M0L7_2aSomeS447 = _M0L11end__offsetS446;
    _M0L11end__offsetS445 = (int32_t)_M0L7_2aSomeS447;
  }
  _M0L5indexS449 = _M0L13start__offsetS455;
  _M0L5countS450 = 0;
  while (1) {
    int32_t _if__result_2426;
    if (_M0L5indexS449 < _M0L11end__offsetS445) {
      _if__result_2426 = _M0L5countS450 < _M0L3lenS451;
    } else {
      _if__result_2426 = 0;
    }
    if (_if__result_2426) {
      int32_t _M0L2c1S452 = _M0L4selfS448[_M0L5indexS449];
      int32_t _if__result_2427;
      int32_t _M0L6_2atmpS1523;
      int32_t _M0L6_2atmpS1524;
      #line 449 "/home/blem/.moon/lib/core/builtin/string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S452)) {
        int32_t _M0L6_2atmpS1519 = _M0L5indexS449 + 1;
        _if__result_2427 = _M0L6_2atmpS1519 < _M0L11end__offsetS445;
      } else {
        _if__result_2427 = 0;
      }
      if (_if__result_2427) {
        int32_t _M0L6_2atmpS1522 = _M0L5indexS449 + 1;
        int32_t _M0L2c2S453 = _M0L4selfS448[_M0L6_2atmpS1522];
        #line 451 "/home/blem/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S453)) {
          int32_t _M0L6_2atmpS1520 = _M0L5indexS449 + 2;
          int32_t _M0L6_2atmpS1521 = _M0L5countS450 + 1;
          _M0L5indexS449 = _M0L6_2atmpS1520;
          _M0L5countS450 = _M0L6_2atmpS1521;
          continue;
        } else {
          #line 454 "/home/blem/.moon/lib/core/builtin/string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_77.data, (moonbit_string_t)moonbit_string_literal_78.data);
        }
      }
      _M0L6_2atmpS1523 = _M0L5indexS449 + 1;
      _M0L6_2atmpS1524 = _M0L5countS450 + 1;
      _M0L5indexS449 = _M0L6_2atmpS1523;
      _M0L5countS450 = _M0L6_2atmpS1524;
      continue;
    } else {
      moonbit_decref(_M0L4selfS448);
      return _M0L5countS450 >= _M0L3lenS451;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS441
) {
  int32_t _M0L3endS1511;
  int32_t _M0L8_2afieldS2198;
  int32_t _M0L5startS1512;
  #line 71 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1511 = _M0L4selfS441.$2;
  _M0L8_2afieldS2198 = _M0L4selfS441.$1;
  moonbit_decref(_M0L4selfS441.$0);
  _M0L5startS1512 = _M0L8_2afieldS2198;
  return _M0L3endS1511 - _M0L5startS1512;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS442
) {
  int32_t _M0L3endS1513;
  int32_t _M0L8_2afieldS2199;
  int32_t _M0L5startS1514;
  #line 71 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1513 = _M0L4selfS442.$2;
  _M0L8_2afieldS2199 = _M0L4selfS442.$1;
  moonbit_decref(_M0L4selfS442.$0);
  _M0L5startS1514 = _M0L8_2afieldS2199;
  return _M0L3endS1513 - _M0L5startS1514;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS443
) {
  int32_t _M0L3endS1515;
  int32_t _M0L8_2afieldS2200;
  int32_t _M0L5startS1516;
  #line 71 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1515 = _M0L4selfS443.$2;
  _M0L8_2afieldS2200 = _M0L4selfS443.$1;
  moonbit_decref(_M0L4selfS443.$0);
  _M0L5startS1516 = _M0L8_2afieldS2200;
  return _M0L3endS1515 - _M0L5startS1516;
}

int32_t _M0MPC15array9ArrayView6lengthGyE(
  struct _M0TPB9ArrayViewGyE _M0L4selfS444
) {
  int32_t _M0L3endS1517;
  int32_t _M0L8_2afieldS2201;
  int32_t _M0L5startS1518;
  #line 71 "/home/blem/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS1517 = _M0L4selfS444.$2;
  _M0L8_2afieldS2201 = _M0L4selfS444.$1;
  moonbit_decref(_M0L4selfS444.$0);
  _M0L5startS1518 = _M0L8_2afieldS2201;
  return _M0L3endS1517 - _M0L5startS1518;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS439,
  int64_t _M0L19start__offset_2eoptS437,
  int64_t _M0L11end__offsetS440
) {
  int32_t _M0L13start__offsetS436;
  if (_M0L19start__offset_2eoptS437 == 4294967296ll) {
    _M0L13start__offsetS436 = 0;
  } else {
    int64_t _M0L7_2aSomeS438 = _M0L19start__offset_2eoptS437;
    _M0L13start__offsetS436 = (int32_t)_M0L7_2aSomeS438;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS439, _M0L13start__offsetS436, _M0L11end__offsetS440);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS434,
  int32_t _M0L13start__offsetS435,
  int64_t _M0L11end__offsetS432
) {
  int32_t _M0L11end__offsetS431;
  int32_t _if__result_2428;
  #line 390 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS432 == 4294967296ll) {
    _M0L11end__offsetS431 = Moonbit_array_length(_M0L4selfS434);
  } else {
    int64_t _M0L7_2aSomeS433 = _M0L11end__offsetS432;
    _M0L11end__offsetS431 = (int32_t)_M0L7_2aSomeS433;
  }
  if (_M0L13start__offsetS435 >= 0) {
    if (_M0L13start__offsetS435 <= _M0L11end__offsetS431) {
      int32_t _M0L6_2atmpS1510 = Moonbit_array_length(_M0L4selfS434);
      _if__result_2428 = _M0L11end__offsetS431 <= _M0L6_2atmpS1510;
    } else {
      _if__result_2428 = 0;
    }
  } else {
    _if__result_2428 = 0;
  }
  if (_if__result_2428) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS435,
                                                 _M0L11end__offsetS431,
                                                 _M0L4selfS434};
  } else {
    moonbit_decref(_M0L4selfS434);
    #line 399 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_52.data, (moonbit_string_t)moonbit_string_literal_79.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS430
) {
  moonbit_string_t _M0L8_2afieldS2203;
  moonbit_string_t _M0L3strS1507;
  int32_t _M0L5startS1508;
  int32_t _M0L8_2afieldS2202;
  int32_t _M0L3endS1509;
  #line 186 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS2203 = _M0L4selfS430.$0;
  _M0L3strS1507 = _M0L8_2afieldS2203;
  _M0L5startS1508 = _M0L4selfS430.$1;
  _M0L8_2afieldS2202 = _M0L4selfS430.$2;
  _M0L3endS1509 = _M0L8_2afieldS2202;
  #line 188 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1507, _M0L5startS1508, _M0L3endS1509);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS428,
  struct _M0TPB6Logger _M0L6loggerS429
) {
  moonbit_string_t _M0L8_2afieldS2205;
  moonbit_string_t _M0L3strS1504;
  int32_t _M0L5startS1505;
  int32_t _M0L8_2afieldS2204;
  int32_t _M0L3endS1506;
  moonbit_string_t _M0L6substrS427;
  #line 166 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS2205 = _M0L4selfS428.$0;
  _M0L3strS1504 = _M0L8_2afieldS2205;
  _M0L5startS1505 = _M0L4selfS428.$1;
  _M0L8_2afieldS2204 = _M0L4selfS428.$2;
  _M0L3endS1506 = _M0L8_2afieldS2204;
  #line 167 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L6substrS427
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1504, _M0L5startS1505, _M0L3endS1506);
  #line 168 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS427, _M0L6loggerS429);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS419,
  struct _M0TPB6Logger _M0L6loggerS417
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS418;
  int32_t _M0L3lenS420;
  int32_t _M0L1iS421;
  int32_t _M0L3segS422;
  #line 88 "/home/blem/.moon/lib/core/builtin/show.mbt"
  if (_M0L6loggerS417.$1) {
    moonbit_incref(_M0L6loggerS417.$1);
  }
  #line 89 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS417.$0->$method_3(_M0L6loggerS417.$1, 34);
  moonbit_incref(_M0L4selfS419);
  if (_M0L6loggerS417.$1) {
    moonbit_incref(_M0L6loggerS417.$1);
  }
  _M0L6_2aenvS418
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS418)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS418->$0 = _M0L4selfS419;
  _M0L6_2aenvS418->$1_0 = _M0L6loggerS417.$0;
  _M0L6_2aenvS418->$1_1 = _M0L6loggerS417.$1;
  _M0L3lenS420 = Moonbit_array_length(_M0L4selfS419);
  _M0L1iS421 = 0;
  _M0L3segS422 = 0;
  _2afor_423:;
  while (1) {
    int32_t _M0L4codeS424;
    int32_t _M0L1cS426;
    int32_t _M0L6_2atmpS1488;
    int32_t _M0L6_2atmpS1489;
    int32_t _M0L6_2atmpS1490;
    int32_t _tmp_2432;
    int32_t _tmp_2433;
    if (_M0L1iS421 >= _M0L3lenS420) {
      moonbit_decref(_M0L4selfS419);
      #line 105 "/home/blem/.moon/lib/core/builtin/show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS418, _M0L3segS422, _M0L1iS421);
      break;
    }
    _M0L4codeS424 = _M0L4selfS419[_M0L1iS421];
    switch (_M0L4codeS424) {
      case 34: {
        _M0L1cS426 = _M0L4codeS424;
        goto join_425;
        break;
      }
      
      case 92: {
        _M0L1cS426 = _M0L4codeS424;
        goto join_425;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1491;
        int32_t _M0L6_2atmpS1492;
        moonbit_incref(_M0L6_2aenvS418);
        #line 118 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS418, _M0L3segS422, _M0L1iS421);
        if (_M0L6loggerS417.$1) {
          moonbit_incref(_M0L6loggerS417.$1);
        }
        #line 119 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS417.$0->$method_0(_M0L6loggerS417.$1, (moonbit_string_t)moonbit_string_literal_80.data);
        _M0L6_2atmpS1491 = _M0L1iS421 + 1;
        _M0L6_2atmpS1492 = _M0L1iS421 + 1;
        _M0L1iS421 = _M0L6_2atmpS1491;
        _M0L3segS422 = _M0L6_2atmpS1492;
        goto _2afor_423;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1493;
        int32_t _M0L6_2atmpS1494;
        moonbit_incref(_M0L6_2aenvS418);
        #line 123 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS418, _M0L3segS422, _M0L1iS421);
        if (_M0L6loggerS417.$1) {
          moonbit_incref(_M0L6loggerS417.$1);
        }
        #line 124 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS417.$0->$method_0(_M0L6loggerS417.$1, (moonbit_string_t)moonbit_string_literal_81.data);
        _M0L6_2atmpS1493 = _M0L1iS421 + 1;
        _M0L6_2atmpS1494 = _M0L1iS421 + 1;
        _M0L1iS421 = _M0L6_2atmpS1493;
        _M0L3segS422 = _M0L6_2atmpS1494;
        goto _2afor_423;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1495;
        int32_t _M0L6_2atmpS1496;
        moonbit_incref(_M0L6_2aenvS418);
        #line 128 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS418, _M0L3segS422, _M0L1iS421);
        if (_M0L6loggerS417.$1) {
          moonbit_incref(_M0L6loggerS417.$1);
        }
        #line 129 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS417.$0->$method_0(_M0L6loggerS417.$1, (moonbit_string_t)moonbit_string_literal_82.data);
        _M0L6_2atmpS1495 = _M0L1iS421 + 1;
        _M0L6_2atmpS1496 = _M0L1iS421 + 1;
        _M0L1iS421 = _M0L6_2atmpS1495;
        _M0L3segS422 = _M0L6_2atmpS1496;
        goto _2afor_423;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1497;
        int32_t _M0L6_2atmpS1498;
        moonbit_incref(_M0L6_2aenvS418);
        #line 133 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS418, _M0L3segS422, _M0L1iS421);
        if (_M0L6loggerS417.$1) {
          moonbit_incref(_M0L6loggerS417.$1);
        }
        #line 134 "/home/blem/.moon/lib/core/builtin/show.mbt"
        _M0L6loggerS417.$0->$method_0(_M0L6loggerS417.$1, (moonbit_string_t)moonbit_string_literal_83.data);
        _M0L6_2atmpS1497 = _M0L1iS421 + 1;
        _M0L6_2atmpS1498 = _M0L1iS421 + 1;
        _M0L1iS421 = _M0L6_2atmpS1497;
        _M0L3segS422 = _M0L6_2atmpS1498;
        goto _2afor_423;
        break;
      }
      default: {
        if (_M0L4codeS424 < 32) {
          int32_t _M0L6_2atmpS1500;
          moonbit_string_t _M0L6_2atmpS1499;
          int32_t _M0L6_2atmpS1501;
          int32_t _M0L6_2atmpS1502;
          moonbit_incref(_M0L6_2aenvS418);
          #line 140 "/home/blem/.moon/lib/core/builtin/show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS418, _M0L3segS422, _M0L1iS421);
          if (_M0L6loggerS417.$1) {
            moonbit_incref(_M0L6loggerS417.$1);
          }
          #line 141 "/home/blem/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS417.$0->$method_0(_M0L6loggerS417.$1, (moonbit_string_t)moonbit_string_literal_84.data);
          _M0L6_2atmpS1500 = _M0L4codeS424 & 0xff;
          #line 142 "/home/blem/.moon/lib/core/builtin/show.mbt"
          _M0L6_2atmpS1499 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1500);
          if (_M0L6loggerS417.$1) {
            moonbit_incref(_M0L6loggerS417.$1);
          }
          #line 142 "/home/blem/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS417.$0->$method_0(_M0L6loggerS417.$1, _M0L6_2atmpS1499);
          if (_M0L6loggerS417.$1) {
            moonbit_incref(_M0L6loggerS417.$1);
          }
          #line 143 "/home/blem/.moon/lib/core/builtin/show.mbt"
          _M0L6loggerS417.$0->$method_3(_M0L6loggerS417.$1, 125);
          _M0L6_2atmpS1501 = _M0L1iS421 + 1;
          _M0L6_2atmpS1502 = _M0L1iS421 + 1;
          _M0L1iS421 = _M0L6_2atmpS1501;
          _M0L3segS422 = _M0L6_2atmpS1502;
          goto _2afor_423;
        } else {
          int32_t _M0L6_2atmpS1503 = _M0L1iS421 + 1;
          int32_t _tmp_2431 = _M0L3segS422;
          _M0L1iS421 = _M0L6_2atmpS1503;
          _M0L3segS422 = _tmp_2431;
          goto _2afor_423;
        }
        break;
      }
    }
    goto joinlet_2430;
    join_425:;
    moonbit_incref(_M0L6_2aenvS418);
    #line 111 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS418, _M0L3segS422, _M0L1iS421);
    if (_M0L6loggerS417.$1) {
      moonbit_incref(_M0L6loggerS417.$1);
    }
    #line 112 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS417.$0->$method_3(_M0L6loggerS417.$1, 92);
    #line 113 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1488 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS426);
    if (_M0L6loggerS417.$1) {
      moonbit_incref(_M0L6loggerS417.$1);
    }
    #line 113 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS417.$0->$method_3(_M0L6loggerS417.$1, _M0L6_2atmpS1488);
    _M0L6_2atmpS1489 = _M0L1iS421 + 1;
    _M0L6_2atmpS1490 = _M0L1iS421 + 1;
    _M0L1iS421 = _M0L6_2atmpS1489;
    _M0L3segS422 = _M0L6_2atmpS1490;
    continue;
    joinlet_2430:;
    _tmp_2432 = _M0L1iS421;
    _tmp_2433 = _M0L3segS422;
    _M0L1iS421 = _tmp_2432;
    _M0L3segS422 = _tmp_2433;
    continue;
    break;
  }
  #line 151 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6loggerS417.$0->$method_3(_M0L6loggerS417.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS413,
  int32_t _M0L3segS416,
  int32_t _M0L1iS415
) {
  struct _M0TPB6Logger _M0L8_2afieldS2207;
  struct _M0TPB6Logger _M0L6loggerS412;
  moonbit_string_t _M0L8_2afieldS2206;
  int32_t _M0L6_2acntS2344;
  moonbit_string_t _M0L4selfS414;
  #line 90 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L8_2afieldS2207
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS413->$1_0, _M0L6_2aenvS413->$1_1
  };
  _M0L6loggerS412 = _M0L8_2afieldS2207;
  _M0L8_2afieldS2206 = _M0L6_2aenvS413->$0;
  _M0L6_2acntS2344 = Moonbit_object_header(_M0L6_2aenvS413)->rc;
  if (_M0L6_2acntS2344 > 1) {
    int32_t _M0L11_2anew__cntS2345 = _M0L6_2acntS2344 - 1;
    Moonbit_object_header(_M0L6_2aenvS413)->rc = _M0L11_2anew__cntS2345;
    if (_M0L6loggerS412.$1) {
      moonbit_incref(_M0L6loggerS412.$1);
    }
    moonbit_incref(_M0L8_2afieldS2206);
  } else if (_M0L6_2acntS2344 == 1) {
    #line 90 "/home/blem/.moon/lib/core/builtin/show.mbt"
    moonbit_free(_M0L6_2aenvS413);
  }
  _M0L4selfS414 = _M0L8_2afieldS2206;
  if (_M0L1iS415 > _M0L3segS416) {
    int32_t _M0L6_2atmpS1487 = _M0L1iS415 - _M0L3segS416;
    #line 92 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6loggerS412.$0->$method_1(_M0L6loggerS412.$1, _M0L4selfS414, _M0L3segS416, _M0L6_2atmpS1487);
  } else {
    moonbit_decref(_M0L4selfS414);
    if (_M0L6loggerS412.$1) {
      moonbit_decref(_M0L6loggerS412.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS411) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS410;
  int32_t _M0L6_2atmpS1484;
  int32_t _M0L6_2atmpS1483;
  int32_t _M0L6_2atmpS1486;
  int32_t _M0L6_2atmpS1485;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1482;
  #line 69 "/home/blem/.moon/lib/core/builtin/show.mbt"
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L7_2aselfS410 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1484 = _M0IPC14byte4BytePB3Div3div(_M0L1bS411, 16);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1483
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1484);
  moonbit_incref(_M0L7_2aselfS410);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS410, _M0L6_2atmpS1483);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1486 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS411, 16);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L6_2atmpS1485
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1486);
  moonbit_incref(_M0L7_2aselfS410);
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS410, _M0L6_2atmpS1485);
  _M0L6_2atmpS1482 = _M0L7_2aselfS410;
  #line 78 "/home/blem/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1482);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS409) {
  #line 70 "/home/blem/.moon/lib/core/builtin/show.mbt"
  if (_M0L1iS409 < 10) {
    int32_t _M0L6_2atmpS1479;
    #line 72 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1479 = _M0IPC14byte4BytePB3Add3add(_M0L1iS409, 48);
    #line 72 "/home/blem/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1479);
  } else {
    int32_t _M0L6_2atmpS1481;
    int32_t _M0L6_2atmpS1480;
    #line 74 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1481 = _M0IPC14byte4BytePB3Add3add(_M0L1iS409, 97);
    #line 74 "/home/blem/.moon/lib/core/builtin/show.mbt"
    _M0L6_2atmpS1480 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1481, 10);
    #line 74 "/home/blem/.moon/lib/core/builtin/show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1480);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS407,
  int32_t _M0L4thatS408
) {
  int32_t _M0L6_2atmpS1477;
  int32_t _M0L6_2atmpS1478;
  int32_t _M0L6_2atmpS1476;
  #line 120 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1477 = (int32_t)_M0L4selfS407;
  _M0L6_2atmpS1478 = (int32_t)_M0L4thatS408;
  _M0L6_2atmpS1476 = _M0L6_2atmpS1477 - _M0L6_2atmpS1478;
  return _M0L6_2atmpS1476 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS405,
  int32_t _M0L4thatS406
) {
  int32_t _M0L6_2atmpS1474;
  int32_t _M0L6_2atmpS1475;
  int32_t _M0L6_2atmpS1473;
  #line 67 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1474 = (int32_t)_M0L4selfS405;
  _M0L6_2atmpS1475 = (int32_t)_M0L4thatS406;
  _M0L6_2atmpS1473 = _M0L6_2atmpS1474 % _M0L6_2atmpS1475;
  return _M0L6_2atmpS1473 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS403,
  int32_t _M0L4thatS404
) {
  int32_t _M0L6_2atmpS1471;
  int32_t _M0L6_2atmpS1472;
  int32_t _M0L6_2atmpS1470;
  #line 62 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1471 = (int32_t)_M0L4selfS403;
  _M0L6_2atmpS1472 = (int32_t)_M0L4thatS404;
  _M0L6_2atmpS1470 = _M0L6_2atmpS1471 / _M0L6_2atmpS1472;
  return _M0L6_2atmpS1470 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS401,
  int32_t _M0L4thatS402
) {
  int32_t _M0L6_2atmpS1468;
  int32_t _M0L6_2atmpS1469;
  int32_t _M0L6_2atmpS1467;
  #line 106 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  _M0L6_2atmpS1468 = (int32_t)_M0L4selfS401;
  _M0L6_2atmpS1469 = (int32_t)_M0L4thatS402;
  _M0L6_2atmpS1467 = _M0L6_2atmpS1468 + _M0L6_2atmpS1469;
  return _M0L6_2atmpS1467 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS398,
  int32_t _M0L5startS396,
  int32_t _M0L3endS397
) {
  int32_t _if__result_2434;
  int32_t _M0L3lenS399;
  int32_t _M0L6_2atmpS1465;
  int32_t _M0L6_2atmpS1466;
  moonbit_bytes_t _M0L5bytesS400;
  moonbit_bytes_t _M0L6_2atmpS1464;
  #line 91 "/home/blem/.moon/lib/core/builtin/string.mbt"
  if (_M0L5startS396 == 0) {
    int32_t _M0L6_2atmpS1463 = Moonbit_array_length(_M0L3strS398);
    _if__result_2434 = _M0L3endS397 == _M0L6_2atmpS1463;
  } else {
    _if__result_2434 = 0;
  }
  if (_if__result_2434) {
    return _M0L3strS398;
  }
  _M0L3lenS399 = _M0L3endS397 - _M0L5startS396;
  _M0L6_2atmpS1465 = _M0L3lenS399 * 2;
  #line 101 "/home/blem/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1466 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS400
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1465, _M0L6_2atmpS1466);
  moonbit_incref(_M0L5bytesS400);
  #line 102 "/home/blem/.moon/lib/core/builtin/string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS400, 0, _M0L3strS398, _M0L5startS396, _M0L3lenS399);
  _M0L6_2atmpS1464 = _M0L5bytesS400;
  #line 103 "/home/blem/.moon/lib/core/builtin/string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1464, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS394) {
  #line 205 "/home/blem/.moon/lib/core/builtin/iterator.mbt"
  return _M0L1fS394;
}

struct _M0TWEOy* _M0MPB4Iter3newGyE(struct _M0TWEOy* _M0L1fS395) {
  #line 205 "/home/blem/.moon/lib/core/builtin/iterator.mbt"
  return _M0L1fS395;
}

struct moonbit_result_0 _M0FPB10assert__eqGiE(
  int32_t _M0L1aS388,
  int32_t _M0L1bS389,
  moonbit_string_t _M0L3msgS391,
  moonbit_string_t _M0L3locS393
) {
  #line 45 "/home/blem/.moon/lib/core/builtin/assert.mbt"
  if (_M0L1aS388 != _M0L1bS389) {
    moonbit_string_t _M0L9fail__msgS390;
    if (_M0L3msgS391 == 0) {
      moonbit_string_t _M0L6_2atmpS1461;
      moonbit_string_t _M0L6_2atmpS1460;
      moonbit_string_t _M0L6_2atmpS2211;
      moonbit_string_t _M0L6_2atmpS1459;
      moonbit_string_t _M0L6_2atmpS2210;
      moonbit_string_t _M0L6_2atmpS1456;
      moonbit_string_t _M0L6_2atmpS1458;
      moonbit_string_t _M0L6_2atmpS1457;
      moonbit_string_t _M0L6_2atmpS2209;
      moonbit_string_t _M0L6_2atmpS1455;
      moonbit_string_t _M0L6_2atmpS2208;
      if (_M0L3msgS391) {
        moonbit_decref(_M0L3msgS391);
      }
      #line 56 "/home/blem/.moon/lib/core/builtin/assert.mbt"
      _M0L6_2atmpS1461 = _M0FPB13debug__stringGiE(_M0L1aS388);
      #line 56 "/home/blem/.moon/lib/core/builtin/assert.mbt"
      _M0L6_2atmpS1460
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1461);
      #line 54 "/home/blem/.moon/lib/core/builtin/assert.mbt"
      _M0L6_2atmpS2211
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_85.data, _M0L6_2atmpS1460);
      moonbit_decref(_M0L6_2atmpS1460);
      _M0L6_2atmpS1459 = _M0L6_2atmpS2211;
      #line 54 "/home/blem/.moon/lib/core/builtin/assert.mbt"
      _M0L6_2atmpS2210
      = moonbit_add_string(_M0L6_2atmpS1459, (moonbit_string_t)moonbit_string_literal_86.data);
      moonbit_decref(_M0L6_2atmpS1459);
      _M0L6_2atmpS1456 = _M0L6_2atmpS2210;
      #line 56 "/home/blem/.moon/lib/core/builtin/assert.mbt"
      _M0L6_2atmpS1458 = _M0FPB13debug__stringGiE(_M0L1bS389);
      #line 56 "/home/blem/.moon/lib/core/builtin/assert.mbt"
      _M0L6_2atmpS1457
      = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1458);
      #line 54 "/home/blem/.moon/lib/core/builtin/assert.mbt"
      _M0L6_2atmpS2209
      = moonbit_add_string(_M0L6_2atmpS1456, _M0L6_2atmpS1457);
      moonbit_decref(_M0L6_2atmpS1456);
      moonbit_decref(_M0L6_2atmpS1457);
      _M0L6_2atmpS1455 = _M0L6_2atmpS2209;
      #line 54 "/home/blem/.moon/lib/core/builtin/assert.mbt"
      _M0L6_2atmpS2208
      = moonbit_add_string(_M0L6_2atmpS1455, (moonbit_string_t)moonbit_string_literal_85.data);
      moonbit_decref(_M0L6_2atmpS1455);
      _M0L9fail__msgS390 = _M0L6_2atmpS2208;
    } else {
      moonbit_string_t _M0L7_2aSomeS392 = _M0L3msgS391;
      _M0L9fail__msgS390 = _M0L7_2aSomeS392;
    }
    #line 58 "/home/blem/.moon/lib/core/builtin/assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS390, _M0L3locS393);
  } else {
    int32_t _M0L6_2atmpS1462;
    struct moonbit_result_0 _result_2435;
    moonbit_decref(_M0L3locS393);
    if (_M0L3msgS391) {
      moonbit_decref(_M0L3msgS391);
    }
    _M0L6_2atmpS1462 = 0;
    _result_2435.tag = 1;
    _result_2435.data.ok = _M0L6_2atmpS1462;
    return _result_2435;
  }
}

struct moonbit_result_0 _M0FPB4failGuE(
  moonbit_string_t _M0L3msgS387,
  moonbit_string_t _M0L3locS386
) {
  moonbit_string_t _M0L6_2atmpS1454;
  moonbit_string_t _M0L6_2atmpS2213;
  moonbit_string_t _M0L6_2atmpS1452;
  moonbit_string_t _M0L6_2atmpS1453;
  moonbit_string_t _M0L6_2atmpS2212;
  moonbit_string_t _M0L6_2atmpS1451;
  void* _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1450;
  struct moonbit_result_0 _result_2436;
  #line 55 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  #line 57 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0L6_2atmpS1454
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS386);
  #line 57 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0L6_2atmpS2213
  = moonbit_add_string(_M0L6_2atmpS1454, (moonbit_string_t)moonbit_string_literal_87.data);
  moonbit_decref(_M0L6_2atmpS1454);
  _M0L6_2atmpS1452 = _M0L6_2atmpS2213;
  #line 57 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0L6_2atmpS1453 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS387);
  #line 57 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0L6_2atmpS2212 = moonbit_add_string(_M0L6_2atmpS1452, _M0L6_2atmpS1453);
  moonbit_decref(_M0L6_2atmpS1452);
  moonbit_decref(_M0L6_2atmpS1453);
  _M0L6_2atmpS1451 = _M0L6_2atmpS2212;
  _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1450
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure));
  Moonbit_object_header(_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1450)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure, $0) >> 2, 1, 0);
  ((struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1450)->$0
  = _M0L6_2atmpS1451;
  _result_2436.tag = 0;
  _result_2436.data.err
  = _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1450;
  return _result_2436;
}

moonbit_string_t _M0FPB13debug__stringGiE(int32_t _M0L1tS385) {
  struct _M0TPB13StringBuilder* _M0L3bufS384;
  struct _M0TPB6Logger _M0L6_2atmpS1449;
  #line 16 "/home/blem/.moon/lib/core/builtin/assert.mbt"
  #line 17 "/home/blem/.moon/lib/core/builtin/assert.mbt"
  _M0L3bufS384 = _M0MPB13StringBuilder11new_2einner(50);
  moonbit_incref(_M0L3bufS384);
  _M0L6_2atmpS1449
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS384
  };
  #line 18 "/home/blem/.moon/lib/core/builtin/assert.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L1tS385, _M0L6_2atmpS1449);
  #line 19 "/home/blem/.moon/lib/core/builtin/assert.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS384);
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS368,
  int32_t _M0L5radixS367
) {
  int32_t _if__result_2437;
  int32_t _M0L12is__negativeS369;
  uint32_t _M0L3numS370;
  uint16_t* _M0L6bufferS371;
  #line 220 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS367 < 2) {
    _if__result_2437 = 1;
  } else {
    _if__result_2437 = _M0L5radixS367 > 36;
  }
  if (_if__result_2437) {
    #line 224 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_88.data, (moonbit_string_t)moonbit_string_literal_89.data);
  }
  if (_M0L4selfS368 == 0) {
    return (moonbit_string_t)moonbit_string_literal_59.data;
  }
  _M0L12is__negativeS369 = _M0L4selfS368 < 0;
  if (_M0L12is__negativeS369) {
    int32_t _M0L6_2atmpS1448 = -_M0L4selfS368;
    _M0L3numS370 = *(uint32_t*)&_M0L6_2atmpS1448;
  } else {
    _M0L3numS370 = *(uint32_t*)&_M0L4selfS368;
  }
  switch (_M0L5radixS367) {
    case 10: {
      int32_t _M0L10digit__lenS372;
      int32_t _M0L6_2atmpS1445;
      int32_t _M0L10total__lenS373;
      uint16_t* _M0L6bufferS374;
      int32_t _M0L12digit__startS375;
      #line 246 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS372 = _M0FPB12dec__count32(_M0L3numS370);
      if (_M0L12is__negativeS369) {
        _M0L6_2atmpS1445 = 1;
      } else {
        _M0L6_2atmpS1445 = 0;
      }
      _M0L10total__lenS373 = _M0L10digit__lenS372 + _M0L6_2atmpS1445;
      _M0L6bufferS374
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS373, 0);
      if (_M0L12is__negativeS369) {
        _M0L12digit__startS375 = 1;
      } else {
        _M0L12digit__startS375 = 0;
      }
      moonbit_incref(_M0L6bufferS374);
      #line 250 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS374, _M0L3numS370, _M0L12digit__startS375, _M0L10total__lenS373);
      _M0L6bufferS371 = _M0L6bufferS374;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS376;
      int32_t _M0L6_2atmpS1446;
      int32_t _M0L10total__lenS377;
      uint16_t* _M0L6bufferS378;
      int32_t _M0L12digit__startS379;
      #line 254 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS376 = _M0FPB12hex__count32(_M0L3numS370);
      if (_M0L12is__negativeS369) {
        _M0L6_2atmpS1446 = 1;
      } else {
        _M0L6_2atmpS1446 = 0;
      }
      _M0L10total__lenS377 = _M0L10digit__lenS376 + _M0L6_2atmpS1446;
      _M0L6bufferS378
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS377, 0);
      if (_M0L12is__negativeS369) {
        _M0L12digit__startS379 = 1;
      } else {
        _M0L12digit__startS379 = 0;
      }
      moonbit_incref(_M0L6bufferS378);
      #line 258 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS378, _M0L3numS370, _M0L12digit__startS379, _M0L10total__lenS377);
      _M0L6bufferS371 = _M0L6bufferS378;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS380;
      int32_t _M0L6_2atmpS1447;
      int32_t _M0L10total__lenS381;
      uint16_t* _M0L6bufferS382;
      int32_t _M0L12digit__startS383;
      #line 262 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS380
      = _M0FPB14radix__count32(_M0L3numS370, _M0L5radixS367);
      if (_M0L12is__negativeS369) {
        _M0L6_2atmpS1447 = 1;
      } else {
        _M0L6_2atmpS1447 = 0;
      }
      _M0L10total__lenS381 = _M0L10digit__lenS380 + _M0L6_2atmpS1447;
      _M0L6bufferS382
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS381, 0);
      if (_M0L12is__negativeS369) {
        _M0L12digit__startS383 = 1;
      } else {
        _M0L12digit__startS383 = 0;
      }
      moonbit_incref(_M0L6bufferS382);
      #line 266 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS382, _M0L3numS370, _M0L12digit__startS383, _M0L10total__lenS381, _M0L5radixS367);
      _M0L6bufferS371 = _M0L6bufferS382;
      break;
    }
  }
  if (_M0L12is__negativeS369) {
    _M0L6bufferS371[0] = 45;
  }
  return _M0L6bufferS371;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS361,
  int32_t _M0L5radixS364
) {
  uint32_t _M0Lm3numS362;
  uint32_t _M0L4baseS363;
  int32_t _M0Lm5countS365;
  #line 198 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS361 == 0u) {
    return 1;
  }
  _M0Lm3numS362 = _M0L5valueS361;
  _M0L4baseS363 = *(uint32_t*)&_M0L5radixS364;
  _M0Lm5countS365 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1442 = _M0Lm3numS362;
    if (_M0L6_2atmpS1442 > 0u) {
      int32_t _M0L6_2atmpS1443 = _M0Lm5countS365;
      uint32_t _M0L6_2atmpS1444;
      _M0Lm5countS365 = _M0L6_2atmpS1443 + 1;
      _M0L6_2atmpS1444 = _M0Lm3numS362;
      _M0Lm3numS362 = _M0L6_2atmpS1444 / _M0L4baseS363;
      continue;
    }
    break;
  }
  return _M0Lm5countS365;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS359) {
  #line 186 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS359 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS360;
    int32_t _M0L6_2atmpS1441;
    int32_t _M0L6_2atmpS1440;
    #line 191 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS360 = moonbit_clz32(_M0L5valueS359);
    _M0L6_2atmpS1441 = 31 - _M0L14leading__zerosS360;
    _M0L6_2atmpS1440 = _M0L6_2atmpS1441 / 4;
    return _M0L6_2atmpS1440 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS358) {
  #line 152 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS358 >= 100000u) {
    if (_M0L5valueS358 >= 10000000u) {
      if (_M0L5valueS358 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS358 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS358 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS358 >= 1000u) {
    if (_M0L5valueS358 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS358 >= 100u) {
    return 3;
  } else if (_M0L5valueS358 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS348,
  uint32_t _M0L3numS336,
  int32_t _M0L12digit__startS339,
  int32_t _M0L10total__lenS338
) {
  uint32_t _M0Lm3numS335;
  int32_t _M0Lm6offsetS337;
  uint32_t _M0L6_2atmpS1439;
  int32_t _M0Lm9remainingS350;
  int32_t _M0L6_2atmpS1420;
  #line 94 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  _M0Lm3numS335 = _M0L3numS336;
  _M0Lm6offsetS337 = _M0L10total__lenS338 - _M0L12digit__startS339;
  while (1) {
    uint32_t _M0L6_2atmpS1383 = _M0Lm3numS335;
    if (_M0L6_2atmpS1383 >= 10000u) {
      uint32_t _M0L6_2atmpS1406 = _M0Lm3numS335;
      uint32_t _M0L1tS340 = _M0L6_2atmpS1406 / 10000u;
      uint32_t _M0L6_2atmpS1405 = _M0Lm3numS335;
      uint32_t _M0L6_2atmpS1404 = _M0L6_2atmpS1405 % 10000u;
      int32_t _M0L1rS341 = *(int32_t*)&_M0L6_2atmpS1404;
      int32_t _M0L2d1S342;
      int32_t _M0L2d2S343;
      int32_t _M0L6_2atmpS1384;
      int32_t _M0L6_2atmpS1403;
      int32_t _M0L6_2atmpS1402;
      int32_t _M0L6d1__hiS344;
      int32_t _M0L6_2atmpS1401;
      int32_t _M0L6_2atmpS1400;
      int32_t _M0L6d1__loS345;
      int32_t _M0L6_2atmpS1399;
      int32_t _M0L6_2atmpS1398;
      int32_t _M0L6d2__hiS346;
      int32_t _M0L6_2atmpS1397;
      int32_t _M0L6_2atmpS1396;
      int32_t _M0L6d2__loS347;
      int32_t _M0L6_2atmpS1386;
      int32_t _M0L6_2atmpS1385;
      int32_t _M0L6_2atmpS1389;
      int32_t _M0L6_2atmpS1388;
      int32_t _M0L6_2atmpS1387;
      int32_t _M0L6_2atmpS1392;
      int32_t _M0L6_2atmpS1391;
      int32_t _M0L6_2atmpS1390;
      int32_t _M0L6_2atmpS1395;
      int32_t _M0L6_2atmpS1394;
      int32_t _M0L6_2atmpS1393;
      _M0Lm3numS335 = _M0L1tS340;
      _M0L2d1S342 = _M0L1rS341 / 100;
      _M0L2d2S343 = _M0L1rS341 % 100;
      _M0L6_2atmpS1384 = _M0Lm6offsetS337;
      _M0Lm6offsetS337 = _M0L6_2atmpS1384 - 4;
      _M0L6_2atmpS1403 = _M0L2d1S342 / 10;
      _M0L6_2atmpS1402 = 48 + _M0L6_2atmpS1403;
      _M0L6d1__hiS344 = (uint16_t)_M0L6_2atmpS1402;
      _M0L6_2atmpS1401 = _M0L2d1S342 % 10;
      _M0L6_2atmpS1400 = 48 + _M0L6_2atmpS1401;
      _M0L6d1__loS345 = (uint16_t)_M0L6_2atmpS1400;
      _M0L6_2atmpS1399 = _M0L2d2S343 / 10;
      _M0L6_2atmpS1398 = 48 + _M0L6_2atmpS1399;
      _M0L6d2__hiS346 = (uint16_t)_M0L6_2atmpS1398;
      _M0L6_2atmpS1397 = _M0L2d2S343 % 10;
      _M0L6_2atmpS1396 = 48 + _M0L6_2atmpS1397;
      _M0L6d2__loS347 = (uint16_t)_M0L6_2atmpS1396;
      _M0L6_2atmpS1386 = _M0Lm6offsetS337;
      _M0L6_2atmpS1385 = _M0L12digit__startS339 + _M0L6_2atmpS1386;
      _M0L6bufferS348[_M0L6_2atmpS1385] = _M0L6d1__hiS344;
      _M0L6_2atmpS1389 = _M0Lm6offsetS337;
      _M0L6_2atmpS1388 = _M0L12digit__startS339 + _M0L6_2atmpS1389;
      _M0L6_2atmpS1387 = _M0L6_2atmpS1388 + 1;
      _M0L6bufferS348[_M0L6_2atmpS1387] = _M0L6d1__loS345;
      _M0L6_2atmpS1392 = _M0Lm6offsetS337;
      _M0L6_2atmpS1391 = _M0L12digit__startS339 + _M0L6_2atmpS1392;
      _M0L6_2atmpS1390 = _M0L6_2atmpS1391 + 2;
      _M0L6bufferS348[_M0L6_2atmpS1390] = _M0L6d2__hiS346;
      _M0L6_2atmpS1395 = _M0Lm6offsetS337;
      _M0L6_2atmpS1394 = _M0L12digit__startS339 + _M0L6_2atmpS1395;
      _M0L6_2atmpS1393 = _M0L6_2atmpS1394 + 3;
      _M0L6bufferS348[_M0L6_2atmpS1393] = _M0L6d2__loS347;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1439 = _M0Lm3numS335;
  _M0Lm9remainingS350 = *(int32_t*)&_M0L6_2atmpS1439;
  while (1) {
    int32_t _M0L6_2atmpS1407 = _M0Lm9remainingS350;
    if (_M0L6_2atmpS1407 >= 100) {
      int32_t _M0L6_2atmpS1419 = _M0Lm9remainingS350;
      int32_t _M0L1tS351 = _M0L6_2atmpS1419 / 100;
      int32_t _M0L6_2atmpS1418 = _M0Lm9remainingS350;
      int32_t _M0L1dS352 = _M0L6_2atmpS1418 % 100;
      int32_t _M0L6_2atmpS1408;
      int32_t _M0L6_2atmpS1417;
      int32_t _M0L6_2atmpS1416;
      int32_t _M0L5d__hiS353;
      int32_t _M0L6_2atmpS1415;
      int32_t _M0L6_2atmpS1414;
      int32_t _M0L5d__loS354;
      int32_t _M0L6_2atmpS1410;
      int32_t _M0L6_2atmpS1409;
      int32_t _M0L6_2atmpS1413;
      int32_t _M0L6_2atmpS1412;
      int32_t _M0L6_2atmpS1411;
      _M0Lm9remainingS350 = _M0L1tS351;
      _M0L6_2atmpS1408 = _M0Lm6offsetS337;
      _M0Lm6offsetS337 = _M0L6_2atmpS1408 - 2;
      _M0L6_2atmpS1417 = _M0L1dS352 / 10;
      _M0L6_2atmpS1416 = 48 + _M0L6_2atmpS1417;
      _M0L5d__hiS353 = (uint16_t)_M0L6_2atmpS1416;
      _M0L6_2atmpS1415 = _M0L1dS352 % 10;
      _M0L6_2atmpS1414 = 48 + _M0L6_2atmpS1415;
      _M0L5d__loS354 = (uint16_t)_M0L6_2atmpS1414;
      _M0L6_2atmpS1410 = _M0Lm6offsetS337;
      _M0L6_2atmpS1409 = _M0L12digit__startS339 + _M0L6_2atmpS1410;
      _M0L6bufferS348[_M0L6_2atmpS1409] = _M0L5d__hiS353;
      _M0L6_2atmpS1413 = _M0Lm6offsetS337;
      _M0L6_2atmpS1412 = _M0L12digit__startS339 + _M0L6_2atmpS1413;
      _M0L6_2atmpS1411 = _M0L6_2atmpS1412 + 1;
      _M0L6bufferS348[_M0L6_2atmpS1411] = _M0L5d__loS354;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1420 = _M0Lm9remainingS350;
  if (_M0L6_2atmpS1420 >= 10) {
    int32_t _M0L6_2atmpS1421 = _M0Lm6offsetS337;
    int32_t _M0L6_2atmpS1432;
    int32_t _M0L6_2atmpS1431;
    int32_t _M0L6_2atmpS1430;
    int32_t _M0L5d__hiS356;
    int32_t _M0L6_2atmpS1429;
    int32_t _M0L6_2atmpS1428;
    int32_t _M0L6_2atmpS1427;
    int32_t _M0L5d__loS357;
    int32_t _M0L6_2atmpS1423;
    int32_t _M0L6_2atmpS1422;
    int32_t _M0L6_2atmpS1426;
    int32_t _M0L6_2atmpS1425;
    int32_t _M0L6_2atmpS1424;
    _M0Lm6offsetS337 = _M0L6_2atmpS1421 - 2;
    _M0L6_2atmpS1432 = _M0Lm9remainingS350;
    _M0L6_2atmpS1431 = _M0L6_2atmpS1432 / 10;
    _M0L6_2atmpS1430 = 48 + _M0L6_2atmpS1431;
    _M0L5d__hiS356 = (uint16_t)_M0L6_2atmpS1430;
    _M0L6_2atmpS1429 = _M0Lm9remainingS350;
    _M0L6_2atmpS1428 = _M0L6_2atmpS1429 % 10;
    _M0L6_2atmpS1427 = 48 + _M0L6_2atmpS1428;
    _M0L5d__loS357 = (uint16_t)_M0L6_2atmpS1427;
    _M0L6_2atmpS1423 = _M0Lm6offsetS337;
    _M0L6_2atmpS1422 = _M0L12digit__startS339 + _M0L6_2atmpS1423;
    _M0L6bufferS348[_M0L6_2atmpS1422] = _M0L5d__hiS356;
    _M0L6_2atmpS1426 = _M0Lm6offsetS337;
    _M0L6_2atmpS1425 = _M0L12digit__startS339 + _M0L6_2atmpS1426;
    _M0L6_2atmpS1424 = _M0L6_2atmpS1425 + 1;
    _M0L6bufferS348[_M0L6_2atmpS1424] = _M0L5d__loS357;
    moonbit_decref(_M0L6bufferS348);
  } else {
    int32_t _M0L6_2atmpS1433 = _M0Lm6offsetS337;
    int32_t _M0L6_2atmpS1438;
    int32_t _M0L6_2atmpS1434;
    int32_t _M0L6_2atmpS1437;
    int32_t _M0L6_2atmpS1436;
    int32_t _M0L6_2atmpS1435;
    _M0Lm6offsetS337 = _M0L6_2atmpS1433 - 1;
    _M0L6_2atmpS1438 = _M0Lm6offsetS337;
    _M0L6_2atmpS1434 = _M0L12digit__startS339 + _M0L6_2atmpS1438;
    _M0L6_2atmpS1437 = _M0Lm9remainingS350;
    _M0L6_2atmpS1436 = 48 + _M0L6_2atmpS1437;
    _M0L6_2atmpS1435 = (uint16_t)_M0L6_2atmpS1436;
    _M0L6bufferS348[_M0L6_2atmpS1434] = _M0L6_2atmpS1435;
    moonbit_decref(_M0L6bufferS348);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS330,
  uint32_t _M0L3numS324,
  int32_t _M0L12digit__startS322,
  int32_t _M0L10total__lenS321,
  int32_t _M0L5radixS326
) {
  int32_t _M0Lm6offsetS320;
  uint32_t _M0Lm1nS323;
  uint32_t _M0L4baseS325;
  int32_t _M0L6_2atmpS1365;
  int32_t _M0L6_2atmpS1364;
  #line 59 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  _M0Lm6offsetS320 = _M0L10total__lenS321 - _M0L12digit__startS322;
  _M0Lm1nS323 = _M0L3numS324;
  _M0L4baseS325 = *(uint32_t*)&_M0L5radixS326;
  _M0L6_2atmpS1365 = _M0L5radixS326 - 1;
  _M0L6_2atmpS1364 = _M0L5radixS326 & _M0L6_2atmpS1365;
  if (_M0L6_2atmpS1364 == 0) {
    int32_t _M0L5shiftS327;
    uint32_t _M0L4maskS328;
    #line 72 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS327 = moonbit_ctz32(_M0L5radixS326);
    _M0L4maskS328 = _M0L4baseS325 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1366 = _M0Lm1nS323;
      if (_M0L6_2atmpS1366 > 0u) {
        int32_t _M0L6_2atmpS1367 = _M0Lm6offsetS320;
        uint32_t _M0L6_2atmpS1373;
        uint32_t _M0L6_2atmpS1372;
        int32_t _M0L5digitS329;
        int32_t _M0L6_2atmpS1370;
        int32_t _M0L6_2atmpS1368;
        int32_t _M0L6_2atmpS1369;
        uint32_t _M0L6_2atmpS1371;
        _M0Lm6offsetS320 = _M0L6_2atmpS1367 - 1;
        _M0L6_2atmpS1373 = _M0Lm1nS323;
        _M0L6_2atmpS1372 = _M0L6_2atmpS1373 & _M0L4maskS328;
        _M0L5digitS329 = *(int32_t*)&_M0L6_2atmpS1372;
        _M0L6_2atmpS1370 = _M0Lm6offsetS320;
        _M0L6_2atmpS1368 = _M0L12digit__startS322 + _M0L6_2atmpS1370;
        _M0L6_2atmpS1369
        = ((moonbit_string_t)moonbit_string_literal_90.data)[
          _M0L5digitS329
        ];
        _M0L6bufferS330[_M0L6_2atmpS1368] = _M0L6_2atmpS1369;
        _M0L6_2atmpS1371 = _M0Lm1nS323;
        _M0Lm1nS323 = _M0L6_2atmpS1371 >> (_M0L5shiftS327 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS330);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1374 = _M0Lm1nS323;
      if (_M0L6_2atmpS1374 > 0u) {
        int32_t _M0L6_2atmpS1375 = _M0Lm6offsetS320;
        uint32_t _M0L6_2atmpS1382;
        uint32_t _M0L1qS332;
        uint32_t _M0L6_2atmpS1380;
        uint32_t _M0L6_2atmpS1381;
        uint32_t _M0L6_2atmpS1379;
        int32_t _M0L5digitS333;
        int32_t _M0L6_2atmpS1378;
        int32_t _M0L6_2atmpS1376;
        int32_t _M0L6_2atmpS1377;
        _M0Lm6offsetS320 = _M0L6_2atmpS1375 - 1;
        _M0L6_2atmpS1382 = _M0Lm1nS323;
        _M0L1qS332 = _M0L6_2atmpS1382 / _M0L4baseS325;
        _M0L6_2atmpS1380 = _M0Lm1nS323;
        _M0L6_2atmpS1381 = _M0L1qS332 * _M0L4baseS325;
        _M0L6_2atmpS1379 = _M0L6_2atmpS1380 - _M0L6_2atmpS1381;
        _M0L5digitS333 = *(int32_t*)&_M0L6_2atmpS1379;
        _M0L6_2atmpS1378 = _M0Lm6offsetS320;
        _M0L6_2atmpS1376 = _M0L12digit__startS322 + _M0L6_2atmpS1378;
        _M0L6_2atmpS1377
        = ((moonbit_string_t)moonbit_string_literal_90.data)[
          _M0L5digitS333
        ];
        _M0L6bufferS330[_M0L6_2atmpS1376] = _M0L6_2atmpS1377;
        _M0Lm1nS323 = _M0L1qS332;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS330);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS317,
  uint32_t _M0L3numS313,
  int32_t _M0L12digit__startS311,
  int32_t _M0L10total__lenS310
) {
  int32_t _M0Lm6offsetS309;
  uint32_t _M0Lm1nS312;
  int32_t _M0L6_2atmpS1360;
  #line 29 "/home/blem/.moon/lib/core/builtin/to_string.mbt"
  _M0Lm6offsetS309 = _M0L10total__lenS310 - _M0L12digit__startS311;
  _M0Lm1nS312 = _M0L3numS313;
  while (1) {
    int32_t _M0L6_2atmpS1348 = _M0Lm6offsetS309;
    if (_M0L6_2atmpS1348 >= 2) {
      int32_t _M0L6_2atmpS1349 = _M0Lm6offsetS309;
      uint32_t _M0L6_2atmpS1359;
      uint32_t _M0L6_2atmpS1358;
      int32_t _M0L9byte__valS314;
      int32_t _M0L2hiS315;
      int32_t _M0L2loS316;
      int32_t _M0L6_2atmpS1352;
      int32_t _M0L6_2atmpS1350;
      int32_t _M0L6_2atmpS1351;
      int32_t _M0L6_2atmpS1356;
      int32_t _M0L6_2atmpS1355;
      int32_t _M0L6_2atmpS1353;
      int32_t _M0L6_2atmpS1354;
      uint32_t _M0L6_2atmpS1357;
      _M0Lm6offsetS309 = _M0L6_2atmpS1349 - 2;
      _M0L6_2atmpS1359 = _M0Lm1nS312;
      _M0L6_2atmpS1358 = _M0L6_2atmpS1359 & 255u;
      _M0L9byte__valS314 = *(int32_t*)&_M0L6_2atmpS1358;
      _M0L2hiS315 = _M0L9byte__valS314 / 16;
      _M0L2loS316 = _M0L9byte__valS314 % 16;
      _M0L6_2atmpS1352 = _M0Lm6offsetS309;
      _M0L6_2atmpS1350 = _M0L12digit__startS311 + _M0L6_2atmpS1352;
      _M0L6_2atmpS1351
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2hiS315
      ];
      _M0L6bufferS317[_M0L6_2atmpS1350] = _M0L6_2atmpS1351;
      _M0L6_2atmpS1356 = _M0Lm6offsetS309;
      _M0L6_2atmpS1355 = _M0L12digit__startS311 + _M0L6_2atmpS1356;
      _M0L6_2atmpS1353 = _M0L6_2atmpS1355 + 1;
      _M0L6_2atmpS1354
      = ((moonbit_string_t)moonbit_string_literal_90.data)[
        _M0L2loS316
      ];
      _M0L6bufferS317[_M0L6_2atmpS1353] = _M0L6_2atmpS1354;
      _M0L6_2atmpS1357 = _M0Lm1nS312;
      _M0Lm1nS312 = _M0L6_2atmpS1357 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1360 = _M0Lm6offsetS309;
  if (_M0L6_2atmpS1360 == 1) {
    uint32_t _M0L6_2atmpS1363 = _M0Lm1nS312;
    uint32_t _M0L6_2atmpS1362 = _M0L6_2atmpS1363 & 15u;
    int32_t _M0L6nibbleS319 = *(int32_t*)&_M0L6_2atmpS1362;
    int32_t _M0L6_2atmpS1361 =
      ((moonbit_string_t)moonbit_string_literal_90.data)[_M0L6nibbleS319];
    _M0L6bufferS317[_M0L12digit__startS311] = _M0L6_2atmpS1361;
    moonbit_decref(_M0L6bufferS317);
  } else {
    moonbit_decref(_M0L6bufferS317);
  }
  return 0;
}

int32_t _M0MPB6Logger19write__iter_2einnerGyE(
  struct _M0TPB6Logger _M0L4selfS292,
  struct _M0TWEOy* _M0L4iterS296,
  moonbit_string_t _M0L6prefixS293,
  moonbit_string_t _M0L6suffixS308,
  moonbit_string_t _M0L3sepS299,
  int32_t _M0L8trailingS294
) {
  #line 156 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  if (_M0L4selfS292.$1) {
    moonbit_incref(_M0L4selfS292.$1);
  }
  #line 164 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L4selfS292.$0->$method_0(_M0L4selfS292.$1, _M0L6prefixS293);
  if (_M0L8trailingS294) {
    while (1) {
      int32_t _M0L7_2abindS295;
      moonbit_incref(_M0L4iterS296);
      #line 166 "/home/blem/.moon/lib/core/builtin/traits.mbt"
      _M0L7_2abindS295 = _M0MPB4Iter4nextGyE(_M0L4iterS296);
      if (_M0L7_2abindS295 == -1) {
        moonbit_decref(_M0L3sepS299);
        moonbit_decref(_M0L4iterS296);
      } else {
        int32_t _M0L7_2aSomeS297 = _M0L7_2abindS295;
        int32_t _M0L4_2axS298 = _M0L7_2aSomeS297;
        if (_M0L4selfS292.$1) {
          moonbit_incref(_M0L4selfS292.$1);
        }
        #line 167 "/home/blem/.moon/lib/core/builtin/traits.mbt"
        _M0MPB6Logger13write__objectGyE(_M0L4selfS292, _M0L4_2axS298);
        moonbit_incref(_M0L3sepS299);
        if (_M0L4selfS292.$1) {
          moonbit_incref(_M0L4selfS292.$1);
        }
        #line 168 "/home/blem/.moon/lib/core/builtin/traits.mbt"
        _M0L4selfS292.$0->$method_0(_M0L4selfS292.$1, _M0L3sepS299);
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L7_2abindS301;
    moonbit_incref(_M0L4iterS296);
    #line 170 "/home/blem/.moon/lib/core/builtin/traits.mbt"
    _M0L7_2abindS301 = _M0MPB4Iter4nextGyE(_M0L4iterS296);
    if (_M0L7_2abindS301 == -1) {
      moonbit_decref(_M0L3sepS299);
      moonbit_decref(_M0L4iterS296);
    } else {
      int32_t _M0L7_2aSomeS302 = _M0L7_2abindS301;
      int32_t _M0L4_2axS303 = _M0L7_2aSomeS302;
      if (_M0L4selfS292.$1) {
        moonbit_incref(_M0L4selfS292.$1);
      }
      #line 171 "/home/blem/.moon/lib/core/builtin/traits.mbt"
      _M0MPB6Logger13write__objectGyE(_M0L4selfS292, _M0L4_2axS303);
      while (1) {
        int32_t _M0L7_2abindS304;
        moonbit_incref(_M0L4iterS296);
        #line 172 "/home/blem/.moon/lib/core/builtin/traits.mbt"
        _M0L7_2abindS304 = _M0MPB4Iter4nextGyE(_M0L4iterS296);
        if (_M0L7_2abindS304 == -1) {
          moonbit_decref(_M0L3sepS299);
          moonbit_decref(_M0L4iterS296);
        } else {
          int32_t _M0L7_2aSomeS305 = _M0L7_2abindS304;
          int32_t _M0L4_2axS306 = _M0L7_2aSomeS305;
          moonbit_incref(_M0L3sepS299);
          if (_M0L4selfS292.$1) {
            moonbit_incref(_M0L4selfS292.$1);
          }
          #line 173 "/home/blem/.moon/lib/core/builtin/traits.mbt"
          _M0L4selfS292.$0->$method_0(_M0L4selfS292.$1, _M0L3sepS299);
          if (_M0L4selfS292.$1) {
            moonbit_incref(_M0L4selfS292.$1);
          }
          #line 174 "/home/blem/.moon/lib/core/builtin/traits.mbt"
          _M0MPB6Logger13write__objectGyE(_M0L4selfS292, _M0L4_2axS306);
          continue;
        }
        break;
      }
    }
  }
  #line 177 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L4selfS292.$0->$method_0(_M0L4selfS292.$1, _M0L6suffixS308);
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS289) {
  struct _M0TWEOs* _M0L7_2afuncS288;
  #line 28 "/home/blem/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS288 = _M0L4selfS289;
  #line 31 "/home/blem/.moon/lib/core/builtin/iterator.mbt"
  return _M0L7_2afuncS288->code(_M0L7_2afuncS288);
}

int32_t _M0MPB4Iter4nextGyE(struct _M0TWEOy* _M0L4selfS291) {
  struct _M0TWEOy* _M0L7_2afuncS290;
  #line 28 "/home/blem/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS290 = _M0L4selfS291;
  #line 31 "/home/blem/.moon/lib/core/builtin/iterator.mbt"
  return _M0L7_2afuncS290->code(_M0L7_2afuncS290);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS281
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS280;
  struct _M0TPB6Logger _M0L6_2atmpS1344;
  #line 142 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 143 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS280 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS280);
  _M0L6_2atmpS1344
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS280
  };
  #line 144 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS281, _M0L6_2atmpS1344);
  #line 145 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS280);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS283
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS282;
  struct _M0TPB6Logger _M0L6_2atmpS1345;
  #line 142 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 143 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS282 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS282);
  _M0L6_2atmpS1345
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS282
  };
  #line 144 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS283, _M0L6_2atmpS1345);
  #line 145 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS282);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGAyE(
  moonbit_bytes_t _M0L4selfS285
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS284;
  struct _M0TPB6Logger _M0L6_2atmpS1346;
  #line 142 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 143 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS284 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS284);
  _M0L6_2atmpS1346
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS284
  };
  #line 144 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPC15array10FixedArrayPB4Show6outputGyE(_M0L4selfS285, _M0L6_2atmpS1346);
  #line 145 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS284);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS287
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS286;
  struct _M0TPB6Logger _M0L6_2atmpS1347;
  #line 142 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 143 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS286 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS286);
  _M0L6_2atmpS1347
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS286
  };
  #line 144 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS287, _M0L6_2atmpS1347);
  #line 145 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS286);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS279
) {
  int32_t _M0L8_2afieldS2214;
  #line 98 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS2214 = _M0L4selfS279.$1;
  moonbit_decref(_M0L4selfS279.$0);
  return _M0L8_2afieldS2214;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS278
) {
  int32_t _M0L3endS1342;
  int32_t _M0L8_2afieldS2215;
  int32_t _M0L5startS1343;
  #line 49 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L3endS1342 = _M0L4selfS278.$2;
  _M0L8_2afieldS2215 = _M0L4selfS278.$1;
  moonbit_decref(_M0L4selfS278.$0);
  _M0L5startS1343 = _M0L8_2afieldS2215;
  return _M0L3endS1342 - _M0L5startS1343;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS277
) {
  moonbit_string_t _M0L8_2afieldS2216;
  #line 91 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS2216 = _M0L4selfS277.$0;
  return _M0L8_2afieldS2216;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS273,
  moonbit_string_t _M0L5valueS274,
  int32_t _M0L5startS275,
  int32_t _M0L3lenS276
) {
  int32_t _M0L6_2atmpS1341;
  int64_t _M0L6_2atmpS1340;
  struct _M0TPC16string10StringView _M0L6_2atmpS1339;
  #line 104 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1341 = _M0L5startS275 + _M0L3lenS276;
  _M0L6_2atmpS1340 = (int64_t)_M0L6_2atmpS1341;
  #line 105 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS1339
  = _M0MPC16string6String11sub_2einner(_M0L5valueS274, _M0L5startS275, _M0L6_2atmpS1340);
  #line 105 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS273, _M0L6_2atmpS1339);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS266,
  int32_t _M0L5startS272,
  int64_t _M0L3endS268
) {
  int32_t _M0L3lenS265;
  int32_t _M0L3endS267;
  int32_t _M0L5startS271;
  int32_t _if__result_2446;
  #line 458 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS265 = Moonbit_array_length(_M0L4selfS266);
  if (_M0L3endS268 == 4294967296ll) {
    _M0L3endS267 = _M0L3lenS265;
  } else {
    int64_t _M0L7_2aSomeS269 = _M0L3endS268;
    int32_t _M0L6_2aendS270 = (int32_t)_M0L7_2aSomeS269;
    if (_M0L6_2aendS270 < 0) {
      _M0L3endS267 = _M0L3lenS265 + _M0L6_2aendS270;
    } else {
      _M0L3endS267 = _M0L6_2aendS270;
    }
  }
  if (_M0L5startS272 < 0) {
    _M0L5startS271 = _M0L3lenS265 + _M0L5startS272;
  } else {
    _M0L5startS271 = _M0L5startS272;
  }
  if (_M0L5startS271 >= 0) {
    if (_M0L5startS271 <= _M0L3endS267) {
      _if__result_2446 = _M0L3endS267 <= _M0L3lenS265;
    } else {
      _if__result_2446 = 0;
    }
  } else {
    _if__result_2446 = 0;
  }
  if (_if__result_2446) {
    if (_M0L5startS271 < _M0L3lenS265) {
      int32_t _M0L6_2atmpS1336 = _M0L4selfS266[_M0L5startS271];
      int32_t _M0L6_2atmpS1335;
      #line 468 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1335
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1336);
      if (!_M0L6_2atmpS1335) {
        
      } else {
        #line 468 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS267 < _M0L3lenS265) {
      int32_t _M0L6_2atmpS1338 = _M0L4selfS266[_M0L3endS267];
      int32_t _M0L6_2atmpS1337;
      #line 471 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS1337
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1338);
      if (!_M0L6_2atmpS1337) {
        
      } else {
        #line 471 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS271,
                                                 _M0L3endS267,
                                                 _M0L4selfS266};
  } else {
    moonbit_decref(_M0L4selfS266);
    #line 466 "/home/blem/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS262) {
  struct _M0TPB6Hasher* _M0L1hS261;
  #line 81 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 82 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L1hS261 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS261);
  #line 83 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS261, _M0L4selfS262);
  #line 84 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS261);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS264
) {
  struct _M0TPB6Hasher* _M0L1hS263;
  #line 81 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 82 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L1hS263 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS263);
  #line 83 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS263, _M0L4selfS264);
  #line 84 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS263);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS259) {
  int32_t _M0L4seedS258;
  if (_M0L10seed_2eoptS259 == 4294967296ll) {
    _M0L4seedS258 = 0;
  } else {
    int64_t _M0L7_2aSomeS260 = _M0L10seed_2eoptS259;
    _M0L4seedS258 = (int32_t)_M0L7_2aSomeS260;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS258);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS257) {
  uint32_t _M0L6_2atmpS1334;
  uint32_t _M0L6_2atmpS1333;
  struct _M0TPB6Hasher* _block_2447;
  #line 75 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1334 = *(uint32_t*)&_M0L4seedS257;
  _M0L6_2atmpS1333 = _M0L6_2atmpS1334 + 374761393u;
  _block_2447
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_2447)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_2447->$0 = _M0L6_2atmpS1333;
  return _block_2447;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS256) {
  uint32_t _M0L6_2atmpS1332;
  #line 437 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  #line 438 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1332 = _M0MPB6Hasher9avalanche(_M0L4selfS256);
  return *(int32_t*)&_M0L6_2atmpS1332;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS255) {
  uint32_t _M0L8_2afieldS2217;
  uint32_t _M0Lm3accS254;
  uint32_t _M0L6_2atmpS1321;
  uint32_t _M0L6_2atmpS1323;
  uint32_t _M0L6_2atmpS1322;
  uint32_t _M0L6_2atmpS1324;
  uint32_t _M0L6_2atmpS1325;
  uint32_t _M0L6_2atmpS1327;
  uint32_t _M0L6_2atmpS1326;
  uint32_t _M0L6_2atmpS1328;
  uint32_t _M0L6_2atmpS1329;
  uint32_t _M0L6_2atmpS1331;
  uint32_t _M0L6_2atmpS1330;
  #line 442 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0L8_2afieldS2217 = _M0L4selfS255->$0;
  moonbit_decref(_M0L4selfS255);
  _M0Lm3accS254 = _M0L8_2afieldS2217;
  _M0L6_2atmpS1321 = _M0Lm3accS254;
  _M0L6_2atmpS1323 = _M0Lm3accS254;
  _M0L6_2atmpS1322 = _M0L6_2atmpS1323 >> 15;
  _M0Lm3accS254 = _M0L6_2atmpS1321 ^ _M0L6_2atmpS1322;
  _M0L6_2atmpS1324 = _M0Lm3accS254;
  _M0Lm3accS254 = _M0L6_2atmpS1324 * 2246822519u;
  _M0L6_2atmpS1325 = _M0Lm3accS254;
  _M0L6_2atmpS1327 = _M0Lm3accS254;
  _M0L6_2atmpS1326 = _M0L6_2atmpS1327 >> 13;
  _M0Lm3accS254 = _M0L6_2atmpS1325 ^ _M0L6_2atmpS1326;
  _M0L6_2atmpS1328 = _M0Lm3accS254;
  _M0Lm3accS254 = _M0L6_2atmpS1328 * 3266489917u;
  _M0L6_2atmpS1329 = _M0Lm3accS254;
  _M0L6_2atmpS1331 = _M0Lm3accS254;
  _M0L6_2atmpS1330 = _M0L6_2atmpS1331 >> 16;
  _M0Lm3accS254 = _M0L6_2atmpS1329 ^ _M0L6_2atmpS1330;
  return _M0Lm3accS254;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS252,
  moonbit_string_t _M0L1yS253
) {
  int32_t _M0L6_2atmpS2218;
  int32_t _M0L6_2atmpS1320;
  #line 25 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 26 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2218 = moonbit_val_array_equal(_M0L1xS252, _M0L1yS253);
  moonbit_decref(_M0L1xS252);
  moonbit_decref(_M0L1yS253);
  _M0L6_2atmpS1320 = _M0L6_2atmpS2218;
  return !_M0L6_2atmpS1320;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS249,
  int32_t _M0L5valueS248
) {
  #line 120 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  #line 121 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS248, _M0L4selfS249);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS251,
  moonbit_string_t _M0L5valueS250
) {
  #line 120 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  #line 121 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS250, _M0L4selfS251);
  return 0;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS246,
  int32_t _M0L5valueS247
) {
  uint32_t _M0L6_2atmpS1319;
  #line 187 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS1319 = *(uint32_t*)&_M0L5valueS247;
  #line 188 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS246, _M0L6_2atmpS1319);
  return 0;
}

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show _M0L3objS236,
  moonbit_string_t _M0L7contentS237,
  moonbit_string_t _M0L3locS239,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS241
) {
  moonbit_string_t _M0L6actualS235;
  #line 184 "/home/blem/.moon/lib/core/builtin/console.mbt"
  #line 191 "/home/blem/.moon/lib/core/builtin/console.mbt"
  _M0L6actualS235 = _M0L3objS236.$0->$method_1(_M0L3objS236.$1);
  moonbit_incref(_M0L7contentS237);
  moonbit_incref(_M0L6actualS235);
  #line 192 "/home/blem/.moon/lib/core/builtin/console.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS235, _M0L7contentS237)
  ) {
    moonbit_string_t _M0L3locS238;
    moonbit_string_t _M0L9args__locS240;
    moonbit_string_t _M0L15expect__escapedS242;
    moonbit_string_t _M0L15actual__escapedS243;
    moonbit_string_t _M0L6_2atmpS1317;
    moonbit_string_t _M0L6_2atmpS1316;
    moonbit_string_t _M0L6_2atmpS2234;
    moonbit_string_t _M0L6_2atmpS1315;
    moonbit_string_t _M0L6_2atmpS2233;
    moonbit_string_t _M0L14expect__base64S244;
    moonbit_string_t _M0L6_2atmpS1314;
    moonbit_string_t _M0L6_2atmpS1313;
    moonbit_string_t _M0L6_2atmpS2232;
    moonbit_string_t _M0L6_2atmpS1312;
    moonbit_string_t _M0L6_2atmpS2231;
    moonbit_string_t _M0L14actual__base64S245;
    moonbit_string_t _M0L6_2atmpS1311;
    moonbit_string_t _M0L6_2atmpS2230;
    moonbit_string_t _M0L6_2atmpS1310;
    moonbit_string_t _M0L6_2atmpS2229;
    moonbit_string_t _M0L6_2atmpS1308;
    moonbit_string_t _M0L6_2atmpS1309;
    moonbit_string_t _M0L6_2atmpS2228;
    moonbit_string_t _M0L6_2atmpS1307;
    moonbit_string_t _M0L6_2atmpS2227;
    moonbit_string_t _M0L6_2atmpS1305;
    moonbit_string_t _M0L6_2atmpS1306;
    moonbit_string_t _M0L6_2atmpS2226;
    moonbit_string_t _M0L6_2atmpS1304;
    moonbit_string_t _M0L6_2atmpS2225;
    moonbit_string_t _M0L6_2atmpS1302;
    moonbit_string_t _M0L6_2atmpS1303;
    moonbit_string_t _M0L6_2atmpS2224;
    moonbit_string_t _M0L6_2atmpS1301;
    moonbit_string_t _M0L6_2atmpS2223;
    moonbit_string_t _M0L6_2atmpS1299;
    moonbit_string_t _M0L6_2atmpS1300;
    moonbit_string_t _M0L6_2atmpS2222;
    moonbit_string_t _M0L6_2atmpS1298;
    moonbit_string_t _M0L6_2atmpS2221;
    moonbit_string_t _M0L6_2atmpS1296;
    moonbit_string_t _M0L6_2atmpS1297;
    moonbit_string_t _M0L6_2atmpS2220;
    moonbit_string_t _M0L6_2atmpS1295;
    moonbit_string_t _M0L6_2atmpS2219;
    moonbit_string_t _M0L6_2atmpS1294;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1293;
    struct moonbit_result_0 _result_2448;
    #line 193 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L3locS238 = _M0MPB9SourceLoc16to__json__string(_M0L3locS239);
    #line 194 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L9args__locS240 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS241);
    moonbit_incref(_M0L7contentS237);
    #line 195 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L15expect__escapedS242
    = _M0MPC16string6String6escape(_M0L7contentS237);
    moonbit_incref(_M0L6actualS235);
    #line 196 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L15actual__escapedS243 = _M0MPC16string6String6escape(_M0L6actualS235);
    #line 197 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1317
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS237);
    #line 197 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1316
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1317);
    #line 197 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2234
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_91.data, _M0L6_2atmpS1316);
    moonbit_decref(_M0L6_2atmpS1316);
    _M0L6_2atmpS1315 = _M0L6_2atmpS2234;
    #line 197 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2233
    = moonbit_add_string(_M0L6_2atmpS1315, (moonbit_string_t)moonbit_string_literal_91.data);
    moonbit_decref(_M0L6_2atmpS1315);
    _M0L14expect__base64S244 = _M0L6_2atmpS2233;
    #line 198 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1314
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS235);
    #line 198 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1313
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1314);
    #line 198 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2232
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_91.data, _M0L6_2atmpS1313);
    moonbit_decref(_M0L6_2atmpS1313);
    _M0L6_2atmpS1312 = _M0L6_2atmpS2232;
    #line 198 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2231
    = moonbit_add_string(_M0L6_2atmpS1312, (moonbit_string_t)moonbit_string_literal_91.data);
    moonbit_decref(_M0L6_2atmpS1312);
    _M0L14actual__base64S245 = _M0L6_2atmpS2231;
    #line 200 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1311 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS238);
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2230
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_92.data, _M0L6_2atmpS1311);
    moonbit_decref(_M0L6_2atmpS1311);
    _M0L6_2atmpS1310 = _M0L6_2atmpS2230;
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2229
    = moonbit_add_string(_M0L6_2atmpS1310, (moonbit_string_t)moonbit_string_literal_93.data);
    moonbit_decref(_M0L6_2atmpS1310);
    _M0L6_2atmpS1308 = _M0L6_2atmpS2229;
    #line 200 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1309
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS240);
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2228 = moonbit_add_string(_M0L6_2atmpS1308, _M0L6_2atmpS1309);
    moonbit_decref(_M0L6_2atmpS1308);
    moonbit_decref(_M0L6_2atmpS1309);
    _M0L6_2atmpS1307 = _M0L6_2atmpS2228;
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2227
    = moonbit_add_string(_M0L6_2atmpS1307, (moonbit_string_t)moonbit_string_literal_94.data);
    moonbit_decref(_M0L6_2atmpS1307);
    _M0L6_2atmpS1305 = _M0L6_2atmpS2227;
    #line 200 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1306
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS242);
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2226 = moonbit_add_string(_M0L6_2atmpS1305, _M0L6_2atmpS1306);
    moonbit_decref(_M0L6_2atmpS1305);
    moonbit_decref(_M0L6_2atmpS1306);
    _M0L6_2atmpS1304 = _M0L6_2atmpS2226;
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2225
    = moonbit_add_string(_M0L6_2atmpS1304, (moonbit_string_t)moonbit_string_literal_95.data);
    moonbit_decref(_M0L6_2atmpS1304);
    _M0L6_2atmpS1302 = _M0L6_2atmpS2225;
    #line 200 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1303
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS243);
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2224 = moonbit_add_string(_M0L6_2atmpS1302, _M0L6_2atmpS1303);
    moonbit_decref(_M0L6_2atmpS1302);
    moonbit_decref(_M0L6_2atmpS1303);
    _M0L6_2atmpS1301 = _M0L6_2atmpS2224;
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2223
    = moonbit_add_string(_M0L6_2atmpS1301, (moonbit_string_t)moonbit_string_literal_96.data);
    moonbit_decref(_M0L6_2atmpS1301);
    _M0L6_2atmpS1299 = _M0L6_2atmpS2223;
    #line 200 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1300
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S244);
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2222 = moonbit_add_string(_M0L6_2atmpS1299, _M0L6_2atmpS1300);
    moonbit_decref(_M0L6_2atmpS1299);
    moonbit_decref(_M0L6_2atmpS1300);
    _M0L6_2atmpS1298 = _M0L6_2atmpS2222;
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2221
    = moonbit_add_string(_M0L6_2atmpS1298, (moonbit_string_t)moonbit_string_literal_97.data);
    moonbit_decref(_M0L6_2atmpS1298);
    _M0L6_2atmpS1296 = _M0L6_2atmpS2221;
    #line 200 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1297
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S245);
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2220 = moonbit_add_string(_M0L6_2atmpS1296, _M0L6_2atmpS1297);
    moonbit_decref(_M0L6_2atmpS1296);
    moonbit_decref(_M0L6_2atmpS1297);
    _M0L6_2atmpS1295 = _M0L6_2atmpS2220;
    #line 199 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS2219
    = moonbit_add_string(_M0L6_2atmpS1295, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1295);
    _M0L6_2atmpS1294 = _M0L6_2atmpS2219;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1293
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1293)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1293)->$0
    = _M0L6_2atmpS1294;
    _result_2448.tag = 0;
    _result_2448.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1293;
    return _result_2448;
  } else {
    int32_t _M0L6_2atmpS1318;
    struct moonbit_result_0 _result_2449;
    moonbit_decref(_M0L9args__locS241);
    moonbit_decref(_M0L3locS239);
    moonbit_decref(_M0L7contentS237);
    moonbit_decref(_M0L6actualS235);
    _M0L6_2atmpS1318 = 0;
    _result_2449.tag = 1;
    _result_2449.data.ok = _M0L6_2atmpS1318;
    return _result_2449;
  }
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS228
) {
  struct _M0TPB13StringBuilder* _M0L3bufS226;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS227;
  int32_t _M0L7_2abindS229;
  int32_t _M0L1iS230;
  #line 140 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  #line 141 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L3bufS226 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS227 = _M0L4selfS228;
  moonbit_incref(_M0L3bufS226);
  #line 143 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS226, 91);
  _M0L7_2abindS229 = _M0L7_2aselfS227->$1;
  _M0L1iS230 = 0;
  while (1) {
    if (_M0L1iS230 < _M0L7_2abindS229) {
      int32_t _if__result_2451;
      moonbit_string_t* _M0L8_2afieldS2236;
      moonbit_string_t* _M0L3bufS1291;
      moonbit_string_t _M0L6_2atmpS2235;
      moonbit_string_t _M0L4itemS231;
      int32_t _M0L6_2atmpS1292;
      if (_M0L1iS230 != 0) {
        moonbit_incref(_M0L3bufS226);
        #line 146 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS226, (moonbit_string_t)moonbit_string_literal_56.data);
      }
      if (_M0L1iS230 < 0) {
        _if__result_2451 = 1;
      } else {
        int32_t _M0L3lenS1290 = _M0L7_2aselfS227->$1;
        _if__result_2451 = _M0L1iS230 >= _M0L3lenS1290;
      }
      if (_if__result_2451) {
        #line 148 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS2236 = _M0L7_2aselfS227->$0;
      _M0L3bufS1291 = _M0L8_2afieldS2236;
      _M0L6_2atmpS2235 = (moonbit_string_t)_M0L3bufS1291[_M0L1iS230];
      _M0L4itemS231 = _M0L6_2atmpS2235;
      if (_M0L4itemS231 == 0) {
        moonbit_incref(_M0L3bufS226);
        #line 150 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS226, (moonbit_string_t)moonbit_string_literal_98.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS232 = _M0L4itemS231;
        moonbit_string_t _M0L6_2alocS233 = _M0L7_2aSomeS232;
        moonbit_string_t _M0L6_2atmpS1289;
        moonbit_incref(_M0L6_2alocS233);
        #line 151 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
        _M0L6_2atmpS1289
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS233);
        moonbit_incref(_M0L3bufS226);
        #line 151 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS226, _M0L6_2atmpS1289);
      }
      _M0L6_2atmpS1292 = _M0L1iS230 + 1;
      _M0L1iS230 = _M0L6_2atmpS1292;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS227);
    }
    break;
  }
  moonbit_incref(_M0L3bufS226);
  #line 154 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS226, 93);
  #line 155 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS226);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS225
) {
  moonbit_string_t _M0L6_2atmpS1288;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1287;
  #line 118 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1288 = _M0L4selfS225;
  #line 119 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1287 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1288);
  #line 119 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1287);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS224
) {
  struct _M0TPB13StringBuilder* _M0L2sbS223;
  struct _M0TPC16string10StringView _M0L8_2afieldS2249;
  struct _M0TPC16string10StringView _M0L3pkgS1272;
  moonbit_string_t _M0L6_2atmpS1271;
  moonbit_string_t _M0L6_2atmpS2248;
  moonbit_string_t _M0L6_2atmpS1270;
  moonbit_string_t _M0L6_2atmpS2247;
  moonbit_string_t _M0L6_2atmpS1269;
  struct _M0TPC16string10StringView _M0L8_2afieldS2246;
  struct _M0TPC16string10StringView _M0L8filenameS1273;
  struct _M0TPC16string10StringView _M0L8_2afieldS2245;
  struct _M0TPC16string10StringView _M0L11start__lineS1276;
  moonbit_string_t _M0L6_2atmpS1275;
  moonbit_string_t _M0L6_2atmpS2244;
  moonbit_string_t _M0L6_2atmpS1274;
  struct _M0TPC16string10StringView _M0L8_2afieldS2243;
  struct _M0TPC16string10StringView _M0L13start__columnS1279;
  moonbit_string_t _M0L6_2atmpS1278;
  moonbit_string_t _M0L6_2atmpS2242;
  moonbit_string_t _M0L6_2atmpS1277;
  struct _M0TPC16string10StringView _M0L8_2afieldS2241;
  struct _M0TPC16string10StringView _M0L9end__lineS1282;
  moonbit_string_t _M0L6_2atmpS1281;
  moonbit_string_t _M0L6_2atmpS2240;
  moonbit_string_t _M0L6_2atmpS1280;
  struct _M0TPC16string10StringView _M0L8_2afieldS2239;
  int32_t _M0L6_2acntS2346;
  struct _M0TPC16string10StringView _M0L11end__columnS1286;
  moonbit_string_t _M0L6_2atmpS1285;
  moonbit_string_t _M0L6_2atmpS2238;
  moonbit_string_t _M0L6_2atmpS1284;
  moonbit_string_t _M0L6_2atmpS2237;
  moonbit_string_t _M0L6_2atmpS1283;
  #line 104 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  #line 105 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L2sbS223 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS2249
  = (struct _M0TPC16string10StringView){
    _M0L4selfS224->$0_1, _M0L4selfS224->$0_2, _M0L4selfS224->$0_0
  };
  _M0L3pkgS1272 = _M0L8_2afieldS2249;
  moonbit_incref(_M0L3pkgS1272.$0);
  #line 106 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1271
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1272);
  #line 106 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS2248
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_99.data, _M0L6_2atmpS1271);
  moonbit_decref(_M0L6_2atmpS1271);
  _M0L6_2atmpS1270 = _M0L6_2atmpS2248;
  #line 106 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS2247
  = moonbit_add_string(_M0L6_2atmpS1270, (moonbit_string_t)moonbit_string_literal_91.data);
  moonbit_decref(_M0L6_2atmpS1270);
  _M0L6_2atmpS1269 = _M0L6_2atmpS2247;
  moonbit_incref(_M0L2sbS223);
  #line 106 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS223, _M0L6_2atmpS1269);
  moonbit_incref(_M0L2sbS223);
  #line 107 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS223, (moonbit_string_t)moonbit_string_literal_100.data);
  _M0L8_2afieldS2246
  = (struct _M0TPC16string10StringView){
    _M0L4selfS224->$1_1, _M0L4selfS224->$1_2, _M0L4selfS224->$1_0
  };
  _M0L8filenameS1273 = _M0L8_2afieldS2246;
  moonbit_incref(_M0L8filenameS1273.$0);
  moonbit_incref(_M0L2sbS223);
  #line 108 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS223, _M0L8filenameS1273);
  _M0L8_2afieldS2245
  = (struct _M0TPC16string10StringView){
    _M0L4selfS224->$2_1, _M0L4selfS224->$2_2, _M0L4selfS224->$2_0
  };
  _M0L11start__lineS1276 = _M0L8_2afieldS2245;
  moonbit_incref(_M0L11start__lineS1276.$0);
  #line 109 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1275
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1276);
  #line 109 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS2244
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_101.data, _M0L6_2atmpS1275);
  moonbit_decref(_M0L6_2atmpS1275);
  _M0L6_2atmpS1274 = _M0L6_2atmpS2244;
  moonbit_incref(_M0L2sbS223);
  #line 109 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS223, _M0L6_2atmpS1274);
  _M0L8_2afieldS2243
  = (struct _M0TPC16string10StringView){
    _M0L4selfS224->$3_1, _M0L4selfS224->$3_2, _M0L4selfS224->$3_0
  };
  _M0L13start__columnS1279 = _M0L8_2afieldS2243;
  moonbit_incref(_M0L13start__columnS1279.$0);
  #line 110 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1278
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1279);
  #line 110 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS2242
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_102.data, _M0L6_2atmpS1278);
  moonbit_decref(_M0L6_2atmpS1278);
  _M0L6_2atmpS1277 = _M0L6_2atmpS2242;
  moonbit_incref(_M0L2sbS223);
  #line 110 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS223, _M0L6_2atmpS1277);
  _M0L8_2afieldS2241
  = (struct _M0TPC16string10StringView){
    _M0L4selfS224->$4_1, _M0L4selfS224->$4_2, _M0L4selfS224->$4_0
  };
  _M0L9end__lineS1282 = _M0L8_2afieldS2241;
  moonbit_incref(_M0L9end__lineS1282.$0);
  #line 111 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1281
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1282);
  #line 111 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS2240
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_103.data, _M0L6_2atmpS1281);
  moonbit_decref(_M0L6_2atmpS1281);
  _M0L6_2atmpS1280 = _M0L6_2atmpS2240;
  moonbit_incref(_M0L2sbS223);
  #line 111 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS223, _M0L6_2atmpS1280);
  _M0L8_2afieldS2239
  = (struct _M0TPC16string10StringView){
    _M0L4selfS224->$5_1, _M0L4selfS224->$5_2, _M0L4selfS224->$5_0
  };
  _M0L6_2acntS2346 = Moonbit_object_header(_M0L4selfS224)->rc;
  if (_M0L6_2acntS2346 > 1) {
    int32_t _M0L11_2anew__cntS2352 = _M0L6_2acntS2346 - 1;
    Moonbit_object_header(_M0L4selfS224)->rc = _M0L11_2anew__cntS2352;
    moonbit_incref(_M0L8_2afieldS2239.$0);
  } else if (_M0L6_2acntS2346 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS2351 =
      (struct _M0TPC16string10StringView){_M0L4selfS224->$4_1,
                                            _M0L4selfS224->$4_2,
                                            _M0L4selfS224->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS2350;
    struct _M0TPC16string10StringView _M0L8_2afieldS2349;
    struct _M0TPC16string10StringView _M0L8_2afieldS2348;
    struct _M0TPC16string10StringView _M0L8_2afieldS2347;
    moonbit_decref(_M0L8_2afieldS2351.$0);
    _M0L8_2afieldS2350
    = (struct _M0TPC16string10StringView){
      _M0L4selfS224->$3_1, _M0L4selfS224->$3_2, _M0L4selfS224->$3_0
    };
    moonbit_decref(_M0L8_2afieldS2350.$0);
    _M0L8_2afieldS2349
    = (struct _M0TPC16string10StringView){
      _M0L4selfS224->$2_1, _M0L4selfS224->$2_2, _M0L4selfS224->$2_0
    };
    moonbit_decref(_M0L8_2afieldS2349.$0);
    _M0L8_2afieldS2348
    = (struct _M0TPC16string10StringView){
      _M0L4selfS224->$1_1, _M0L4selfS224->$1_2, _M0L4selfS224->$1_0
    };
    moonbit_decref(_M0L8_2afieldS2348.$0);
    _M0L8_2afieldS2347
    = (struct _M0TPC16string10StringView){
      _M0L4selfS224->$0_1, _M0L4selfS224->$0_2, _M0L4selfS224->$0_0
    };
    moonbit_decref(_M0L8_2afieldS2347.$0);
    #line 112 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
    moonbit_free(_M0L4selfS224);
  }
  _M0L11end__columnS1286 = _M0L8_2afieldS2239;
  #line 112 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1285
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1286);
  #line 112 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS2238
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_104.data, _M0L6_2atmpS1285);
  moonbit_decref(_M0L6_2atmpS1285);
  _M0L6_2atmpS1284 = _M0L6_2atmpS2238;
  #line 112 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS2237
  = moonbit_add_string(_M0L6_2atmpS1284, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1284);
  _M0L6_2atmpS1283 = _M0L6_2atmpS2237;
  moonbit_incref(_M0L2sbS223);
  #line 112 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS223, _M0L6_2atmpS1283);
  #line 113 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS223);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS221,
  moonbit_string_t _M0L3strS222
) {
  int32_t _M0L3lenS1259;
  int32_t _M0L6_2atmpS1261;
  int32_t _M0L6_2atmpS1260;
  int32_t _M0L6_2atmpS1258;
  moonbit_bytes_t _M0L8_2afieldS2251;
  moonbit_bytes_t _M0L4dataS1262;
  int32_t _M0L3lenS1263;
  int32_t _M0L6_2atmpS1264;
  int32_t _M0L3lenS1266;
  int32_t _M0L6_2atmpS2250;
  int32_t _M0L6_2atmpS1268;
  int32_t _M0L6_2atmpS1267;
  int32_t _M0L6_2atmpS1265;
  #line 66 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS1259 = _M0L4selfS221->$1;
  _M0L6_2atmpS1261 = Moonbit_array_length(_M0L3strS222);
  _M0L6_2atmpS1260 = _M0L6_2atmpS1261 * 2;
  _M0L6_2atmpS1258 = _M0L3lenS1259 + _M0L6_2atmpS1260;
  moonbit_incref(_M0L4selfS221);
  #line 67 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS221, _M0L6_2atmpS1258);
  _M0L8_2afieldS2251 = _M0L4selfS221->$0;
  _M0L4dataS1262 = _M0L8_2afieldS2251;
  _M0L3lenS1263 = _M0L4selfS221->$1;
  _M0L6_2atmpS1264 = Moonbit_array_length(_M0L3strS222);
  moonbit_incref(_M0L4dataS1262);
  moonbit_incref(_M0L3strS222);
  #line 68 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1262, _M0L3lenS1263, _M0L3strS222, 0, _M0L6_2atmpS1264);
  _M0L3lenS1266 = _M0L4selfS221->$1;
  _M0L6_2atmpS2250 = Moonbit_array_length(_M0L3strS222);
  moonbit_decref(_M0L3strS222);
  _M0L6_2atmpS1268 = _M0L6_2atmpS2250;
  _M0L6_2atmpS1267 = _M0L6_2atmpS1268 * 2;
  _M0L6_2atmpS1265 = _M0L3lenS1266 + _M0L6_2atmpS1267;
  _M0L4selfS221->$1 = _M0L6_2atmpS1265;
  moonbit_decref(_M0L4selfS221);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS213,
  int32_t _M0L13bytes__offsetS208,
  moonbit_string_t _M0L3strS215,
  int32_t _M0L11str__offsetS211,
  int32_t _M0L6lengthS209
) {
  int32_t _M0L6_2atmpS1257;
  int32_t _M0L6_2atmpS1256;
  int32_t _M0L2e1S207;
  int32_t _M0L6_2atmpS1255;
  int32_t _M0L2e2S210;
  int32_t _M0L4len1S212;
  int32_t _M0L4len2S214;
  int32_t _if__result_2452;
  #line 124 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
  _M0L6_2atmpS1257 = _M0L6lengthS209 * 2;
  _M0L6_2atmpS1256 = _M0L13bytes__offsetS208 + _M0L6_2atmpS1257;
  _M0L2e1S207 = _M0L6_2atmpS1256 - 1;
  _M0L6_2atmpS1255 = _M0L11str__offsetS211 + _M0L6lengthS209;
  _M0L2e2S210 = _M0L6_2atmpS1255 - 1;
  _M0L4len1S212 = Moonbit_array_length(_M0L4selfS213);
  _M0L4len2S214 = Moonbit_array_length(_M0L3strS215);
  if (_M0L6lengthS209 >= 0) {
    if (_M0L13bytes__offsetS208 >= 0) {
      if (_M0L2e1S207 < _M0L4len1S212) {
        if (_M0L11str__offsetS211 >= 0) {
          _if__result_2452 = _M0L2e2S210 < _M0L4len2S214;
        } else {
          _if__result_2452 = 0;
        }
      } else {
        _if__result_2452 = 0;
      }
    } else {
      _if__result_2452 = 0;
    }
  } else {
    _if__result_2452 = 0;
  }
  if (_if__result_2452) {
    int32_t _M0L16end__str__offsetS216 =
      _M0L11str__offsetS211 + _M0L6lengthS209;
    int32_t _M0L1iS217 = _M0L11str__offsetS211;
    int32_t _M0L1jS218 = _M0L13bytes__offsetS208;
    while (1) {
      if (_M0L1iS217 < _M0L16end__str__offsetS216) {
        int32_t _M0L6_2atmpS1252 = _M0L3strS215[_M0L1iS217];
        int32_t _M0L6_2atmpS1251 = (int32_t)_M0L6_2atmpS1252;
        uint32_t _M0L1cS219 = *(uint32_t*)&_M0L6_2atmpS1251;
        uint32_t _M0L6_2atmpS1247 = _M0L1cS219 & 255u;
        int32_t _M0L6_2atmpS1246;
        int32_t _M0L6_2atmpS1248;
        uint32_t _M0L6_2atmpS1250;
        int32_t _M0L6_2atmpS1249;
        int32_t _M0L6_2atmpS1253;
        int32_t _M0L6_2atmpS1254;
        #line 141 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1246 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1247);
        if (
          _M0L1jS218 < 0 || _M0L1jS218 >= Moonbit_array_length(_M0L4selfS213)
        ) {
          #line 141 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS213[_M0L1jS218] = _M0L6_2atmpS1246;
        _M0L6_2atmpS1248 = _M0L1jS218 + 1;
        _M0L6_2atmpS1250 = _M0L1cS219 >> 8;
        #line 142 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS1249 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1250);
        if (
          _M0L6_2atmpS1248 < 0
          || _M0L6_2atmpS1248 >= Moonbit_array_length(_M0L4selfS213)
        ) {
          #line 142 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS213[_M0L6_2atmpS1248] = _M0L6_2atmpS1249;
        _M0L6_2atmpS1253 = _M0L1iS217 + 1;
        _M0L6_2atmpS1254 = _M0L1jS218 + 2;
        _M0L1iS217 = _M0L6_2atmpS1253;
        _M0L1jS218 = _M0L6_2atmpS1254;
        continue;
      } else {
        moonbit_decref(_M0L3strS215);
        moonbit_decref(_M0L4selfS213);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS215);
    moonbit_decref(_M0L4selfS213);
    #line 137 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS206,
  struct _M0TPC16string10StringView _M0L3objS205
) {
  struct _M0TPB6Logger _M0L6_2atmpS1245;
  #line 17 "/home/blem/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0L6_2atmpS1245
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS206
  };
  #line 21 "/home/blem/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS205, _M0L6_2atmpS1245);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS151
) {
  int32_t _M0L6_2atmpS1244;
  struct _M0TPC16string10StringView _M0L7_2abindS150;
  moonbit_string_t _M0L7_2adataS152;
  int32_t _M0L8_2astartS153;
  int32_t _M0L6_2atmpS1243;
  int32_t _M0L6_2aendS154;
  int32_t _M0Lm9_2acursorS155;
  int32_t _M0Lm13accept__stateS156;
  int32_t _M0Lm10match__endS157;
  int32_t _M0Lm20match__tag__saver__0S158;
  int32_t _M0Lm20match__tag__saver__1S159;
  int32_t _M0Lm20match__tag__saver__2S160;
  int32_t _M0Lm20match__tag__saver__3S161;
  int32_t _M0Lm20match__tag__saver__4S162;
  int32_t _M0Lm6tag__0S163;
  int32_t _M0Lm6tag__1S164;
  int32_t _M0Lm9tag__1__1S165;
  int32_t _M0Lm9tag__1__2S166;
  int32_t _M0Lm6tag__3S167;
  int32_t _M0Lm6tag__2S168;
  int32_t _M0Lm9tag__2__1S169;
  int32_t _M0Lm6tag__4S170;
  int32_t _M0L6_2atmpS1201;
  #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1244 = Moonbit_array_length(_M0L4reprS151);
  _M0L7_2abindS150
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1244, _M0L4reprS151
  };
  moonbit_incref(_M0L7_2abindS150.$0);
  #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L7_2adataS152 = _M0MPC16string10StringView4data(_M0L7_2abindS150);
  moonbit_incref(_M0L7_2abindS150.$0);
  #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L8_2astartS153
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS150);
  #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
  _M0L6_2atmpS1243 = _M0MPC16string10StringView6length(_M0L7_2abindS150);
  _M0L6_2aendS154 = _M0L8_2astartS153 + _M0L6_2atmpS1243;
  _M0Lm9_2acursorS155 = _M0L8_2astartS153;
  _M0Lm13accept__stateS156 = -1;
  _M0Lm10match__endS157 = -1;
  _M0Lm20match__tag__saver__0S158 = -1;
  _M0Lm20match__tag__saver__1S159 = -1;
  _M0Lm20match__tag__saver__2S160 = -1;
  _M0Lm20match__tag__saver__3S161 = -1;
  _M0Lm20match__tag__saver__4S162 = -1;
  _M0Lm6tag__0S163 = -1;
  _M0Lm6tag__1S164 = -1;
  _M0Lm9tag__1__1S165 = -1;
  _M0Lm9tag__1__2S166 = -1;
  _M0Lm6tag__3S167 = -1;
  _M0Lm6tag__2S168 = -1;
  _M0Lm9tag__2__1S169 = -1;
  _M0Lm6tag__4S170 = -1;
  _M0L6_2atmpS1201 = _M0Lm9_2acursorS155;
  if (_M0L6_2atmpS1201 < _M0L6_2aendS154) {
    int32_t _M0L6_2atmpS1203 = _M0Lm9_2acursorS155;
    int32_t _M0L6_2atmpS1202;
    moonbit_incref(_M0L7_2adataS152);
    #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
    _M0L6_2atmpS1202
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1203);
    if (_M0L6_2atmpS1202 == 64) {
      int32_t _M0L6_2atmpS1204 = _M0Lm9_2acursorS155;
      _M0Lm9_2acursorS155 = _M0L6_2atmpS1204 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1205;
        _M0Lm6tag__0S163 = _M0Lm9_2acursorS155;
        _M0L6_2atmpS1205 = _M0Lm9_2acursorS155;
        if (_M0L6_2atmpS1205 < _M0L6_2aendS154) {
          int32_t _M0L6_2atmpS1242 = _M0Lm9_2acursorS155;
          int32_t _M0L10next__charS178;
          int32_t _M0L6_2atmpS1206;
          moonbit_incref(_M0L7_2adataS152);
          #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
          _M0L10next__charS178
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1242);
          _M0L6_2atmpS1206 = _M0Lm9_2acursorS155;
          _M0Lm9_2acursorS155 = _M0L6_2atmpS1206 + 1;
          if (_M0L10next__charS178 == 58) {
            int32_t _M0L6_2atmpS1207 = _M0Lm9_2acursorS155;
            if (_M0L6_2atmpS1207 < _M0L6_2aendS154) {
              int32_t _M0L6_2atmpS1208 = _M0Lm9_2acursorS155;
              int32_t _M0L12dispatch__15S179;
              _M0Lm9_2acursorS155 = _M0L6_2atmpS1208 + 1;
              _M0L12dispatch__15S179 = 0;
              loop__label__15_182:;
              while (1) {
                int32_t _M0L6_2atmpS1209;
                switch (_M0L12dispatch__15S179) {
                  case 3: {
                    int32_t _M0L6_2atmpS1212;
                    _M0Lm9tag__1__2S166 = _M0Lm9tag__1__1S165;
                    _M0Lm9tag__1__1S165 = _M0Lm6tag__1S164;
                    _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                    _M0L6_2atmpS1212 = _M0Lm9_2acursorS155;
                    if (_M0L6_2atmpS1212 < _M0L6_2aendS154) {
                      int32_t _M0L6_2atmpS1217 = _M0Lm9_2acursorS155;
                      int32_t _M0L10next__charS186;
                      int32_t _M0L6_2atmpS1213;
                      moonbit_incref(_M0L7_2adataS152);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS186
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1217);
                      _M0L6_2atmpS1213 = _M0Lm9_2acursorS155;
                      _M0Lm9_2acursorS155 = _M0L6_2atmpS1213 + 1;
                      if (_M0L10next__charS186 < 58) {
                        if (_M0L10next__charS186 < 48) {
                          goto join_185;
                        } else {
                          int32_t _M0L6_2atmpS1214;
                          _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                          _M0Lm9tag__2__1S169 = _M0Lm6tag__2S168;
                          _M0Lm6tag__2S168 = _M0Lm9_2acursorS155;
                          _M0Lm6tag__3S167 = _M0Lm9_2acursorS155;
                          _M0L6_2atmpS1214 = _M0Lm9_2acursorS155;
                          if (_M0L6_2atmpS1214 < _M0L6_2aendS154) {
                            int32_t _M0L6_2atmpS1216 = _M0Lm9_2acursorS155;
                            int32_t _M0L10next__charS188;
                            int32_t _M0L6_2atmpS1215;
                            moonbit_incref(_M0L7_2adataS152);
                            #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                            _M0L10next__charS188
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1216);
                            _M0L6_2atmpS1215 = _M0Lm9_2acursorS155;
                            _M0Lm9_2acursorS155 = _M0L6_2atmpS1215 + 1;
                            if (_M0L10next__charS188 < 48) {
                              if (_M0L10next__charS188 == 45) {
                                goto join_180;
                              } else {
                                goto join_187;
                              }
                            } else if (_M0L10next__charS188 > 57) {
                              if (_M0L10next__charS188 < 59) {
                                _M0L12dispatch__15S179 = 3;
                                goto loop__label__15_182;
                              } else {
                                goto join_187;
                              }
                            } else {
                              _M0L12dispatch__15S179 = 6;
                              goto loop__label__15_182;
                            }
                            join_187:;
                            _M0L12dispatch__15S179 = 0;
                            goto loop__label__15_182;
                          } else {
                            goto join_171;
                          }
                        }
                      } else if (_M0L10next__charS186 > 58) {
                        goto join_185;
                      } else {
                        _M0L12dispatch__15S179 = 1;
                        goto loop__label__15_182;
                      }
                      join_185:;
                      _M0L12dispatch__15S179 = 0;
                      goto loop__label__15_182;
                    } else {
                      goto join_171;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1218;
                    _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                    _M0Lm6tag__2S168 = _M0Lm9_2acursorS155;
                    _M0L6_2atmpS1218 = _M0Lm9_2acursorS155;
                    if (_M0L6_2atmpS1218 < _M0L6_2aendS154) {
                      int32_t _M0L6_2atmpS1220 = _M0Lm9_2acursorS155;
                      int32_t _M0L10next__charS190;
                      int32_t _M0L6_2atmpS1219;
                      moonbit_incref(_M0L7_2adataS152);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS190
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1220);
                      _M0L6_2atmpS1219 = _M0Lm9_2acursorS155;
                      _M0Lm9_2acursorS155 = _M0L6_2atmpS1219 + 1;
                      if (_M0L10next__charS190 < 58) {
                        if (_M0L10next__charS190 < 48) {
                          goto join_189;
                        } else {
                          _M0L12dispatch__15S179 = 2;
                          goto loop__label__15_182;
                        }
                      } else if (_M0L10next__charS190 > 58) {
                        goto join_189;
                      } else {
                        _M0L12dispatch__15S179 = 3;
                        goto loop__label__15_182;
                      }
                      join_189:;
                      _M0L12dispatch__15S179 = 0;
                      goto loop__label__15_182;
                    } else {
                      goto join_171;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1221;
                    _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                    _M0L6_2atmpS1221 = _M0Lm9_2acursorS155;
                    if (_M0L6_2atmpS1221 < _M0L6_2aendS154) {
                      int32_t _M0L6_2atmpS1223 = _M0Lm9_2acursorS155;
                      int32_t _M0L10next__charS191;
                      int32_t _M0L6_2atmpS1222;
                      moonbit_incref(_M0L7_2adataS152);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS191
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1223);
                      _M0L6_2atmpS1222 = _M0Lm9_2acursorS155;
                      _M0Lm9_2acursorS155 = _M0L6_2atmpS1222 + 1;
                      if (_M0L10next__charS191 == 58) {
                        _M0L12dispatch__15S179 = 1;
                        goto loop__label__15_182;
                      } else {
                        _M0L12dispatch__15S179 = 0;
                        goto loop__label__15_182;
                      }
                    } else {
                      goto join_171;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1224;
                    _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                    _M0Lm6tag__4S170 = _M0Lm9_2acursorS155;
                    _M0L6_2atmpS1224 = _M0Lm9_2acursorS155;
                    if (_M0L6_2atmpS1224 < _M0L6_2aendS154) {
                      int32_t _M0L6_2atmpS1232 = _M0Lm9_2acursorS155;
                      int32_t _M0L10next__charS193;
                      int32_t _M0L6_2atmpS1225;
                      moonbit_incref(_M0L7_2adataS152);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS193
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1232);
                      _M0L6_2atmpS1225 = _M0Lm9_2acursorS155;
                      _M0Lm9_2acursorS155 = _M0L6_2atmpS1225 + 1;
                      if (_M0L10next__charS193 < 58) {
                        if (_M0L10next__charS193 < 48) {
                          goto join_192;
                        } else {
                          _M0L12dispatch__15S179 = 4;
                          goto loop__label__15_182;
                        }
                      } else if (_M0L10next__charS193 > 58) {
                        goto join_192;
                      } else {
                        int32_t _M0L6_2atmpS1226;
                        _M0Lm9tag__1__2S166 = _M0Lm9tag__1__1S165;
                        _M0Lm9tag__1__1S165 = _M0Lm6tag__1S164;
                        _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                        _M0L6_2atmpS1226 = _M0Lm9_2acursorS155;
                        if (_M0L6_2atmpS1226 < _M0L6_2aendS154) {
                          int32_t _M0L6_2atmpS1231 = _M0Lm9_2acursorS155;
                          int32_t _M0L10next__charS195;
                          int32_t _M0L6_2atmpS1227;
                          moonbit_incref(_M0L7_2adataS152);
                          #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                          _M0L10next__charS195
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1231);
                          _M0L6_2atmpS1227 = _M0Lm9_2acursorS155;
                          _M0Lm9_2acursorS155 = _M0L6_2atmpS1227 + 1;
                          if (_M0L10next__charS195 < 58) {
                            if (_M0L10next__charS195 < 48) {
                              goto join_194;
                            } else {
                              int32_t _M0L6_2atmpS1228;
                              _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                              _M0Lm9tag__2__1S169 = _M0Lm6tag__2S168;
                              _M0Lm6tag__2S168 = _M0Lm9_2acursorS155;
                              _M0L6_2atmpS1228 = _M0Lm9_2acursorS155;
                              if (_M0L6_2atmpS1228 < _M0L6_2aendS154) {
                                int32_t _M0L6_2atmpS1230 =
                                  _M0Lm9_2acursorS155;
                                int32_t _M0L10next__charS197;
                                int32_t _M0L6_2atmpS1229;
                                moonbit_incref(_M0L7_2adataS152);
                                #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                                _M0L10next__charS197
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1230);
                                _M0L6_2atmpS1229 = _M0Lm9_2acursorS155;
                                _M0Lm9_2acursorS155 = _M0L6_2atmpS1229 + 1;
                                if (_M0L10next__charS197 < 58) {
                                  if (_M0L10next__charS197 < 48) {
                                    goto join_196;
                                  } else {
                                    _M0L12dispatch__15S179 = 5;
                                    goto loop__label__15_182;
                                  }
                                } else if (_M0L10next__charS197 > 58) {
                                  goto join_196;
                                } else {
                                  _M0L12dispatch__15S179 = 3;
                                  goto loop__label__15_182;
                                }
                                join_196:;
                                _M0L12dispatch__15S179 = 0;
                                goto loop__label__15_182;
                              } else {
                                goto join_184;
                              }
                            }
                          } else if (_M0L10next__charS195 > 58) {
                            goto join_194;
                          } else {
                            _M0L12dispatch__15S179 = 1;
                            goto loop__label__15_182;
                          }
                          join_194:;
                          _M0L12dispatch__15S179 = 0;
                          goto loop__label__15_182;
                        } else {
                          goto join_171;
                        }
                      }
                      join_192:;
                      _M0L12dispatch__15S179 = 0;
                      goto loop__label__15_182;
                    } else {
                      goto join_171;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1233;
                    _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                    _M0Lm6tag__2S168 = _M0Lm9_2acursorS155;
                    _M0L6_2atmpS1233 = _M0Lm9_2acursorS155;
                    if (_M0L6_2atmpS1233 < _M0L6_2aendS154) {
                      int32_t _M0L6_2atmpS1235 = _M0Lm9_2acursorS155;
                      int32_t _M0L10next__charS199;
                      int32_t _M0L6_2atmpS1234;
                      moonbit_incref(_M0L7_2adataS152);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS199
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1235);
                      _M0L6_2atmpS1234 = _M0Lm9_2acursorS155;
                      _M0Lm9_2acursorS155 = _M0L6_2atmpS1234 + 1;
                      if (_M0L10next__charS199 < 58) {
                        if (_M0L10next__charS199 < 48) {
                          goto join_198;
                        } else {
                          _M0L12dispatch__15S179 = 5;
                          goto loop__label__15_182;
                        }
                      } else if (_M0L10next__charS199 > 58) {
                        goto join_198;
                      } else {
                        _M0L12dispatch__15S179 = 3;
                        goto loop__label__15_182;
                      }
                      join_198:;
                      _M0L12dispatch__15S179 = 0;
                      goto loop__label__15_182;
                    } else {
                      goto join_184;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1236;
                    _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                    _M0Lm6tag__2S168 = _M0Lm9_2acursorS155;
                    _M0Lm6tag__3S167 = _M0Lm9_2acursorS155;
                    _M0L6_2atmpS1236 = _M0Lm9_2acursorS155;
                    if (_M0L6_2atmpS1236 < _M0L6_2aendS154) {
                      int32_t _M0L6_2atmpS1238 = _M0Lm9_2acursorS155;
                      int32_t _M0L10next__charS201;
                      int32_t _M0L6_2atmpS1237;
                      moonbit_incref(_M0L7_2adataS152);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS201
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1238);
                      _M0L6_2atmpS1237 = _M0Lm9_2acursorS155;
                      _M0Lm9_2acursorS155 = _M0L6_2atmpS1237 + 1;
                      if (_M0L10next__charS201 < 48) {
                        if (_M0L10next__charS201 == 45) {
                          goto join_180;
                        } else {
                          goto join_200;
                        }
                      } else if (_M0L10next__charS201 > 57) {
                        if (_M0L10next__charS201 < 59) {
                          _M0L12dispatch__15S179 = 3;
                          goto loop__label__15_182;
                        } else {
                          goto join_200;
                        }
                      } else {
                        _M0L12dispatch__15S179 = 6;
                        goto loop__label__15_182;
                      }
                      join_200:;
                      _M0L12dispatch__15S179 = 0;
                      goto loop__label__15_182;
                    } else {
                      goto join_171;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1239;
                    _M0Lm9tag__1__1S165 = _M0Lm6tag__1S164;
                    _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                    _M0L6_2atmpS1239 = _M0Lm9_2acursorS155;
                    if (_M0L6_2atmpS1239 < _M0L6_2aendS154) {
                      int32_t _M0L6_2atmpS1241 = _M0Lm9_2acursorS155;
                      int32_t _M0L10next__charS203;
                      int32_t _M0L6_2atmpS1240;
                      moonbit_incref(_M0L7_2adataS152);
                      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                      _M0L10next__charS203
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1241);
                      _M0L6_2atmpS1240 = _M0Lm9_2acursorS155;
                      _M0Lm9_2acursorS155 = _M0L6_2atmpS1240 + 1;
                      if (_M0L10next__charS203 < 58) {
                        if (_M0L10next__charS203 < 48) {
                          goto join_202;
                        } else {
                          _M0L12dispatch__15S179 = 2;
                          goto loop__label__15_182;
                        }
                      } else if (_M0L10next__charS203 > 58) {
                        goto join_202;
                      } else {
                        _M0L12dispatch__15S179 = 1;
                        goto loop__label__15_182;
                      }
                      join_202:;
                      _M0L12dispatch__15S179 = 0;
                      goto loop__label__15_182;
                    } else {
                      goto join_171;
                    }
                    break;
                  }
                  default: {
                    goto join_171;
                    break;
                  }
                }
                join_184:;
                _M0Lm6tag__1S164 = _M0Lm9tag__1__2S166;
                _M0Lm6tag__2S168 = _M0Lm9tag__2__1S169;
                _M0Lm20match__tag__saver__0S158 = _M0Lm6tag__0S163;
                _M0Lm20match__tag__saver__1S159 = _M0Lm6tag__1S164;
                _M0Lm20match__tag__saver__2S160 = _M0Lm6tag__2S168;
                _M0Lm20match__tag__saver__3S161 = _M0Lm6tag__3S167;
                _M0Lm20match__tag__saver__4S162 = _M0Lm6tag__4S170;
                _M0Lm13accept__stateS156 = 0;
                _M0Lm10match__endS157 = _M0Lm9_2acursorS155;
                goto join_171;
                join_180:;
                _M0Lm9tag__1__1S165 = _M0Lm9tag__1__2S166;
                _M0Lm6tag__1S164 = _M0Lm9_2acursorS155;
                _M0Lm6tag__2S168 = _M0Lm9tag__2__1S169;
                _M0L6_2atmpS1209 = _M0Lm9_2acursorS155;
                if (_M0L6_2atmpS1209 < _M0L6_2aendS154) {
                  int32_t _M0L6_2atmpS1211 = _M0Lm9_2acursorS155;
                  int32_t _M0L10next__charS183;
                  int32_t _M0L6_2atmpS1210;
                  moonbit_incref(_M0L7_2adataS152);
                  #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
                  _M0L10next__charS183
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS152, _M0L6_2atmpS1211);
                  _M0L6_2atmpS1210 = _M0Lm9_2acursorS155;
                  _M0Lm9_2acursorS155 = _M0L6_2atmpS1210 + 1;
                  if (_M0L10next__charS183 < 58) {
                    if (_M0L10next__charS183 < 48) {
                      goto join_181;
                    } else {
                      _M0L12dispatch__15S179 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS183 > 58) {
                    goto join_181;
                  } else {
                    _M0L12dispatch__15S179 = 1;
                    continue;
                  }
                  join_181:;
                  _M0L12dispatch__15S179 = 0;
                  continue;
                } else {
                  goto join_171;
                }
                break;
              }
            } else {
              goto join_171;
            }
          } else {
            continue;
          }
        } else {
          goto join_171;
        }
        break;
      }
    } else {
      goto join_171;
    }
  } else {
    goto join_171;
  }
  join_171:;
  switch (_M0Lm13accept__stateS156) {
    case 0: {
      int32_t _M0L6_2atmpS1200 = _M0Lm20match__tag__saver__1S159;
      int32_t _M0L6_2atmpS1199 = _M0L6_2atmpS1200 + 1;
      int64_t _M0L6_2atmpS1196 = (int64_t)_M0L6_2atmpS1199;
      int32_t _M0L6_2atmpS1198 = _M0Lm20match__tag__saver__2S160;
      int64_t _M0L6_2atmpS1197 = (int64_t)_M0L6_2atmpS1198;
      struct _M0TPC16string10StringView _M0L11start__lineS172;
      int32_t _M0L6_2atmpS1195;
      int32_t _M0L6_2atmpS1194;
      int64_t _M0L6_2atmpS1191;
      int32_t _M0L6_2atmpS1193;
      int64_t _M0L6_2atmpS1192;
      struct _M0TPC16string10StringView _M0L13start__columnS173;
      int32_t _M0L6_2atmpS1190;
      int64_t _M0L6_2atmpS1187;
      int32_t _M0L6_2atmpS1189;
      int64_t _M0L6_2atmpS1188;
      struct _M0TPC16string10StringView _M0L3pkgS174;
      int32_t _M0L6_2atmpS1186;
      int32_t _M0L6_2atmpS1185;
      int64_t _M0L6_2atmpS1182;
      int32_t _M0L6_2atmpS1184;
      int64_t _M0L6_2atmpS1183;
      struct _M0TPC16string10StringView _M0L8filenameS175;
      int32_t _M0L6_2atmpS1181;
      int32_t _M0L6_2atmpS1180;
      int64_t _M0L6_2atmpS1177;
      int32_t _M0L6_2atmpS1179;
      int64_t _M0L6_2atmpS1178;
      struct _M0TPC16string10StringView _M0L9end__lineS176;
      int32_t _M0L6_2atmpS1176;
      int32_t _M0L6_2atmpS1175;
      int64_t _M0L6_2atmpS1172;
      int32_t _M0L6_2atmpS1174;
      int64_t _M0L6_2atmpS1173;
      struct _M0TPC16string10StringView _M0L11end__columnS177;
      struct _M0TPB13SourceLocRepr* _block_2469;
      moonbit_incref(_M0L7_2adataS152);
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11start__lineS172
      = _M0MPC16string6String4view(_M0L7_2adataS152, _M0L6_2atmpS1196, _M0L6_2atmpS1197);
      _M0L6_2atmpS1195 = _M0Lm20match__tag__saver__2S160;
      _M0L6_2atmpS1194 = _M0L6_2atmpS1195 + 1;
      _M0L6_2atmpS1191 = (int64_t)_M0L6_2atmpS1194;
      _M0L6_2atmpS1193 = _M0Lm20match__tag__saver__3S161;
      _M0L6_2atmpS1192 = (int64_t)_M0L6_2atmpS1193;
      moonbit_incref(_M0L7_2adataS152);
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L13start__columnS173
      = _M0MPC16string6String4view(_M0L7_2adataS152, _M0L6_2atmpS1191, _M0L6_2atmpS1192);
      _M0L6_2atmpS1190 = _M0L8_2astartS153 + 1;
      _M0L6_2atmpS1187 = (int64_t)_M0L6_2atmpS1190;
      _M0L6_2atmpS1189 = _M0Lm20match__tag__saver__0S158;
      _M0L6_2atmpS1188 = (int64_t)_M0L6_2atmpS1189;
      moonbit_incref(_M0L7_2adataS152);
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L3pkgS174
      = _M0MPC16string6String4view(_M0L7_2adataS152, _M0L6_2atmpS1187, _M0L6_2atmpS1188);
      _M0L6_2atmpS1186 = _M0Lm20match__tag__saver__0S158;
      _M0L6_2atmpS1185 = _M0L6_2atmpS1186 + 1;
      _M0L6_2atmpS1182 = (int64_t)_M0L6_2atmpS1185;
      _M0L6_2atmpS1184 = _M0Lm20match__tag__saver__1S159;
      _M0L6_2atmpS1183 = (int64_t)_M0L6_2atmpS1184;
      moonbit_incref(_M0L7_2adataS152);
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L8filenameS175
      = _M0MPC16string6String4view(_M0L7_2adataS152, _M0L6_2atmpS1182, _M0L6_2atmpS1183);
      _M0L6_2atmpS1181 = _M0Lm20match__tag__saver__3S161;
      _M0L6_2atmpS1180 = _M0L6_2atmpS1181 + 1;
      _M0L6_2atmpS1177 = (int64_t)_M0L6_2atmpS1180;
      _M0L6_2atmpS1179 = _M0Lm20match__tag__saver__4S162;
      _M0L6_2atmpS1178 = (int64_t)_M0L6_2atmpS1179;
      moonbit_incref(_M0L7_2adataS152);
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L9end__lineS176
      = _M0MPC16string6String4view(_M0L7_2adataS152, _M0L6_2atmpS1177, _M0L6_2atmpS1178);
      _M0L6_2atmpS1176 = _M0Lm20match__tag__saver__4S162;
      _M0L6_2atmpS1175 = _M0L6_2atmpS1176 + 1;
      _M0L6_2atmpS1172 = (int64_t)_M0L6_2atmpS1175;
      _M0L6_2atmpS1174 = _M0Lm10match__endS157;
      _M0L6_2atmpS1173 = (int64_t)_M0L6_2atmpS1174;
      #line 83 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      _M0L11end__columnS177
      = _M0MPC16string6String4view(_M0L7_2adataS152, _M0L6_2atmpS1172, _M0L6_2atmpS1173);
      _block_2469
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_2469)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_2469->$0_0 = _M0L3pkgS174.$0;
      _block_2469->$0_1 = _M0L3pkgS174.$1;
      _block_2469->$0_2 = _M0L3pkgS174.$2;
      _block_2469->$1_0 = _M0L8filenameS175.$0;
      _block_2469->$1_1 = _M0L8filenameS175.$1;
      _block_2469->$1_2 = _M0L8filenameS175.$2;
      _block_2469->$2_0 = _M0L11start__lineS172.$0;
      _block_2469->$2_1 = _M0L11start__lineS172.$1;
      _block_2469->$2_2 = _M0L11start__lineS172.$2;
      _block_2469->$3_0 = _M0L13start__columnS173.$0;
      _block_2469->$3_1 = _M0L13start__columnS173.$1;
      _block_2469->$3_2 = _M0L13start__columnS173.$2;
      _block_2469->$4_0 = _M0L9end__lineS176.$0;
      _block_2469->$4_1 = _M0L9end__lineS176.$1;
      _block_2469->$4_2 = _M0L9end__lineS176.$2;
      _block_2469->$5_0 = _M0L11end__columnS177.$0;
      _block_2469->$5_1 = _M0L11end__columnS177.$1;
      _block_2469->$5_2 = _M0L11end__columnS177.$2;
      return _block_2469;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS152);
      #line 99 "/home/blem/.moon/lib/core/builtin/autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS148,
  int32_t _M0L5indexS149
) {
  int32_t _M0L3lenS147;
  int32_t _if__result_2470;
  #line 183 "/home/blem/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS147 = _M0L4selfS148->$1;
  if (_M0L5indexS149 >= 0) {
    _if__result_2470 = _M0L5indexS149 < _M0L3lenS147;
  } else {
    _if__result_2470 = 0;
  }
  if (_if__result_2470) {
    moonbit_string_t* _M0L6_2atmpS1171;
    moonbit_string_t _M0L6_2atmpS2252;
    #line 188 "/home/blem/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS1171 = _M0MPC15array5Array6bufferGsE(_M0L4selfS148);
    if (
      _M0L5indexS149 < 0
      || _M0L5indexS149 >= Moonbit_array_length(_M0L6_2atmpS1171)
    ) {
      #line 188 "/home/blem/.moon/lib/core/builtin/array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2252 = (moonbit_string_t)_M0L6_2atmpS1171[_M0L5indexS149];
    moonbit_incref(_M0L6_2atmpS2252);
    moonbit_decref(_M0L6_2atmpS1171);
    return _M0L6_2atmpS2252;
  } else {
    moonbit_decref(_M0L4selfS148);
    #line 187 "/home/blem/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS145
) {
  moonbit_string_t* _M0L8_2afieldS2253;
  int32_t _M0L6_2acntS2353;
  #line 124 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS2253 = _M0L4selfS145->$0;
  _M0L6_2acntS2353 = Moonbit_object_header(_M0L4selfS145)->rc;
  if (_M0L6_2acntS2353 > 1) {
    int32_t _M0L11_2anew__cntS2354 = _M0L6_2acntS2353 - 1;
    Moonbit_object_header(_M0L4selfS145)->rc = _M0L11_2anew__cntS2354;
    moonbit_incref(_M0L8_2afieldS2253);
  } else if (_M0L6_2acntS2353 == 1) {
    #line 125 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS145);
  }
  return _M0L8_2afieldS2253;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS146
) {
  struct _M0TUsiE** _M0L8_2afieldS2254;
  int32_t _M0L6_2acntS2355;
  #line 124 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS2254 = _M0L4selfS146->$0;
  _M0L6_2acntS2355 = Moonbit_object_header(_M0L4selfS146)->rc;
  if (_M0L6_2acntS2355 > 1) {
    int32_t _M0L11_2anew__cntS2356 = _M0L6_2acntS2355 - 1;
    Moonbit_object_header(_M0L4selfS146)->rc = _M0L11_2anew__cntS2356;
    moonbit_incref(_M0L8_2afieldS2254);
  } else if (_M0L6_2acntS2355 == 1) {
    #line 125 "/home/blem/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS146);
  }
  return _M0L8_2afieldS2254;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS144) {
  struct _M0TPB13StringBuilder* _M0L3bufS143;
  struct _M0TPB6Logger _M0L6_2atmpS1170;
  #line 184 "/home/blem/.moon/lib/core/builtin/show.mbt"
  #line 185 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0L3bufS143 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS143);
  _M0L6_2atmpS1170
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS143
  };
  #line 186 "/home/blem/.moon/lib/core/builtin/show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS144, _M0L6_2atmpS1170);
  #line 187 "/home/blem/.moon/lib/core/builtin/show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS143);
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS137
) {
  int32_t _M0L17codepoint__lengthS136;
  int32_t _M0L6_2atmpS1169;
  moonbit_bytes_t _M0L4dataS138;
  int32_t _M0L1iS139;
  int32_t _M0L12utf16__indexS140;
  #line 102 "/home/blem/.moon/lib/core/builtin/console.mbt"
  moonbit_incref(_M0L1sS137);
  #line 104 "/home/blem/.moon/lib/core/builtin/console.mbt"
  _M0L17codepoint__lengthS136
  = _M0MPC16string6String20char__length_2einner(_M0L1sS137, 0, 4294967296ll);
  _M0L6_2atmpS1169 = _M0L17codepoint__lengthS136 * 4;
  _M0L4dataS138 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1169, 0);
  _M0L1iS139 = 0;
  _M0L12utf16__indexS140 = 0;
  while (1) {
    if (_M0L1iS139 < _M0L17codepoint__lengthS136) {
      int32_t _M0L6_2atmpS1166;
      int32_t _M0L1cS141;
      int32_t _M0L6_2atmpS1167;
      int32_t _M0L6_2atmpS1168;
      moonbit_incref(_M0L1sS137);
      #line 109 "/home/blem/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1166
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS137, _M0L12utf16__indexS140);
      _M0L1cS141 = _M0L6_2atmpS1166;
      if (_M0L1cS141 > 65535) {
        int32_t _M0L6_2atmpS1134 = _M0L1iS139 * 4;
        int32_t _M0L6_2atmpS1136 = _M0L1cS141 & 255;
        int32_t _M0L6_2atmpS1135 = _M0L6_2atmpS1136 & 0xff;
        int32_t _M0L6_2atmpS1141;
        int32_t _M0L6_2atmpS1137;
        int32_t _M0L6_2atmpS1140;
        int32_t _M0L6_2atmpS1139;
        int32_t _M0L6_2atmpS1138;
        int32_t _M0L6_2atmpS1146;
        int32_t _M0L6_2atmpS1142;
        int32_t _M0L6_2atmpS1145;
        int32_t _M0L6_2atmpS1144;
        int32_t _M0L6_2atmpS1143;
        int32_t _M0L6_2atmpS1151;
        int32_t _M0L6_2atmpS1147;
        int32_t _M0L6_2atmpS1150;
        int32_t _M0L6_2atmpS1149;
        int32_t _M0L6_2atmpS1148;
        int32_t _M0L6_2atmpS1152;
        int32_t _M0L6_2atmpS1153;
        if (
          _M0L6_2atmpS1134 < 0
          || _M0L6_2atmpS1134 >= Moonbit_array_length(_M0L4dataS138)
        ) {
          #line 111 "/home/blem/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS138[_M0L6_2atmpS1134] = _M0L6_2atmpS1135;
        _M0L6_2atmpS1141 = _M0L1iS139 * 4;
        _M0L6_2atmpS1137 = _M0L6_2atmpS1141 + 1;
        _M0L6_2atmpS1140 = _M0L1cS141 >> 8;
        _M0L6_2atmpS1139 = _M0L6_2atmpS1140 & 255;
        _M0L6_2atmpS1138 = _M0L6_2atmpS1139 & 0xff;
        if (
          _M0L6_2atmpS1137 < 0
          || _M0L6_2atmpS1137 >= Moonbit_array_length(_M0L4dataS138)
        ) {
          #line 112 "/home/blem/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS138[_M0L6_2atmpS1137] = _M0L6_2atmpS1138;
        _M0L6_2atmpS1146 = _M0L1iS139 * 4;
        _M0L6_2atmpS1142 = _M0L6_2atmpS1146 + 2;
        _M0L6_2atmpS1145 = _M0L1cS141 >> 16;
        _M0L6_2atmpS1144 = _M0L6_2atmpS1145 & 255;
        _M0L6_2atmpS1143 = _M0L6_2atmpS1144 & 0xff;
        if (
          _M0L6_2atmpS1142 < 0
          || _M0L6_2atmpS1142 >= Moonbit_array_length(_M0L4dataS138)
        ) {
          #line 113 "/home/blem/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS138[_M0L6_2atmpS1142] = _M0L6_2atmpS1143;
        _M0L6_2atmpS1151 = _M0L1iS139 * 4;
        _M0L6_2atmpS1147 = _M0L6_2atmpS1151 + 3;
        _M0L6_2atmpS1150 = _M0L1cS141 >> 24;
        _M0L6_2atmpS1149 = _M0L6_2atmpS1150 & 255;
        _M0L6_2atmpS1148 = _M0L6_2atmpS1149 & 0xff;
        if (
          _M0L6_2atmpS1147 < 0
          || _M0L6_2atmpS1147 >= Moonbit_array_length(_M0L4dataS138)
        ) {
          #line 114 "/home/blem/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS138[_M0L6_2atmpS1147] = _M0L6_2atmpS1148;
        _M0L6_2atmpS1152 = _M0L1iS139 + 1;
        _M0L6_2atmpS1153 = _M0L12utf16__indexS140 + 2;
        _M0L1iS139 = _M0L6_2atmpS1152;
        _M0L12utf16__indexS140 = _M0L6_2atmpS1153;
        continue;
      } else {
        int32_t _M0L6_2atmpS1154 = _M0L1iS139 * 4;
        int32_t _M0L6_2atmpS1156 = _M0L1cS141 & 255;
        int32_t _M0L6_2atmpS1155 = _M0L6_2atmpS1156 & 0xff;
        int32_t _M0L6_2atmpS1161;
        int32_t _M0L6_2atmpS1157;
        int32_t _M0L6_2atmpS1160;
        int32_t _M0L6_2atmpS1159;
        int32_t _M0L6_2atmpS1158;
        int32_t _M0L6_2atmpS1163;
        int32_t _M0L6_2atmpS1162;
        int32_t _M0L6_2atmpS1165;
        int32_t _M0L6_2atmpS1164;
        if (
          _M0L6_2atmpS1154 < 0
          || _M0L6_2atmpS1154 >= Moonbit_array_length(_M0L4dataS138)
        ) {
          #line 117 "/home/blem/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS138[_M0L6_2atmpS1154] = _M0L6_2atmpS1155;
        _M0L6_2atmpS1161 = _M0L1iS139 * 4;
        _M0L6_2atmpS1157 = _M0L6_2atmpS1161 + 1;
        _M0L6_2atmpS1160 = _M0L1cS141 >> 8;
        _M0L6_2atmpS1159 = _M0L6_2atmpS1160 & 255;
        _M0L6_2atmpS1158 = _M0L6_2atmpS1159 & 0xff;
        if (
          _M0L6_2atmpS1157 < 0
          || _M0L6_2atmpS1157 >= Moonbit_array_length(_M0L4dataS138)
        ) {
          #line 118 "/home/blem/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS138[_M0L6_2atmpS1157] = _M0L6_2atmpS1158;
        _M0L6_2atmpS1163 = _M0L1iS139 * 4;
        _M0L6_2atmpS1162 = _M0L6_2atmpS1163 + 2;
        if (
          _M0L6_2atmpS1162 < 0
          || _M0L6_2atmpS1162 >= Moonbit_array_length(_M0L4dataS138)
        ) {
          #line 119 "/home/blem/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS138[_M0L6_2atmpS1162] = 0;
        _M0L6_2atmpS1165 = _M0L1iS139 * 4;
        _M0L6_2atmpS1164 = _M0L6_2atmpS1165 + 3;
        if (
          _M0L6_2atmpS1164 < 0
          || _M0L6_2atmpS1164 >= Moonbit_array_length(_M0L4dataS138)
        ) {
          #line 120 "/home/blem/.moon/lib/core/builtin/console.mbt"
          moonbit_panic();
        }
        _M0L4dataS138[_M0L6_2atmpS1164] = 0;
      }
      _M0L6_2atmpS1167 = _M0L1iS139 + 1;
      _M0L6_2atmpS1168 = _M0L12utf16__indexS140 + 1;
      _M0L1iS139 = _M0L6_2atmpS1167;
      _M0L12utf16__indexS140 = _M0L6_2atmpS1168;
      continue;
    } else {
      moonbit_decref(_M0L1sS137);
    }
    break;
  }
  #line 123 "/home/blem/.moon/lib/core/builtin/console.mbt"
  return _M0FPB14base64__encode(_M0L4dataS138);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS133,
  int32_t _M0L5indexS134
) {
  int32_t _M0L2c1S132;
  #line 90 "/home/blem/.moon/lib/core/builtin/deprecated.mbt"
  _M0L2c1S132 = _M0L4selfS133[_M0L5indexS134];
  #line 93 "/home/blem/.moon/lib/core/builtin/deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S132)) {
    int32_t _M0L6_2atmpS1133 = _M0L5indexS134 + 1;
    int32_t _M0L6_2atmpS2255 = _M0L4selfS133[_M0L6_2atmpS1133];
    int32_t _M0L2c2S135;
    int32_t _M0L6_2atmpS1131;
    int32_t _M0L6_2atmpS1132;
    moonbit_decref(_M0L4selfS133);
    _M0L2c2S135 = _M0L6_2atmpS2255;
    _M0L6_2atmpS1131 = (int32_t)_M0L2c1S132;
    _M0L6_2atmpS1132 = (int32_t)_M0L2c2S135;
    #line 95 "/home/blem/.moon/lib/core/builtin/deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1131, _M0L6_2atmpS1132);
  } else {
    moonbit_decref(_M0L4selfS133);
    #line 97 "/home/blem/.moon/lib/core/builtin/deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S132);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS131) {
  int32_t _M0L6_2atmpS1130;
  #line 68 "/home/blem/.moon/lib/core/builtin/uint16_char.mbt"
  _M0L6_2atmpS1130 = (int32_t)_M0L4selfS131;
  return _M0L6_2atmpS1130;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS129,
  int32_t _M0L8trailingS130
) {
  int32_t _M0L6_2atmpS1129;
  int32_t _M0L6_2atmpS1128;
  int32_t _M0L6_2atmpS1127;
  int32_t _M0L6_2atmpS1126;
  int32_t _M0L6_2atmpS1125;
  #line 40 "/home/blem/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS1129 = _M0L7leadingS129 - 55296;
  _M0L6_2atmpS1128 = _M0L6_2atmpS1129 * 1024;
  _M0L6_2atmpS1127 = _M0L6_2atmpS1128 + _M0L8trailingS130;
  _M0L6_2atmpS1126 = _M0L6_2atmpS1127 - 56320;
  _M0L6_2atmpS1125 = _M0L6_2atmpS1126 + 65536;
  return _M0L6_2atmpS1125;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS122,
  int32_t _M0L13start__offsetS123,
  int64_t _M0L11end__offsetS120
) {
  int32_t _M0L11end__offsetS119;
  int32_t _if__result_2472;
  #line 60 "/home/blem/.moon/lib/core/builtin/string.mbt"
  if (_M0L11end__offsetS120 == 4294967296ll) {
    _M0L11end__offsetS119 = Moonbit_array_length(_M0L4selfS122);
  } else {
    int64_t _M0L7_2aSomeS121 = _M0L11end__offsetS120;
    _M0L11end__offsetS119 = (int32_t)_M0L7_2aSomeS121;
  }
  if (_M0L13start__offsetS123 >= 0) {
    if (_M0L13start__offsetS123 <= _M0L11end__offsetS119) {
      int32_t _M0L6_2atmpS1118 = Moonbit_array_length(_M0L4selfS122);
      _if__result_2472 = _M0L11end__offsetS119 <= _M0L6_2atmpS1118;
    } else {
      _if__result_2472 = 0;
    }
  } else {
    _if__result_2472 = 0;
  }
  if (_if__result_2472) {
    int32_t _M0L12utf16__indexS124 = _M0L13start__offsetS123;
    int32_t _M0L11char__countS125 = 0;
    while (1) {
      if (_M0L12utf16__indexS124 < _M0L11end__offsetS119) {
        int32_t _M0L2c1S126 = _M0L4selfS122[_M0L12utf16__indexS124];
        int32_t _if__result_2474;
        int32_t _M0L6_2atmpS1123;
        int32_t _M0L6_2atmpS1124;
        #line 76 "/home/blem/.moon/lib/core/builtin/string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S126)) {
          int32_t _M0L6_2atmpS1119 = _M0L12utf16__indexS124 + 1;
          _if__result_2474 = _M0L6_2atmpS1119 < _M0L11end__offsetS119;
        } else {
          _if__result_2474 = 0;
        }
        if (_if__result_2474) {
          int32_t _M0L6_2atmpS1122 = _M0L12utf16__indexS124 + 1;
          int32_t _M0L2c2S127 = _M0L4selfS122[_M0L6_2atmpS1122];
          #line 78 "/home/blem/.moon/lib/core/builtin/string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S127)) {
            int32_t _M0L6_2atmpS1120 = _M0L12utf16__indexS124 + 2;
            int32_t _M0L6_2atmpS1121 = _M0L11char__countS125 + 1;
            _M0L12utf16__indexS124 = _M0L6_2atmpS1120;
            _M0L11char__countS125 = _M0L6_2atmpS1121;
            continue;
          } else {
            #line 81 "/home/blem/.moon/lib/core/builtin/string.mbt"
            _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_77.data, (moonbit_string_t)moonbit_string_literal_105.data);
          }
        }
        _M0L6_2atmpS1123 = _M0L12utf16__indexS124 + 1;
        _M0L6_2atmpS1124 = _M0L11char__countS125 + 1;
        _M0L12utf16__indexS124 = _M0L6_2atmpS1123;
        _M0L11char__countS125 = _M0L6_2atmpS1124;
        continue;
      } else {
        moonbit_decref(_M0L4selfS122);
        return _M0L11char__countS125;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L4selfS122);
    #line 70 "/home/blem/.moon/lib/core/builtin/string.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_106.data, (moonbit_string_t)moonbit_string_literal_107.data);
  }
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS118) {
  #line 45 "/home/blem/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS118 >= 56320) {
    return _M0L4selfS118 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS117) {
  #line 28 "/home/blem/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS117 >= 55296) {
    return _M0L4selfS117 <= 56319;
  } else {
    return 0;
  }
}

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t _M0L4dataS98) {
  struct _M0TPB13StringBuilder* _M0L3bufS96;
  int32_t _M0L3lenS97;
  int32_t _M0L3remS99;
  int32_t _M0L1iS100;
  #line 61 "/home/blem/.moon/lib/core/builtin/console.mbt"
  #line 63 "/home/blem/.moon/lib/core/builtin/console.mbt"
  _M0L3bufS96 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS97 = Moonbit_array_length(_M0L4dataS98);
  _M0L3remS99 = _M0L3lenS97 % 3;
  _M0L1iS100 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1070 = _M0L3lenS97 - _M0L3remS99;
    if (_M0L1iS100 < _M0L6_2atmpS1070) {
      int32_t _M0L6_2atmpS1092;
      int32_t _M0L2b0S101;
      int32_t _M0L6_2atmpS1091;
      int32_t _M0L6_2atmpS1090;
      int32_t _M0L2b1S102;
      int32_t _M0L6_2atmpS1089;
      int32_t _M0L6_2atmpS1088;
      int32_t _M0L2b2S103;
      int32_t _M0L6_2atmpS1087;
      int32_t _M0L6_2atmpS1086;
      int32_t _M0L2x0S104;
      int32_t _M0L6_2atmpS1085;
      int32_t _M0L6_2atmpS1082;
      int32_t _M0L6_2atmpS1084;
      int32_t _M0L6_2atmpS1083;
      int32_t _M0L6_2atmpS1081;
      int32_t _M0L2x1S105;
      int32_t _M0L6_2atmpS1080;
      int32_t _M0L6_2atmpS1077;
      int32_t _M0L6_2atmpS1079;
      int32_t _M0L6_2atmpS1078;
      int32_t _M0L6_2atmpS1076;
      int32_t _M0L2x2S106;
      int32_t _M0L6_2atmpS1075;
      int32_t _M0L2x3S107;
      int32_t _M0L6_2atmpS1071;
      int32_t _M0L6_2atmpS1072;
      int32_t _M0L6_2atmpS1073;
      int32_t _M0L6_2atmpS1074;
      int32_t _M0L6_2atmpS1093;
      if (_M0L1iS100 < 0 || _M0L1iS100 >= Moonbit_array_length(_M0L4dataS98)) {
        #line 67 "/home/blem/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1092 = (int32_t)_M0L4dataS98[_M0L1iS100];
      _M0L2b0S101 = (int32_t)_M0L6_2atmpS1092;
      _M0L6_2atmpS1091 = _M0L1iS100 + 1;
      if (
        _M0L6_2atmpS1091 < 0
        || _M0L6_2atmpS1091 >= Moonbit_array_length(_M0L4dataS98)
      ) {
        #line 68 "/home/blem/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1090 = (int32_t)_M0L4dataS98[_M0L6_2atmpS1091];
      _M0L2b1S102 = (int32_t)_M0L6_2atmpS1090;
      _M0L6_2atmpS1089 = _M0L1iS100 + 2;
      if (
        _M0L6_2atmpS1089 < 0
        || _M0L6_2atmpS1089 >= Moonbit_array_length(_M0L4dataS98)
      ) {
        #line 69 "/home/blem/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1088 = (int32_t)_M0L4dataS98[_M0L6_2atmpS1089];
      _M0L2b2S103 = (int32_t)_M0L6_2atmpS1088;
      _M0L6_2atmpS1087 = _M0L2b0S101 & 252;
      _M0L6_2atmpS1086 = _M0L6_2atmpS1087 >> 2;
      if (
        _M0L6_2atmpS1086 < 0
        || _M0L6_2atmpS1086
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 70 "/home/blem/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x0S104 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1086];
      _M0L6_2atmpS1085 = _M0L2b0S101 & 3;
      _M0L6_2atmpS1082 = _M0L6_2atmpS1085 << 4;
      _M0L6_2atmpS1084 = _M0L2b1S102 & 240;
      _M0L6_2atmpS1083 = _M0L6_2atmpS1084 >> 4;
      _M0L6_2atmpS1081 = _M0L6_2atmpS1082 | _M0L6_2atmpS1083;
      if (
        _M0L6_2atmpS1081 < 0
        || _M0L6_2atmpS1081
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 71 "/home/blem/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x1S105 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1081];
      _M0L6_2atmpS1080 = _M0L2b1S102 & 15;
      _M0L6_2atmpS1077 = _M0L6_2atmpS1080 << 2;
      _M0L6_2atmpS1079 = _M0L2b2S103 & 192;
      _M0L6_2atmpS1078 = _M0L6_2atmpS1079 >> 6;
      _M0L6_2atmpS1076 = _M0L6_2atmpS1077 | _M0L6_2atmpS1078;
      if (
        _M0L6_2atmpS1076 < 0
        || _M0L6_2atmpS1076
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 72 "/home/blem/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x2S106 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1076];
      _M0L6_2atmpS1075 = _M0L2b2S103 & 63;
      if (
        _M0L6_2atmpS1075 < 0
        || _M0L6_2atmpS1075
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 73 "/home/blem/.moon/lib/core/builtin/console.mbt"
        moonbit_panic();
      }
      _M0L2x3S107 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1075];
      #line 74 "/home/blem/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1071 = _M0MPC14byte4Byte8to__char(_M0L2x0S104);
      moonbit_incref(_M0L3bufS96);
      #line 74 "/home/blem/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, _M0L6_2atmpS1071);
      #line 75 "/home/blem/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1072 = _M0MPC14byte4Byte8to__char(_M0L2x1S105);
      moonbit_incref(_M0L3bufS96);
      #line 75 "/home/blem/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, _M0L6_2atmpS1072);
      #line 76 "/home/blem/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1073 = _M0MPC14byte4Byte8to__char(_M0L2x2S106);
      moonbit_incref(_M0L3bufS96);
      #line 76 "/home/blem/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, _M0L6_2atmpS1073);
      #line 77 "/home/blem/.moon/lib/core/builtin/console.mbt"
      _M0L6_2atmpS1074 = _M0MPC14byte4Byte8to__char(_M0L2x3S107);
      moonbit_incref(_M0L3bufS96);
      #line 77 "/home/blem/.moon/lib/core/builtin/console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, _M0L6_2atmpS1074);
      _M0L6_2atmpS1093 = _M0L1iS100 + 3;
      _M0L1iS100 = _M0L6_2atmpS1093;
      continue;
    }
    break;
  }
  if (_M0L3remS99 == 1) {
    int32_t _M0L6_2atmpS1101 = _M0L3lenS97 - 1;
    int32_t _M0L6_2atmpS2256;
    int32_t _M0L6_2atmpS1100;
    int32_t _M0L2b0S109;
    int32_t _M0L6_2atmpS1099;
    int32_t _M0L6_2atmpS1098;
    int32_t _M0L2x0S110;
    int32_t _M0L6_2atmpS1097;
    int32_t _M0L6_2atmpS1096;
    int32_t _M0L2x1S111;
    int32_t _M0L6_2atmpS1094;
    int32_t _M0L6_2atmpS1095;
    if (
      _M0L6_2atmpS1101 < 0
      || _M0L6_2atmpS1101 >= Moonbit_array_length(_M0L4dataS98)
    ) {
      #line 80 "/home/blem/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2256 = (int32_t)_M0L4dataS98[_M0L6_2atmpS1101];
    moonbit_decref(_M0L4dataS98);
    _M0L6_2atmpS1100 = _M0L6_2atmpS2256;
    _M0L2b0S109 = (int32_t)_M0L6_2atmpS1100;
    _M0L6_2atmpS1099 = _M0L2b0S109 & 252;
    _M0L6_2atmpS1098 = _M0L6_2atmpS1099 >> 2;
    if (
      _M0L6_2atmpS1098 < 0
      || _M0L6_2atmpS1098
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 81 "/home/blem/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S110 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1098];
    _M0L6_2atmpS1097 = _M0L2b0S109 & 3;
    _M0L6_2atmpS1096 = _M0L6_2atmpS1097 << 4;
    if (
      _M0L6_2atmpS1096 < 0
      || _M0L6_2atmpS1096
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 82 "/home/blem/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S111 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1096];
    #line 83 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1094 = _M0MPC14byte4Byte8to__char(_M0L2x0S110);
    moonbit_incref(_M0L3bufS96);
    #line 83 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, _M0L6_2atmpS1094);
    #line 84 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1095 = _M0MPC14byte4Byte8to__char(_M0L2x1S111);
    moonbit_incref(_M0L3bufS96);
    #line 84 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, _M0L6_2atmpS1095);
    moonbit_incref(_M0L3bufS96);
    #line 85 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, 61);
    moonbit_incref(_M0L3bufS96);
    #line 86 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, 61);
  } else if (_M0L3remS99 == 2) {
    int32_t _M0L6_2atmpS1117 = _M0L3lenS97 - 2;
    int32_t _M0L6_2atmpS1116;
    int32_t _M0L2b0S112;
    int32_t _M0L6_2atmpS1115;
    int32_t _M0L6_2atmpS2257;
    int32_t _M0L6_2atmpS1114;
    int32_t _M0L2b1S113;
    int32_t _M0L6_2atmpS1113;
    int32_t _M0L6_2atmpS1112;
    int32_t _M0L2x0S114;
    int32_t _M0L6_2atmpS1111;
    int32_t _M0L6_2atmpS1108;
    int32_t _M0L6_2atmpS1110;
    int32_t _M0L6_2atmpS1109;
    int32_t _M0L6_2atmpS1107;
    int32_t _M0L2x1S115;
    int32_t _M0L6_2atmpS1106;
    int32_t _M0L6_2atmpS1105;
    int32_t _M0L2x2S116;
    int32_t _M0L6_2atmpS1102;
    int32_t _M0L6_2atmpS1103;
    int32_t _M0L6_2atmpS1104;
    if (
      _M0L6_2atmpS1117 < 0
      || _M0L6_2atmpS1117 >= Moonbit_array_length(_M0L4dataS98)
    ) {
      #line 88 "/home/blem/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1116 = (int32_t)_M0L4dataS98[_M0L6_2atmpS1117];
    _M0L2b0S112 = (int32_t)_M0L6_2atmpS1116;
    _M0L6_2atmpS1115 = _M0L3lenS97 - 1;
    if (
      _M0L6_2atmpS1115 < 0
      || _M0L6_2atmpS1115 >= Moonbit_array_length(_M0L4dataS98)
    ) {
      #line 89 "/home/blem/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2257 = (int32_t)_M0L4dataS98[_M0L6_2atmpS1115];
    moonbit_decref(_M0L4dataS98);
    _M0L6_2atmpS1114 = _M0L6_2atmpS2257;
    _M0L2b1S113 = (int32_t)_M0L6_2atmpS1114;
    _M0L6_2atmpS1113 = _M0L2b0S112 & 252;
    _M0L6_2atmpS1112 = _M0L6_2atmpS1113 >> 2;
    if (
      _M0L6_2atmpS1112 < 0
      || _M0L6_2atmpS1112
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 90 "/home/blem/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x0S114 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1112];
    _M0L6_2atmpS1111 = _M0L2b0S112 & 3;
    _M0L6_2atmpS1108 = _M0L6_2atmpS1111 << 4;
    _M0L6_2atmpS1110 = _M0L2b1S113 & 240;
    _M0L6_2atmpS1109 = _M0L6_2atmpS1110 >> 4;
    _M0L6_2atmpS1107 = _M0L6_2atmpS1108 | _M0L6_2atmpS1109;
    if (
      _M0L6_2atmpS1107 < 0
      || _M0L6_2atmpS1107
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 91 "/home/blem/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x1S115 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1107];
    _M0L6_2atmpS1106 = _M0L2b1S113 & 15;
    _M0L6_2atmpS1105 = _M0L6_2atmpS1106 << 2;
    if (
      _M0L6_2atmpS1105 < 0
      || _M0L6_2atmpS1105
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 92 "/home/blem/.moon/lib/core/builtin/console.mbt"
      moonbit_panic();
    }
    _M0L2x2S116 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1105];
    #line 93 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1102 = _M0MPC14byte4Byte8to__char(_M0L2x0S114);
    moonbit_incref(_M0L3bufS96);
    #line 93 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, _M0L6_2atmpS1102);
    #line 94 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1103 = _M0MPC14byte4Byte8to__char(_M0L2x1S115);
    moonbit_incref(_M0L3bufS96);
    #line 94 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, _M0L6_2atmpS1103);
    #line 95 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0L6_2atmpS1104 = _M0MPC14byte4Byte8to__char(_M0L2x2S116);
    moonbit_incref(_M0L3bufS96);
    #line 95 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, _M0L6_2atmpS1104);
    moonbit_incref(_M0L3bufS96);
    #line 96 "/home/blem/.moon/lib/core/builtin/console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS96, 61);
  } else {
    moonbit_decref(_M0L4dataS98);
  }
  #line 98 "/home/blem/.moon/lib/core/builtin/console.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS96);
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS93,
  int32_t _M0L2chS95
) {
  int32_t _M0L3lenS1065;
  int32_t _M0L6_2atmpS1064;
  moonbit_bytes_t _M0L8_2afieldS2258;
  moonbit_bytes_t _M0L4dataS1068;
  int32_t _M0L3lenS1069;
  int32_t _M0L3incS94;
  int32_t _M0L3lenS1067;
  int32_t _M0L6_2atmpS1066;
  #line 74 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS1065 = _M0L4selfS93->$1;
  _M0L6_2atmpS1064 = _M0L3lenS1065 + 4;
  moonbit_incref(_M0L4selfS93);
  #line 75 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS93, _M0L6_2atmpS1064);
  _M0L8_2afieldS2258 = _M0L4selfS93->$0;
  _M0L4dataS1068 = _M0L8_2afieldS2258;
  _M0L3lenS1069 = _M0L4selfS93->$1;
  moonbit_incref(_M0L4dataS1068);
  #line 76 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3incS94
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1068, _M0L3lenS1069, _M0L2chS95);
  _M0L3lenS1067 = _M0L4selfS93->$1;
  _M0L6_2atmpS1066 = _M0L3lenS1067 + _M0L3incS94;
  _M0L4selfS93->$1 = _M0L6_2atmpS1066;
  moonbit_decref(_M0L4selfS93);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS88,
  int32_t _M0L8requiredS89
) {
  moonbit_bytes_t _M0L8_2afieldS2262;
  moonbit_bytes_t _M0L4dataS1063;
  int32_t _M0L6_2atmpS2261;
  int32_t _M0L12current__lenS87;
  int32_t _M0Lm13enough__spaceS90;
  int32_t _M0L6_2atmpS1061;
  int32_t _M0L6_2atmpS1062;
  moonbit_bytes_t _M0L9new__dataS92;
  moonbit_bytes_t _M0L8_2afieldS2260;
  moonbit_bytes_t _M0L4dataS1059;
  int32_t _M0L3lenS1060;
  moonbit_bytes_t _M0L6_2aoldS2259;
  #line 45 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8_2afieldS2262 = _M0L4selfS88->$0;
  _M0L4dataS1063 = _M0L8_2afieldS2262;
  _M0L6_2atmpS2261 = Moonbit_array_length(_M0L4dataS1063);
  _M0L12current__lenS87 = _M0L6_2atmpS2261;
  if (_M0L8requiredS89 <= _M0L12current__lenS87) {
    moonbit_decref(_M0L4selfS88);
    return 0;
  }
  _M0Lm13enough__spaceS90 = _M0L12current__lenS87;
  while (1) {
    int32_t _M0L6_2atmpS1057 = _M0Lm13enough__spaceS90;
    if (_M0L6_2atmpS1057 < _M0L8requiredS89) {
      int32_t _M0L6_2atmpS1058 = _M0Lm13enough__spaceS90;
      _M0Lm13enough__spaceS90 = _M0L6_2atmpS1058 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1061 = _M0Lm13enough__spaceS90;
  #line 59 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS1062 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS92
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1061, _M0L6_2atmpS1062);
  _M0L8_2afieldS2260 = _M0L4selfS88->$0;
  _M0L4dataS1059 = _M0L8_2afieldS2260;
  _M0L3lenS1060 = _M0L4selfS88->$1;
  moonbit_incref(_M0L4dataS1059);
  moonbit_incref(_M0L9new__dataS92);
  #line 60 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS92, 0, _M0L4dataS1059, 0, _M0L3lenS1060);
  _M0L6_2aoldS2259 = _M0L4selfS88->$0;
  moonbit_decref(_M0L6_2aoldS2259);
  _M0L4selfS88->$0 = _M0L9new__dataS92;
  moonbit_decref(_M0L4selfS88);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/blem/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS82,
  int32_t _M0L6offsetS83,
  int32_t _M0L5valueS81
) {
  uint32_t _M0L4codeS80;
  #line 278 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
  #line 283 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
  _M0L4codeS80 = _M0MPC14char4Char8to__uint(_M0L5valueS81);
  if (_M0L4codeS80 < 65536u) {
    uint32_t _M0L6_2atmpS1040 = _M0L4codeS80 & 255u;
    int32_t _M0L6_2atmpS1039;
    int32_t _M0L6_2atmpS1041;
    uint32_t _M0L6_2atmpS1043;
    int32_t _M0L6_2atmpS1042;
    #line 285 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS1039 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1040);
    if (
      _M0L6offsetS83 < 0
      || _M0L6offsetS83 >= Moonbit_array_length(_M0L4selfS82)
    ) {
      #line 285 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS82[_M0L6offsetS83] = _M0L6_2atmpS1039;
    _M0L6_2atmpS1041 = _M0L6offsetS83 + 1;
    _M0L6_2atmpS1043 = _M0L4codeS80 >> 8;
    #line 286 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS1042 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1043);
    if (
      _M0L6_2atmpS1041 < 0
      || _M0L6_2atmpS1041 >= Moonbit_array_length(_M0L4selfS82)
    ) {
      #line 286 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS82[_M0L6_2atmpS1041] = _M0L6_2atmpS1042;
    moonbit_decref(_M0L4selfS82);
    return 2;
  } else if (_M0L4codeS80 < 1114112u) {
    uint32_t _M0L2hiS84 = _M0L4codeS80 - 65536u;
    uint32_t _M0L6_2atmpS1056 = _M0L2hiS84 >> 10;
    uint32_t _M0L2loS85 = _M0L6_2atmpS1056 | 55296u;
    uint32_t _M0L6_2atmpS1055 = _M0L2hiS84 & 1023u;
    uint32_t _M0L2hiS86 = _M0L6_2atmpS1055 | 56320u;
    uint32_t _M0L6_2atmpS1045 = _M0L2loS85 & 255u;
    int32_t _M0L6_2atmpS1044;
    int32_t _M0L6_2atmpS1046;
    uint32_t _M0L6_2atmpS1048;
    int32_t _M0L6_2atmpS1047;
    int32_t _M0L6_2atmpS1049;
    uint32_t _M0L6_2atmpS1051;
    int32_t _M0L6_2atmpS1050;
    int32_t _M0L6_2atmpS1052;
    uint32_t _M0L6_2atmpS1054;
    int32_t _M0L6_2atmpS1053;
    #line 292 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS1044 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1045);
    if (
      _M0L6offsetS83 < 0
      || _M0L6offsetS83 >= Moonbit_array_length(_M0L4selfS82)
    ) {
      #line 292 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS82[_M0L6offsetS83] = _M0L6_2atmpS1044;
    _M0L6_2atmpS1046 = _M0L6offsetS83 + 1;
    _M0L6_2atmpS1048 = _M0L2loS85 >> 8;
    #line 293 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS1047 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1048);
    if (
      _M0L6_2atmpS1046 < 0
      || _M0L6_2atmpS1046 >= Moonbit_array_length(_M0L4selfS82)
    ) {
      #line 293 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS82[_M0L6_2atmpS1046] = _M0L6_2atmpS1047;
    _M0L6_2atmpS1049 = _M0L6offsetS83 + 2;
    _M0L6_2atmpS1051 = _M0L2hiS86 & 255u;
    #line 294 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS1050 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1051);
    if (
      _M0L6_2atmpS1049 < 0
      || _M0L6_2atmpS1049 >= Moonbit_array_length(_M0L4selfS82)
    ) {
      #line 294 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS82[_M0L6_2atmpS1049] = _M0L6_2atmpS1050;
    _M0L6_2atmpS1052 = _M0L6offsetS83 + 3;
    _M0L6_2atmpS1054 = _M0L2hiS86 >> 8;
    #line 295 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    _M0L6_2atmpS1053 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1054);
    if (
      _M0L6_2atmpS1052 < 0
      || _M0L6_2atmpS1052 >= Moonbit_array_length(_M0L4selfS82)
    ) {
      #line 295 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS82[_M0L6_2atmpS1052] = _M0L6_2atmpS1053;
    moonbit_decref(_M0L4selfS82);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS82);
    #line 298 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_108.data, (moonbit_string_t)moonbit_string_literal_109.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS79) {
  int32_t _M0L6_2atmpS1038;
  #line 2554 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1038 = *(int32_t*)&_M0L4selfS79;
  return _M0L6_2atmpS1038 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS78) {
  int32_t _M0L6_2atmpS1037;
  #line 1270 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1037 = _M0L4selfS78;
  return *(uint32_t*)&_M0L6_2atmpS1037;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS77
) {
  moonbit_bytes_t _M0L8_2afieldS2264;
  moonbit_bytes_t _M0L4dataS1036;
  moonbit_bytes_t _M0L6_2atmpS1033;
  int32_t _M0L8_2afieldS2263;
  int32_t _M0L3lenS1035;
  int64_t _M0L6_2atmpS1034;
  #line 115 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8_2afieldS2264 = _M0L4selfS77->$0;
  _M0L4dataS1036 = _M0L8_2afieldS2264;
  moonbit_incref(_M0L4dataS1036);
  _M0L6_2atmpS1033 = _M0L4dataS1036;
  _M0L8_2afieldS2263 = _M0L4selfS77->$1;
  moonbit_decref(_M0L4selfS77);
  _M0L3lenS1035 = _M0L8_2afieldS2263;
  _M0L6_2atmpS1034 = (int64_t)_M0L3lenS1035;
  #line 116 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1033, 0, _M0L6_2atmpS1034);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS72,
  int32_t _M0L6offsetS76,
  int64_t _M0L6lengthS74
) {
  int32_t _M0L3lenS71;
  int32_t _M0L6lengthS73;
  int32_t _if__result_2477;
  #line 76 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
  _M0L3lenS71 = Moonbit_array_length(_M0L4selfS72);
  if (_M0L6lengthS74 == 4294967296ll) {
    _M0L6lengthS73 = _M0L3lenS71 - _M0L6offsetS76;
  } else {
    int64_t _M0L7_2aSomeS75 = _M0L6lengthS74;
    _M0L6lengthS73 = (int32_t)_M0L7_2aSomeS75;
  }
  if (_M0L6offsetS76 >= 0) {
    if (_M0L6lengthS73 >= 0) {
      int32_t _M0L6_2atmpS1032 = _M0L6offsetS76 + _M0L6lengthS73;
      _if__result_2477 = _M0L6_2atmpS1032 <= _M0L3lenS71;
    } else {
      _if__result_2477 = 0;
    }
  } else {
    _if__result_2477 = 0;
  }
  if (_if__result_2477) {
    #line 84 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS72, _M0L6offsetS76, _M0L6lengthS73);
  } else {
    moonbit_decref(_M0L4selfS72);
    #line 83 "/home/blem/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS69
) {
  int32_t _M0L7initialS68;
  moonbit_bytes_t _M0L4dataS70;
  struct _M0TPB13StringBuilder* _block_2478;
  #line 32 "/home/blem/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS69 < 1) {
    _M0L7initialS68 = 1;
  } else {
    _M0L7initialS68 = _M0L10size__hintS69;
  }
  _M0L4dataS70 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS68, 0);
  _block_2478
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_2478)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_2478->$0 = _M0L4dataS70;
  _block_2478->$1 = 0;
  return _block_2478;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS67) {
  int32_t _M0L6_2atmpS1031;
  #line 1903 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1031 = (int32_t)_M0L4selfS67;
  return _M0L6_2atmpS1031;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS57,
  int32_t _M0L11dst__offsetS58,
  moonbit_string_t* _M0L3srcS59,
  int32_t _M0L11src__offsetS60,
  int32_t _M0L3lenS61
) {
  #line 104 "/home/blem/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/home/blem/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS57, _M0L11dst__offsetS58, _M0L3srcS59, _M0L11src__offsetS60, _M0L3lenS61);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS62,
  int32_t _M0L11dst__offsetS63,
  struct _M0TUsiE** _M0L3srcS64,
  int32_t _M0L11src__offsetS65,
  int32_t _M0L3lenS66
) {
  #line 104 "/home/blem/.moon/lib/core/builtin/uninitialized_array.mbt"
  #line 113 "/home/blem/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS62, _M0L11dst__offsetS63, _M0L3srcS64, _M0L11src__offsetS65, _M0L3lenS66);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS30,
  int32_t _M0L11dst__offsetS32,
  moonbit_bytes_t _M0L3srcS31,
  int32_t _M0L11src__offsetS33,
  int32_t _M0L3lenS35
) {
  int32_t _if__result_2479;
  #line 38 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS30 == _M0L3srcS31) {
    _if__result_2479 = _M0L11dst__offsetS32 < _M0L11src__offsetS33;
  } else {
    _if__result_2479 = 0;
  }
  if (_if__result_2479) {
    int32_t _M0L1iS34 = 0;
    while (1) {
      if (_M0L1iS34 < _M0L3lenS35) {
        int32_t _M0L6_2atmpS1004 = _M0L11dst__offsetS32 + _M0L1iS34;
        int32_t _M0L6_2atmpS1006 = _M0L11src__offsetS33 + _M0L1iS34;
        int32_t _M0L6_2atmpS1005;
        int32_t _M0L6_2atmpS1007;
        if (
          _M0L6_2atmpS1006 < 0
          || _M0L6_2atmpS1006 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1005 = (int32_t)_M0L3srcS31[_M0L6_2atmpS1006];
        if (
          _M0L6_2atmpS1004 < 0
          || _M0L6_2atmpS1004 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS30[_M0L6_2atmpS1004] = _M0L6_2atmpS1005;
        _M0L6_2atmpS1007 = _M0L1iS34 + 1;
        _M0L1iS34 = _M0L6_2atmpS1007;
        continue;
      } else {
        moonbit_decref(_M0L3srcS31);
        moonbit_decref(_M0L3dstS30);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1012 = _M0L3lenS35 - 1;
    int32_t _M0L1iS37 = _M0L6_2atmpS1012;
    while (1) {
      if (_M0L1iS37 >= 0) {
        int32_t _M0L6_2atmpS1008 = _M0L11dst__offsetS32 + _M0L1iS37;
        int32_t _M0L6_2atmpS1010 = _M0L11src__offsetS33 + _M0L1iS37;
        int32_t _M0L6_2atmpS1009;
        int32_t _M0L6_2atmpS1011;
        if (
          _M0L6_2atmpS1010 < 0
          || _M0L6_2atmpS1010 >= Moonbit_array_length(_M0L3srcS31)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1009 = (int32_t)_M0L3srcS31[_M0L6_2atmpS1010];
        if (
          _M0L6_2atmpS1008 < 0
          || _M0L6_2atmpS1008 >= Moonbit_array_length(_M0L3dstS30)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS30[_M0L6_2atmpS1008] = _M0L6_2atmpS1009;
        _M0L6_2atmpS1011 = _M0L1iS37 - 1;
        _M0L1iS37 = _M0L6_2atmpS1011;
        continue;
      } else {
        moonbit_decref(_M0L3srcS31);
        moonbit_decref(_M0L3dstS30);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS39,
  int32_t _M0L11dst__offsetS41,
  moonbit_string_t* _M0L3srcS40,
  int32_t _M0L11src__offsetS42,
  int32_t _M0L3lenS44
) {
  int32_t _if__result_2482;
  #line 38 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS39 == _M0L3srcS40) {
    _if__result_2482 = _M0L11dst__offsetS41 < _M0L11src__offsetS42;
  } else {
    _if__result_2482 = 0;
  }
  if (_if__result_2482) {
    int32_t _M0L1iS43 = 0;
    while (1) {
      if (_M0L1iS43 < _M0L3lenS44) {
        int32_t _M0L6_2atmpS1013 = _M0L11dst__offsetS41 + _M0L1iS43;
        int32_t _M0L6_2atmpS1015 = _M0L11src__offsetS42 + _M0L1iS43;
        moonbit_string_t _M0L6_2atmpS2266;
        moonbit_string_t _M0L6_2atmpS1014;
        moonbit_string_t _M0L6_2aoldS2265;
        int32_t _M0L6_2atmpS1016;
        if (
          _M0L6_2atmpS1015 < 0
          || _M0L6_2atmpS1015 >= Moonbit_array_length(_M0L3srcS40)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2266 = (moonbit_string_t)_M0L3srcS40[_M0L6_2atmpS1015];
        _M0L6_2atmpS1014 = _M0L6_2atmpS2266;
        if (
          _M0L6_2atmpS1013 < 0
          || _M0L6_2atmpS1013 >= Moonbit_array_length(_M0L3dstS39)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2265 = (moonbit_string_t)_M0L3dstS39[_M0L6_2atmpS1013];
        moonbit_incref(_M0L6_2atmpS1014);
        moonbit_decref(_M0L6_2aoldS2265);
        _M0L3dstS39[_M0L6_2atmpS1013] = _M0L6_2atmpS1014;
        _M0L6_2atmpS1016 = _M0L1iS43 + 1;
        _M0L1iS43 = _M0L6_2atmpS1016;
        continue;
      } else {
        moonbit_decref(_M0L3srcS40);
        moonbit_decref(_M0L3dstS39);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1021 = _M0L3lenS44 - 1;
    int32_t _M0L1iS46 = _M0L6_2atmpS1021;
    while (1) {
      if (_M0L1iS46 >= 0) {
        int32_t _M0L6_2atmpS1017 = _M0L11dst__offsetS41 + _M0L1iS46;
        int32_t _M0L6_2atmpS1019 = _M0L11src__offsetS42 + _M0L1iS46;
        moonbit_string_t _M0L6_2atmpS2268;
        moonbit_string_t _M0L6_2atmpS1018;
        moonbit_string_t _M0L6_2aoldS2267;
        int32_t _M0L6_2atmpS1020;
        if (
          _M0L6_2atmpS1019 < 0
          || _M0L6_2atmpS1019 >= Moonbit_array_length(_M0L3srcS40)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2268 = (moonbit_string_t)_M0L3srcS40[_M0L6_2atmpS1019];
        _M0L6_2atmpS1018 = _M0L6_2atmpS2268;
        if (
          _M0L6_2atmpS1017 < 0
          || _M0L6_2atmpS1017 >= Moonbit_array_length(_M0L3dstS39)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2267 = (moonbit_string_t)_M0L3dstS39[_M0L6_2atmpS1017];
        moonbit_incref(_M0L6_2atmpS1018);
        moonbit_decref(_M0L6_2aoldS2267);
        _M0L3dstS39[_M0L6_2atmpS1017] = _M0L6_2atmpS1018;
        _M0L6_2atmpS1020 = _M0L1iS46 - 1;
        _M0L1iS46 = _M0L6_2atmpS1020;
        continue;
      } else {
        moonbit_decref(_M0L3srcS40);
        moonbit_decref(_M0L3dstS39);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS48,
  int32_t _M0L11dst__offsetS50,
  struct _M0TUsiE** _M0L3srcS49,
  int32_t _M0L11src__offsetS51,
  int32_t _M0L3lenS53
) {
  int32_t _if__result_2485;
  #line 38 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS48 == _M0L3srcS49) {
    _if__result_2485 = _M0L11dst__offsetS50 < _M0L11src__offsetS51;
  } else {
    _if__result_2485 = 0;
  }
  if (_if__result_2485) {
    int32_t _M0L1iS52 = 0;
    while (1) {
      if (_M0L1iS52 < _M0L3lenS53) {
        int32_t _M0L6_2atmpS1022 = _M0L11dst__offsetS50 + _M0L1iS52;
        int32_t _M0L6_2atmpS1024 = _M0L11src__offsetS51 + _M0L1iS52;
        struct _M0TUsiE* _M0L6_2atmpS2270;
        struct _M0TUsiE* _M0L6_2atmpS1023;
        struct _M0TUsiE* _M0L6_2aoldS2269;
        int32_t _M0L6_2atmpS1025;
        if (
          _M0L6_2atmpS1024 < 0
          || _M0L6_2atmpS1024 >= Moonbit_array_length(_M0L3srcS49)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2270 = (struct _M0TUsiE*)_M0L3srcS49[_M0L6_2atmpS1024];
        _M0L6_2atmpS1023 = _M0L6_2atmpS2270;
        if (
          _M0L6_2atmpS1022 < 0
          || _M0L6_2atmpS1022 >= Moonbit_array_length(_M0L3dstS48)
        ) {
          #line 49 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2269 = (struct _M0TUsiE*)_M0L3dstS48[_M0L6_2atmpS1022];
        if (_M0L6_2atmpS1023) {
          moonbit_incref(_M0L6_2atmpS1023);
        }
        if (_M0L6_2aoldS2269) {
          moonbit_decref(_M0L6_2aoldS2269);
        }
        _M0L3dstS48[_M0L6_2atmpS1022] = _M0L6_2atmpS1023;
        _M0L6_2atmpS1025 = _M0L1iS52 + 1;
        _M0L1iS52 = _M0L6_2atmpS1025;
        continue;
      } else {
        moonbit_decref(_M0L3srcS49);
        moonbit_decref(_M0L3dstS48);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1030 = _M0L3lenS53 - 1;
    int32_t _M0L1iS55 = _M0L6_2atmpS1030;
    while (1) {
      if (_M0L1iS55 >= 0) {
        int32_t _M0L6_2atmpS1026 = _M0L11dst__offsetS50 + _M0L1iS55;
        int32_t _M0L6_2atmpS1028 = _M0L11src__offsetS51 + _M0L1iS55;
        struct _M0TUsiE* _M0L6_2atmpS2272;
        struct _M0TUsiE* _M0L6_2atmpS1027;
        struct _M0TUsiE* _M0L6_2aoldS2271;
        int32_t _M0L6_2atmpS1029;
        if (
          _M0L6_2atmpS1028 < 0
          || _M0L6_2atmpS1028 >= Moonbit_array_length(_M0L3srcS49)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2272 = (struct _M0TUsiE*)_M0L3srcS49[_M0L6_2atmpS1028];
        _M0L6_2atmpS1027 = _M0L6_2atmpS2272;
        if (
          _M0L6_2atmpS1026 < 0
          || _M0L6_2atmpS1026 >= Moonbit_array_length(_M0L3dstS48)
        ) {
          #line 53 "/home/blem/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2271 = (struct _M0TUsiE*)_M0L3dstS48[_M0L6_2atmpS1026];
        if (_M0L6_2atmpS1027) {
          moonbit_incref(_M0L6_2atmpS1027);
        }
        if (_M0L6_2aoldS2271) {
          moonbit_decref(_M0L6_2aoldS2271);
        }
        _M0L3dstS48[_M0L6_2atmpS1026] = _M0L6_2atmpS1027;
        _M0L6_2atmpS1029 = _M0L1iS55 - 1;
        _M0L1iS55 = _M0L6_2atmpS1029;
        continue;
      } else {
        moonbit_decref(_M0L3srcS49);
        moonbit_decref(_M0L3dstS48);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS983;
  moonbit_string_t _M0L6_2atmpS2275;
  moonbit_string_t _M0L6_2atmpS981;
  moonbit_string_t _M0L6_2atmpS982;
  moonbit_string_t _M0L6_2atmpS2274;
  moonbit_string_t _M0L6_2atmpS980;
  moonbit_string_t _M0L6_2atmpS2273;
  moonbit_string_t _M0L6_2atmpS979;
  #line 69 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  #line 73 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS983 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2275
  = moonbit_add_string(_M0L6_2atmpS983, (moonbit_string_t)moonbit_string_literal_110.data);
  moonbit_decref(_M0L6_2atmpS983);
  _M0L6_2atmpS981 = _M0L6_2atmpS2275;
  #line 74 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS982
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2274 = moonbit_add_string(_M0L6_2atmpS981, _M0L6_2atmpS982);
  moonbit_decref(_M0L6_2atmpS981);
  moonbit_decref(_M0L6_2atmpS982);
  _M0L6_2atmpS980 = _M0L6_2atmpS2274;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2273
  = moonbit_add_string(_M0L6_2atmpS980, (moonbit_string_t)moonbit_string_literal_111.data);
  moonbit_decref(_M0L6_2atmpS980);
  _M0L6_2atmpS979 = _M0L6_2atmpS2273;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS979);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS988;
  moonbit_string_t _M0L6_2atmpS2278;
  moonbit_string_t _M0L6_2atmpS986;
  moonbit_string_t _M0L6_2atmpS987;
  moonbit_string_t _M0L6_2atmpS2277;
  moonbit_string_t _M0L6_2atmpS985;
  moonbit_string_t _M0L6_2atmpS2276;
  moonbit_string_t _M0L6_2atmpS984;
  #line 69 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  #line 73 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS988 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2278
  = moonbit_add_string(_M0L6_2atmpS988, (moonbit_string_t)moonbit_string_literal_110.data);
  moonbit_decref(_M0L6_2atmpS988);
  _M0L6_2atmpS986 = _M0L6_2atmpS2278;
  #line 74 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS987
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2277 = moonbit_add_string(_M0L6_2atmpS986, _M0L6_2atmpS987);
  moonbit_decref(_M0L6_2atmpS986);
  moonbit_decref(_M0L6_2atmpS987);
  _M0L6_2atmpS985 = _M0L6_2atmpS2277;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2276
  = moonbit_add_string(_M0L6_2atmpS985, (moonbit_string_t)moonbit_string_literal_111.data);
  moonbit_decref(_M0L6_2atmpS985);
  _M0L6_2atmpS984 = _M0L6_2atmpS2276;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS984);
  return 0;
}

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L6stringS24,
  moonbit_string_t _M0L3locS25
) {
  moonbit_string_t _M0L6_2atmpS993;
  moonbit_string_t _M0L6_2atmpS2281;
  moonbit_string_t _M0L6_2atmpS991;
  moonbit_string_t _M0L6_2atmpS992;
  moonbit_string_t _M0L6_2atmpS2280;
  moonbit_string_t _M0L6_2atmpS990;
  moonbit_string_t _M0L6_2atmpS2279;
  moonbit_string_t _M0L6_2atmpS989;
  #line 69 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  #line 73 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS993 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS24);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2281
  = moonbit_add_string(_M0L6_2atmpS993, (moonbit_string_t)moonbit_string_literal_110.data);
  moonbit_decref(_M0L6_2atmpS993);
  _M0L6_2atmpS991 = _M0L6_2atmpS2281;
  #line 74 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS992
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS25);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2280 = moonbit_add_string(_M0L6_2atmpS991, _M0L6_2atmpS992);
  moonbit_decref(_M0L6_2atmpS991);
  moonbit_decref(_M0L6_2atmpS992);
  _M0L6_2atmpS990 = _M0L6_2atmpS2280;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2279
  = moonbit_add_string(_M0L6_2atmpS990, (moonbit_string_t)moonbit_string_literal_111.data);
  moonbit_decref(_M0L6_2atmpS990);
  _M0L6_2atmpS989 = _M0L6_2atmpS2279;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  return _M0FPC15abort5abortGRPC15bytes9BytesViewE(_M0L6_2atmpS989);
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS26,
  moonbit_string_t _M0L3locS27
) {
  moonbit_string_t _M0L6_2atmpS998;
  moonbit_string_t _M0L6_2atmpS2284;
  moonbit_string_t _M0L6_2atmpS996;
  moonbit_string_t _M0L6_2atmpS997;
  moonbit_string_t _M0L6_2atmpS2283;
  moonbit_string_t _M0L6_2atmpS995;
  moonbit_string_t _M0L6_2atmpS2282;
  moonbit_string_t _M0L6_2atmpS994;
  #line 69 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  #line 73 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS998 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS26);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2284
  = moonbit_add_string(_M0L6_2atmpS998, (moonbit_string_t)moonbit_string_literal_110.data);
  moonbit_decref(_M0L6_2atmpS998);
  _M0L6_2atmpS996 = _M0L6_2atmpS2284;
  #line 74 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS997
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS27);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2283 = moonbit_add_string(_M0L6_2atmpS996, _M0L6_2atmpS997);
  moonbit_decref(_M0L6_2atmpS996);
  moonbit_decref(_M0L6_2atmpS997);
  _M0L6_2atmpS995 = _M0L6_2atmpS2283;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2282
  = moonbit_add_string(_M0L6_2atmpS995, (moonbit_string_t)moonbit_string_literal_111.data);
  moonbit_decref(_M0L6_2atmpS995);
  _M0L6_2atmpS994 = _M0L6_2atmpS2282;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS994);
}

moonbit_string_t _M0FPB5abortGsE(
  moonbit_string_t _M0L6stringS28,
  moonbit_string_t _M0L3locS29
) {
  moonbit_string_t _M0L6_2atmpS1003;
  moonbit_string_t _M0L6_2atmpS2287;
  moonbit_string_t _M0L6_2atmpS1001;
  moonbit_string_t _M0L6_2atmpS1002;
  moonbit_string_t _M0L6_2atmpS2286;
  moonbit_string_t _M0L6_2atmpS1000;
  moonbit_string_t _M0L6_2atmpS2285;
  moonbit_string_t _M0L6_2atmpS999;
  #line 69 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  #line 73 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1003 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS28);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2287
  = moonbit_add_string(_M0L6_2atmpS1003, (moonbit_string_t)moonbit_string_literal_110.data);
  moonbit_decref(_M0L6_2atmpS1003);
  _M0L6_2atmpS1001 = _M0L6_2atmpS2287;
  #line 74 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS1002
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS29);
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2286 = moonbit_add_string(_M0L6_2atmpS1001, _M0L6_2atmpS1002);
  moonbit_decref(_M0L6_2atmpS1001);
  moonbit_decref(_M0L6_2atmpS1002);
  _M0L6_2atmpS1000 = _M0L6_2atmpS2286;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2285
  = moonbit_add_string(_M0L6_2atmpS1000, (moonbit_string_t)moonbit_string_literal_111.data);
  moonbit_decref(_M0L6_2atmpS1000);
  _M0L6_2atmpS999 = _M0L6_2atmpS2285;
  #line 71 "/home/blem/.moon/lib/core/builtin/intrinsics.mbt"
  return _M0FPC15abort5abortGsE(_M0L6_2atmpS999);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS18,
  uint32_t _M0L5valueS19
) {
  uint32_t _M0L3accS978;
  uint32_t _M0L6_2atmpS977;
  #line 236 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS978 = _M0L4selfS18->$0;
  _M0L6_2atmpS977 = _M0L3accS978 + 4u;
  _M0L4selfS18->$0 = _M0L6_2atmpS977;
  #line 238 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS18, _M0L5valueS19);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS16,
  uint32_t _M0L5inputS17
) {
  uint32_t _M0L3accS975;
  uint32_t _M0L6_2atmpS976;
  uint32_t _M0L6_2atmpS974;
  uint32_t _M0L6_2atmpS973;
  uint32_t _M0L6_2atmpS972;
  #line 453 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0L3accS975 = _M0L4selfS16->$0;
  _M0L6_2atmpS976 = _M0L5inputS17 * 3266489917u;
  _M0L6_2atmpS974 = _M0L3accS975 + _M0L6_2atmpS976;
  #line 454 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS973 = _M0FPB4rotl(_M0L6_2atmpS974, 17);
  _M0L6_2atmpS972 = _M0L6_2atmpS973 * 668265263u;
  _M0L4selfS16->$0 = _M0L6_2atmpS972;
  moonbit_decref(_M0L4selfS16);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS14, int32_t _M0L1rS15) {
  uint32_t _M0L6_2atmpS969;
  int32_t _M0L6_2atmpS971;
  uint32_t _M0L6_2atmpS970;
  #line 463 "/home/blem/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS969 = _M0L1xS14 << (_M0L1rS15 & 31);
  _M0L6_2atmpS971 = 32 - _M0L1rS15;
  _M0L6_2atmpS970 = _M0L1xS14 >> (_M0L6_2atmpS971 & 31);
  return _M0L6_2atmpS969 | _M0L6_2atmpS970;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S10,
  struct _M0TPB6Logger _M0L10_2ax__4934S13
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS11;
  moonbit_string_t _M0L8_2afieldS2288;
  int32_t _M0L6_2acntS2357;
  moonbit_string_t _M0L15_2a_2aarg__4935S12;
  #line 37 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2aFailureS11
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S10;
  _M0L8_2afieldS2288 = _M0L10_2aFailureS11->$0;
  _M0L6_2acntS2357 = Moonbit_object_header(_M0L10_2aFailureS11)->rc;
  if (_M0L6_2acntS2357 > 1) {
    int32_t _M0L11_2anew__cntS2358 = _M0L6_2acntS2357 - 1;
    Moonbit_object_header(_M0L10_2aFailureS11)->rc = _M0L11_2anew__cntS2358;
    moonbit_incref(_M0L8_2afieldS2288);
  } else if (_M0L6_2acntS2357 == 1) {
    #line 37 "/home/blem/.moon/lib/core/builtin/failure.mbt"
    moonbit_free(_M0L10_2aFailureS11);
  }
  _M0L15_2a_2aarg__4935S12 = _M0L8_2afieldS2288;
  if (_M0L10_2ax__4934S13.$1) {
    moonbit_incref(_M0L10_2ax__4934S13.$1);
  }
  #line 37 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__4934S13.$0->$method_0(_M0L10_2ax__4934S13.$1, (moonbit_string_t)moonbit_string_literal_112.data);
  if (_M0L10_2ax__4934S13.$1) {
    moonbit_incref(_M0L10_2ax__4934S13.$1);
  }
  #line 37 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S13, _M0L15_2a_2aarg__4935S12);
  #line 37 "/home/blem/.moon/lib/core/builtin/failure.mbt"
  _M0L10_2ax__4934S13.$0->$method_0(_M0L10_2ax__4934S13.$1, (moonbit_string_t)moonbit_string_literal_113.data);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGyE(
  struct _M0TPB6Logger _M0L4selfS7,
  int32_t _M0L3objS6
) {
  #line 150 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 151 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPC14byte4BytePB4Show6output(_M0L3objS6, _M0L4selfS7);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS9,
  moonbit_string_t _M0L3objS8
) {
  #line 150 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  #line 151 "/home/blem/.moon/lib/core/builtin/traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS8, _M0L4selfS9);
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

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L3msgS3
) {
  #line 47 "/home/blem/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS3);
  moonbit_decref(_M0L3msgS3);
  #line 50 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS4
) {
  #line 47 "/home/blem/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS4);
  moonbit_decref(_M0L3msgS4);
  #line 50 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FPC15abort5abortGsE(moonbit_string_t _M0L3msgS5) {
  #line 47 "/home/blem/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS5);
  moonbit_decref(_M0L3msgS5);
  #line 50 "/home/blem/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS873) {
  switch (Moonbit_object_tag(_M0L4_2aeS873)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS873);
      return (moonbit_string_t)moonbit_string_literal_114.data;
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS873);
      return (moonbit_string_t)moonbit_string_literal_115.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS873);
      return (moonbit_string_t)moonbit_string_literal_116.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS873);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS873);
      return (moonbit_string_t)moonbit_string_literal_117.data;
      break;
    }
  }
}

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGAyE(
  void* _M0L11_2aobj__ptrS910
) {
  moonbit_bytes_t _M0L7_2aselfS909 = (moonbit_bytes_t)_M0L11_2aobj__ptrS910;
  return _M0IP016_24default__implPB4Show10to__stringGAyE(_M0L7_2aselfS909);
}

int32_t _M0IPC15array10FixedArrayPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGyE(
  void* _M0L11_2aobj__ptrS908,
  struct _M0TPB6Logger _M0L8_2aparamS907
) {
  moonbit_bytes_t _M0L7_2aselfS906 = (moonbit_bytes_t)_M0L11_2aobj__ptrS908;
  _M0IPC15array10FixedArrayPB4Show6outputGyE(_M0L7_2aselfS906, _M0L8_2aparamS907);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS905,
  int32_t _M0L8_2aparamS904
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS903 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS905;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS903, _M0L8_2aparamS904);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS902,
  struct _M0TPC16string10StringView _M0L8_2aparamS901
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS900 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS902;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS900, _M0L8_2aparamS901);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS899,
  moonbit_string_t _M0L8_2aparamS896,
  int32_t _M0L8_2aparamS897,
  int32_t _M0L8_2aparamS898
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS895 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS899;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS895, _M0L8_2aparamS896, _M0L8_2aparamS897, _M0L8_2aparamS898);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS894,
  moonbit_string_t _M0L8_2aparamS893
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS892 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS894;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS892, _M0L8_2aparamS893);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS968 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS967;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS930;
  moonbit_string_t* _M0L6_2atmpS966;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS965;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS931;
  moonbit_string_t* _M0L6_2atmpS964;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS963;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS932;
  moonbit_string_t* _M0L6_2atmpS962;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS961;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS933;
  moonbit_string_t* _M0L6_2atmpS960;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS959;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS934;
  moonbit_string_t* _M0L6_2atmpS958;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS957;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS935;
  moonbit_string_t* _M0L6_2atmpS956;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS955;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS936;
  moonbit_string_t* _M0L6_2atmpS954;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS953;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS937;
  moonbit_string_t* _M0L6_2atmpS952;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS951;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS938;
  moonbit_string_t* _M0L6_2atmpS950;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS949;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS939;
  moonbit_string_t* _M0L6_2atmpS948;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS947;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS940;
  moonbit_string_t* _M0L6_2atmpS946;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS945;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS941;
  moonbit_string_t* _M0L6_2atmpS944;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS943;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS942;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS798;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS929;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS928;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS927;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS918;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS799;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS926;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS925;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS924;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS919;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS800;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS923;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS922;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS921;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS920;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS797;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS917;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS916;
  _M0L6_2atmpS968[0] = (moonbit_string_t)moonbit_string_literal_118.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS967
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS967)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS967->$0
  = _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS967->$1 = _M0L6_2atmpS968;
  _M0L8_2atupleS930
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS930)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS930->$0 = 0;
  _M0L8_2atupleS930->$1 = _M0L8_2atupleS967;
  _M0L6_2atmpS966 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS966[0] = (moonbit_string_t)moonbit_string_literal_119.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__1_2eclo);
  _M0L8_2atupleS965
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS965)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS965->$0
  = _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__1_2eclo;
  _M0L8_2atupleS965->$1 = _M0L6_2atmpS966;
  _M0L8_2atupleS931
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS931)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS931->$0 = 1;
  _M0L8_2atupleS931->$1 = _M0L8_2atupleS965;
  _M0L6_2atmpS964 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS964[0] = (moonbit_string_t)moonbit_string_literal_120.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__2_2eclo);
  _M0L8_2atupleS963
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS963)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS963->$0
  = _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__2_2eclo;
  _M0L8_2atupleS963->$1 = _M0L6_2atmpS964;
  _M0L8_2atupleS932
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS932)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS932->$0 = 2;
  _M0L8_2atupleS932->$1 = _M0L8_2atupleS963;
  _M0L6_2atmpS962 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS962[0] = (moonbit_string_t)moonbit_string_literal_121.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__3_2eclo);
  _M0L8_2atupleS961
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS961)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS961->$0
  = _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__3_2eclo;
  _M0L8_2atupleS961->$1 = _M0L6_2atmpS962;
  _M0L8_2atupleS933
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS933)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS933->$0 = 3;
  _M0L8_2atupleS933->$1 = _M0L8_2atupleS961;
  _M0L6_2atmpS960 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS960[0] = (moonbit_string_t)moonbit_string_literal_122.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__4_2eclo);
  _M0L8_2atupleS959
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS959)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS959->$0
  = _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__4_2eclo;
  _M0L8_2atupleS959->$1 = _M0L6_2atmpS960;
  _M0L8_2atupleS934
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS934)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS934->$0 = 4;
  _M0L8_2atupleS934->$1 = _M0L8_2atupleS959;
  _M0L6_2atmpS958 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS958[0] = (moonbit_string_t)moonbit_string_literal_123.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__5_2eclo);
  _M0L8_2atupleS957
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS957)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS957->$0
  = _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__5_2eclo;
  _M0L8_2atupleS957->$1 = _M0L6_2atmpS958;
  _M0L8_2atupleS935
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS935)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS935->$0 = 5;
  _M0L8_2atupleS935->$1 = _M0L8_2atupleS957;
  _M0L6_2atmpS956 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS956[0] = (moonbit_string_t)moonbit_string_literal_124.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__6_2eclo);
  _M0L8_2atupleS955
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS955)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS955->$0
  = _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__6_2eclo;
  _M0L8_2atupleS955->$1 = _M0L6_2atmpS956;
  _M0L8_2atupleS936
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS936)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS936->$0 = 6;
  _M0L8_2atupleS936->$1 = _M0L8_2atupleS955;
  _M0L6_2atmpS954 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS954[0] = (moonbit_string_t)moonbit_string_literal_125.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__7_2eclo);
  _M0L8_2atupleS953
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS953)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS953->$0
  = _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__7_2eclo;
  _M0L8_2atupleS953->$1 = _M0L6_2atmpS954;
  _M0L8_2atupleS937
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS937)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS937->$0 = 7;
  _M0L8_2atupleS937->$1 = _M0L8_2atupleS953;
  _M0L6_2atmpS952 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS952[0] = (moonbit_string_t)moonbit_string_literal_126.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__8_2eclo);
  _M0L8_2atupleS951
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS951)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS951->$0
  = _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__8_2eclo;
  _M0L8_2atupleS951->$1 = _M0L6_2atmpS952;
  _M0L8_2atupleS938
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS938)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS938->$0 = 8;
  _M0L8_2atupleS938->$1 = _M0L8_2atupleS951;
  _M0L6_2atmpS950 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS950[0] = (moonbit_string_t)moonbit_string_literal_127.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__9_2eclo);
  _M0L8_2atupleS949
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS949)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS949->$0
  = _M0FP28bikallem20blit__blackbox__test45____test__626c69745f746573742e6d6274__9_2eclo;
  _M0L8_2atupleS949->$1 = _M0L6_2atmpS950;
  _M0L8_2atupleS939
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS939)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS939->$0 = 9;
  _M0L8_2atupleS939->$1 = _M0L8_2atupleS949;
  _M0L6_2atmpS948 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS948[0] = (moonbit_string_t)moonbit_string_literal_128.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test46____test__626c69745f746573742e6d6274__10_2eclo);
  _M0L8_2atupleS947
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS947)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS947->$0
  = _M0FP28bikallem20blit__blackbox__test46____test__626c69745f746573742e6d6274__10_2eclo;
  _M0L8_2atupleS947->$1 = _M0L6_2atmpS948;
  _M0L8_2atupleS940
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS940)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS940->$0 = 10;
  _M0L8_2atupleS940->$1 = _M0L8_2atupleS947;
  _M0L6_2atmpS946 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS946[0] = (moonbit_string_t)moonbit_string_literal_129.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test46____test__626c69745f746573742e6d6274__11_2eclo);
  _M0L8_2atupleS945
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS945)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS945->$0
  = _M0FP28bikallem20blit__blackbox__test46____test__626c69745f746573742e6d6274__11_2eclo;
  _M0L8_2atupleS945->$1 = _M0L6_2atmpS946;
  _M0L8_2atupleS941
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS941)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS941->$0 = 11;
  _M0L8_2atupleS941->$1 = _M0L8_2atupleS945;
  _M0L6_2atmpS944 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS944[0] = (moonbit_string_t)moonbit_string_literal_130.data;
  moonbit_incref(_M0FP28bikallem20blit__blackbox__test46____test__626c69745f746573742e6d6274__12_2eclo);
  _M0L8_2atupleS943
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS943)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS943->$0
  = _M0FP28bikallem20blit__blackbox__test46____test__626c69745f746573742e6d6274__12_2eclo;
  _M0L8_2atupleS943->$1 = _M0L6_2atmpS944;
  _M0L8_2atupleS942
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS942)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS942->$0 = 12;
  _M0L8_2atupleS942->$1 = _M0L8_2atupleS943;
  _M0L7_2abindS798
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(13);
  _M0L7_2abindS798[0] = _M0L8_2atupleS930;
  _M0L7_2abindS798[1] = _M0L8_2atupleS931;
  _M0L7_2abindS798[2] = _M0L8_2atupleS932;
  _M0L7_2abindS798[3] = _M0L8_2atupleS933;
  _M0L7_2abindS798[4] = _M0L8_2atupleS934;
  _M0L7_2abindS798[5] = _M0L8_2atupleS935;
  _M0L7_2abindS798[6] = _M0L8_2atupleS936;
  _M0L7_2abindS798[7] = _M0L8_2atupleS937;
  _M0L7_2abindS798[8] = _M0L8_2atupleS938;
  _M0L7_2abindS798[9] = _M0L8_2atupleS939;
  _M0L7_2abindS798[10] = _M0L8_2atupleS940;
  _M0L7_2abindS798[11] = _M0L8_2atupleS941;
  _M0L7_2abindS798[12] = _M0L8_2atupleS942;
  _M0L6_2atmpS929 = _M0L7_2abindS798;
  _M0L6_2atmpS928
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 13, _M0L6_2atmpS929
  };
  #line 398 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS927
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS928);
  _M0L8_2atupleS918
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS918)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS918->$0 = (moonbit_string_t)moonbit_string_literal_131.data;
  _M0L8_2atupleS918->$1 = _M0L6_2atmpS927;
  _M0L7_2abindS799
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS926 = _M0L7_2abindS799;
  _M0L6_2atmpS925
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS926
  };
  #line 413 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS924
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS925);
  _M0L8_2atupleS919
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS919)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS919->$0 = (moonbit_string_t)moonbit_string_literal_132.data;
  _M0L8_2atupleS919->$1 = _M0L6_2atmpS924;
  _M0L7_2abindS800
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS923 = _M0L7_2abindS800;
  _M0L6_2atmpS922
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS923
  };
  #line 415 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS921
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS922);
  _M0L8_2atupleS920
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS920)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS920->$0 = (moonbit_string_t)moonbit_string_literal_133.data;
  _M0L8_2atupleS920->$1 = _M0L6_2atmpS921;
  _M0L7_2abindS797
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS797[0] = _M0L8_2atupleS918;
  _M0L7_2abindS797[1] = _M0L8_2atupleS919;
  _M0L7_2abindS797[2] = _M0L8_2atupleS920;
  _M0L6_2atmpS917 = _M0L7_2abindS797;
  _M0L6_2atmpS916
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 3, _M0L6_2atmpS917
  };
  #line 397 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0FP28bikallem20blit__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS916);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS915;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS867;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS868;
  int32_t _M0L7_2abindS869;
  int32_t _M0L2__S870;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS915
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS867
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS867)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS867->$0 = _M0L6_2atmpS915;
  _M0L12async__testsS867->$1 = 0;
  #line 454 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS868
  = _M0FP28bikallem20blit__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS869 = _M0L7_2abindS868->$1;
  _M0L2__S870 = 0;
  while (1) {
    if (_M0L2__S870 < _M0L7_2abindS869) {
      struct _M0TUsiE** _M0L8_2afieldS2292 = _M0L7_2abindS868->$0;
      struct _M0TUsiE** _M0L3bufS914 = _M0L8_2afieldS2292;
      struct _M0TUsiE* _M0L6_2atmpS2291 =
        (struct _M0TUsiE*)_M0L3bufS914[_M0L2__S870];
      struct _M0TUsiE* _M0L3argS871 = _M0L6_2atmpS2291;
      moonbit_string_t _M0L8_2afieldS2290 = _M0L3argS871->$0;
      moonbit_string_t _M0L6_2atmpS911 = _M0L8_2afieldS2290;
      int32_t _M0L8_2afieldS2289 = _M0L3argS871->$1;
      int32_t _M0L6_2atmpS912 = _M0L8_2afieldS2289;
      int32_t _M0L6_2atmpS913;
      moonbit_incref(_M0L6_2atmpS911);
      moonbit_incref(_M0L12async__testsS867);
      #line 455 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
      _M0FP28bikallem20blit__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS867, _M0L6_2atmpS911, _M0L6_2atmpS912);
      _M0L6_2atmpS913 = _M0L2__S870 + 1;
      _M0L2__S870 = _M0L6_2atmpS913;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS868);
    }
    break;
  }
  #line 457 "/home/blem/projects/blit/src/__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP28bikallem20blit__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP28bikallem20blit__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS867);
  return 0;
}