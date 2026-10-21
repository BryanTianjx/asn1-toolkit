/* Shared C11 UPER/APER core. X.691 (2021) clauses 11, 13, 16, 17.
 * No compiler/IR or C++ runtime dependency. Legacy wrappers are in per.c. */
#include <asn1/c_runtime/per.h>
#include <stdlib.h>
#include <string.h>
#include "alloc_internal.h"

#define TRY(expr) do { asn1_status_t status_ = (expr); if (status_ != ASN1_OK) return status_; } while (0)
static int valid_variant(asn1_per_variant_t v) {
  return v == ASN1_PER_UNALIGNED || v == ASN1_PER_ALIGNED;
}
static size_t width(uint64_t max) {
  size_t bits = 0;
  while (max) { ++bits; max >>= 1; }
  return bits;
}
static size_t octets(uint64_t value) { size_t n = (width(value) + 7u) / 8u; return n ? n : 1u; }
static asn1_status_t align_write(asn1_bit_writer_t* w, asn1_per_variant_t v) {
  return v == ASN1_PER_ALIGNED ? asn1_bit_writer_align_to_octet(w) : ASN1_OK;
}
static asn1_status_t align_read(asn1_bit_reader_t* r, asn1_per_variant_t v) {
  return v == ASN1_PER_ALIGNED ? asn1_bit_reader_align_to_octet(r) : ASN1_OK;
}
static int64_t signed_bits(uint64_t value) {
  return value <= (uint64_t)INT64_MAX ? (int64_t)value
                                    : -1 - (int64_t)(UINT64_MAX - value);
}

/* max is the largest offset, not cardinality: UINT64_MAX covers 2^64 values. */
static asn1_status_t put_offset(asn1_bit_writer_t* w, asn1_per_variant_t v, uint64_t max, uint64_t value) {
  if (value > max) return ASN1_ERR_CONSTRAINT_VIOLATION;
  if (!max) return ASN1_OK;
  if (v == ASN1_PER_UNALIGNED || max < 255u)
    return asn1_bit_writer_put_bits(w, value, width(max));
  if (max <= 65535u) {
    TRY(align_write(w, v));
    return asn1_bit_writer_put_bits(w, value, max == 255u ? 8u : 16u);
  }
  size_t used = octets(value), capacity = octets(max);
  TRY(put_offset(w, v, capacity - 1u, used - 1u));
  TRY(align_write(w, v));
  return asn1_bit_writer_put_bits(w, value, used * 8u);
}
static asn1_status_t get_offset(asn1_bit_reader_t* r, asn1_per_variant_t v, uint64_t max, uint64_t* out) {
  uint64_t value = 0;
  if (!max) { *out = 0; return ASN1_OK; }
  if (v == ASN1_PER_UNALIGNED || max < 255u) {
    TRY(asn1_bit_reader_get_bits(r, width(max), &value));
  } else if (max <= 65535u) {
    TRY(align_read(r, v));
    TRY(asn1_bit_reader_get_bits(r, max == 255u ? 8u : 16u, &value));
  } else {
    uint64_t used_m1 = 0;
    TRY(get_offset(r, v, octets(max) - 1u, &used_m1));
    TRY(align_read(r, v));
    TRY(asn1_bit_reader_get_bits(r, (size_t)(used_m1 + 1u) * 8u, &value));
  }
  if (value > max) return ASN1_ERR_CONSTRAINT_VIOLATION;
  *out = value;
  return ASN1_OK;
}

