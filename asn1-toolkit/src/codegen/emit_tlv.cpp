/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_tlv.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   TLV family emitter (BER/DER share one implementation via the der
   flag): tag-length-value encode/decode body emission.
**
** Specification: Generated code layout follows ITU type rules; emission
**                 logic is toolchain-internal.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "emit_internal.hpp"

namespace asn1 {
namespace codegen {

  /**
   *  Function    : tlv_ns
   *  Description : Computes tlv ns from (der).
   *  Parameters  : der — bool der
   *  Returns     : const char*
   */
const char* Emitter::tlv_ns(bool der) const { return der ? "asn1::der" : "asn1::ber"; }

  /**
   *  Function    : tlv_fn
   *  Description : Computes tlv fn from (der).
   *  Parameters  : der — bool der
   *  Returns     : const char*
   */
const char* Emitter::tlv_fn(bool der) const { return der ? "der" : "ber"; }

  /**
   *  Function    : ber_tag_expr
   *  Description : Builds and returns a string for ber tag expr.
   *  Parameters  : tag — const ir::Tag& tag; constructed — bool constructed
   *  Returns     : static std::string
   */
std::string Emitter::ber_tag_expr(const ir::Tag& tag, bool constructed) {
    std::ostringstream os;
    const char* fn = "asn1::ber::universal";
    switch (tag.cls) {
      case ir::TagClass::Application:
        fn = "asn1::ber::application";
        break;
      case ir::TagClass::Context:
        fn = "asn1::ber::context";
        break;
      case ir::TagClass::Private:
        fn = "asn1::ber::private_tag";
        break;
      case ir::TagClass::Universal:
      default:
        fn = "asn1::ber::universal";
        break;
    }
    os << fn << "(" << tag.number << "u, " << (constructed ? "true" : "false") << ")";
    return os.str();
  }

  /**
   *  Function    : universal_number_for
   *  Description : Computes universal number for from (t).
   *  Parameters  : t — const ir::Type& t
   *  Returns     : static std::uint64_t
   */
std::uint64_t Emitter::universal_number_for(const ir::Type& t) {
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        return 1;
      case ir::TypeKind::Integer:
        return 2;
      case ir::TypeKind::BitString:
        return 3;
      case ir::TypeKind::OctetString:
        return 4;
      case ir::TypeKind::Null:
        return 5;
      case ir::TypeKind::ObjectIdentifier:
        return 6;
      case ir::TypeKind::InstanceOf:
        return 8;
      case ir::TypeKind::Real:
        return 9;
      case ir::TypeKind::Enumerated:
        return 10;
      case ir::TypeKind::RelativeOid:
        return 13;
      case ir::TypeKind::Sequence:
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::Choice:
        return 16;
      case ir::TypeKind::Set:
      case ir::TypeKind::SetOf:
        return 17;
      case ir::TypeKind::String:
        return 12;  // UTF8String
      default:
        return 16;
    }
  }

  /**
   *  Function    : natural_tag_expr
   *  Description : Builds and returns a string for natural tag expr.
   *  Parameters  : t — const ir::Type& t
   *  Returns     : static std::string
   */
std::string Emitter::natural_tag_expr(const ir::Type& t) {
    ir::Tag tag;
    tag.cls = ir::TagClass::Universal;
    tag.number = universal_number_for(t);
    tag.is_explicit = false;
    return ber_tag_expr(tag, type_is_constructed(t));
  }

  /**
   *  Function    : emit_tlv_codec_fns
   *  Description : Performs emit tlv codec fns (definition).
   *  Parameters  : out — std::ostream& out; id — ir::TypeId id; der — bool der
   *  Returns     : void
   */
void Emitter::emit_tlv_codec_fns(std::ostream& out, ir::TypeId id, bool der) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    const char* suf = tlv_fn(der);
    std::ostream* body = begin_codec_fn(
        out, std::string("asn1::Result<void> encode_") + suf +
                "(asn1::ByteWriter& w, const " + name + "& value)");
    emit_tlv_encode_body(*body, t, "value", der, /*override_tag=*/nullptr, "  ");
    *body << "  return asn1::Result<void>::success();\n}\n\n";

