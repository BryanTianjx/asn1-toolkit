/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/fuzz/decoder_fuzz.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising decoder fuzz behaviour.
**
** Specification: See asn1-toolkit/docs/architecture.md for mapping to
**                 ITU-T encoding rules.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
// PROCODEC (2026). Bounded decoder fuzz entry and deterministic seed regression.
#include <asn1/runtime/ber/codec.hpp>
#include <asn1/runtime/der/codec.hpp>
#include <asn1/runtime/cer/codec.hpp>
#include <asn1/runtime/oer/codec.hpp>
#include <asn1/runtime/coer/codec.hpp>
#include <asn1/runtime/xer/xml.hpp>
#include <asn1/runtime/jer/json.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size < 1 || size > 4096) return 0;
  const auto input = asn1::Span<const std::uint8_t>(data + 1, size - 1);
  asn1::ByteReader reader(input);
  switch (data[0] % 9) {
    case 0: (void)asn1::ber::decode_real(reader); break;
    case 1: (void)asn1::der::decode_real(reader); break;
    case 2: (void)asn1::cer::decode_constructed(reader, asn1::ber::universal(16, true)); break;
    case 3: (void)asn1::cer::decode_octet_string(reader); break;
    case 4: (void)asn1::cer::decode_bit_string(reader); break;
    case 5: (void)asn1::oer::decode_length(reader); break;
    case 6: (void)asn1::coer::decode_length(reader); break;
    case 7: (void)asn1::xer::parse_document(std::string(input.begin(), input.end())); break;
    case 8: (void)asn1::jer::parse_document(std::string(input.begin(), input.end())); break;
  }
  return 0;
}

#ifdef ASN1_FUZZ_REGRESSION_MAIN
#include <fstream>
#include <iostream>
int main() {
  std::ifstream file(ASN1_FUZZ_SEEDS);
  if (!file) return 1;
  std::string line;
  std::size_t cases = 0;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    if (line.size() % 2) return 2;
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < line.size(); i += 2) {
      const auto hi = std::string("0123456789abcdef").find(line[i]);
      const auto lo = std::string("0123456789abcdef").find(line[i + 1]);
      if (hi > 15 || lo > 15) return 3;
      bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    for (std::size_t n = 0; n <= bytes.size(); ++n) {
      LLVMFuzzerTestOneInput(bytes.data(), n); ++cases;
    }
    for (std::size_t i = 0; i < bytes.size(); ++i) {
      const auto saved = bytes[i];
      for (unsigned mask : {1u, 0x80u, 0xffu}) {
        bytes[i] = static_cast<std::uint8_t>(saved ^ mask);
        LLVMFuzzerTestOneInput(bytes.data(), bytes.size()); ++cases;
      }
      bytes[i] = saved;
    }
  }
  if (cases == 0) return 4;
  std::cout << cases << " deterministic decoder seed/truncation/mutation calls passed\n";
}
#endif