asn1_status_t asn1_per_encode_length_fragment(asn1_bit_writer_t* w, asn1_per_variant_t v, size_t n, int more) {
  if (!w || !valid_variant(v) || (more != 0 && more != 1)) return ASN1_ERR_INVALID_ARGUMENT;
  if ((more && (n < 16384u || n > 65536u || n % 16384u)) || (!more && n >= 16384u))
    return ASN1_ERR_INVALID_ARGUMENT;
  TRY(align_write(w, v));
  if (more) return asn1_bit_writer_put_octet(w, (uint8_t)(0xC0u | n / 16384u));
  if (n < 128u) return asn1_bit_writer_put_octet(w, (uint8_t)n);
  TRY(asn1_bit_writer_put_octet(w, (uint8_t)(0x80u | (n >> 8))));
  return asn1_bit_writer_put_octet(w, (uint8_t)n);
}
asn1_status_t asn1_per_decode_length_fragment(asn1_bit_reader_t* r, asn1_per_variant_t v, size_t* out, int* more) {
  if (!r || !out || !more || !valid_variant(v)) return ASN1_ERR_INVALID_ARGUMENT;
  TRY(align_read(r, v));
  uint8_t first = 0, second = 0;
  TRY(asn1_bit_reader_get_octet(r, &first));
  size_t n; int continues = 0;
  if (first < 128u) n = first;
  else if (first < 192u) {
    TRY(asn1_bit_reader_get_octet(r, &second));
    n = ((size_t)(first & 63u) << 8) | second;
    if (n < 128u) return ASN1_ERR_NON_CANONICAL;
  } else {
    if (first < 0xC1u || first > 0xC4u) return ASN1_ERR_INVALID_ARGUMENT;
    n = (size_t)(first & 63u) * 16384u; continues = 1;
  }
  *out = n; *more = continues;
  return ASN1_OK;
}

static int valid_size(const asn1_per_length_constraint_t* c) {
  if (!c || (c->extensible != 0 && c->extensible != 1)) return 0;
  if (c->kind == ASN1_PER_LENGTH_CONSTRAINED) return c->lower <= c->upper;
  return c->kind == ASN1_PER_LENGTH_UNCONSTRAINED || c->kind == ASN1_PER_LENGTH_SEMI_CONSTRAINED;
}
static int in_size(const asn1_per_length_constraint_t* c, size_t n) {
  if (c->kind == ASN1_PER_LENGTH_UNCONSTRAINED) return 1;
  return n >= c->lower && (c->kind != ASN1_PER_LENGTH_CONSTRAINED || n <= c->upper);
}
static int bounded_size(const asn1_per_length_constraint_t* c, int extension) {
  return !extension && c->kind == ASN1_PER_LENGTH_CONSTRAINED && c->upper < 65536u;
}
asn1_status_t asn1_per_encode_size_ex(asn1_bit_writer_t* w, asn1_per_variant_t v,
                                    const asn1_per_length_constraint_t* c, size_t n) {
  if (!w || !valid_variant(v) || !valid_size(c)) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = !in_size(c, n);
  if (ext && !c->extensible) return ASN1_ERR_CONSTRAINT_VIOLATION;
  int bounded = bounded_size(c, ext);
  if (!bounded && n >= 16384u) return ASN1_ERR_UNSUPPORTED;
  if (c->extensible) TRY(asn1_bit_writer_put_bit(w, ext));
  return bounded ? put_offset(w, v, c->upper - c->lower, n - c->lower)
                 : asn1_per_encode_length_fragment(w, v, n, 0);
}
asn1_status_t asn1_per_decode_size_ex(asn1_bit_reader_t* r, asn1_per_variant_t v,
                                    const asn1_per_length_constraint_t* c, size_t* out) {
  if (!r || !out || !valid_variant(v) || !valid_size(c)) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = 0; size_t n;
  if (c->extensible) TRY(asn1_bit_reader_get_bit(r, &ext));
  if (bounded_size(c, ext)) {
    uint64_t offset = 0;
    TRY(get_offset(r, v, c->upper - c->lower, &offset));
    n = c->lower + (size_t)offset;
  } else {
    int more = 0;
    TRY(asn1_per_decode_length_fragment(r, v, &n, &more));
    if (more) return ASN1_ERR_UNSUPPORTED;
  }
  if ((!ext && !in_size(c, n)) || (ext && in_size(c, n))) return ASN1_ERR_CONSTRAINT_VIOLATION;
  *out = n; return ASN1_OK;
}

