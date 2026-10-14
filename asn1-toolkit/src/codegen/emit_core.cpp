/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_core.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Core emitter: driver (run/emit_header), naming and type
   declarations, plus the shared helpers used by every codec
   family file.
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
 *  Function    : is_cpp_keyword
 *  Description : Returns whether cpp keyword holds for the given inputs.
 *  Parameters  : id — std::string_view id
 *  Returns     : bool
 */
bool is_cpp_keyword(std::string_view id) {
  // Identifiers that would be illegal as C++ enum/member/type names.
  static constexpr const char* kWords[] = {
      "alignas",      "alignof",    "and",         "and_eq",     "asm",
      "auto",         "bitand",     "bitor",       "bool",       "break",
      "case",         "catch",      "char",        "char8_t",    "char16_t",
      "char32_t",     "class",      "compl",       "concept",    "const",
      "consteval",    "constexpr",  "constinit",   "const_cast", "continue",
      "co_await",     "co_return",  "co_yield",    "decltype",   "default",
      "delete",       "do",         "double",      "dynamic_cast","else",
      "enum",         "explicit",   "export",      "extern",     "false",
      "float",        "for",        "friend",      "goto",       "if",
      "inline",       "int",        "long",        "mutable",    "namespace",
      "new",          "noexcept",   "not",         "not_eq",     "nullptr",
      "operator",     "or",         "or_eq",       "private",    "protected",
      "public",       "register",   "reinterpret_cast","requires","return",
      "short",        "signed",     "sizeof",      "static",     "static_assert",
      "static_cast",  "struct",     "switch",      "template",   "this",
      "thread_local", "throw",      "true",        "try",        "typedef",
      "typeid",       "typename",   "union",       "unsigned",   "using",
      "virtual",      "void",       "volatile",    "wchar_t",    "while",
      "xor",          "xor_eq",
  };
  for (const char* w : kWords) {
    if (id == w) {
      return true;
    }
  }
  return false;
}

/**
 *  Function    : cpp_ident
 *  Description : Builds and returns a string for cpp ident.
 *  Parameters  : name — std::string_view name
 *  Returns     : std::string
 */
std::string cpp_ident(std::string_view name) {
  std::string out;
  out.reserve(name.size() + 1);
  for (char c : name) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '_') {
      out.push_back(c);
    } else if (c == '-' || c == ' ') {
      out.push_back('_');
    }
  }
  if (out.empty() || (out[0] >= '0' && out[0] <= '9')) {
    out.insert(out.begin(), '_');
  }
  if (is_cpp_keyword(out)) {
    out.push_back('_');
  }
  return out;
}

/**
 *  Function    : resolve
 *  Description : Computes resolve from (model, id).
 *  Parameters  : model — const ir::Model& model; id — ir::TypeId id
 *  Returns     : const ir::Type&
 */
const ir::Type& resolve(const ir::Model& model, ir::TypeId id) {
  const ir::Type* t = &model.arena.get(id);
  while (t->kind == ir::TypeKind::Referenced && t->referenced.resolved != ir::kInvalidType) {
    t = &model.arena.get(t->referenced.resolved);
  }
  return *t;
}

/**
 *  Function    : is_named
 *  Description : Returns whether the given type carries an ASN.1 name.
 *  Parameters  : t — const ir::Type& t
 *  Returns     : bool
 */
bool is_named(const ir::Type& t) { return !t.name.empty(); }


/**
 *  Function    : named_type_cpp
 *  Description : Builds the C++ name of a named type.
 *  Parameters  : t — const ir::Type& t
 *  Returns     : std::string
 */
std::string named_type_cpp(const ir::Type& t) { return cpp_ident(t.name); }


/**
 *  Function    : integer_is_bigint
 *  Description : Returns whether the integer needs the BigInt host type.
 *  Parameters  : d — const ir::IntegerDesc& d
 *  Returns     : bool
 */
bool integer_is_bigint(const ir::IntegerDesc& d) { return !d.host_bits.has_value(); }


/**
 *  Function    : integer_host_type
 *  Description : Builds and returns a string for integer host type.
 *  Parameters  : d — const ir::IntegerDesc& d
 *  Returns     : std::string
 */
