/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/standard_oracle_driver.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising standard oracle driver behaviour.
**
** Specification: Validates toolchain output against internal IR/codegen
**                 contracts (X.680 type model).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
// PROCODEC (2026). Emit and accept independently sourced Record encodings.
#include "generated.hpp"
#include <iostream>
#include <string>
#include <vector>
namespace gen = asn1_gen;
static std::string hex(const std::string& bytes) {
  std::string out;
  for (unsigned char b : bytes) { out += "0123456789abcdef"[b >> 4]; out += "0123456789abcdef"[b & 15]; }
  return out;
}
static std::string unhex(const std::string& bytes) {
  std::string out;
  for (std::size_t i = 0; i + 1 < bytes.size(); i += 2) {
    const auto hi = std::string("0123456789abcdef").find(bytes[i]);
    const auto lo = std::string("0123456789abcdef").find(bytes[i + 1]);
    if (hi > 15 || lo > 15) return {};
    out += static_cast<char>((hi << 4) | lo);
  }
  return out;
}
#define CHECK(x) do { if (!(x)) return 1; } while (0)
#define VERIFY() CHECK(decoded.count == 42 && decoded.enabled && decoded.text == "ASN1")
#define BINARY(codec, Writer, Reader) { \
  if (argc == 1) { asn1::Writer w; CHECK(gen::encode_##codec(w, value)); \
    const auto& b = w.buffer(); std::cout << #codec << "|" << hex(std::string(b.begin(), b.end())) << "\n"; } \
  else if (selected == #codec) { asn1::Reader r(bytes); gen::Record decoded{}; \
    CHECK(gen::decode_##codec(r, decoded)); VERIFY(); return 0; } }
#define TREE(codec, ns) { \
  if (argc == 1) { auto v = gen::encode_##codec(value); CHECK(v); \
    std::cout << #codec << "|" << hex(asn1::ns::to_string(v.value())) << "\n"; } \
  else if (selected == #codec) { auto v = asn1::ns::parse_document(raw); CHECK(v); gen::Record decoded{}; \
    CHECK(gen::decode_##codec(v.value(), decoded)); VERIFY(); return 0; } }
int main(int argc, char** argv) {
  if (argc != 1 && argc != 3) return 2;
  gen::Record value{}; value.count = 42; value.enabled = true; value.text = "ASN1";
  const std::string selected = argc == 3 ? argv[1] : "";
  const std::string raw = argc == 3 ? unhex(argv[2]) : "";
  const std::vector<std::uint8_t> bytes(raw.begin(), raw.end());
  BINARY(ber, ByteWriter, ByteReader)
  BINARY(der, ByteWriter, ByteReader)
  BINARY(cer, ByteWriter, ByteReader)
  BINARY(uper, BitWriter, BitReader)
  BINARY(aper, BitWriter, BitReader)
  BINARY(oer, ByteWriter, ByteReader)
  BINARY(coer, ByteWriter, ByteReader)
  TREE(xer, xer)
  TREE(cxer, cxer)
  TREE(exer, exer)
  TREE(jer, jer)
  TREE(jeri, jeri)
  return argc == 1 ? 0 : 2;
}
