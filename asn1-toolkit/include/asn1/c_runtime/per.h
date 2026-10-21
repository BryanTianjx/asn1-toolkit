/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/per.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) Packed Encoding Rules (X.691) entry points.

    One C11 core parameterized by Unaligned/Aligned. Existing entry points
    wrap UPER; _ex functions explicitly select a variant and constraint.
    This primitive API does not imply that the C emitter or all ASN.1
    types are implemented. See docs/c-runtime-contracts.md and
    docs/c-runtime-test-coverage.md for supported paths and verification.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_PER_H
#define ASN1_C_PER_H

#include <asn1/c_runtime/asn1_c_common.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/bit_io.h>
#include <asn1/c_runtime/byte_io.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Value-side helpers (consumed by emit_c) ------------------------- */

/* Constrained whole number (X.691 11.5 / 13): the runtime needs the
   inclusive range so it can choose between unconstrained (variable-
   length), constrained (fixed-width), or extension-bit-prefixed
   encodings. */
typedef struct {
    int64_t lower;     /* meaningful only when lower_set != 0 */
    int      lower_set;
    int64_t upper;     /* meaningful only when upper_set != 0 */
    int      upper_set;
    int      extensible;
} asn1_per_int_range_t;

/* Legacy inclusive SIZE bounds, not normally-small length. encode_size/
   decode_size alone historically ignore these bounds and handle only one
   final general determinant; constrained_size/bitstring interpret bounds. */
typedef struct {
    size_t lower;
    size_t upper;
} asn1_per_size_range_t;

/* One shared core for both variants. Legacy functions below wrap UPER. */
typedef enum { ASN1_PER_UNALIGNED = 0, ASN1_PER_ALIGNED = 1 } asn1_per_variant_t;
typedef enum {
  ASN1_PER_LENGTH_UNCONSTRAINED = 0,
  ASN1_PER_LENGTH_CONSTRAINED = 1,
  ASN1_PER_LENGTH_SEMI_CONSTRAINED = 2
} asn1_per_length_kind_t;
/* Fixed size is CONSTRAINED with lower==upper. Bounds are actual lengths,
   not offsets. upper is ignored for semi/unconstrained. */
typedef struct {
  asn1_per_length_kind_t kind;
  size_t lower, upper;
  int extensible;
} asn1_per_length_constraint_t;

/* A single determinant; more!=0 requires 16K/32K/48K/64K units.
   Caller interleaves determinants with payload and writes a final determinant
   (including zero after an exact fragment). No whole-value length ambiguity. */
asn1_status_t asn1_per_encode_length_fragment(asn1_bit_writer_t*, asn1_per_variant_t, size_t, int more);
asn1_status_t asn1_per_decode_length_fragment(asn1_bit_reader_t*, asn1_per_variant_t, size_t*, int* more);
/* Scalar length helper cannot represent a fragmented payload: >=16K general
   lengths return Unsupported; use the whole string/open-type APIs instead. */
asn1_status_t asn1_per_encode_size_ex(asn1_bit_writer_t*, asn1_per_variant_t, const asn1_per_length_constraint_t*, size_t);
asn1_status_t asn1_per_decode_size_ex(asn1_bit_reader_t*, asn1_per_variant_t, const asn1_per_length_constraint_t*, size_t*);
asn1_status_t asn1_per_encode_int_ex(asn1_bit_writer_t*, asn1_per_variant_t, const asn1_per_int_range_t*, int64_t);
asn1_status_t asn1_per_decode_int_ex(asn1_bit_reader_t*, asn1_per_variant_t, const asn1_per_int_range_t*, int64_t*);
asn1_status_t asn1_per_encode_unbounded_int_ex(asn1_bit_writer_t*, asn1_per_variant_t, const asn1_bigint_t*);
asn1_status_t asn1_per_decode_unbounded_int_ex(asn1_bit_reader_t*, asn1_per_variant_t, asn1_bigint_t*);
asn1_status_t asn1_per_encode_bitstring_ex(asn1_bit_writer_t*, asn1_per_variant_t, const asn1_per_length_constraint_t*, const asn1_bitstring_t*);
asn1_status_t asn1_per_decode_bitstring_ex(asn1_bit_reader_t*, asn1_per_variant_t, const asn1_per_length_constraint_t*, asn1_bitstring_t*);
asn1_status_t asn1_per_encode_octetstring_ex(asn1_bit_writer_t*, asn1_per_variant_t, const asn1_per_length_constraint_t*, asn1_byte_view_t);
asn1_status_t asn1_per_decode_octetstring_ex(asn1_bit_reader_t*, asn1_per_variant_t, const asn1_per_length_constraint_t*, uint8_t**, size_t*);
/* Open-type content is already complete-encoded octets. Both variants use
   the same length/chunk core; APER aligns. Caller pads inner encodings,
   including one zero octet for an otherwise empty inner encoding.
   Empty content is rejected rather than silently emitting an invalid open type. */