std::string integer_host_type(const ir::IntegerDesc& d) {
  if (d.host_bits) {
    if (!d.is_signed) {
      switch (*d.host_bits) {
        case 8:
          return "std::uint8_t";
        case 16:
          return "std::uint16_t";
        case 32:
          return "std::uint32_t";
        default:
          return "std::uint64_t";
      }
    }
    switch (*d.host_bits) {
      case 8:
        return "std::int8_t";
      case 16:
        return "std::int16_t";
      case 32:
        return "std::int32_t";
      default:
        return "std::int64_t";
    }
  }
  return "asn1::BigInteger";
}

/**
 *  Function    : opt_i64_expr
 *  Description : Builds and returns a string for opt i64 expr.
 *  Parameters  : b — const std::optional<ir::BigInt>& b
 *  Returns     : std::string
 */
std::string opt_i64_expr(const std::optional<ir::BigInt>& b) {
  if (b && b->as_i64) {
    return "std::int64_t{" + std::to_string(*b->as_i64) + "}";
  }
  return "std::nullopt";
}

/**
 *  Function    : opt_size_expr
 *  Description : Builds and returns a string for opt size expr.
 *  Parameters  : b — const std::optional<ir::BigInt>& b
 *  Returns     : std::string
 */
std::string opt_size_expr(const std::optional<ir::BigInt>& b) {
  if (b && b->as_i64 && *b->as_i64 >= 0) {
    return "std::size_t{" + std::to_string(static_cast<std::uint64_t>(*b->as_i64)) + "}";
  }
  return "std::nullopt";
}

  /**
   *  Function    : run
   *  Description : Performs run (definition).
   *  Parameters  : header — std::ostream& header; source — std::ostream* source
   *  Returns     : void
   */
void Emitter::run(std::ostream& header, std::ostream* source) {
    collect_named();
    const std::string ns = cpp_ident(options_.namespace_name);
    def_out_ = (source && !options_.emit_inline) ? source : nullptr;
    if (def_out_) {
      // Out-of-line mode: preamble first, namespace opened before the
      // definitions that emit_header streams in, closed afterwards.
      *source << "// Generated by asn1cxx - definitions TU (see "
              << options_.basename << ".hpp for declarations).\n"
              << "#include \"" << options_.basename << ".hpp\"\n\n"
              << "namespace " << ns << " {\n\n";
    } else if (source) {
      *source << "// Generated by asn1cxx - companion TU (types are header-only).\n"
              << "#include \"" << options_.basename << ".hpp\"\n";
    }
    emit_header(header);
    if (def_out_) {
      *source << "}  // namespace " << ns << "\n";
      def_out_ = nullptr;
    }
  }

  /**
   *  Function    : begin_codec_fn
   *  Description : Emits a codec function header. Inline mode writes
   *                "inline <sig> {" to the declaration stream; out-of-line
   *                mode writes "<sig>;" to the header and "<sig> {" to the
   *                definition stream.
   *  Parameters  : decl_out — header stream; sig — full signature text
   *  Returns     : std::ostream* — stream the body must be written to
   */
std::ostream* Emitter::begin_codec_fn(std::ostream& decl_out, const std::string& sig) {
    if (def_out_) {
      decl_out << sig << ";\n";
      *def_out_ << sig << " {\n";
      return def_out_;
    }
    decl_out << "inline " << sig << " {\n";
    return &decl_out;
  }

  /**
   *  Function    : collect_named
   *  Description : Performs collect named (definition).
   *  Parameters  : none
   *  Returns     : void
   */
