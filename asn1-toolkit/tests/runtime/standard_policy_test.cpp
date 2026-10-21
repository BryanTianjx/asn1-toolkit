/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/runtime/standard_policy_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising standard policy behaviour.
**
** Specification: See asn1-toolkit/docs/architecture.md for mapping to
**                 ITU-T encoding rules.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
// PROCODEC (2026). X.690 clauses 9.2, 11.1, 11.3; binary64 profile regression.
#include <asn1/runtime/ber/codec.hpp>
#include <asn1/runtime/der/codec.hpp>
#include <asn1/runtime/cer/codec.hpp>
#include <asn1/runtime/oer/codec.hpp>
#include <asn1/runtime/coer/codec.hpp>
#include <asn1/runtime/jer/codec.hpp>
#include <asn1/runtime/limits.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <vector>

namespace {
template<class Decode>
bool accepts(const std::vector<std::uint8_t>& bytes, Decode decode) {
  asn1::ByteReader reader(bytes);
  return decode(reader).ok();
}
}

TEST(StandardPolicy, BerBooleanAlternativeIsNotCanonical) {
  const std::vector<std::uint8_t> bytes{1, 1, 1};
  EXPECT_TRUE(accepts(bytes, [](auto& r) { return asn1::ber::decode_boolean(r); }));
  EXPECT_FALSE(accepts(bytes, [](auto& r) { return asn1::der::decode_boolean(r); }));
  EXPECT_FALSE(accepts(bytes, [](auto& r) { return asn1::cer::decode_boolean(r); }));
}

TEST(StandardPolicy, BinaryRealAlternativeIsNotCanonical) {
  // 2 * 2^-1 is legal BER but canonical mantissa must be odd.
  const std::vector<std::uint8_t> bytes{9, 3, 0x80, 0xff, 2};
  EXPECT_TRUE(accepts(bytes, [](auto& r) { return asn1::ber::decode_real(r); }));
  EXPECT_FALSE(accepts(bytes, [](auto& r) { return asn1::der::decode_real(r); }));
  EXPECT_FALSE(accepts(bytes, [](auto& r) { return asn1::cer::decode_real(r); }));
}

TEST(StandardPolicy, RealProfileRejectsBinaryPrecisionLoss) {
  // 2^56 + 1 cannot be represented by binary64.
  EXPECT_FALSE(accepts({9, 10, 0x80, 0, 1, 0, 0, 0, 0, 0, 0, 1},
                     [](auto& r) { return asn1::ber::decode_real(r); }));
}

TEST(StandardPolicy, RealProfileRejectsOverflowAndUnderflow) {
  const std::vector<std::uint8_t> empty;
  asn1::ByteReader reader(empty);
  auto oversized = asn1::ber::decode_real_content(reader, asn1::kMaxCodecBytes + 1);
  ASSERT_FALSE(oversized);
  EXPECT_EQ(oversized.error().code, asn1::Error::Code::LengthOverflow);
  EXPECT_FALSE(accepts({9, 4, 0x81, 0x7f, 0xff, 1},
                     [](auto& r) { return asn1::ber::decode_real(r); }));
  EXPECT_FALSE(accepts({9, 4, 0x81, 0x80, 0, 1},
                     [](auto& r) { return asn1::ber::decode_real(r); }));
}

TEST(StandardPolicy, CerRejectsShortNonfinalOctetFragment) {
  std::vector<std::uint8_t> bytes{0x24, 0x80, 4, 1, 0x41, 4, 0x82, 3, 0xe8};
  bytes.insert(bytes.end(), 1000, 0x42);
  bytes.insert(bytes.end(), {0, 0});
  EXPECT_FALSE(accepts(bytes, [](auto& r) { return asn1::cer::decode_octet_string(r); }));
}