/* Whole strings and open types share one chunk engine. For BIT STRING each
 * non-final chunk is octet-sized, so aggregate offsets remain byte-aligned. */
static asn1_status_t put_string(asn1_bit_writer_t* w, asn1_per_variant_t v,
                                const asn1_per_length_constraint_t* c,
                                asn1_byte_view_t data, size_t units, int bits) {
  if (!w || !valid_variant(v) || !valid_size(c) || (!data.data && data.size))
    return ASN1_ERR_INVALID_ARGUMENT;
  size_t need = bits ? units / 8u + (units % 8u != 0) : units;
  if (need > data.size) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = !in_size(c, units);
  if (ext && !c->extensible) return ASN1_ERR_CONSTRAINT_VIOLATION;
  if (c->extensible) TRY(asn1_bit_writer_put_bit(w, ext));
  if (bounded_size(c, ext)) {
    TRY(put_offset(w, v, c->upper - c->lower, units - c->lower));
    if (units && (c->lower != c->upper || units > (bits ? 16u : 2u))) TRY(align_write(w, v));
    return bits ? asn1_bit_writer_put_bits_from_bytes(w, data, units)
                : asn1_bit_writer_put_octets(w, asn1_byte_view(data.data, units));
  }
  size_t offset = 0, remaining = units;
  for (;;) {
    int more = remaining >= 16384u;
    size_t chunk = more ? (remaining >= 65536u ? 65536u : (remaining / 16384u) * 16384u) : remaining;
    TRY(asn1_per_encode_length_fragment(w, v, chunk, more));
    const uint8_t* p = chunk ? data.data + (bits ? offset / 8u : offset) : NULL;
    if (bits) TRY(asn1_bit_writer_put_bits_from_bytes(w, asn1_byte_view(p, chunk / 8u + (chunk % 8u != 0)), chunk));
    else TRY(asn1_bit_writer_put_octets(w, asn1_byte_view(p, chunk)));
    offset += chunk; remaining -= chunk;
    if (!more) return ASN1_OK;
  }
}
static asn1_status_t get_string(asn1_bit_reader_t* r, asn1_per_variant_t v,
                                const asn1_per_length_constraint_t* c,
                                uint8_t** out, size_t* out_units, int bits) {
  if (!r || !out || !out_units || !valid_variant(v) || !valid_size(c)) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = 0;
  if (c->extensible) TRY(asn1_bit_reader_get_bit(r, &ext));
  int bounded = bounded_size(c, ext);
  size_t fixed = 0;
  if (bounded) {
    uint64_t offset = 0;
    TRY(get_offset(r, v, c->upper - c->lower, &offset));
    fixed = c->lower + (size_t)offset;
    if (fixed && (c->lower != c->upper || fixed > (bits ? 16u : 2u))) TRY(align_read(r, v));
  }
  size_t total = 0;
  uint8_t* result = NULL;
  asn1_status_t s = ASN1_OK;
  do {
    size_t chunk = fixed; int more = 0;
    if (!bounded) {
      s = asn1_per_decode_length_fragment(r, v, &chunk, &more);
      if (s != ASN1_OK) break;
    }
    size_t limit = (size_t)ASN1_MAX_STRING_BYTES * (bits ? 8u : 1u);
    if (total > limit || chunk > limit - total) { s = ASN1_ERR_RESOURCE_LIMIT; break; }
    size_t n = bits ? chunk / 8u + (chunk % 8u != 0) : chunk;
    uint8_t* part = NULL; size_t read = 0;
    s = bits ? asn1_bit_reader_get_bits_as_bytes(r, chunk, &part, &read)
             : asn1_bit_reader_get_octets(r, chunk, &part, &read);
    if (s != ASN1_OK) break;
    size_t position = bits ? total / 8u : total;
    uint8_t* grown = (uint8_t*)realloc(result, position + n ? position + n : 1u);
    if (!grown) { free(part); s = ASN1_ERR_OUT_OF_MEMORY; break; }
    result = grown;
    if (n) memcpy(result + position, part, n);
    free(part); total += chunk;
    if (!more) break;
  } while (1);
  if (s == ASN1_OK && ((!ext && !in_size(c, total)) || (ext && in_size(c, total))))
    s = ASN1_ERR_CONSTRAINT_VIOLATION;
  if (s != ASN1_OK) { free(result); return s; }
  *out = result; *out_units = total;
  return ASN1_OK;
}
asn1_status_t asn1_per_encode_octetstring_ex(asn1_bit_writer_t* w, asn1_per_variant_t v, const asn1_per_length_constraint_t* c, asn1_byte_view_t data) {
  return put_string(w, v, c, data, data.size, 0);
}
asn1_status_t asn1_per_decode_octetstring_ex(asn1_bit_reader_t* r, asn1_per_variant_t v, const asn1_per_length_constraint_t* c, uint8_t** out, size_t* n) {
  return get_string(r, v, c, out, n, 0);
}
asn1_status_t asn1_per_encode_bitstring_ex(asn1_bit_writer_t* w, asn1_per_variant_t v, const asn1_per_length_constraint_t* c, const asn1_bitstring_t* data) {
  if (!data) return ASN1_ERR_INVALID_ARGUMENT;
  return put_string(w, v, c, asn1_byte_view(data->bits, data->cap_bytes), data->bit_length, 1);
}
asn1_status_t asn1_per_decode_bitstring_ex(asn1_bit_reader_t* r, asn1_per_variant_t v, const asn1_per_length_constraint_t* c, asn1_bitstring_t* out) {
  if (!out) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* data = NULL; size_t n = 0;
  TRY(get_string(r, v, c, &data, &n, 1));
  asn1_bitstring_free(out);
  out->bits = data; out->bit_length = n; out->cap_bytes = n / 8u + (n % 8u != 0);
  return ASN1_OK;
}
static const asn1_per_length_constraint_t unconstrained = {ASN1_PER_LENGTH_UNCONSTRAINED, 0, 0, 0};
asn1_status_t asn1_per_encode_open_type(asn1_bit_writer_t* w, asn1_per_variant_t v, asn1_byte_view_t data) {
  if (data.size == 0) return ASN1_ERR_INVALID_ARGUMENT;
  return asn1_per_encode_octetstring_ex(w, v, &unconstrained, data);
}
asn1_status_t asn1_per_decode_open_type(asn1_bit_reader_t* r, asn1_per_variant_t v, uint8_t** out, size_t* n) {
  if (!out || !n) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* bytes = NULL; size_t size = 0;
  TRY(asn1_per_decode_octetstring_ex(r, v, &unconstrained, &bytes, &size));
  if (!size) { free(bytes); return ASN1_ERR_INVALID_ARGUMENT; }
  *out = bytes; *n = size;
  return ASN1_OK;
}

