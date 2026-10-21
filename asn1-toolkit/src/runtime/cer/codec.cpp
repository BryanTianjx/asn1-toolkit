/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/cer/codec.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
**
**  File Description:
**
**   CER codec implementation.
**
** Specification: ITU-T X.690 — Canonical Encoding Rules (CER).
**                 Clause 9 (CER restrictions) + clause 11 (restrictions
**                 common to CER and DER).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
**************************************************************************/
#include <asn1/runtime/cer/codec.hpp>
#include <asn1/runtime/der/codec.hpp>
#include <asn1/runtime/limits.hpp>

#include <algorithm>
#include <utility>

namespace asn1 {
namespace cer {
namespace {

/// Maximum contents octets of a primitive CER string (X.690 9.2).
constexpr std::size_t kFragmentContents = 1000;
/// Data octets per non-final BIT STRING fragment: a fragment's 1000 contents
/// octets include its own initial (unused-bits) octet (X.690 9.2 + 8.6.4).
constexpr std::size_t kBitStringFragmentData = kFragmentContents - 1;

Result<void> tag_ok(ByteReader& in, const ber::Tag& got, const ber::Tag& expected,
                    const char* what) {
  if (got.cls != expected.cls || got.number != expected.number) {
    return make_error(Error::Code::TagMismatch, in.offset(), what);
  }
  return Result<void>::success();
}

Result<void> require_indefinite(ByteReader& in, const ber::TlvHeader& hdr) {
  if (!hdr.length.indefinite) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "CER requires the indefinite length form for constructed encodings");
  }
  return Result<void>::success();
}

bool at_end_of_contents(ByteReader& in) {
  auto p0 = in.peek(0);
  auto p1 = in.peek(1);
  return p0 && p1 && p0.value() == 0x00 && p1.value() == 0x00;
}

// Validate the common CER identifier/length policy before content decoding.
Result<ber::TlvHeader> decode_header(ByteReader& in) {
  const auto raw = in.remaining_span();
  auto h = ber::decode_tlv_header(in);
  if (!h) return h.error();
  if (h.value().tag.constructed != h.value().length.indefinite) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "CER requires indefinite constructed and definite primitive lengths");
  }
  ByteWriter minimal;
  ber::encode_tag(minimal, h.value().tag);
  if (h.value().length.indefinite) minimal.put(0x80);
  else ber::encode_length(minimal, h.value().length.value);
  if (raw.size() < minimal.buffer().size() ||
      !std::equal(minimal.buffer().begin(), minimal.buffer().end(), raw.begin())) {
    return make_error(Error::Code::NonCanonical, in.offset(), "nonminimal CER identifier/length");
  }
  return h;
}

// Preserve complete nested TLVs with bounded recursion and bounded accumulation.
Result<void> copy_one_tlv(ByteReader& in, ByteWriter& out, unsigned depth = 0) {
  if (depth >= 64) return make_error(Error::Code::Unsupported, in.offset(), "CER nesting limit exceeded");
  const auto raw = in.remaining_span();
  const auto start = in.offset();
  auto h = decode_header(in);
  if (!h) return h.error();
  const auto header_size = in.offset() - start;
  if (out.buffer().size() > kMaxCodecBytes - header_size)
    return make_error(Error::Code::LengthOverflow, in.offset(), "CER content limit exceeded");
  out.write(raw.subspan(0, header_size));
  if (h.value().length.indefinite) {
    while (!at_end_of_contents(in)) {
      auto result = copy_one_tlv(in, out, depth + 1);
      if (!result) return result.error();
    }
    if (out.buffer().size() > kMaxCodecBytes - 2)
      return make_error(Error::Code::LengthOverflow, in.offset(), "CER content limit exceeded");
    auto eoc = in.read(2);
    if (!eoc) return eoc.error();
    out.write(eoc.value());
  } else {
    if (h.value().length.value > kMaxCodecBytes - out.buffer().size())
      return make_error(Error::Code::LengthOverflow, in.offset(), "CER content limit exceeded");
    auto content = in.read(h.value().length.value);
    if (!content) return content.error();
    out.write(content.value());
  }
  return Result<void>::success();
}

/// Length of the leading identifier octets of a complete TLV.
std::size_t tag_octet_count(Span<const std::uint8_t> tlv) {
  if (tlv.empty()) {
    return 0;
  }
  std::size_t n = 1;
  if ((tlv[0] & 0x1F) == 0x1F) {
    while (n < tlv.size() && (tlv[n] & 0x80) != 0) {
      ++n;
    }
    if (n < tlv.size()) {
      ++n;
    }
  }
  return n;
}