    body = begin_codec_fn(out, std::string("asn1::Result<void> decode_") + suf +
                                   "(asn1::ByteReader& r, " + name + "& value)");
    emit_tlv_decode_body(*body, t, "value", der, /*override_tag=*/nullptr, "  ");
    *body << "  return asn1::Result<void>::success();\n}\n\n";
  }

void Emitter::emit_tlv_field_encode(std::ostream& out, const ir::Type& ft, const ir::Field* field,
                             const std::string& expr, bool der, const std::string& ind) {
    if (field && (field->tag.cls != ir::TagClass::Universal || field->tag.number != 0 ||
                  field->tag.is_explicit)) {
      if (field->tag.is_explicit) {
        out << ind << "{\n";
        out << ind << "  asn1::ByteWriter inner;\n";
        out << ind << "  {\n";
        out << ind << "    asn1::ByteWriter& w = inner;\n";
        if (is_named(ft)) {
          out << ind << "    if (auto r = encode_" << tlv_fn(der) << "(w, " << expr
              << "); !r) return r;\n";
        } else {
          emit_tlv_encode_body(out, ft, expr, der, nullptr, ind + "    ");
        }
        out << ind << "  }\n";
        out << ind << "  asn1::ber::encode_constructed(w, "
            << ber_tag_expr(field->tag, true) << ", asn1::Span<const std::uint8_t>(inner.buffer().data(), "
            << "inner.buffer().size()));\n";
        out << ind << "}\n";
        return;
      }
      // IMPLICIT: replace natural tag
      if (is_named(ft) && type_is_constructed(ft)) {
        out << ind << "{\n";
        out << ind << "  asn1::ByteWriter inner;\n";
        out << ind << "  {\n";
        out << ind << "    asn1::ByteWriter& w = inner;\n";
        emit_tlv_encode_content(out, ft, expr, der, ind + "    ");
        out << ind << "  }\n";
        out << ind << "  asn1::ber::encode_constructed(w, "
            << ber_tag_expr(field->tag, true)
            << ", asn1::Span<const std::uint8_t>(inner.buffer().data(), inner.buffer().size()));\n";
        out << ind << "}\n";
        return;
      }
      emit_tlv_encode_body(out, ft, expr, der, &field->tag, ind);
      return;
    }
    if (is_named(ft)) {
      out << ind << "if (auto r = encode_" << tlv_fn(der) << "(w, " << expr
          << "); !r) return r;\n";
    } else {
      emit_tlv_encode_body(out, ft, expr, der, nullptr, ind);
    }
  }

void Emitter::emit_tlv_encode_content(std::ostream& out, const ir::Type& t, const std::string& expr,
                               bool der, const std::string& ind) {
    // Encode without outermost constructed wrapper (for IMPLICIT SEQUENCE/SET).
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    if (rt.kind == ir::TypeKind::Sequence) {
      emit_tlv_sequence_components(out, rt.sequence, expr, der, ind);
    } else if (rt.kind == ir::TypeKind::Set) {
      emit_tlv_sequence_components(out, rt.set, expr, der, ind);
    } else {
      emit_tlv_encode_body(out, rt, expr, der, nullptr, ind);
    }
  }

void Emitter::emit_tlv_sequence_field_encode(std::ostream& out, const ir::Field& f,
                                      const std::string& expr, bool der,
                                      const std::string& ind) {
    const std::string fname = cpp_ident(f.name);
    const ir::Type& ft = resolve(model_, f.type);
    if (f.presence == ir::Presence::Optional) {
      out << ind << "if (" << expr << "." << fname << ") {\n";
      emit_tlv_field_encode(out, ft, &f, expr + "." + fname + ".value()", der, ind + "  ");
      out << ind << "}\n";
    } else if (f.presence == ir::Presence::Default && f.default_value) {
      const std::string def_val = default_val_cpp(*f.default_value);
      if (!def_val.empty()) {
        out << ind << "if (" << expr << "." << fname << " != " << def_val << ") {\n";
        emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind + "  ");
        out << ind << "}\n";
      } else {
        emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind);
      }
    } else {
      emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind);
    }
  }

