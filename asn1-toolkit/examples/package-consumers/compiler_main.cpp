/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/examples/package-consumers/compiler_main.cpp
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
#include <asn1/frontend/lexer.hpp>
#include <asn1/frontend/parser.hpp>
#include <asn1/semantic/analyzer.hpp>
#include <asn1/codegen/emit.hpp>
int main() {
  auto source = asn1::SourceFile::from_string("memory.asn", "M DEFINITIONS ::= BEGIN T ::= BOOLEAN END");
  asn1::Diagnostics diagnostics;
  asn1::Lexer lexer(source, diagnostics);
  asn1::Parser parser(lexer, diagnostics);
  auto modules = parser.parse_modules();
  asn1::Analyzer analyzer(diagnostics);
  auto model = analyzer.analyze(std::move(modules));
  auto output = asn1::codegen::CppGenerator{}.emit_header_string(model, {}, diagnostics);
  return diagnostics.ok() && !output.empty() ? 0 : 1;
}
