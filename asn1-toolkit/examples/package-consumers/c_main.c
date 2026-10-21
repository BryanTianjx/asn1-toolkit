/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/examples/package-consumers/c_main.c
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
#include <asn1/c_runtime/bit_io.h>
int main(void) {
  asn1_bit_writer_t* writer = 0;
  if (asn1_bit_writer_create(&writer) != ASN1_OK) return 1;
  asn1_status_t status = asn1_bit_writer_put_bits(writer, 42, 8);
  int ok = status == ASN1_OK && asn1_bit_writer_bit_size(writer) == 8;
  asn1_bit_writer_destroy(writer);
  return ok ? 0 : 1;
}