void Emitter::collect_named() {
    std::unordered_set<ir::TypeId> visiting;
    std::function<void(ir::TypeId)> consider = [&](ir::TypeId id) {
      if (id == ir::kInvalidType || named_set_.count(id) || visiting.count(id)) {
        return;
      }
      const ir::Type& t = model_.arena.get(id);
      if (!is_named(t)) {
        return;
      }
      for (ir::TypeId existing : named_order_) {
        const ir::Type& e = model_.arena.get(existing);
        // C++ names carry no module qualifier, so two arena types with the
        // same name collide in the output even from different modules (the
        // same parameterized instantiation reached via two paths). Dedup on
        // the emitted name.
        if (cpp_ident(e.name) == cpp_ident(t.name)) {
          named_set_.insert(id);
          return;
        }
      }
      visiting.insert(id);
      std::function<void(ir::TypeId)> dep = [&](ir::TypeId d) {
        if (d == ir::kInvalidType) {
          return;
        }
        const ir::Type& dt = model_.arena.get(d);
        if (dt.kind == ir::TypeKind::Referenced &&
            dt.referenced.resolved != ir::kInvalidType) {
          consider(dt.referenced.resolved);
        }
        // An ObjectClassField type is unnamed, so consider() bails on it; its
        // fixed-type governor (e.g. &id -> ProtocolExtensionID) is otherwise
        // never traversed and the member declaration references a missing type.
        if (dt.kind == ir::TypeKind::ObjectClassField) {
          dep(dt.object_class_field.fixed_type);
        }
        // An inline (anonymous) SEQUENCE OF / SET OF is unnamed too; its
        // element must still be emitted before the containing type so the
        // generated codec call resolves at its definition point.
        if (dt.kind == ir::TypeKind::SequenceOf) {
          dep(dt.sequence_of.element);
        }
        if (dt.kind == ir::TypeKind::SetOf) {
          dep(dt.set_of.element);
        }
        consider(d);
      };
      if (t.kind == ir::TypeKind::Sequence) {
        for (const auto& f : t.sequence.root) {
          dep(f.type);
          if (f.table_constraint) {
            for (const auto& e : f.table_constraint->entries) {
              dep(e.type);
            }
          }
        }
        for (const auto& g : t.sequence.extension_groups) {
          for (const auto& f : g.fields) {
            dep(f.type);
            if (f.table_constraint) {
              for (const auto& e : f.table_constraint->entries) {
                dep(e.type);
              }
            }
          }
        }
        for (const auto& f : t.sequence.trailing_root) {
          dep(f.type);
          if (f.table_constraint) {
            for (const auto& e : f.table_constraint->entries) {
              dep(e.type);
            }
          }
        }
      } else if (t.kind == ir::TypeKind::Set) {
        for (const auto& f : t.set.root) {
          dep(f.type);
          if (f.table_constraint) {
            for (const auto& e : f.table_constraint->entries) {
              dep(e.type);
            }
          }
        }
        for (const auto& g : t.set.extension_groups) {
          for (const auto& f : g.fields) {
            dep(f.type);
            if (f.table_constraint) {
              for (const auto& e : f.table_constraint->entries) {
                dep(e.type);
              }
            }
          }
        }
        for (const auto& f : t.set.trailing_root) {
          dep(f.type);
          if (f.table_constraint) {
            for (const auto& e : f.table_constraint->entries) {
              dep(e.type);
            }
          }
        }
      } else if (t.kind == ir::TypeKind::Choice) {
        for (const auto& f : t.choice.alternatives) {
          dep(f.type);
        }
        for (const auto& f : t.choice.extensions) {
          dep(f.type);
        }
      } else if (t.kind == ir::TypeKind::SequenceOf) {
        dep(t.sequence_of.element);
      } else if (t.kind == ir::TypeKind::SetOf) {
        dep(t.set_of.element);
      } else if (t.kind == ir::TypeKind::Referenced) {
        dep(t.referenced.resolved);
      }
      visiting.erase(id);
      named_set_.insert(id);
      named_order_.push_back(id);
    };
    for (ir::TypeId id : model_.exported_types) {
      consider(id);
    }
    for (std::size_t i = 0; i < model_.arena.type_count(); ++i) {
      consider(static_cast<ir::TypeId>(i));
    }
  }

  /**
   *  Function    : type_cpp
   *  Description : Builds and returns a string for type cpp.
   *  Parameters  : id — ir::TypeId id; for_optional — bool for_optional
   *  Returns     : std::string
   */
std::string Emitter::type_cpp(ir::TypeId id, bool for_optional) {
    if (named_set_.count(id)) {
      return named_type_cpp(model_.arena.get(id));
    }
    const ir::Type& t = resolve(model_, id);
    if (is_named(t)) {
      for (ir::TypeId nid : named_order_) {
        const ir::Type& n = model_.arena.get(nid);
        if (cpp_ident(n.name) == cpp_ident(t.name)) {
          return named_type_cpp(n);
        }
      }
      return named_type_cpp(t);
    }
    return inline_type_cpp(t, for_optional);
  }

  /**
   *  Function    : inline_type_cpp
   *  Description : Builds and returns a string for inline type cpp.
   *  Parameters  : t — const ir::Type& t; for_optional — bool for_optional
   *  Returns     : std::string
   */