void Emitter::emit_tlv_sequence_components(std::ostream& out, const ir::SequenceDesc& seq,
                                    const std::string& expr, bool der,
                                    const std::string& ind) {
    for (const ir::Field& f : seq.root) {
      emit_tlv_sequence_field_encode(out, f, expr, der, ind);
    }
    for (const ir::Field& f : seq.trailing_root) {
      emit_tlv_sequence_field_encode(out, f, expr, der, ind);
    }
    for (const auto& group : seq.extension_groups) {
      for (const ir::Field& f : group.fields) {
        emit_tlv_sequence_field_encode(out, f, expr, der, ind);
      }
    }
  }

void Emitter::emit_tlv_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            bool der, const ir::Tag* override_tag, const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    const std::string NS = tlv_ns(der);
    const std::string tag =
        override_tag ? ber_tag_expr(*override_tag, type_is_constructed(rt))
                     : (rt.tag.cls != ir::TagClass::Universal || rt.tag.number != 0
                            ? ber_tag_expr(rt.tag, type_is_constructed(rt))
                            : natural_tag_expr(rt));

    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << NS << "::encode_boolean(w, " << expr << ", " << tag << ");\n";
        break;
      case ir::TypeKind::Null:
        out << ind << NS << "::encode_null(w, " << tag << ");\n";
        break;
      case ir::TypeKind::Integer:
        if (integer_is_bigint(rt.integer)) {
          out << ind << NS << "::encode_integer(w, " << expr << ", " << tag << ");\n";
        } else {
          out << ind << NS << "::encode_integer(w, static_cast<std::int64_t>(" << expr
              << "), " << tag << ");\n";
        }
        break;
      case ir::TypeKind::OctetString:
        out << ind << NS << "::encode_octet_string(w, asn1::Span<const std::uint8_t>("
            << expr << ".data(), " << expr << ".size()), " << tag << ");\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "{\n";
        out << ind << "  const std::uint8_t unused = static_cast<std::uint8_t>(("
            << expr << ".bit_length % 8u == 0u) ? 0u : (8u - (" << expr
            << ".bit_length % 8u)));\n";
        out << ind << "  " << NS << "::encode_bit_string(w, asn1::Span<const std::uint8_t>("
            << expr << ".bits.data(), " << expr << ".bits.size()), unused, " << tag << ");\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::String:
        out << ind << NS << "::encode_utf8_string(w, " << expr << ", " << tag << ");\n";
        break;
      case ir::TypeKind::Enumerated:
        out << ind << NS << "::encode_enumerated(w, static_cast<std::int64_t>(" << expr
            << "), " << tag << ");\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
        out << ind << NS << "::encode_object_identifier(w, asn1::Span<const std::uint64_t>("
            << expr << ".data(), " << expr << ".size()), " << tag << ");\n";
        break;
      case ir::TypeKind::RelativeOid:
        out << ind << NS << "::encode_relative_oid(w, asn1::Span<const std::uint64_t>("
            << expr << ".data(), " << expr << ".size()), " << tag << ");\n";
        break;
      case ir::TypeKind::Real:
        out << ind << NS << "::encode_real(w, " << expr << ", " << tag << ");\n";
        break;
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::Set) {
          out << ind << "  std::vector<std::vector<std::uint8_t>> parts;\n";
          auto emit_set_parts = [&](const std::vector<ir::Field>& fields) {
            for (const ir::Field& f : fields) {
              const std::string fname = cpp_ident(f.name);
              const ir::Type& ft = resolve(model_, f.type);
              if (f.presence == ir::Presence::Optional) {
                out << ind << "  if (" << expr << "." << fname << ") {\n";
                out << ind << "    asn1::ByteWriter ew;\n";
                out << ind << "    {\n";
                out << ind << "      asn1::ByteWriter& w = ew;\n";
                emit_tlv_field_encode(out, ft, &f, expr + "." + fname + ".value()", der,
                                      ind + "      ");
                out << ind << "    }\n";
                out << ind << "    parts.push_back(ew.take());\n";
                out << ind << "  }\n";
              } else if (f.presence == ir::Presence::Default) {
                if (f.default_value) {
                  std::string def_val = default_val_cpp(*f.default_value);
                  if (!def_val.empty()) {
                    out << ind << "  if (" << expr << "." << fname << " != " << def_val << ") {\n";
                    out << ind << "    asn1::ByteWriter ew;\n";
                    out << ind << "    {\n";
                    out << ind << "      asn1::ByteWriter& w = ew;\n";
                    emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind + "      ");
                    out << ind << "    }\n";
                    out << ind << "    parts.push_back(ew.take());\n";
                    out << ind << "  }\n";
                    continue;
                  }
                }
                out << ind << "  {\n";
                out << ind << "    asn1::ByteWriter ew;\n";
                out << ind << "    {\n";
                out << ind << "      asn1::ByteWriter& w = ew;\n";
                emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind + "      ");
                out << ind << "    }\n";
                out << ind << "    parts.push_back(ew.take());\n";
                out << ind << "  }\n";
              } else {
                out << ind << "  {\n";
                out << ind << "    asn1::ByteWriter ew;\n";
                out << ind << "    {\n";
                out << ind << "      asn1::ByteWriter& w = ew;\n";
                emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind + "      ");
                out << ind << "    }\n";
                out << ind << "    parts.push_back(ew.take());\n";
                out << ind << "  }\n";
              }
            }
          };
          emit_set_parts(rt.set.root);
          emit_set_parts(rt.set.trailing_root);
          out << ind << "  asn1::der::encode_set(w, std::move(parts), " << tag << ");\n";
        } else {
          out << ind << "  asn1::ByteWriter comps;\n";
          out << ind << "  {\n";
          out << ind << "    asn1::ByteWriter& w = comps;\n";
          if (rt.kind == ir::TypeKind::Sequence) {
            emit_tlv_sequence_components(out, rt.sequence, expr, der, ind + "    ");
          } else {
            emit_tlv_sequence_components(out, rt.set, expr, der, ind + "    ");
          }
          out << ind << "  }\n";
          if (der) {
            out << ind << "  asn1::der::encode_sequence(w, asn1::Span<const std::uint8_t>("
                << "comps.buffer().data(), comps.buffer().size()), " << tag << ");\n";
          } else {
            out << ind << "  asn1::ber::encode_constructed(w, " << tag
                << ", asn1::Span<const std::uint8_t>(comps.buffer().data(), comps.buffer().size()));\n";
          }
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::SetOf) {
          out << ind << "  std::vector<std::vector<std::uint8_t>> parts;\n";
          out << ind << "  parts.reserve(" << expr << ".value.size());\n";
          out << ind << "  for (const auto& elem : " << expr << ".value) {\n";
          out << ind << "    asn1::ByteWriter ew;\n";
          out << ind << "    {\n";
          out << ind << "      asn1::ByteWriter& w = ew;\n";
          if (is_named(elem)) {
            out << ind << "      if (auto r = encode_" << tlv_fn(der)
                << "(w, elem); !r) return r;\n";
          } else {
            emit_tlv_encode_body(out, elem, "elem", der, nullptr, ind + "      ");
          }
          out << ind << "    }\n";
          out << ind << "    parts.push_back(ew.take());\n";
          out << ind << "  }\n";
          out << ind << "  asn1::der::encode_set(w, std::move(parts), " << tag << ");\n";
        } else {
          out << ind << "  asn1::ByteWriter comps;\n";
          out << ind << "  for (const auto& elem : " << expr << ".value) {\n";
          out << ind << "    asn1::ByteWriter& w = comps;\n";
          if (is_named(elem)) {
            out << ind << "    if (auto r = encode_" << tlv_fn(der)
                << "(w, elem); !r) return r;\n";
          } else {
            emit_tlv_encode_body(out, elem, "elem", der, nullptr, ind + "    ");
          }
          out << ind << "  }\n";
          if (der) {
            out << ind << "  asn1::der::encode_sequence(w, asn1::Span<const std::uint8_t>("
                << "comps.buffer().data(), comps.buffer().size()), " << tag << ");\n";
          } else {
            out << ind << "  asn1::ber::encode_constructed(w, " << tag
                << ", asn1::Span<const std::uint8_t>(comps.buffer().data(), comps.buffer().size()));\n";
          }
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        out << ind << "{\n";
        out << ind << "  const std::size_t index = " << expr << ".alt.index();\n";
        out << ind << "  switch (index) {\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          out << ind << "    case " << i << ": {\n";
          out << ind << "      const auto& alt = std::get<" << i << ">(" << expr << ".alt);\n";
          emit_tlv_field_encode(out, ft, &f, "alt.value", der, ind + "      ");
          out << ind << "      break;\n";
          out << ind << "    }\n";
        }
        out << ind << "    default:\n";
        out << ind << "      return asn1::make_error(asn1::Error::Code::InvalidArgument, "
               "w.size(), \"CHOICE index out of range\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n";
        out << ind << "  asn1::ByteWriter comps;\n";
        out << ind << "  " << NS << "::encode_object_identifier(comps, asn1::Span<const std::uint64_t>("
            << expr << ".type_id.data(), " << expr << ".type_id.size()));\n";
        out << ind << "  " << NS << "::encode_octet_string(comps, asn1::Span<const std::uint8_t>("
            << expr << ".value.data(), " << expr << ".value.size()));\n";
        out << ind << "  asn1::ber::encode_constructed(w, " << tag
            << ", asn1::Span<const std::uint8_t>(comps.buffer().data(), comps.buffer().size()));\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_tlv_encode_body(out, resolve(model_, rt.referenced.resolved), expr, der,
                               override_tag, ind);
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, w.size(), "
               "\"TLV encode not generated for this type\");\n";
        break;
    }
  }

