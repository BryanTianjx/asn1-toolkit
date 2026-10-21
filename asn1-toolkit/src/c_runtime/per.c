/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/per.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) Packed Encoding Rules (X.691) primitive bodies.

    Direct port of asn1-toolkit/src/runtime/per/primitives.cpp
    (namespace asn1::per). The C runtime stays a parallel implementation
    rather than a wrapper around the C++ one: the C ABI rejects C++
    Result<T> / std::optional, and the byte layout must mirror the C++
    runtime exactly so emit_c's generated stubs produce byte-equal
    output for the same ASN.1 input.

    Sub-task 2 surface (matches per.h declarations):

      asn1_per_encode_bool / decode_bool
      asn1_per_encode_int  / decode_int  — constrained whole number
      asn1_per_encode_unbounded_int / decode_unbounded_int
      asn1_per_encode_size / decode_size — unconstrained length
      asn1_per_encode_constrained_size / decode_constrained_size
      asn1_per_encode_bitstring / decode_bitstring
      asn1_per_encode_octetstring / decode_octetstring

    Sub-task 5+ deferred (header TODO list): UTF8String / BMPString
    text primitives, open type (X.691 12.3 / 25), presence bitmap,
    extension-bit, normally-small helpers.

    Why the C port skips Variant::Aligned differentiation in sub-task 2:
    the per.h surface for ints/sizes/bitstring/octetstring doesn't yet
    take a variant arg (the C header deliberately exposes the
    byte-equivalent primitives only — APER variants where alignment
    matters live behind an aper_-prefixed shim, deferred to a later
    sub-task). UPER encodes unaligned per X.691 11.5; APER's
    differences kick in for the length determinant + open-type +
    alignment-sensitive integer range cases. Per.h's constrained int
    here uses unaligned encoding for range>2 as well (the sub-task 3
    emitter selects aper_-prefixed variants when alignment matters);
    this matches the C++ runtime's Variant::Unaligned default.

    Specification: ITU-T X.691 — Packed Encoding Rules (PER)
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/per.h
***************************************************************************/

#include <asn1/c_runtime/per.h>
#include <asn1/c_runtime/bit_io.h>

#include <stdint.h>
#include <stdlib.h>
#include "alloc_internal.h"
#include <string.h>

/* --------------------------------------------------------------------- */
/* Internal helpers (private; mirrors src/runtime/per/primitives.cpp)     */
/* --------------------------------------------------------------------- */

static uint64_t range_size_i64(int64_t lower, int64_t upper) {
  const uint64_t u_lo = (uint64_t)lower;
  const uint64_t u_hi = (uint64_t)upper;
  const uint64_t diff = u_hi - u_lo;
  if (diff == UINT64_MAX) {
    return 0;  /* lower..upper spans the full int64 domain; +1 overflows */
  }
  return diff + 1u;
}

static uint64_t mask_bits(size_t n) {
  return n >= 64 ? ~(uint64_t)0 : (((uint64_t)1 << n) - 1u);
}

/* Bits needed to hold values 0..range-1 (X.691 12.2.2). */
static size_t bits_for_range(uint64_t range) {
  if (range <= 1u) {
    return 0;
  }
  size_t bits = 0;
  uint64_t x = range - 1u;
  while (x > 0) {
    ++bits;
    x >>= 1;
  }
  return bits;
}

/* X.691 length determinant encoder (10.1, 12.2.6). */
static asn1_status_t encode_length_determinant(asn1_bit_writer_t* out,
                                                 size_t length) {
  if (length < 128u) {
    return asn1_bit_writer_put_octet(out, (uint8_t)length);
  }
  if (length < 16384u) {
    asn1_status_t s = asn1_bit_writer_put_octet(
        out, (uint8_t)(0x80u | ((length >> 8) & 0x3Fu)));
    if (s != ASN1_OK) {
      return s;
    }
    return asn1_bit_writer_put_octet(out, (uint8_t)(length & 0xFFu));
  }
  /* Fragmentation (X.691 12.2.6): signal chunk size for >16K contents. */
  uint8_t code;
  if (length >= 65536u) {
    code = 0xC4;
  } else if (length >= 49152u) {
    code = 0xC3;
  } else if (length >= 32768u) {
    code = 0xC2;
  } else {
    code = 0xC1;
  }
  return asn1_bit_writer_put_octet(out, code);
}