TEST(StandardPolicy, CerRejectsUnusedBitsInNonfinalFragment) {
  std::vector<std::uint8_t> bytes{0x23, 0x80, 3, 0x82, 3, 0xe8, 1};
  bytes.insert(bytes.end(), 999, 0);
  bytes.insert(bytes.end(), {3, 2, 0, 0x80, 0, 0});
  EXPECT_FALSE(accepts(bytes, [](auto& r) { return asn1::cer::decode_bit_string(r); }));
}

TEST(StandardPolicy, CerBoundaryOctets) {
  for (std::size_t n : {999u, 1000u, 1001u}) {
    std::vector<std::uint8_t> value(n, 0x41);
    asn1::ByteWriter out;
    asn1::cer::encode_octet_string(out, value);
    EXPECT_EQ(out.buffer()[0], n <= 1000 ? 4 : 0x24);
    asn1::ByteReader in(out.buffer());
    auto decoded = asn1::cer::decode_octet_string(in);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded.value(), value);
    EXPECT_EQ(in.remaining(), 0u);
  }
}

TEST(StandardPolicy, JerRealDoesNotUseTruncatedIntegerStorage) {
  for (const std::string text : {"18446744073709551617", "9007199254740993"}) {
    auto value = asn1::jer::parse_document(text);
    ASSERT_TRUE(value);
    auto decoded = asn1::jer::decode_real(value.value());
    ASSERT_FALSE(decoded);
    EXPECT_EQ(decoded.error().code, asn1::Error::Code::Unsupported);
  }
}

TEST(StandardPolicy, OerBinary32DoesNotRoundSilently) {
  asn1::ByteWriter out;
  EXPECT_FALSE(asn1::oer::encode_real(out, 1.1, asn1::oer::RealIeeeForm::Binary32));
  EXPECT_EQ(out.size(), 0u);
  EXPECT_TRUE(asn1::oer::encode_real(out, 1.5, asn1::oer::RealIeeeForm::Binary32));
  EXPECT_EQ(out.size(), 4u);
}

TEST(StandardPolicy, OerLengthAlternativeIsNotCanonical) {
  const std::vector<std::uint8_t> bytes{0x81, 1};
  EXPECT_TRUE(accepts(bytes, [](auto& r) { return asn1::oer::decode_length(r); }));
  EXPECT_FALSE(accepts(bytes, [](auto& r) { return asn1::coer::decode_length(r); }));
}

TEST(StandardPolicy, CerRejectsLengthOverflowAndDeepNesting) {
  EXPECT_FALSE(accepts({0x30, 0x80, 4, 0x89, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0, 0},
      [](auto& r) { return asn1::cer::decode_constructed(r, asn1::ber::universal(16, true)); }));
  std::vector<std::uint8_t> nested;
  for (int i = 0; i < 70; ++i) nested.insert(nested.end(), {0x30, 0x80});
  for (int i = 0; i < 70; ++i) nested.insert(nested.end(), {0, 0});
  EXPECT_FALSE(accepts(nested, [](auto& r) { return asn1::cer::decode_constructed(r, asn1::ber::universal(16, true)); }));
}

TEST(StandardPolicy, RealExactDecimalAndBinaryBoundaries) {
  for (double value : {std::numeric_limits<double>::denorm_min(),
                       std::numeric_limits<double>::min(), std::numeric_limits<double>::max(),
                       -0.0, 1.1}) {
    asn1::ByteWriter out;
    asn1::ber::encode_real(out, value);
    asn1::ByteReader in(out.buffer());
    auto decoded = asn1::ber::decode_real(in);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded.value(), value);
    EXPECT_EQ(std::signbit(decoded.value()), std::signbit(value));
  }
  EXPECT_TRUE(accepts({9, 5, 3, '1', '.', 'E', '2'}, [](auto& r) { return asn1::der::decode_real(r); }));
  EXPECT_FALSE(accepts({9, 4, 2, '0', '.', '1'}, [](auto& r) { return asn1::ber::decode_real(r); }));
  EXPECT_FALSE(accepts({9, 4, 2, '1', '.', '5'}, [](auto& r) { return asn1::der::decode_real(r); }));
}