void sort_components(std::vector<std::vector<std::uint8_t>>& components) {
  std::sort(components.begin(), components.end(),
            [](const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
              const std::size_t na = tag_octet_count(a);
              const std::size_t nb = tag_octet_count(b);
              const std::size_t n = std::min(na, nb);
              for (std::size_t i = 0; i < n; ++i) {
                if (a[i] != b[i]) {
                  return a[i] < b[i];
                }
              }
              if (na != nb) {
                return na < nb;
              }
              return a < b;  // X.690 11.6: equal tags order by full encoding
            });
}

void write_indefinite(ByteWriter& out, ber::Tag tag, Span<const std::uint8_t> components) {
  ber::Tag t = tag;
  t.constructed = true;
  ber::encode_tag(out, t);
  out.put(0x80);
  out.write(components);
  out.put(0x00);
  out.put(0x00);  // end-of-contents (X.690 8.1.5)
}

/// Concatenate primitive fragments of an indefinite constructed string.
/// Fragments carry `expected` (constructed bit ignored) and each keeps at
/// most 1000 contents octets (X.690 9.2).
Result<std::vector<std::uint8_t>> read_string_fragments(ByteReader& in, ber::Tag expected) {
  std::vector<std::uint8_t> out;
  for (;;) {
    if (at_end_of_contents(in)) {
      if (auto eoc = ber::decode_end_of_contents(in); !eoc) {
        return eoc.error();
      }
      break;
    }
    auto fh = decode_header(in);
    if (!fh) {
      return fh.error();
    }
    if (auto r = tag_ok(in, fh.value().tag, expected, "CER string fragment tag mismatch"); !r) {
      return r.error();
    }
    if (fh.value().tag.constructed) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER string fragments shall be primitive");
    }
    if (fh.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER string fragments shall be definite");
    }
    if (fh.value().length.value > kFragmentContents) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER string fragment exceeds 1000 contents octets");
    }
    auto part = in.read(fh.value().length.value);
    if (!part) {
      return part.error();
    }
    const bool final = at_end_of_contents(in);
    if (part.value().empty() || (!final && part.value().size() != kFragmentContents))
      return make_error(Error::Code::NonCanonical, in.offset(), "invalid CER fragment size");
    if (part.value().size() > kMaxCodecBytes - out.size())
      return make_error(Error::Code::LengthOverflow, in.offset(), "CER string limit exceeded");
    out.insert(out.end(), part.value().begin(), part.value().end());
  }
  return out;
}

}  // namespace

// ---- Primitives: BER encodings are already primitive / minimal ----

void encode_boolean(ByteWriter& out, bool value, ber::Tag tag) {
  ber::encode_boolean(out, value, tag);
}

Result<bool> decode_boolean(ByteReader& in, ber::Tag expected) {
  return der::decode_boolean(in, expected);
}

void encode_integer(ByteWriter& out, std::int64_t value, ber::Tag tag) {
  ber::encode_integer(out, value, tag);
}

void encode_integer(ByteWriter& out, const BigInteger& value, ber::Tag tag) {
  ber::encode_integer(out, value, tag);
}

Result<std::int64_t> decode_integer(ByteReader& in, ber::Tag expected) {
  return der::decode_integer(in, expected);
}

Result<BigInteger> decode_big_integer(ByteReader& in, ber::Tag expected) {
  return der::decode_big_integer(in, expected);
}

void encode_null(ByteWriter& out, ber::Tag tag) { ber::encode_null(out, tag); }

Result<void> decode_null(ByteReader& in, ber::Tag expected) {
  return der::decode_null(in, expected);
}

void encode_enumerated(ByteWriter& out, std::int64_t value, ber::Tag tag) {
  ber::encode_enumerated(out, value, tag);
}

Result<std::int64_t> decode_enumerated(ByteReader& in, ber::Tag expected) {
  return der::decode_enumerated(in, expected);
}

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs, ber::Tag tag) {
  ber::encode_object_identifier(out, arcs, tag);
}

Result<std::vector<std::uint64_t>> decode_object_identifier(ByteReader& in, ber::Tag expected) {
  return der::decode_object_identifier(in, expected);
}

void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs, ber::Tag tag) {
  ber::encode_relative_oid(out, arcs, tag);
}

Result<std::vector<std::uint64_t>> decode_relative_oid(ByteReader& in, ber::Tag expected) {
  return der::decode_relative_oid(in, expected);
}

void encode_real(ByteWriter& out, double value, ber::Tag tag) {
  ber::encode_real(out, value, tag);
}

Result<double> decode_real(ByteReader& in, ber::Tag expected) {
  return der::decode_real(in, expected);
}

// ---- Strings (X.690 9.2) ----

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value, ber::Tag tag) {
  if (value.size() <= kFragmentContents) {
    ber::Tag t = tag;
    t.constructed = false;
    ber::encode_octet_string(out, value, t);
    return;
  }
  ByteWriter body;
  std::size_t off = 0;
  while (off < value.size()) {
    const std::size_t n = std::min<std::size_t>(kFragmentContents, value.size() - off);
    ber::Tag ft = tag;
    ft.constructed = false;
    ber::encode_octet_string(body, Span<const std::uint8_t>(value.data() + off, n), ft);
    off += n;
  }
  write_indefinite(out, tag, body.buffer());
}

