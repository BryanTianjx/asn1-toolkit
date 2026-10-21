#include "generated.hpp"
#include <asn1/runtime/exer/codec.hpp>
#include <gtest/gtest.h>

#ifdef ATTR_SPLIT
namespace generated = asn1_gen::AttrTest;
#else
namespace generated = asn1_gen;
#endif
namespace {
auto decode(const std::string& attributes, generated::Record& value) {
  auto xml = asn1::exer::parse_document("<Record " + attributes + " />");
  if (!xml) return asn1::Result<void>(xml.error());
  return generated::decode_xer(xml.value(), value);
}
const std::string required = "flag=\"true\" count=\"12\" big=\"184467440737095516160\"";
}

TEST(GeneratedAttributes, PresenceAndDefaults) {
  generated::Record value{};
  value.opt = 9;
  value.mode = 8;
  ASSERT_TRUE(decode(required, value).ok());
  EXPECT_TRUE(value.flag);
  EXPECT_EQ(value.count, 12);
  EXPECT_EQ(value.big.to_decimal(), "184467440737095516160");
  EXPECT_FALSE(value.opt.has_value());
  EXPECT_EQ(value.mode, 3);
  EXPECT_EQ(value.minimum, INT64_MIN);
  ASSERT_TRUE(decode(required + " opt=\"7\" mode=\"6\"", value).ok());
  EXPECT_EQ(*value.opt, 7);
  EXPECT_EQ(value.mode, 6);
  ASSERT_TRUE(decode(required + " minimum=\"-9223372036854775808\"", value).ok());
  EXPECT_EQ(value.minimum, INT64_MIN);
  ASSERT_TRUE(decode(required + " minimum=\"9223372036854775807\"", value).ok());
  EXPECT_EQ(value.minimum, INT64_MAX);
}

TEST(GeneratedAttributes, RejectsMalformedAndOverflow) {
  for (const std::string attrs : {
      "count=\"12\" big=\"0\"", "flag=\"true\" big=\"0\"",
      "flag=\"true\" count=\"12\"", "flag=\"invalid\" count=\"12\" big=\"0\"",
      "flag=\"true\" count=\"12x\" big=\"0\"",
      "flag=\"true\" count=\"999999999999999999999999\" big=\"0\"",
      "flag=\"true\" count=\"101\" big=\"0\"",
      "flag=\"true\" count=\"12\" big=\"12x\""}) {
    generated::Record value{};
    EXPECT_FALSE(decode(attrs, value).ok()) << attrs;
  }
  for (const std::string suffix : {" opt=\"12x\"", " opt=\"11\"", " mode=\"bad\"", " mode=\"11\"",
                                  " minimum=\"9223372036854775808\"", " minimum=\"-9223372036854775809\""}) {
    generated::Record value{};
    EXPECT_FALSE(decode(required + suffix, value).ok()) << suffix;
  }
}

TEST(GeneratedAttributes, ErrorIdentifiesLogicalField) {
  generated::Record value{};
  auto result = decode(required + " opt=\"12x\"", value);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().field_path, "opt");
  EXPECT_EQ(result.error().offset_unit, asn1::Error::OffsetUnit::Unknown);
}