asn1_status_t asn1_per_encode_unbounded_int_ex(asn1_bit_writer_t* w, asn1_per_variant_t v, const asn1_bigint_t* value) {
  if (!w || !value || !valid_variant(v)) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* data = NULL; size_t n = 0;
  TRY(asn1_bigint_to_twos_complement(value, &data, &n));
  asn1_status_t s = asn1_per_encode_open_type(w, v, asn1_byte_view(data, n));
  free(data); return s;
}
asn1_status_t asn1_per_decode_unbounded_int_ex(asn1_bit_reader_t* r, asn1_per_variant_t v, asn1_bigint_t* out) {
  if (!r || !out || !valid_variant(v)) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* data = NULL; size_t n = 0;
  TRY(asn1_per_decode_open_type(r, v, &data, &n));
  asn1_status_t s = asn1_bigint_init_from_twos_complement(out, asn1_byte_view(data, n));
  free(data); return s;
}
static int valid_int(const asn1_per_int_range_t* c) {
  return c && (!c->lower_set || !c->upper_set || c->lower <= c->upper) &&
         (c->extensible == 0 || c->extensible == 1);
}
static int in_int(const asn1_per_int_range_t* c, int64_t n) {
  return (!c->lower_set || n >= c->lower) && (!c->upper_set || n <= c->upper);
}
static asn1_status_t put_i64(asn1_bit_writer_t* w, asn1_per_variant_t v, int64_t n) {
  uint8_t data[8]; uint64_t value = (uint64_t)n;
  for (size_t i = 0; i < 8; ++i) data[7u - i] = (uint8_t)(value >> (i * 8u));
  size_t start = 0;
  while (start < 7u && ((data[start] == 0 && !(data[start+1] & 128u)) ||
                       (data[start] == 255u && (data[start+1] & 128u)))) ++start;
  return asn1_per_encode_open_type(w, v, asn1_byte_view(data + start, 8u - start));
}
static asn1_status_t get_i64(asn1_bit_reader_t* r, asn1_per_variant_t v, int64_t* out) {
  asn1_bigint_t tmp; asn1_bigint_init(&tmp);
  TRY(asn1_per_decode_unbounded_int_ex(r, v, &tmp));
  asn1_status_t s = asn1_bigint_to_int64(&tmp, out);
  asn1_bigint_free(&tmp); return s;
}
asn1_status_t asn1_per_encode_int_ex(asn1_bit_writer_t* w, asn1_per_variant_t v, const asn1_per_int_range_t* c, int64_t n) {
  if (!w || !valid_variant(v) || !valid_int(c)) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = !in_int(c, n);
  if (ext && !c->extensible) return ASN1_ERR_CONSTRAINT_VIOLATION;
  if (c->extensible) TRY(asn1_bit_writer_put_bit(w, ext));
  if (!ext && c->lower_set && c->upper_set)
    return put_offset(w, v, (uint64_t)c->upper - (uint64_t)c->lower, (uint64_t)n - (uint64_t)c->lower);
  if (!ext && c->lower_set) {
    uint64_t offset = (uint64_t)n - (uint64_t)c->lower;
    size_t used = octets(offset);
    TRY(asn1_per_encode_length_fragment(w, v, used, 0));
    return asn1_bit_writer_put_bits(w, offset, used * 8u);
  }
  return put_i64(w, v, n);
}
asn1_status_t asn1_per_decode_int_ex(asn1_bit_reader_t* r, asn1_per_variant_t v, const asn1_per_int_range_t* c, int64_t* out) {
  if (!r || !out || !valid_variant(v) || !valid_int(c)) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = 0; int64_t n;
  if (c->extensible) TRY(asn1_bit_reader_get_bit(r, &ext));
  if (!ext && c->lower_set && c->upper_set) {
    uint64_t offset = 0;
    TRY(get_offset(r, v, (uint64_t)c->upper - (uint64_t)c->lower, &offset));
    n = signed_bits((uint64_t)c->lower + offset);
  } else if (!ext && c->lower_set) {
    size_t used = 0; int more = 0;
    TRY(asn1_per_decode_length_fragment(r, v, &used, &more));
    if (more || used > 8) return ASN1_ERR_UNSUPPORTED;
    if (!used) return ASN1_ERR_INVALID_ARGUMENT;
    uint64_t offset = 0;
    TRY(asn1_bit_reader_get_bits(r, used * 8u, &offset));
    if (used > 1u && offset < (UINT64_C(1) << ((used - 1u) * 8u))) return ASN1_ERR_NON_CANONICAL;
    if (offset > (uint64_t)INT64_MAX - (uint64_t)c->lower) return ASN1_ERR_LENGTH_OVERFLOW;
    n = signed_bits((uint64_t)c->lower + offset);
  } else {
    TRY(get_i64(r, v, &n));
  }
  if ((!ext && !in_int(c, n)) || (ext && in_int(c, n))) return ASN1_ERR_CONSTRAINT_VIOLATION;
  *out = n; return ASN1_OK;
}