/* X.691 length determinant decoder. */
static asn1_status_t decode_length_determinant(asn1_bit_reader_t* in,
                                                 size_t* out_length) {
  uint8_t first = 0;
  asn1_status_t s = asn1_bit_reader_get_octet(in, &first);
  if (s != ASN1_OK) {
    return s;
  }
  if ((first & 0x80u) == 0u) {
    *out_length = (size_t)first;
    return ASN1_OK;
  }
  if ((first & 0xC0u) == 0x80u) {
    uint8_t second = 0;
    s = asn1_bit_reader_get_octet(in, &second);
    if (s != ASN1_OK) {
      return s;
    }
    *out_length = ((size_t)(first & 0x3Fu) << 8) | (size_t)second;
    return ASN1_OK;
  }
  switch (first) {
    case 0xC1: *out_length = 16384u; return ASN1_OK;
    case 0xC2: *out_length = 32768u; return ASN1_OK;
    case 0xC3: *out_length = 49152u; return ASN1_OK;
    case 0xC4: *out_length = 65536u; return ASN1_OK;
    default:   return ASN1_ERR_INVALID_ARGUMENT;
  }
}

/* --------------------------------------------------------------------- */
/* Bool                                                                   */
/* --------------------------------------------------------------------- */

asn1_status_t asn1_per_encode_bool(asn1_bit_writer_t* w, int value) {
  if (!w) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  return asn1_bit_writer_put_bit(w, value ? 1 : 0);
}

asn1_status_t asn1_per_decode_bool(asn1_bit_reader_t* r, int* out_value) {
  return asn1_bit_reader_get_bit(r, out_value);
}

/* --------------------------------------------------------------------- */
/* Constrained whole number (X.691 12.2.2) — int range                   */
/* --------------------------------------------------------------------- */

