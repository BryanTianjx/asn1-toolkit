/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/bigint.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) BigInteger bodies.

    Sign-plus-limbs storage (matches the C++ asn1::BigInteger layout so
    per_golden / tlv_golden / P1-3 protocol vectors stay byte-equal
    across both runtimes):

      sign  : -1 / 0 / +1 (0 = zero)
      limbs : heap array of uint32_t in little-endian magnitude order;
              no leading zero limbs except for zero (empty array).
      nlimbs: count of valid limbs
      cap   : capacity of limbs

    Sub-task 2 surface (matches bigint.h declarations):

      asn1_bigint_init / free
      asn1_bigint_init_from_decimal
      asn1_bigint_is_zero
      asn1_bigint_sign  (declared in asn1_c_common.h to avoid duplicate
                         declaration with conflicting signatures)
      asn1_bigint_cmp   (declared in asn1_c_common.h)
      asn1_bigint_to_int64
      asn1_bigint_to_bytes  (BER length-prefixed magnitude form, used by
                              SET / SET OF canonical sorting; same wire
                              format as the C++ BigInteger::to_bytes)

    Algorithm notes (mirror asn1::BigInteger in src/runtime/bigint.cpp):

      - Decimal parse: mul_add_u32(10, digit) per char.
      - Compare: sign first, then magnitude (longest wins, then MSB-down
        lexical).
      - to_bytes: minimal big-endian byte form (no leading zeros except
        for zero itself → {0x00}); the BER length-prefixed form is the
        caller's responsibility in the C++ runtime too.

    Specification: ITU-T X.680 INTEGER type
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/bigint.h
***************************************************************************/

#include <asn1/c_runtime/bigint.h>

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------- */
/* Internal helpers                                                       */
/* --------------------------------------------------------------------- */

/* Append one limb; grow capacity in powers of two. Returns 0 on Ok. */
static int bigint_push_limb(asn1_bigint_t* b, uint32_t limb) {
  if (b->nlimbs == b->cap) {
    size_t new_cap = b->cap == 0 ? 4u : b->cap * 2u;
    if (new_cap > ASN1_MAX_BIGINT_LIMBS) {
      return -1;
    }
    uint32_t* nl = (uint32_t*)realloc(b->limbs, new_cap * sizeof(uint32_t));
    if (!nl) {
      return -1;
    }
    b->limbs = nl;
    b->cap = new_cap;
  }
  b->limbs[b->nlimbs++] = limb;
  return 0;
}

/* Multiply magnitude by `mul` (single limb), then add `add`; same shape
   as BigInteger::mul_add_u32 in the C++ runtime. */
static void bigint_mul_add_u32(asn1_bigint_t* b, uint32_t mul, uint32_t add) {
  uint64_t carry = add;
  for (size_t i = 0; i < b->nlimbs; ++i) {
    const uint64_t prod = (uint64_t)b->limbs[i] * (uint64_t)mul + carry;
    b->limbs[i] = (uint32_t)(prod & 0xFFFFFFFFu);
    carry = prod >> 32;
  }
  while (carry != 0) {
    bigint_push_limb(b, (uint32_t)carry);
    carry = 0;
  }
}

/* Three-way magnitude compare: longest wins, then MSB-down. */
static int bigint_cmp_mag(const asn1_bigint_t* a, const asn1_bigint_t* b) {
  if (a->nlimbs != b->nlimbs) {
    return a->nlimbs < b->nlimbs ? -1 : 1;
  }
  for (size_t i = a->nlimbs; i-- > 0;) {
    if (a->limbs[i] != b->limbs[i]) {
      return a->limbs[i] < b->limbs[i] ? -1 : 1;
    }
  }
  return 0;
}

/* Minimal big-endian magnitude byte form: zero → {0x00}; positive
   non-zero → strip leading zero bytes (none should exist after normalize);
   negative handled by caller. */
static int bigint_magnitude_be(const asn1_bigint_t* b,
                                uint8_t** out_buf, size_t* out_size) {
  if (b->nlimbs == 0) {
    uint8_t* z = (uint8_t*)malloc(1);
    if (!z) {
      return -1;
    }
    z[0] = 0x00;
    *out_buf = z;
    *out_size = 1;
    return 0;
  }
  uint8_t* buf = (uint8_t*)malloc(b->nlimbs * 4u);
  if (!buf) {
    return -1;
  }
  for (size_t i = 0; i < b->nlimbs; ++i) {
    const uint32_t w = b->limbs[b->nlimbs - 1 - i];
    const size_t o = i * 4u;
    buf[o + 0] = (uint8_t)((w >> 24) & 0xFFu);
    buf[o + 1] = (uint8_t)((w >> 16) & 0xFFu);
    buf[o + 2] = (uint8_t)((w >> 8) & 0xFFu);
    buf[o + 3] = (uint8_t)(w & 0xFFu);
  }
  /* Strip leading zero bytes (keep at least one). */
  size_t start = 0;
  while (start + 1u < b->nlimbs * 4u && buf[start] == 0u) {
    ++start;
  }
  size_t out_n = b->nlimbs * 4u - start;
  uint8_t* trimmed = (uint8_t*)malloc(out_n);
  if (!trimmed) {
    free(buf);
    return -1;
  }
  memcpy(trimmed, buf + start, out_n);
  free(buf);
  *out_buf = trimmed;
  *out_size = out_n;
  return 0;
}