std::string Emitter::inline_type_cpp(const ir::Type& t, bool /*for_optional*/) {
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        return "bool";
      case ir::TypeKind::Null:
        return cpp_ident(options_.namespace_name) + "_detail::Null";
      case ir::TypeKind::Integer:
        return integer_host_type(t.integer);
      case ir::TypeKind::OctetString:
        return "std::vector<std::uint8_t>";
      case ir::TypeKind::BitString:
        return "asn1::BitStringValue";
      case ir::TypeKind::String:
        return "std::string";
      case ir::TypeKind::SequenceOf: {
        return "std::vector<" + type_cpp(t.sequence_of.element) + ">";
      }
      case ir::TypeKind::SetOf: {
        return "std::vector<" + type_cpp(t.set_of.element) + ">";
      }
      case ir::TypeKind::Enumerated:
        return "std::int64_t";
      case ir::TypeKind::ObjectIdentifier:
      case ir::TypeKind::RelativeOid:
        return "std::vector<std::uint64_t>";
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set:
      case ir::TypeKind::Choice:
        diag_.error({}, "anonymous SEQUENCE/SET/CHOICE nested types are not emitted as "
                         "inline structs; assign a type name");
        return "/*unsupported-anonymous*/void";
      case ir::TypeKind::ObjectClassField:
        if (t.object_class_field.open_type) {
          return "std::vector<std::uint8_t>";
        }
        return type_cpp(t.object_class_field.fixed_type);
      case ir::TypeKind::Real:
        return "double";
      case ir::TypeKind::InstanceOf:
        return "struct { std::vector<std::uint64_t> type_id; std::vector<std::uint8_t> value; }";
      case ir::TypeKind::Referenced:
        if (t.referenced.resolved != ir::kInvalidType) {
          return type_cpp(t.referenced.resolved);
        }
        diag_.error({}, "unresolved type reference '" + t.referenced.name + "'");
        return "void";
    }
    return "void";
  }

  /**
   *  Function    : emit_header
   *  Description : Performs emit header (definition).
   *  Parameters  : out — std::ostream& out
   *  Returns     : void
   */
