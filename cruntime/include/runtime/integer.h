#pragma once
#ifndef __INTEGER_H__
#define __INTEGER_H__
/*---------------------------------------------------------------------------
  Copyright 2020 Daan Leijen, Microsoft Corporation.

  This is free software; you can redistribute it and/or modify it under the
  terms of the Apache License, Version 2.0. A copy of the License can be
  found in the file "license.txt" at the root of this distribution.
---------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------
  Integers are always boxed: and either a pointer to a `bigint_t`, or a boxed (small) int.
  The boxed small int is restricted in size to SMALLINT_BITS such that we can do
  efficient arithmetic on the boxed representation of a small int directly where
  `boxed(n) == 4*n + 1`. The `smallint_t` size is chosen to allow efficient overflow detection.
--------------------------------------------------------------------------------------------------*/

#if INTPTR_SIZE==8                           // always us 32-bits on 64-bit platforms
typedef int32_t smallint_t;
#define SMALLINT_BITS       (32)
#elif (INTPTR_SIZE==4) && defined(__GNUC__)  // with overflow builtins we can use the full 32 bits
typedef int32_t smallint_t;
#define SMALLINT_BITS       (32)
#elif INTPTR_SIZE==4                         // otherwise generic overflow detect uses 16-bit operands with 32-bit operations
typedef int16_t smallint_t;
#define SMALLINT_BITS       (16)
#else
# error "platform must be 32 or 64 bits."
#endif

#define SMALLINT_MAX  ((int_t)(((uint_t)INTPTR_MAX >> (INTPTR_BITS - SMALLINT_BITS)) >> 2))  // use unsigned shift to avoid UB
#define SMALLINT_MIN  (-SMALLINT_MAX - 1)

static inline bool is_integer(integer_t i) {
  return ((is_int(i)  && unbox_int(i) >= SMALLINT_MIN && unbox_int(i) <= SMALLINT_MAX) 
         || (is_ptr(i) && ptr_tag(unbox_ptr(i)) == TAG_BIGINT));
}

static inline bool is_bigint(integer_t i) {
  assert_internal(is_integer(i));
  return is_ptr_fast(i);
}

static inline bool is_smallint(integer_t i) {
  assert_internal(is_integer(i));
  return is_int_fast(i);
}

static inline bool are_smallints(integer_t i, integer_t j) {
  assert_internal(is_integer(i) && is_integer(j));
  return ((i&j)&1)!=0;
}

static inline integer_t integer_from_small(int_t i) {   // use for known small ints (under 14 bits)
  assert_internal(i >= SMALLINT_MIN && i <= SMALLINT_MAX);
  return box_int(i);
}

/*---------------------------------------------------------------------------------
  Generic operations on integers
-----------------------------------------------------------------------------------*/

static inline box_t     box_integer_t(integer_t i) { return i; }
static inline integer_t unbox_integer_t(box_t b)   { return b; }

static inline void      integer_incref(integer_t x) { boxed_dup(x); }
static inline integer_t integer_dup(integer_t x)    { return boxed_dup(x); }
static inline void      integer_decref(integer_t x, context_t* ctx) { boxed_drop(x, ctx); }

decl_export integer_t  integer_parse(const char* num, context_t* ctx);
decl_export integer_t  integer_from_str(const char* num, context_t* ctx); // for known correct string number
decl_export integer_t  integer_from_big(int_t i, context_t* ctx);         // for possibly large i
decl_export integer_t  integer_from_double(double d, context_t* ctx);     // round d and convert to integer (0 for NaN/Inf)

decl_export int32_t    integer_clamp32_generic(integer_t i, context_t* ctx);
decl_export int64_t    integer_clamp64_generic(integer_t i, context_t* ctx);
decl_export double     integer_as_double_generic(integer_t i, context_t* ctx);

decl_export integer_t  integer_add_generic(integer_t x, integer_t y, context_t* ctx);
decl_export integer_t  integer_sub_generic(integer_t x, integer_t y, context_t* ctx);
decl_export integer_t  integer_mul_generic(integer_t x, integer_t y, context_t* ctx);
decl_export integer_t  integer_div_generic(integer_t x, integer_t y, context_t* ctx);
decl_export integer_t  integer_mod_generic(integer_t x, integer_t y, context_t* ctx);
decl_export integer_t  integer_div_mod_generic(integer_t x, integer_t y, integer_t* mod, context_t* ctx);