Result<std::vector<std::uint8_t>> decode_octet_string(ByteReader& in, ber::Tag expected) {
  auto hdr = decode_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = tag_ok(in, hdr.value().tag, expected, "CER octet string tag mismatch"); !r) {
    return r.error();
  }
  if (!hdr.value().tag.constructed) {
    if (hdr.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER primitive string with indefinite length");
    }
    if (hdr.value().length.value > kFragmentContents) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER octet strings over 1000 contents octets shall be constructed");
    }
    return ber::decode_octet_string_content(in, hdr.value().length.value);
  }
  if (auto r = require_indefinite(in, hdr.value()); !r) {
    return r.error();
  }
  auto out = read_string_fragments(in, expected);
  if (!out) {
    return out.error();
  }
  if (out.value().size() <= kFragmentContents) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "CER octet strings within 1000 contents octets shall be primitive");
  }
  return out;
}

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::uint8_t unused_bits,
                       ber::Tag tag) {
  // X.690 11.2: the unused bits of the last octet shall be zero. The BER
  // primitive encoder writes the octets verbatim, so CER encodes a masked
  // copy (DER shares the gap and is left untouched).
  std::vector<std::uint8_t> masked;
  if (unused_bits > 0 && !bits.empty()) {
    masked.assign(bits.begin(), bits.end());
    masked.back() &= static_cast<std::uint8_t>(0xFFu << unused_bits);
    bits = Span<const std::uint8_t>(masked.data(), masked.size());
  }
  const std::size_t data = bits.size();
  if (data + 1 <= kFragmentContents) {
    ber::encode_bit_string(out, bits, unused_bits, tag);
    return;
  }
  ByteWriter body;
  std::size_t off = 0;
  while (off < data) {
    const std::size_t n = std::min<std::size_t>(kBitStringFragmentData, data - off);
    const std::uint8_t seg_unused = (n == data - off) ? unused_bits : 0;
    ber::Tag ft = tag;
    ft.constructed = false;
    ber::encode_bit_string(body, Span<const std::uint8_t>(bits.data() + off, n), seg_unused, ft);
    off += n;
  }
  write_indefinite(out, tag, body.buffer());
}

Result<ber::BitStringValue> decode_bit_string(ByteReader& in, ber::Tag expected) {
  auto hdr = decode_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = tag_ok(in, hdr.value().tag, expected, "CER bit string tag mismatch"); !r) {
    return r.error();
  }
  if (!hdr.value().tag.constructed) {
    if (hdr.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER primitive string with indefinite length");
    }
    if (hdr.value().length.value > kFragmentContents) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER bit strings over 1000 contents octets shall be constructed");
    }
    auto value = ber::decode_bit_string_content(in, hdr.value().length.value);
    if (!value) return value.error();
    const auto unused = value.value().bits.size() * 8 - value.value().bit_length;
    if (unused && (value.value().bits.back() & ((1u << unused) - 1u)))
      return make_error(Error::Code::NonCanonical, in.offset(), "CER unused bits must be zero");
    return value;
  }
  if (auto r = require_indefinite(in, hdr.value()); !r) {
    return r.error();
  }
  // Each fragment is a complete BIT STRING TLV (X.690 8.6.4): its own
  // initial octet counts toward its 1000 contents octets; only the last
  // fragment may carry unused bits.
  std::vector<std::uint8_t> bits;
  std::size_t total_bits = 0;
  for (;;) {
    if (at_end_of_contents(in)) {
      if (auto eoc = ber::decode_end_of_contents(in); !eoc) {
        return eoc.error();
      }
      break;
    }
    auto fh = decode_header(in);
    if (!fh) {
      return fh.error();
    }
    if (auto r = tag_ok(in, fh.value().tag, expected, "CER bit string fragment tag mismatch"); !r) {
      return r.error();
    }
    if (fh.value().tag.constructed || fh.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER bit string fragments shall be primitive and definite");
    }
    if (fh.value().length.value > kFragmentContents) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER bit string fragment exceeds 1000 contents octets");
    }
    auto seg = ber::decode_bit_string_content(in, fh.value().length.value);
    if (!seg) {
      return seg.error();
    }
    const auto unused = seg.value().bits.size() * 8 - seg.value().bit_length;
    if ((unused && (seg.value().bits.back() & ((1u << unused) - 1u))) ||
        (!at_end_of_contents(in) && (unused != 0 || fh.value().length.value != kFragmentContents)) ||
        seg.value().bits.empty())
      return make_error(Error::Code::NonCanonical, in.offset(), "invalid CER BIT STRING fragment");
    if (seg.value().bits.size() > kMaxCodecBytes - bits.size())
      return make_error(Error::Code::LengthOverflow, in.offset(), "CER BIT STRING limit exceeded");
    bits.insert(bits.end(), seg.value().bits.begin(), seg.value().bits.end());
    total_bits += seg.value().bit_length;
  }
  if (bits.size() + 1 <= kFragmentContents) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "CER bit strings within 1000 contents octets shall be primitive");
  }
  ber::BitStringValue v;
  v.bits = std::move(bits);
  v.bit_length = total_bits;
  return v;
}