void Emitter::emit_header(std::ostream& out) {
    const std::string ns = cpp_ident(options_.namespace_name);
    const std::string guard =
        "ASN1_GEN_" + ns + "_" + cpp_ident(options_.basename) + "_HPP_";
    out << "// Generated by asn1cxx - do not edit.\n"
        << "#pragma once\n\n"
        << "#ifndef " << guard << "\n"
        << "#define " << guard << "\n\n";

    const bool emit_jer = (options_.codec & CodecKind::Jer) != CodecKind::None;
    const bool emit_xml = (options_.codec &
                           (CodecKind::Xer | CodecKind::Exer | CodecKind::Cxer)) !=
                          CodecKind::None;
    const bool emit_per = (options_.codec & (CodecKind::Uper | CodecKind::Aper)) !=
                          CodecKind::None;
    const bool emit_oer =
        (options_.codec & (CodecKind::Oer | CodecKind::Coer)) != CodecKind::None;
    const bool emit_tlv =
        (options_.codec & (CodecKind::Ber | CodecKind::Der)) != CodecKind::None;
    out << "#include <algorithm>\n"
        << "#include <asn1/runtime/bigint.hpp>\n"
        << "#include <asn1/runtime/bit_string.hpp>\n";
    if (emit_per) {
      out << "#include <asn1/runtime/bit_io.hpp>\n"
          << "#include <asn1/runtime/byte_io.hpp>\n"
          << "#include <asn1/runtime/per/codec.hpp>\n";
    }
    if (emit_oer || emit_tlv) {
      out << "#include <asn1/runtime/byte_io.hpp>\n";
    }
    if ((options_.codec & CodecKind::Uper) != CodecKind::None) {
      out << "#include <asn1/runtime/uper.hpp>\n";
    }
    if ((options_.codec & CodecKind::Aper) != CodecKind::None) {
      out << "#include <asn1/runtime/aper.hpp>\n";
    }
    if (emit_jer) {
      out << "#include <asn1/runtime/byte_io.hpp>\n"
          << "#include <asn1/runtime/jer/json.hpp>\n"
          << "#include <asn1/runtime/jer/codec.hpp>\n"
          << "#include <asn1/runtime/jeri/codec.hpp>\n";
    }
    if (emit_xml) {
      out << "#include <asn1/runtime/xer/codec.hpp>\n";
      if ((options_.codec & CodecKind::Exer) != CodecKind::None) {
        out << "#include <asn1/runtime/exer/codec.hpp>\n";
      }
      if ((options_.codec & CodecKind::Cxer) != CodecKind::None) {
        out << "#include <asn1/runtime/cxer/codec.hpp>\n";
      }
    }
    if ((options_.codec & CodecKind::Oer) != CodecKind::None) {
      out << "#include <asn1/runtime/oer/codec.hpp>\n";
    }
    if ((options_.codec & CodecKind::Coer) != CodecKind::None) {
      out << "#include <asn1/runtime/coer/codec.hpp>\n";
    }
    if (emit_tlv) {
      out << "#include <asn1/runtime/ber/tlv.hpp>\n"
          << "#include <asn1/runtime/ber/codec.hpp>\n";
      if ((options_.codec & CodecKind::Der) != CodecKind::None) {
        out << "#include <asn1/runtime/der/codec.hpp>\n";
      }
    }
    out << "\n"
        << "#include <cstdint>\n"
        << "#include <optional>\n"
        << "#include <string>\n"
        << "#include <type_traits>\n"
        << "#include <utility>\n"
        << "#include <variant>\n"
        << "#include <vector>\n\n"
        << "namespace " << ns << "_detail {\n"
        << "struct Null {};\n"
        << "}  // namespace " << ns << "_detail\n\n"
        << "namespace " << ns << " {\n\n";

    // Forward declarations for named sequence/choice.
    for (ir::TypeId id : named_order_) {
      const ir::Type& t = model_.arena.get(id);
      if (t.kind == ir::TypeKind::Sequence || t.kind == ir::TypeKind::Set ||
          t.kind == ir::TypeKind::Choice) {
        out << "struct " << named_type_cpp(t) << ";\n";
      }
    }
    out << "\n";

    for (ir::TypeId id : named_order_) {
      emit_type_decl(out, id);
      out << "\n";
    }

    for (ir::TypeId id : named_order_) {
      // Type aliases (`using A = B`) share B's codecs; do not emit duplicates.
      if (model_.arena.get(id).kind == ir::TypeKind::Referenced) {
        continue;
      }
      if ((options_.codec & CodecKind::Uper) != CodecKind::None) {
        emit_codec_fns(out, id, /*aper=*/false);
      }
      if ((options_.codec & CodecKind::Aper) != CodecKind::None) {
        emit_codec_fns(out, id, /*aper=*/true);
      }
      if (emit_jer) {
        emit_jer_codec_fns(out, id);
      }
      if (emit_xml) {
        emit_xer_codec_fns(out, id);
      }
      if (emit_oer) {
        emit_oer_codec_fns(out, id);
      }
      if ((options_.codec & CodecKind::Ber) != CodecKind::None) {
        emit_tlv_codec_fns(out, id, /*der=*/false);
      }
      if ((options_.codec & CodecKind::Der) != CodecKind::None) {
        emit_tlv_codec_fns(out, id, /*der=*/true);
      }
    }

    out << "}  // namespace " << cpp_ident(options_.namespace_name) << "\n\n"
        << "#endif  // " << guard << "\n";
  }

std::string Emitter::table_entry_alt_name(const ir::TableConstraintEntry& entry, std::size_t idx) {
    std::string base;
    if (!entry.id_symbol.empty()) {
      base = cpp_ident(entry.id_symbol);
    } else if (!entry.type_name.empty()) {
      base = cpp_ident(entry.type_name);
    } else if (entry.has_id_value) {
      base = "id_" + std::to_string(entry.id_value.as_i64.value_or(static_cast<std::int64_t>(idx)));
    } else {
      base = "Alt_" + std::to_string(idx);
    }
    return base + "_";
  }

std::vector<std::string> Emitter::compute_table_entry_alt_names(
      const std::vector<ir::TableConstraintEntry>& entries) {
    std::vector<std::string> names;
    std::unordered_map<std::string, int> seen;
    for (std::size_t i = 0; i < entries.size(); ++i) {
      std::string base = table_entry_alt_name(entries[i], i);
      int count = ++seen[base];
      if (count > 1) {
        if (!base.empty() && base.back() == '_') base.pop_back();
        base += "_" + std::to_string(count) + "_";
      }
      names.push_back(std::move(base));
    }
    return names;
  }