decl_export int        integer_cmp_generic(integer_t x, integer_t y, context_t* ctx);
decl_export integer_t  integer_neg_generic(integer_t x, context_t* ctx);
decl_export integer_t  integer_sqr_generic(integer_t x, context_t* ctx);
decl_export integer_t  integer_pow(integer_t x, integer_t p, context_t* ctx);

decl_export bool       integer_is_even_generic(integer_t x, context_t* ctx);
decl_export int        integer_signum_generic(integer_t x, context_t* ctx);

decl_export integer_t  integer_ctz(integer_t x, context_t* ctx);           // count trailing zero digits
decl_export integer_t  integer_count_digits(integer_t x, context_t* ctx);  // count decimal digits
decl_export integer_t  integer_mul_pow10(integer_t x, integer_t p, context_t* ctx);  // x*(10^p)
decl_export integer_t  integer_div_pow10(integer_t x, integer_t p, context_t* ctx);  // x/(10^p)

decl_export void       integer_fprint(FILE* f, integer_t x, context_t* ctx);
decl_export void       integer_print(integer_t x, context_t* ctx);


/*---------------------------------------------------------------------------------
  Inlined operation to allow for fast operation on small integers
-----------------------------------------------------------------------------------*/

static inline integer_t integer_from_int(int_t i, context_t* ctx) {
  return (likely(i >= SMALLINT_MIN && i <= SMALLINT_MAX) ? integer_from_small(i) : integer_from_big(i,ctx));
}

#if defined(__GNUC__) && (SMALLINT_BITS >= 32)  

// Use the overflow detecting primitives

static inline bool smallint_add_ovf(int_t h, int_t y, int_t* r) {
  smallint_t i;
  const bool ovf = __builtin_add_overflow((smallint_t)h, (smallint_t)y, &i);
  *r = (int_t)i; // sign extend
  return ovf;
}

static inline bool smallint_sub_ovf(int_t h, int_t y, int_t* r) {
  smallint_t i;
  const bool ovf = __builtin_sub_overflow((smallint_t)h, (smallint_t)y, &i);
  *r = (int_t)i; // sign extend
  return ovf;
}

static inline bool smallint_mul_ovf(int_t h, int_t y, int_t* r) {
  smallint_t i;
  const bool ovf = __builtin_mul_overflow((smallint_t)h, (smallint_t)y, &i);
  *r = (int_t)i; // sign extend
  return ovf;
}

#else 
// Generic overflow detection, still quite good.

static inline bool smallint_add_ovf(int_t x, int_t y, int_t* r) {
  int_t z = x + y; // do a full add
  *r = z;
  uint_t s = (uint_t)(sar(z, SMALLINT_BITS - 1)); // cast to unsigned to avoid UB for s+1 on overflow
  return (s + 1 > 1); // are the top bits not 0 or -1 ?
}

static inline bool smallint_sub_ovf(int_t x, int_t y, int_t* r) {
  int_t z = x - y; // do a full sub
  *r = z;
  uint_t s = (uint_t)(sar(z, SMALLINT_BITS - 1)); // cast to unsigned to avoid UB for s+1 on overflow
  return (s + 1 > 1); // are the top bits not 0 or -1 ?
}

static inline bool smallint_mul_ovf(int_t x, int_t y, int_t* r) {
  int_t z = x * y; // do a full multiply (as smallint is at most half the int_t size)
  *r = z;
  uint_t s = (uint_t)(sar(z, SMALLINT_BITS - 1)); // cast to unsigned to avoid UB for s+1 on overflow
  return (s + 1 > 1); // are the top bits not 0 or -1 ?
}

#endif


/* Fast addition on small integers. Since `boxed(n) = n*4 + 1`, we can add as:
      boxed(n) + (boxed(m) - 1)
      = (n*4 + 1) + ((m*4 + 1) - 1)
      = n*4 + m*4 + 1
      = 4*(n + m) + 1
      = boxed(n+m)
    (we use (^ 1) instead of (- 1) to reduce register stalls (since we know the bottom bits of `y` are 01)
*/
static inline integer_t integer_add_small(integer_t x, integer_t y, context_t* ctx) {
  assert_internal(are_smallints(x, y));
  int_t i;
  if (likely(!smallint_add_ovf((int_t)x, (int_t)(y^1), &i))) return (integer_t)i;
  return integer_add_generic(x, y, ctx);
}