/* --------------------------------------------------------------------- */
/* Lifecycle                                                              */
/* --------------------------------------------------------------------- */

void asn1_bigint_init(asn1_bigint_t* b) {
  if (!b) {
    return;
  }
  b->sign = 0;
  b->limbs = NULL;
  b->nlimbs = 0;
  b->cap = 0;
}

void asn1_bigint_free(asn1_bigint_t* b) {
  if (!b) {
    return;
  }
  free(b->limbs);
  b->limbs = NULL;
  b->nlimbs = 0;
  b->cap = 0;
  b->sign = 0;
}

asn1_status_t asn1_bigint_init_from_decimal(asn1_bigint_t* b,
                                             const char* text,
                                             int negative) {
  if (!b || !text) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  asn1_bigint_init(b);
  if (text[0] == '\0') {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  size_t i = 0;
  /* `negative` from the caller takes precedence (matches the C++ runtime
     where `from_decimal` inspects the leading '-' itself; here the
     C ABI keeps the parser tighter by letting the caller choose). */
  int neg = negative ? 1 : 0;
  if (!neg && (text[0] == '-' || text[0] == '+')) {
    if (text[0] == '-') {
      neg = 1;
    }
    i = 1;
  }
  if (text[i] == '\0') {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  /* Skip leading zeros so we don't end up with a zero sign on a string
     like "000". */
  while (text[i] == '0') {
    ++i;
  }
  if (text[i] == '\0') {
    /* All zeros → zero. */
    asn1_bigint_init(b);
    return ASN1_OK;
  }
  for (; text[i] != '\0'; ++i) {
    if (!isdigit((unsigned char)text[i])) {
      asn1_bigint_free(b);
      return ASN1_ERR_INVALID_ARGUMENT;
    }
    bigint_mul_add_u32(b, 10u, (uint32_t)(text[i] - '0'));
  }
  /* Don't sign a zero — mirrors the C++ runtime. */
  b->sign = (neg && b->nlimbs > 0) ? -1 : +1;
  return ASN1_OK;
}

/* --------------------------------------------------------------------- */
/* Inspection                                                             */
/* --------------------------------------------------------------------- */

int asn1_bigint_is_zero(const asn1_bigint_t* b) {
  if (!b) {
    return 1;
  }
  return b->nlimbs == 0;
}

int asn1_bigint_sign(const asn1_bigint_t* b) {
  if (!b) {
    return 0;
  }
  if (b->nlimbs == 0) {
    return 0;
  }
  return b->sign < 0 ? -1 : +1;
}

int asn1_bigint_cmp(const asn1_bigint_t* a, const asn1_bigint_t* b) {
  if (!a || !b) {
    /* Null pointers compare unequal; consistent direction so a > NULL
       yields -1 below (a is non-null → a is "smaller" → -1 means "less"). */
    return (a == b) ? 0 : (a ? -1 : 1);
  }
  const int a_zero = (a->nlimbs == 0);
  const int b_zero = (b->nlimbs == 0);
  if (a_zero && b_zero) {
    return 0;
  }
  if ((a->sign < 0) != (b->sign < 0)) {
    return (a->sign < 0) ? -1 : 1;
  }
  const int mag = bigint_cmp_mag(a, b);
  return (a->sign < 0) ? -mag : mag;
}

asn1_status_t asn1_bigint_to_int64(const asn1_bigint_t* b, int64_t* out) {
  if (!b || !out) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (b->nlimbs == 0) {
    *out = 0;
    return ASN1_OK;
  }
  if (b->nlimbs > 2u) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  uint64_t mag = b->limbs[0];
  if (b->nlimbs == 2u) {
    mag |= (uint64_t)b->limbs[1] << 32;
  }
  if (b->sign >= 0) {
    if (mag > (uint64_t)INT64_MAX) {
      return ASN1_ERR_LENGTH_OVERFLOW;
    }
    *out = (int64_t)mag;
    return ASN1_OK;
  }
  /* Negative: INT64_MIN has magnitude INT64_MAX+1. */
  const uint64_t min_mag = (uint64_t)INT64_MAX + 1u;
  if (mag > min_mag) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  if (mag == min_mag) {
    *out = INT64_MIN;
    return ASN1_OK;
  }
  *out = -(int64_t)mag;
  return ASN1_OK;
}

/* --------------------------------------------------------------------- */
/* Serialization                                                          */
/* --------------------------------------------------------------------- */

asn1_status_t asn1_bigint_to_bytes(const asn1_bigint_t* b,
                                    uint8_t** out_buf, size_t* out_size) {
  if (!b || !out_buf || !out_size) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (b->sign < 0) {
    /* Mirrors asn1::BigInteger::to_unsigned_bytes: refuse negative. */
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  if (bigint_magnitude_be(b, out_buf, out_size) != 0) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  return ASN1_OK;
}