void encode_utf8_string(ByteWriter& out, const std::string& value, ber::Tag tag) {
  if (value.size() <= kFragmentContents) {
    ber::Tag t = tag;
    t.constructed = false;
    ber::encode_utf8_string(out, value, t);
    return;
  }
  ByteWriter body;
  std::size_t off = 0;
  while (off < value.size()) {
    const std::size_t n = std::min<std::size_t>(kFragmentContents, value.size() - off);
    ber::Tag ft = tag;
    ft.constructed = false;
    ber::encode_utf8_string(body, value.substr(off, n), ft);
    off += n;
  }
  write_indefinite(out, tag, body.buffer());
}

Result<std::string> decode_utf8_string(ByteReader& in, ber::Tag expected) {
  auto hdr = decode_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = tag_ok(in, hdr.value().tag, expected, "CER utf8 string tag mismatch"); !r) {
    return r.error();
  }
  if (!hdr.value().tag.constructed) {
    if (hdr.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER primitive string with indefinite length");
    }
    if (hdr.value().length.value > kFragmentContents) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER utf8 strings over 1000 contents octets shall be constructed");
    }
    return ber::decode_utf8_string_content(in, hdr.value().length.value);
  }
  if (auto r = require_indefinite(in, hdr.value()); !r) {
    return r.error();
  }
  auto out = read_string_fragments(in, expected);
  if (!out) {
    return out.error();
  }
  if (out.value().size() <= kFragmentContents) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "CER utf8 strings within 1000 contents octets shall be primitive");
  }
  return std::string(out.value().begin(), out.value().end());
}

// ---- Constructed wrappers (X.690 9.1 indefinite) ----

void encode_constructed(ByteWriter& out, ber::Tag tag, Span<const std::uint8_t> components) {
  write_indefinite(out, tag, components);
}

void encode_sequence(ByteWriter& out, Span<const std::uint8_t> components, ber::Tag tag) {
  write_indefinite(out, tag, components);
}

Result<std::vector<std::uint8_t>> decode_constructed(ByteReader& in, ber::Tag expected) {
  auto hdr = decode_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = tag_ok(in, hdr.value().tag, expected, "CER constructed tag mismatch"); !r) {
    return r.error();
  }
  if (!hdr.value().tag.constructed) {
    return make_error(Error::Code::TagMismatch, in.offset(), "CER constructed encoding expected");
  }
  if (auto r = require_indefinite(in, hdr.value()); !r) {
    return r.error();
  }
  ByteWriter acc;
  for (;;) {
    if (at_end_of_contents(in)) {
      if (auto eoc = ber::decode_end_of_contents(in); !eoc) {
        return eoc.error();
      }
      break;
    }
    if (auto r = copy_one_tlv(in, acc); !r) {
      return r.error();
    }
  }
  return acc.take();
}

Result<std::vector<std::uint8_t>> decode_sequence(ByteReader& in, ber::Tag expected) {
  // Qualified: ADL would also bring asn1::ber::decode_constructed into the
  // candidate set (ber::Tag is an argument type) and the call would be
  // ambiguous.
  return cer::decode_constructed(in, expected);
}

void encode_set(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components, ber::Tag tag) {
  sort_components(components);
  ByteWriter body;
  for (const auto& c : components) {
    body.write(c);
  }
  write_indefinite(out, tag, body.buffer());
}

void encode_set_ordered(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components,
                        ber::Tag tag) {
  ByteWriter body;
  for (const auto& c : components) {
    body.write(c);
  }
  write_indefinite(out, tag, body.buffer());
}

Result<std::vector<std::uint8_t>> decode_set(ByteReader& in, ber::Tag expected) {
  // Component order is intentionally NOT validated on decode: X.690 9.3
  // orders an untagged CHOICE component by the SMALLEST tag in the choice,
  // which the runtime cannot reconstruct from the encoded bytes — a valid
  // CER SET may therefore not be ascending in its encoded tag prefixes.
  // Qualified: ADL would also bring asn1::ber::decode_constructed into the
  // candidate set (ber::Tag is an argument type) and the call would be
  // ambiguous.
  return cer::decode_constructed(in, expected);
}

}  // namespace cer
}  // namespace asn1