/*
static inline integer_t integer_add(integer_t x, integer_t y) {
  if (likely(are_smallints(x, y))) return integer_add_small(x, y);
  return integer_add_generic(x, y);
}
*/

/* We further optimize addition on integers, if we add directly, our lowest 2 bits are :
    x + y = z
    00  00  00    ptr + ptr
    00  01  01    ptr + int
    01  00  01    int + ptr
    01  01  10    int + int
  and we can detect afterwards if it was correct to assume these were smallint's
*/
static inline integer_t integer_add(integer_t x, integer_t y, context_t* ctx) {
  assert_internal(is_integer(x) && is_integer(y));
  int_t i;
  if (likely(!smallint_add_ovf((int_t)x, (int_t)y, &i) && (i&2)!=0)) {
    integer_t z = (integer_t)(i) ^ 3;  // == i - 1
    assert_internal(is_int(z));
    return z;
  }
  return integer_add_generic(x, y, ctx);
}


/* Fast subtract on small integers. Since `boxed(n) = n*4 + 1`, we can subtract as:
      boxed(n) - (boxed(m) - 1)
      = (n*4 + 1) - ((m*4 + 1) - 1)
      = n*4 + 1 - m*4
      = (n - m)*4 + 1
      = boxed(n-m)
*/
static inline integer_t integer_sub_small(integer_t x, integer_t y, context_t* ctx) {
  assert_internal(are_smallints(x, y));
  int_t i;
  if (likely(!smallint_sub_ovf((int_t)x, (int_t)(y^1), &i))) return (integer_t)(i);
  return integer_sub_generic(x, y, ctx);
}

static inline integer_t integer_sub(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return integer_sub_small(x, y, ctx);
  return integer_sub_generic(x, y, ctx);
}

/* Fast multiply on small integers. Since `boxed(n) = n*4 + 1`, we can multiply as:
    (boxed(n)/2) * (boxed(m)/2) + 1
    = (n*4+1)/2 * (m*4+1)/2 + 1
    = (n*2) * (m*2) + 1
    = (n*m*4) + 1
    = boxed(n*m)
*/
static inline integer_t integer_mul_small(integer_t x, integer_t y, context_t* ctx) {
  assert_internal(are_smallints(x, y));
  int_t i = sar((int_t)x, 1);
  int_t j = sar((int_t)y, 1);
  int_t k;
  if (likely(!smallint_mul_ovf(i, j, &k))) {
    integer_t z = (integer_t)(k)|1;
    assert_internal(is_int(z));
    return z;
  }
  return integer_mul_generic(x, y, ctx);
}

static inline integer_t integer_mul(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return integer_mul_small(x, y, ctx);
  return integer_mul_generic(x, y, ctx);
}

/* Fast divide on small integers. Since `boxed(n) = n*4 + 1`, we can divide as:
    4*((boxed(n)/2)/((boxed(m)/2) + 1
    = 4*((n*2)/(m*2)) + 1
    = 4*(n/m) + 1
    = boxed(n/m)
*/
static inline integer_t integer_div_small(integer_t x, integer_t y) {
  assert_internal(are_smallints(x, y));
  int_t i = sar((int_t)x, 1);
  int_t j = sar((int_t)y, 1);
  return (shr(i/j, 2)|1);
}

/* Fast modulus on small integers. Since `boxed(n) = n*4 + 1`, we can divide as:
    2*((boxed(n)/2)/((boxed(m)/2) + 1
    = 2*((n*2)%(m*2)) + 1
    = 2*2*(n%m) + 1
    = boxed(n%m)
*/
static inline integer_t integer_mod_small(integer_t x, integer_t y) {
  assert_internal(are_smallints(x, y));
  int_t i = sar((int_t)x, 1);
  int_t j = sar((int_t)y, 1);
  return (shr(i%j, 1)|1);
}

static inline integer_t integer_div_mod_small(integer_t x, integer_t y, integer_t* mod) {
  assert_internal(are_smallints(x, y)); assert_internal(mod!=NULL);
  int_t i = sar((int_t)x, 1);
  int_t j = sar((int_t)y, 1);
  *mod = shr(i%j, 1)|1;
  return (shr(i/j, 2)|1);
}