asn1_status_t asn1_per_encode_open_type(asn1_bit_writer_t*, asn1_per_variant_t, asn1_byte_view_t);
asn1_status_t asn1_per_decode_open_type(asn1_bit_reader_t*, asn1_per_variant_t, uint8_t**, size_t*);

/* Legacy UPER primitive wrappers. Per-type C generated functions remain
   a separate codegen task; these declarations are implemented now. */
asn1_status_t asn1_per_encode_bool(asn1_bit_writer_t* w, int value);
asn1_status_t asn1_per_decode_bool(asn1_bit_reader_t* r, int* out_value);

asn1_status_t asn1_per_encode_int(asn1_bit_writer_t* w,
                                 const asn1_per_int_range_t* range,
                                 int64_t value);
asn1_status_t asn1_per_decode_int(asn1_bit_reader_t* r,
                                 const asn1_per_int_range_t* range,
                                 int64_t* out_value);

asn1_status_t asn1_per_encode_unbounded_int(asn1_bit_writer_t* w,
                                            const asn1_bigint_t* value);
/* Constructor output: new/zero/freed BigInt only; failure leaves output
   unchanged but may advance r. Uses signed minimal two's complement. */
asn1_status_t asn1_per_decode_unbounded_int(asn1_bit_reader_t* r,
                                            asn1_bigint_t* out_value);

asn1_status_t asn1_per_encode_size(asn1_bit_writer_t* w,
                                   const asn1_per_size_range_t* range,
                                   size_t value);
asn1_status_t asn1_per_decode_size(asn1_bit_reader_t* r,
                                   const asn1_per_size_range_t* range,
                                   size_t* out_value);

/* Constrained size determinant (X.691 11.9). */
asn1_status_t asn1_per_encode_constrained_size(asn1_bit_writer_t* w,
                                               const asn1_per_size_range_t* range,
                                               size_t value);
asn1_status_t asn1_per_decode_constrained_size(asn1_bit_reader_t* r,
                                               const asn1_per_size_range_t* range,
                                               size_t* out_value);

/* Bit-string field (X.691 16). For NamedBitList types, the trailing-
   zero-removal rule (16.2/16.3) is enforced by the caller. */
asn1_status_t asn1_per_encode_bitstring(asn1_bit_writer_t* w,
                                       const asn1_per_size_range_t* range,
                                       const asn1_bitstring_t* value);
/* out_value must be initialized. Success replaces its owned storage;
   failure leaves it unchanged, though r may advance. */
asn1_status_t asn1_per_decode_bitstring(asn1_bit_reader_t* r,
                                       const asn1_per_size_range_t* range,
                                       asn1_bitstring_t* out_value);

/* Octet string (X.691 17). The length determinant is one of the size
   encoders above. */
asn1_status_t asn1_per_encode_octetstring(asn1_bit_writer_t* w,
                                         asn1_byte_view_t value);
/* Legacy raw payload reader: length is already known, no determinant is
   consumed. Use decode_octetstring_ex for a complete (fragmented) value. */
asn1_status_t asn1_per_decode_octetstring(asn1_bit_reader_t* r,
                                         size_t length,
                                         uint8_t** out_buf,
                                         size_t* out_size);

/* Future work: UTF8String / BMPString / etc. text primitives,
   the presence bitmap for OPTIONAL /
   DEFAULT fields, and the extension-bit write/read at the top of an
   extensible SEQUENCE/CHOICE. */

/* ----- UPER / APER entry points (per codec) ---------------------------- */
/* Future C emitter supplies per-type entry points in generated headers,
   rather than declaring nonexistent fixture-specific functions here. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_PER_H */