void Emitter::emit_tlv_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            bool der, const ir::Tag* override_tag, const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    const std::string NS = tlv_ns(der);
    const std::string tag =
        override_tag ? ber_tag_expr(*override_tag, type_is_constructed(rt))
                     : (rt.tag.cls != ir::TagClass::Universal || rt.tag.number != 0
                            ? ber_tag_expr(rt.tag, type_is_constructed(rt))
                            : natural_tag_expr(rt));

    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_boolean(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = tmp.value();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Null:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_null(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Integer:
        out << ind << "{\n";
        if (integer_is_bigint(rt.integer)) {
          out << ind << "  auto tmp = " << NS << "::decode_big_integer(r, " << tag << ");\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = std::move(tmp.value());\n";
        } else {
          out << ind << "  auto tmp = " << NS << "::decode_integer(r, " << tag << ");\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = static_cast<" << integer_host_type(rt.integer)
              << ">(tmp.value());\n";
        }
        out << ind << "}\n";
        break;
      case ir::TypeKind::OctetString:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_octet_string(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_bit_string(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::String:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_utf8_string(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Enumerated:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_enumerated(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = static_cast<std::remove_cv_t<std::remove_reference_t<decltype(" << expr << ")>>>(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_object_identifier(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::RelativeOid:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_relative_oid(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Real:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_real(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = tmp.value();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::Set) {
          out << ind << "  auto content = asn1::der::decode_set(r, " << tag << ");\n";
        } else if (der) {
          out << ind << "  auto content = asn1::der::decode_sequence(r, " << tag << ");\n";
        } else {
          out << ind << "  auto content = asn1::ber::decode_constructed(r, " << tag << ");\n";
        }
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  asn1::ByteReader cr(content.value());\n";
        out << ind << "  {\n";
        out << ind << "    asn1::ByteReader& r = cr;\n";
        const auto& fields = rt.kind == ir::TypeKind::Sequence ? rt.sequence.root : rt.set.root;
        const auto& trailing =
            rt.kind == ir::TypeKind::Sequence ? rt.sequence.trailing_root : rt.set.trailing_root;
        for (const ir::Field& f : fields) {
          emit_tlv_field_decode_stmt(out, f, expr, der, ind + "    ");
        }
        for (const ir::Field& f : trailing) {
          emit_tlv_field_decode_stmt(out, f, expr, der, ind + "    ");
        }
        if (rt.kind == ir::TypeKind::Sequence) {
          for (const auto& group : rt.sequence.extension_groups) {
            for (const ir::Field& f : group.fields) {
              emit_tlv_field_decode_stmt(out, f, expr, der, ind + "    ");
            }
          }
        }
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::SetOf) {
          out << ind << "  auto content = asn1::der::decode_set(r, " << tag << ");\n";
        } else if (der) {
          out << ind << "  auto content = asn1::der::decode_sequence(r, " << tag << ");\n";
        } else {
          out << ind << "  auto content = asn1::ber::decode_constructed(r, " << tag << ");\n";
        }
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  asn1::ByteReader cr(content.value());\n";
        out << ind << "  " << expr << ".value.clear();\n";
        out << ind << "  while (cr.remaining() > 0) {\n";
        out << ind << "    " << type_cpp(elem_id) << " elem{};\n";
        out << ind << "    {\n";
        out << ind << "      asn1::ByteReader& r = cr;\n";
        if (is_named(elem)) {
          out << ind << "      if (auto dr = decode_" << tlv_fn(der)
              << "(r, elem); !dr) return dr;\n";
        } else {
          emit_tlv_decode_body(out, elem, "elem", der, nullptr, ind + "      ");
        }
        out << ind << "    }\n";
        out << ind << "    " << expr << ".value.push_back(std::move(elem));\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        const std::string choice_ty = is_named(rt) ? named_type_cpp(rt) : std::string{};
        out << ind << "{\n";
        out << ind << "  auto hdr = asn1::ber::decode_tlv_header(r);\n";
        out << ind << "  if (!hdr) return hdr.error();\n";
        out << ind << "  if (hdr.value().length.indefinite) {\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::Unsupported, r.offset(), "
               "\"indefinite CHOICE not generated\");\n";
        out << ind << "  }\n";
        out << ind << "  auto content = r.read(hdr.value().length.value);\n";
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  const auto hdr_tag = hdr.value().tag;\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          const std::string expect =
              ber_tag_expr(f.tag, type_is_constructed(ft) || f.tag.is_explicit);
          out << ind << "  if (hdr_tag == " << expect << ") {\n";
          if (!choice_ty.empty()) {
            out << ind << "    " << choice_ty << "::" << cpp_ident(f.name) << "_ alt{};\n";
          } else {
            out << ind << "    " << cpp_ident(f.name) << "_ alt{};\n";
          }
          if (f.tag.is_explicit) {
            out << ind << "    asn1::ByteReader cr(content.value());\n";
            out << ind << "    {\n";
            out << ind << "      asn1::ByteReader& r = cr;\n";
            if (is_named(ft)) {
              out << ind << "      if (auto dr = decode_" << tlv_fn(der)
                  << "(r, alt.value); !dr) return dr;\n";
            } else {
              emit_tlv_decode_body(out, ft, "alt.value", der, nullptr, ind + "      ");
            }
            out << ind << "    }\n";
          } else {
            out << ind << "    asn1::ByteWriter tmpw;\n";
            out << ind << "    asn1::ber::encode_tlv(tmpw, " << natural_tag_expr(ft)
                << ", content.value());\n";
            out << ind << "    asn1::ByteReader rr(tmpw.buffer());\n";
            out << ind << "    {\n";
            out << ind << "      asn1::ByteReader& r = rr;\n";
            if (is_named(ft)) {
              out << ind << "      if (auto dr = decode_" << tlv_fn(der)
                  << "(r, alt.value); !dr) return dr;\n";
            } else {
              emit_tlv_decode_body(out, ft, "alt.value", der, nullptr, ind + "      ");
            }
            out << ind << "    }\n";
          }
          out << ind << "    " << expr << ".alt = std::move(alt);\n";
          out << ind << "  } else ";
        }
        out << "{\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::InvalidArgument, r.offset(), "
               "\"unknown CHOICE tag\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n";
        out << ind << "  auto content = asn1::ber::decode_constructed(r, " << tag << ");\n";
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  asn1::ByteReader cr(content.value());\n";
        out << ind << "  auto tid = " << NS << "::decode_object_identifier(cr);\n";
        out << ind << "  if (!tid) return tid.error();\n";
        out << ind << "  " << expr << ".type_id = std::move(tid.value());\n";
        out << ind << "  auto val = " << NS << "::decode_octet_string(cr);\n";
        out << ind << "  if (!val) return val.error();\n";
        out << ind << "  " << expr << ".value = std::move(val.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_tlv_decode_body(out, resolve(model_, rt.referenced.resolved), expr, der,
                               override_tag, ind);
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, r.offset(), "
               "\"TLV decode not generated for this type\");\n";
        break;
    }
  }

void Emitter::emit_tlv_field_decode_stmt(std::ostream& out, const ir::Field& f, const std::string& expr,
                                  bool der, const std::string& ind) {
    const std::string fname = cpp_ident(f.name);
    const ir::Type& ft = resolve(model_, f.type);
    if (f.presence == ir::Presence::Default) {
      const std::string expect =
          ber_tag_expr(f.tag, type_is_constructed(ft) || f.tag.is_explicit);
      out << ind << "if (r.remaining() > 0) {\n";
      out << ind << "  asn1::ByteReader peek_r(r.remaining_span());\n";
      out << ind << "  auto hdr = asn1::ber::decode_tlv_header(peek_r);\n";
      out << ind << "  if (hdr && hdr.value().tag == " << expect << ") {\n";
      out << ind << "    emit_field_present: ;\n";
      emit_tlv_field_decode_value(out, ft, &f, expr + "." + fname, der, ind + "    ");
      out << ind << "  } else {\n";
      if (f.default_value) {
        const std::string def_expr = default_val_cpp(*f.default_value);
        if (!def_expr.empty()) {
          out << ind << "    " << expr << "." << fname << " = " << def_expr << ";\n";
        }
      }
      out << ind << "  }\n";
      out << ind << "} else {\n";
      if (f.default_value) {
        const std::string def_expr = default_val_cpp(*f.default_value);
        if (!def_expr.empty()) {
          out << ind << "  " << expr << "." << fname << " = " << def_expr << ";\n";
        }
      }
      out << ind << "}\n";
      return;
    }
    if (f.presence == ir::Presence::Optional) {
      const std::string expect =
          ber_tag_expr(f.tag, type_is_constructed(ft) || f.tag.is_explicit);
      out << ind << "if (r.remaining() == 0) {\n";
      out << ind << "  " << expr << "." << fname << " = std::nullopt;\n";
      out << ind << "} else {\n";
      out << ind << "  asn1::ByteReader peek_r(r.remaining_span());\n";
      out << ind << "  auto hdr = asn1::ber::decode_tlv_header(peek_r);\n";
      out << ind << "  if (!hdr) return hdr.error();\n";
      out << ind << "  if (hdr.value().tag == " << expect << ") {\n";
      out << ind << "    " << type_cpp(f.type) << " field{};\n";
      emit_tlv_field_decode_value(out, ft, &f, "field", der, ind + "    ");
      out << ind << "    " << expr << "." << fname << " = std::move(field);\n";
      out << ind << "  } else {\n";
      out << ind << "    " << expr << "." << fname << " = std::nullopt;\n";
      out << ind << "  }\n";
      out << ind << "}\n";
    } else {
      emit_tlv_field_decode_value(out, ft, &f, expr + "." + fname, der, ind);
    }
  }

void Emitter::emit_tlv_field_decode_value(std::ostream& out, const ir::Type& ft, const ir::Field* field,
                                   const std::string& expr, bool der, const std::string& ind) {
    if (field && field->tag.is_explicit) {
      out << ind << "{\n";
      out << ind << "  auto content = asn1::ber::decode_constructed(r, "
          << ber_tag_expr(field->tag, true) << ");\n";
      out << ind << "  if (!content) return content.error();\n";
      out << ind << "  asn1::ByteReader cr(content.value());\n";
      out << ind << "  {\n";
      out << ind << "    asn1::ByteReader& r = cr;\n";
      if (is_named(ft)) {
        out << ind << "    if (auto dr = decode_" << tlv_fn(der) << "(r, " << expr
            << "); !dr) return dr;\n";
      } else {
        emit_tlv_decode_body(out, ft, expr, der, nullptr, ind + "    ");
      }
      out << ind << "  }\n";
      out << ind << "}\n";
      return;
    }
    if (field && field->tag.cls != ir::TagClass::Universal) {
      emit_tlv_decode_body(out, ft, expr, der, &field->tag, ind);
      return;
    }
    if (is_named(ft)) {
      out << ind << "if (auto dr = decode_" << tlv_fn(der) << "(r, " << expr
          << "); !dr) return dr;\n";
    } else {
      emit_tlv_decode_body(out, ft, expr, der, nullptr, ind);
    }
  }

}  // namespace codegen
}  // namespace asn1