void Emitter::emit_table_constraint_structs(std::ostream& out, const std::vector<ir::Field>& fields) {
    for (const auto& f : fields) {
      if (!f.table_constraint || f.table_constraint->entries.empty()) {
        continue;
      }
      const std::string fname = cpp_ident(f.name);
      const auto alt_names = compute_table_entry_alt_names(f.table_constraint->entries);
      out << "\n  // Table constraint specialization for " << fname << "\n";
      out << "  struct Decoded_" << fname << " {\n";
      for (std::size_t ei = 0; ei < f.table_constraint->entries.size(); ++ei) {
        const auto& entry = f.table_constraint->entries[ei];
        out << "    struct " << alt_names[ei] << " { " << type_cpp(entry.type) << " value; };\n";
      }
      out << "    std::variant<\n";
      out << "      std::monostate";
      for (std::size_t ei = 0; ei < f.table_constraint->entries.size(); ++ei) {
        out << ",\n      " << alt_names[ei];
      }
      out << "\n    > choice;\n";
      out << "  };\n";
      out << "  std::optional<Decoded_" << fname << "> decoded_" << fname << ";\n";
    }
  }

Emitter::AliasShape Emitter::alias_shape(const ir::Type& t) const {
    const ir::Type& r =
        (t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType)
            ? resolve(model_, t.referenced.resolved)
            : t;
    switch (r.kind) {
      case ir::TypeKind::Boolean:
      case ir::TypeKind::Real:
      case ir::TypeKind::Enumerated:
        return AliasShape::Scalar;
      case ir::TypeKind::Integer:
        return integer_is_bigint(r.integer) ? AliasShape::Class : AliasShape::Scalar;
      case ir::TypeKind::ObjectClassField:
        if (r.object_class_field.open_type) {
          return AliasShape::Class;
        }
        return alias_shape(resolve(model_, r.object_class_field.fixed_type));
      default:
        return AliasShape::Class;
    }
  }

void Emitter::emit_distinct_alias(std::ostream& out, const std::string& name, const std::string& host,
                           AliasShape shape) {
    if (shape == AliasShape::Scalar) {
      out << "struct " << name << " {\n"
          << "  " << host << " value{};\n"
          << "  " << name << "() = default;\n"
          << "  " << name << "(" << host << " v) : value(v) {}\n"
          << "  operator " << host << "() const { return value; }\n"
          << "};\n";
      return;
    }
    out << "struct " << name << " : " << host << " {\n"
        << "  " << name << "() = default;\n"
        << "  " << name << "(const " << host << "& v) : " << host << "(v) {}\n"
        << "  " << name << "(" << host << "&& v) : " << host << "(std::move(v)) {}\n"
        << "  " << name << "& operator=(const " << host << "& v) {\n"
        << "    static_cast<" << host << "&>(*this) = v;\n"
        << "    return *this;\n"
        << "  }\n"
        << "  " << name << "& operator=(" << host << "&& v) {\n"
        << "    static_cast<" << host << "&>(*this) = std::move(v);\n"
        << "    return *this;\n"
        << "  }\n"
        << "};\n";
  }

/**
 *  Function    : emit_type_decl
 *  Description : Performs emit type decl (definition).
 *  Parameters  : out — std::ostream& out; id — ir::TypeId id
 *  Returns     : void
 */