asn1_status_t asn1_per_encode_int(asn1_bit_writer_t* w,
                                   const asn1_per_int_range_t* range,
                                   int64_t value) {
  if (!w || !range) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (range->upper_set && value > range->upper) {
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  if (range->lower_set && value < range->lower) {
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  const int64_t lower = range->lower_set ? range->lower : 0;
  const int64_t upper = range->upper_set ? range->upper : value;
  if (upper < lower) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const uint64_t range_size = range_size_i64(lower, upper);
  if (range_size == 0) {
    return ASN1_ERR_UNSUPPORTED;
  }
  if (range_size == 1u) {
    return ASN1_OK;
  }
  const uint64_t offset = (uint64_t)value - (uint64_t)lower;
  /* Per.h is unaligned for sub-task 2; aper_-prefixed variants land
     in a later sub-task with the alignment-sensitive UPER/APER split
     (X.691 11.5.6 vs 11.5.7). */
  return asn1_bit_writer_put_bits(w, offset, bits_for_range(range_size));
}

asn1_status_t asn1_per_decode_int(asn1_bit_reader_t* r,
                                   const asn1_per_int_range_t* range,
                                   int64_t* out_value) {
  if (!r || !range || !out_value) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const int64_t lower = range->lower_set ? range->lower : 0;
  const int64_t upper = range->upper_set ? range->upper : 0;
  if (range->lower_set && !range->upper_set) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (upper < lower) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const uint64_t range_size = range_size_i64(lower, upper);
  if (range_size == 0) {
    return ASN1_ERR_UNSUPPORTED;
  }
  if (range_size == 1u) {
    *out_value = lower;
    return ASN1_OK;
  }
  uint64_t offset = 0;
  asn1_status_t s = asn1_bit_reader_get_bits(r, bits_for_range(range_size), &offset);
  if (s != ASN1_OK) {
    return s;
  }
  if (offset >= range_size) {
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  *out_value = (int64_t)((uint64_t)lower + offset);
  return ASN1_OK;
}

/* --------------------------------------------------------------------- */
/* Unbounded whole number (X.691 12.2.4) — BigInteger on the wire        */
/* --------------------------------------------------------------------- */

asn1_status_t asn1_per_encode_unbounded_int(asn1_bit_writer_t* w,
                                              const asn1_bigint_t* value) {
  if (!w || !value) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  /* BigInteger magnitude → big-endian two's-complement bytes (X.680
     INTEGER value form). For now we emit the minimal positive-magnitude
     form (zero → {0x00}). Negative support lands with the BigInteger
     helpers (sub-task 3+ wires two's-complement; for sub-task 2 we
     accept non-negative only). */
  if (value->sign < 0) {
    return ASN1_ERR_UNSUPPORTED;
  }
  uint8_t* bytes = NULL;
  size_t nbytes = 0;
  asn1_status_t s = asn1_bigint_to_bytes(value, &bytes, &nbytes);
  if (s != ASN1_OK) {
    return s;
  }
  s = encode_length_determinant(w, nbytes);
  if (s != ASN1_OK) {
    free(bytes);
    return s;
  }
  s = asn1_bit_writer_put_octets(w, asn1_byte_view(bytes, nbytes));
  free(bytes);
  return s;
}

asn1_status_t asn1_per_decode_unbounded_int(asn1_bit_reader_t* r,
                                              asn1_bigint_t* out_value) {
  if (!r || !out_value) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  size_t len = 0;
  asn1_status_t s = decode_length_determinant(r, &len);
  if (s != ASN1_OK) {
    return s;
  }
  if (len > (size_t)ASN1_MAX_STRING_BYTES) {
    return ASN1_ERR_RESOURCE_LIMIT;
  }
  uint8_t* bytes = NULL;
  size_t n = 0;
  s = asn1_bit_reader_get_octets(r, len, &bytes, &n);
  if (s != ASN1_OK) {
    return s;
  }
  /* Positive-magnitude BigInteger decode path: re-use the magnitude
     byte form to construct the limbs. The C BigInteger doesn't yet
     have a public from_unsigned_bytes helper, so we synthesize the
     limbs directly here. */
  asn1_bigint_t tmp;
  asn1_bigint_init(&tmp);
  if (n == 0) {
    tmp.sign = 0;
    free(bytes);
    *out_value = tmp;
    return ASN1_OK;
  }
  size_t start = 0;
  while (start + 1u < n && bytes[start] == 0u) {
    ++start;
  }
  for (size_t i = start; i < n; ++i) {
    /* bigint_mul_add_u32 is private; replicate it inline. */
    uint64_t carry = (uint64_t)bytes[i];
    for (size_t k = 0; k < tmp.nlimbs; ++k) {
      const uint64_t prod = (uint64_t)tmp.limbs[k] * 256u + carry;
      tmp.limbs[k] = (uint32_t)(prod & 0xFFFFFFFFu);
      carry = prod >> 32;
    }
    while (carry != 0) {
      if (tmp.nlimbs == tmp.cap) {
        size_t new_cap = tmp.cap == 0 ? 4u : tmp.cap * 2u;
        if (tmp.nlimbs >= ASN1_MAX_BIGINT_LIMBS) {
          free(bytes); asn1_bigint_free(&tmp); return ASN1_ERR_RESOURCE_LIMIT;
        }
        if (new_cap > ASN1_MAX_BIGINT_LIMBS) new_cap = ASN1_MAX_BIGINT_LIMBS;
        if (new_cap > SIZE_MAX / sizeof(uint32_t)) {
          free(bytes); asn1_bigint_free(&tmp); return ASN1_ERR_LENGTH_OVERFLOW;
        }
        uint32_t* nl = (uint32_t*)realloc(tmp.limbs,
                                            new_cap * sizeof(uint32_t));
        if (!nl) {
          free(bytes);
          asn1_bigint_free(&tmp);
          return ASN1_ERR_OUT_OF_MEMORY;
        }
        tmp.limbs = nl;
        tmp.cap = new_cap;
      }
      tmp.limbs[tmp.nlimbs++] = (uint32_t)carry;
      carry = 0;
    }
  }
  tmp.sign = tmp.nlimbs > 0 ? +1 : 0;
  free(bytes);
  *out_value = tmp;
  return ASN1_OK;
}

/* --------------------------------------------------------------------- */
/* Unconstrained length (X.691 12.2.6 — unbounded size)                  */
/* --------------------------------------------------------------------- */

asn1_status_t asn1_per_encode_size(asn1_bit_writer_t* w,
                                    const asn1_per_size_range_t* range,
                                    size_t value) {
  if (!w || !range) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  (void)range;  /* unconstrained-length has no upper cap on this side */
  return encode_length_determinant(w, value);
}

asn1_status_t asn1_per_decode_size(asn1_bit_reader_t* r,
                                    const asn1_per_size_range_t* range,
                                    size_t* out_value) {
  if (!r || !range || !out_value) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  (void)range;
  return decode_length_determinant(r, out_value);
}

/* --------------------------------------------------------------------- */
/* Constrained size (X.691 12.2.6 + 12.2.7)                              */
/* --------------------------------------------------------------------- */

asn1_status_t asn1_per_encode_constrained_size(asn1_bit_writer_t* w,
                                                const asn1_per_size_range_t* range,
                                                size_t value) {
  if (!w || !range) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (value < range->lower) {
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  if (value > range->upper) {
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  if (range->upper < range->lower) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const uint64_t range_size = (uint64_t)range->upper - (uint64_t)range->lower + 1u;
  if (range_size == 1u) {
    return ASN1_OK;
  }
  const uint64_t offset = (uint64_t)value - (uint64_t)range->lower;
  return asn1_bit_writer_put_bits(w, offset, bits_for_range(range_size));
}

asn1_status_t asn1_per_decode_constrained_size(asn1_bit_reader_t* r,
                                                const asn1_per_size_range_t* range,
                                                size_t* out_value) {
  if (!r || !range || !out_value) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (range->upper < range->lower) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const uint64_t range_size = (uint64_t)range->upper - (uint64_t)range->lower + 1u;
  if (range_size == 1u) {
    *out_value = range->lower;
    return ASN1_OK;
  }
  uint64_t offset = 0;
  asn1_status_t s = asn1_bit_reader_get_bits(r, bits_for_range(range_size), &offset);
  if (s != ASN1_OK) {
    return s;
  }
  if (offset >= range_size) {
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  *out_value = (size_t)((uint64_t)range->lower + offset);
  return ASN1_OK;
}

/* --------------------------------------------------------------------- */
/* BIT STRING (X.691 16) — fixed-length by `range` upper                  */
/* --------------------------------------------------------------------- */

asn1_status_t asn1_per_encode_bitstring(asn1_bit_writer_t* w,
                                          const asn1_per_size_range_t* range,
                                          const asn1_bitstring_t* value) {
  if (!w || !range || !value) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (value->bit_length < range->lower) {
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  if (value->bit_length > range->upper) {
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  /* Constrained-length BIT STRING with a fully-known size is just
     raw bits. NamedBitList trailing-zero removal lives with the
     emitter. */
  const size_t nbits = value->bit_length;
  const size_t whole = nbits / 8u;
  asn1_status_t s = asn1_bit_writer_put_bits_from_bytes(
      w, asn1_byte_view(value->bits, whole), whole * 8u);
  if (s != ASN1_OK) {
    return s;
  }
  const size_t tail = nbits - whole * 8u;
  if (tail > 0u) {
    if (whole >= value->cap_bytes) {
      return ASN1_ERR_INVALID_ARGUMENT;
    }
    s = asn1_bit_writer_put_bits_from_bytes(
        w, asn1_byte_view(value->bits + whole, 1u), tail);
    if (s != ASN1_OK) {
      return s;
    }
  }
  return ASN1_OK;
}

asn1_status_t asn1_per_decode_bitstring(asn1_bit_reader_t* r,
                                          const asn1_per_size_range_t* range,
                                          asn1_bitstring_t* out_value) {
  if (!r || !range || !out_value) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  asn1_bitstring_t tmp;
  asn1_bitstring_init(&tmp);
  const size_t nbits = range->upper;
  asn1_status_t s = asn1_bitstring_resize(&tmp, nbits);
  if (s != ASN1_OK) {
    return s;
  }
  if (nbits == 0u) {
    asn1_bitstring_free(out_value);
    *out_value = tmp;
    return ASN1_OK;
  }
  const size_t whole = nbits / 8u;
  uint8_t* bytes = NULL;
  size_t n = 0;
  s = asn1_bit_reader_get_bits_as_bytes(r, nbits, &bytes, &n);
  if (s != ASN1_OK) {
    asn1_bitstring_free(&tmp);
    return s;
  }
  /* Bytes are MSB-first within each octet, matching the asn1_bitstring_t
     layout. */
  if (whole > 0) {
    memcpy(tmp.bits, bytes, whole);
  }
  if (nbits - whole * 8u > 0u && whole < n) {
    /* Tail goes into the next byte at the top `tail` bits. */
    tmp.bits[whole] = (uint8_t)(bytes[whole]
                                         & (uint8_t)(mask_bits(nbits - whole * 8u)
                                                      << (8u - (nbits - whole * 8u))));
  }
  free(bytes);
  asn1_bitstring_free(out_value);
  *out_value = tmp;
  return ASN1_OK;
}

/* --------------------------------------------------------------------- */
/* OCTET STRING (X.691 17) — length-prefixed raw bytes                   */
/* --------------------------------------------------------------------- */

asn1_status_t asn1_per_encode_octetstring(asn1_bit_writer_t* w,
                                            asn1_byte_view_t value) {
  if (!w) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  asn1_status_t s = encode_length_determinant(w, value.size);
  if (s != ASN1_OK) {
    return s;
  }
  return asn1_bit_writer_put_octets(w, value);
}

asn1_status_t asn1_per_decode_octetstring(asn1_bit_reader_t* r,
                                            size_t length,
                                            uint8_t** out_buf,
                                            size_t* out_size) {
  if (!r || !out_buf || !out_size) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (length > (size_t)ASN1_MAX_STRING_BYTES) {
    return ASN1_ERR_RESOURCE_LIMIT;
  }
  return asn1_bit_reader_get_octets(r, length, out_buf, out_size);
}