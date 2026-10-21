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
int main() {
  asn1::BitWriter writer;
  writer.put_bits(42, 8);
  auto bytes = writer.take();
  return bytes.size() == 1 && bytes[0] == 42 ? 0 : 1;
}