void Emitter::emit_type_decl(std::ostream& out, ir::TypeId id) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        emit_distinct_alias(out, name, "bool", AliasShape::Scalar);
        break;
      case ir::TypeKind::Null:
        emit_distinct_alias(out, name,
                            cpp_ident(options_.namespace_name) + "_detail::Null",
                            AliasShape::Class);
        break;
      case ir::TypeKind::Integer:
        emit_distinct_alias(out, name, integer_host_type(t.integer), alias_shape(t));
        break;
      case ir::TypeKind::OctetString:
        emit_distinct_alias(out, name, "std::vector<std::uint8_t>", AliasShape::Class);
        break;
      case ir::TypeKind::BitString:
        emit_distinct_alias(out, name, "asn1::BitStringValue", AliasShape::Class);
        break;
      case ir::TypeKind::String:
        emit_distinct_alias(out, name, "std::string", AliasShape::Class);
        break;
      case ir::TypeKind::SequenceOf:
        out << "struct " << name << " {\n"
            << "  std::vector<" << type_cpp(t.sequence_of.element) << "> value;\n"
            << "};\n";
        break;
      case ir::TypeKind::SetOf:
        out << "struct " << name << " {\n"
            << "  std::vector<" << type_cpp(t.set_of.element) << "> value;\n"
            << "};\n";
        break;
      case ir::TypeKind::Enumerated: {
        out << "enum class " << name << " : std::int64_t {\n";
        for (const auto& nn : t.enumerated.root) {
          out << "  " << cpp_ident(nn.name);
          if (nn.value.as_i64) {
            out << " = " << *nn.value.as_i64;
          }
          out << ",\n";
        }
        for (const auto& nn : t.enumerated.extensions) {
          out << "  " << cpp_ident(nn.name);
          if (nn.value.as_i64) {
            out << " = " << *nn.value.as_i64;
          }
          out << ",\n";
        }
        out << "};\n";
        break;
      }
      case ir::TypeKind::ObjectIdentifier:
      case ir::TypeKind::RelativeOid:
        emit_distinct_alias(out, name, "std::vector<std::uint64_t>", AliasShape::Class);
        break;
      case ir::TypeKind::ObjectClassField:
        if (t.object_class_field.open_type) {
          emit_distinct_alias(out, name, "std::vector<std::uint8_t>", AliasShape::Class);
        } else {
          const ir::Type& fixed = resolve(model_, t.object_class_field.fixed_type);
          const std::string host = type_cpp(t.object_class_field.fixed_type);
          // Inherit a named class so this name stays distinct from the governor
          // type. Enumerations cannot be base classes; hold them by value.
          AliasShape shape = alias_shape(fixed);
          if (is_named(fixed) && fixed.kind != ir::TypeKind::Enumerated) {
            shape = AliasShape::Class;
          }
          emit_distinct_alias(out, name, host, shape);
        }
        break;
      case ir::TypeKind::Real:
        emit_distinct_alias(out, name, "double", AliasShape::Scalar);
        break;
      case ir::TypeKind::InstanceOf:
        out << "struct " << name << " {\n"
            << "  std::vector<std::uint64_t> type_id;\n"
            << "  std::vector<std::uint8_t> value;\n"
            << "};\n";
        break;
      case ir::TypeKind::Set: {
        out << "struct " << name << " {\n";
        emit_struct_fields(out, t.set.root, false);
        for (const auto& group : t.set.extension_groups) {
          emit_struct_fields(out, group.fields, true);
        }
        emit_struct_fields(out, t.set.trailing_root, false);
        emit_table_constraint_structs(out, t.set.root);
        for (const auto& group : t.set.extension_groups) {
          emit_table_constraint_structs(out, group.fields);
        }
        emit_table_constraint_structs(out, t.set.trailing_root);
        out << "};\n";
        break;
      }
      case ir::TypeKind::Sequence: {
        out << "struct " << name << " {\n";
        emit_struct_fields(out, t.sequence.root, false);
        for (const auto& group : t.sequence.extension_groups) {
          emit_struct_fields(out, group.fields, true);
        }
        emit_struct_fields(out, t.sequence.trailing_root, false);
        emit_table_constraint_structs(out, t.sequence.root);
        for (const auto& group : t.sequence.extension_groups) {
          emit_table_constraint_structs(out, group.fields);
        }
        emit_table_constraint_structs(out, t.sequence.trailing_root);
        out << "};\n";
        break;
      }
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(t.choice);
        out << "struct " << name << " {\n";
        for (const ir::Field* f : all) {
          out << "  struct " << cpp_ident(f->name) << "_ {\n"
              << "    " << type_cpp(f->type) << " value;\n"
              << "  };\n";
        }
        out << "  std::variant<";
        for (std::size_t i = 0; i < all.size(); ++i) {
          if (i) {
            out << ", ";
          }
          out << cpp_ident(all[i]->name) << "_";
        }
        out << "> alt;\n};\n";
        break;
      }
      case ir::TypeKind::Referenced:
        if (t.referenced.resolved != ir::kInvalidType) {
          out << "using " << name << " = " << type_cpp(t.referenced.resolved) << ";\n";
        } else {
          diag_.error({}, "unresolved type alias '" + name + "'");
          out << "using " << name << " = void;\n";
        }
        break;
    }
  }

  /**
   *  Function    : choice_all_fields
   *  Description : Computes choice all fields from (c).
   *  Parameters  : c — const ir::ChoiceDesc& c
   *  Returns     : static std::vector<const ir::Field*>
   */