static inline integer_t integer_div(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return integer_div_small(x, y);
  return integer_div_generic(x, y, ctx);
}

static inline integer_t integer_mod(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return integer_mod_small(x, y);
  return integer_mod_generic(x, y, ctx);
}

static inline integer_t integer_div_mod(integer_t x, integer_t y, integer_t* mod, context_t* ctx) {
  assert_internal(mod!=NULL);
  if (likely(are_smallints(x, y))) return integer_div_mod_small(x, y, mod);
  return integer_div_mod_generic(x, y, mod, ctx);
}

static inline int32_t integer_clamp32(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return (int32_t)unbox_int(x);
  return integer_clamp32_generic(x, ctx);
}

static inline int64_t integer_clamp64(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return (int64_t)unbox_int(x);
  return integer_clamp64_generic(x, ctx);
}

static inline double integer_as_double(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return (double)unbox_int(x);
  return integer_as_double_generic(x, ctx);
}

static inline integer_t integer_sqr(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return integer_mul_small(x,x,ctx);
  return integer_sqr_generic(x,ctx);
}

static inline integer_t integer_neg(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return integer_sub_small(box_int(0),x,ctx);
  return integer_neg_generic(x,ctx);
}

static inline integer_t integer_dec(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return integer_sub(x,integer_from_small(1),ctx);
  return integer_sub_generic(x, integer_from_small(1),ctx);
}

static inline integer_t integer_inc(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return integer_add(x, integer_from_small(1),ctx);
  return integer_add_generic(x, integer_from_small(1),ctx);
}

static inline int integer_cmp(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return (x == y ? 0 : (x > y ? 1 : -1));
  return integer_cmp_generic(x, y, ctx);
}

static inline bool integer_lt(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return (x < y);
  return (integer_cmp(x, y, ctx) == -1);
}

static inline bool integer_lte(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return (x <= y);
  return (integer_cmp(x, y, ctx) <= 0);
}

static inline bool integer_gt(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return (x > y);
  return (integer_cmp(x, y, ctx) == 1);
}

static inline bool integer_gte(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return (x >= y);
  return (integer_cmp(x, y, ctx) >= 0);
}

static inline bool integer_eq(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return (x == y);
  return (integer_cmp(x, y, ctx) == 0);
}

static inline bool integer_neq(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return (x != y);
  return (integer_cmp(x, y, ctx) != 0);
}

static inline bool integer_is_zero(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return (x == integer_from_small(0));
  integer_decref(x,ctx);
  return false;
}

static inline bool integer_is_one(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return (x == integer_from_small(1));
  integer_decref(x,ctx);
  return false;
}

static inline bool integer_is_minus_one(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return (x == integer_from_small(-1));
  integer_decref(x,ctx);
  return false;
}

static inline bool integer_is_even(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return ((x&0x08)==0);
  return integer_is_even_generic(x,ctx);
}

static inline bool integer_is_odd(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return ((x&0x08)!=0);
  return !integer_is_even_generic(x,ctx);
}

static inline int integer_signum(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return (((int_t)x>1)-((int_t)x<0));
  return integer_signum_generic(x,ctx);
}

static inline bool integer_is_neg(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return ((int_t)x<0);
  return (integer_signum_generic(x,ctx) < 0);
}

static inline bool integer_is_pos(integer_t x, context_t* ctx) {
  if (likely(is_smallint(x))) return ((int_t)x>1);
  return (integer_signum_generic(x,ctx) > 0);
}

static inline integer_t integer_max(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return (x>=y ? x : y);
  integer_incref(x); integer_incref(y);
  if (integer_gte(x,y,ctx)) {
    integer_decref(y,ctx); return x;
  }
  else {
    integer_decref(x,ctx); return y;
  }
}

static inline integer_t integer_min(integer_t x, integer_t y, context_t* ctx) {
  if (likely(are_smallints(x, y))) return (x<=y ? x : y);
  integer_incref(x); integer_incref(y);
  if (integer_lte(x, y, ctx)) {
    integer_decref(y, ctx); return x;
  }
  else {
    integer_decref(x, ctx); return y;
  }
}

#endif // include guard