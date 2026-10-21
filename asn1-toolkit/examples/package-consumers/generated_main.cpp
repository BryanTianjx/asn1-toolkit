/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/examples/package-consumers/generated_main.cpp
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
#include "generated.hpp"
int main() {
  asn1_gen::Record value{}; value.count = 42;
  asn1::BitWriter writer;
  if (!asn1_gen::encode_uper(writer, value)) return 1;
  auto bytes = writer.take();
  asn1::BitReader reader(bytes);
  asn1_gen::Record decoded{};
  return asn1_gen::decode_uper(reader, decoded) && decoded.count == 42 ? 0 : 1;
}