std::vector<const ir::Field*> Emitter::choice_all_fields(const ir::ChoiceDesc& c) {
    std::vector<const ir::Field*> all;
    all.reserve(c.alternatives.size() + c.extensions.size());
    for (const ir::Field& f : c.alternatives) {
      all.push_back(&f);
    }
    for (const ir::Field& f : c.extensions) {
      all.push_back(&f);
    }
    return all;
  }

  /**
   *  Function    : field_has_presence_bit
   *  Description : Returns a boolean result from f.
   *  Parameters  : f — const ir::Field& f
   *  Returns     : static bool
   */
bool Emitter::field_has_presence_bit(const ir::Field& f) {
    return f.presence == ir::Presence::Optional || f.presence == ir::Presence::Default;
  }

  /**
   *  Function    : field_wraps_optional
   *  Description : Returns a boolean result from f, force_optional.
   *  Parameters  : f — const ir::Field& f; force_optional — bool force_optional
   *  Returns     : static bool
   */
bool Emitter::field_wraps_optional(const ir::Field& f, bool force_optional) {
    if (f.presence == ir::Presence::Default) {
      return false;
    }
    return force_optional || f.presence == ir::Presence::Optional;
  }

void Emitter::emit_ext_group_field_present(std::ostream& out, const ir::Field& f,
                                             const std::string& expr) {
    if (f.presence == ir::Presence::Default) {
      out << "true";
    } else if (f.presence == ir::Presence::Optional) {
      out << expr << "." << cpp_ident(f.name) << ".has_value()";
    } else {
      out << "true";
    }
  }

  /**
   *  Function    : default_val_cpp
   *  Description : Builds and returns a string for default val cpp.
   *  Parameters  : vid — ir::ValueId vid
   *  Returns     : std::string
   */
std::string Emitter::default_val_cpp(const ir::Field& f) {
    if (!f.default_value) return "";
    const ir::Value& val = model_.arena.get_value(*f.default_value);
    std::string base;
    switch (val.kind) {
      case ir::ValueKind::Boolean:
        base = val.boolean ? "true" : "false";
        break;
      case ir::ValueKind::Integer:
        if (val.integer.as_i64) {
          base = std::to_string(*val.integer.as_i64);
        }
        break;
      case ir::ValueKind::String: {
        // The IR may carry character-string literals with their quotes
        // still attached (lexer capture); normalise before wrapping.
        std::string s = val.text;
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
          s = s.substr(1, s.size() - 2);
        }
        base = "\"" + s + "\"";
        break;
      }
      default:
        break;
    }
    if (base.empty()) return "";
    // An unconstrained INTEGER member is BigInteger: a bare int literal
    // neither converts to it nor compares with it.
    const ir::Type& ft = resolve(model_, f.type);
    if (ft.kind == ir::TypeKind::Integer && integer_is_bigint(ft.integer)) {
      return "asn1::BigInteger::from_i64(" + base + ")";
    }
    return base;
  }

void Emitter::emit_struct_fields(std::ostream& out, const std::vector<ir::Field>& fields,
                          bool force_optional) {
    for (const ir::Field& f : fields) {
      const std::string field_ty = type_cpp(f.type);
      const std::string field_name = cpp_ident(f.name);
      if (field_wraps_optional(f, force_optional)) {
        out << "  std::optional<" << field_ty << "> " << field_name << ";\n";
      } else {
        std::string def_init;
        if (f.presence == ir::Presence::Default) {
          std::string v = default_val_cpp(f);
          if (!v.empty()) {
            def_init = " = " + v;
          }
        }
        out << "  " << field_ty << " " << field_name << def_init << ";\n";
      }
    }
  }

  /**
   *  Function    : type_is_constructed
   *  Description : Returns a boolean result from t.
   *  Parameters  : t — const ir::Type& t
   *  Returns     : static bool
   */
bool Emitter::type_is_constructed(const ir::Type& t) {
    switch (t.kind) {
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set:
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf:
      case ir::TypeKind::Choice:
      case ir::TypeKind::InstanceOf:
        return true;
      default:
        return false;
    }
  }

void CppGenerator::emit(const ir::Model& model, const EmitOptions& options, Diagnostics& diag,
                        std::ostream& header, std::ostream* source) {
  Emitter emitter(model, options, diag);
  emitter.run(header, source);
}

std::string CppGenerator::emit_header_string(const ir::Model& model, const EmitOptions& options,
                                             Diagnostics& diag) {
  std::ostringstream os;
  emit(model, options, diag, os, nullptr);
  return os.str();
}

}  // namespace codegen
}  // namespace asn1
