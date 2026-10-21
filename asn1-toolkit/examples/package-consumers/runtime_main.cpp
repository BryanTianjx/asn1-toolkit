/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/examples/package-consumers/runtime_main.cpp
**   $Version: 0.1
**   $Date:   2026-10-21
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Source file in the asn1-toolkit tree.
**
** Specification: See asn1-toolkit/docs/architecture.md for mapping to
**                 ITU-T encoding rules.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/bit_io.hpp>
#include <asn1/runtime/cer/codec.hpp>
int main() {
  asn1::BitWriter writer;
  writer.put_bits(42, 8);
  auto bytes = writer.take();
  if (bytes.size() != 1 || bytes[0] != 42) return 1;
  // CER's canonical primitive decoder must bring DER/BER transitively from
  // the installed package; consumers do not name those implementation targets.
  asn1::ByteWriter encoded;
  asn1::cer::encode_boolean(encoded, true);
  asn1::ByteReader input(encoded.buffer());
  auto value = asn1::cer::decode_boolean(input);
  return value && value.value() && input.remaining() == 0 ? 0 : 1;
}
