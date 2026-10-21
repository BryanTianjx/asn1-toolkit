/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/byte_io.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) byte reader bodies.

    C11 port of asn1::ByteReader (asn1-toolkit/src/runtime/byte_io.cpp).
    The C runtime's byte reader owns a heap copy of the source buffer
    (mirroring the C++ owning-ctor overload that keeps the bytes alive
    across function boundaries; the C++ borrowing ctor's lifetime
    reasoning does not survive a C ABI translation).

    Sub-task 2 surface: create / destroy. The byte-aligned read helpers
    are deferred — bit_io's get_octet / get_octets already cover the
    octet-aligned PER / BER decode path, and the dedicated byte reader
    is only consumed by CER indefinite-length parsing (sub-task 14+).

    Specification: Internal byte-aligned decode helpers
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/byte_io.h
***************************************************************************/

#include <asn1/c_runtime/byte_io.h>

#include <stdlib.h>
#include "alloc_internal.h"
#include <string.h>

struct asn1_byte_reader {
  uint8_t* buf;   /* owned copy of source */
  size_t   size;
  size_t   pos;   /* byte cursor */
};

asn1_status_t asn1_byte_reader_create(asn1_byte_reader_t** out,
                                      asn1_byte_view_t src) {
  if (!out || (!src.data && src.size != 0u)) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  asn1_byte_reader_t* r = (asn1_byte_reader_t*)calloc(1, sizeof(*r));
  if (!r) {
    return ASN1_ERR_OUT_OF_MEMORY;
  }
  if (src.size > 0 && src.data) {
    r->buf = (uint8_t*)malloc(src.size);
    if (!r->buf) {
      free(r);
      return ASN1_ERR_OUT_OF_MEMORY;
    }
    memcpy(r->buf, src.data, src.size);
    r->size = src.size;
  }
  *out = r;
  return ASN1_OK;
}

void asn1_byte_reader_destroy(asn1_byte_reader_t* r) {
  if (!r) {
    return;
  }
  free(r->buf);
  free(r);
}