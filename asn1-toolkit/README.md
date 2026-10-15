# asn1-toolkit

C++17 ASN.1 compiler and codec runtime.

## Status

**Phase 43** -- CER (X.690) codec family.

- CER: indefinite constructed wrappers (9.1), 1000-octet string fragmentation
  (9.2 + 8.6.4), SET/SET OF canonical tag ordering (9.3), canonical strictness
- All-TLV SET decoder fix: untagged-CHOICE SET components now dispatch on the
  union of the choice's alternative tags (BER/DER/CER)
- `cer_golden` fixture + 10 byte-pinned round-trip tests; CER/DER value equivalence
- Full 12-codec emit compile gate: RRC/LPP/S1AP/rrc_slice/tlv_oer_golden/cer_golden

## Requirements

- CMake 3.16+
- C++17 compiler (GCC or Clang; MSVC also accepted for local Windows work)
- Network once at configure time (GoogleTest via FetchContent). For an
  offline build, point FetchContent at a local GoogleTest checkout:
  `cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest`

Primary target: Linux / Ubuntu 24.04 with GCC or Clang.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Sanitizer build (dev / CI)

Build and test under AddressSanitizer + UndefinedBehaviorSanitizer to catch
memory/lifetime and UB bugs. CI runs this as a separate gate.

```bash
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DASN1_ENABLE_SANITIZERS=ON
cmake --build build-san
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ASAN_OPTIONS=detect_leaks=1 \
  ctest --test-dir build-san --output-on-failure
```

```bash
./build/asn1cxx --dump-ast examples/person.asn
./build/asn1cxx --dump-ir examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec uper examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec jer examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec exer examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec ber examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec der examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec cer examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec oer examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec coer examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec cxer examples/person.asn
./build/asn1cxx --dump-ir tests/fixtures/telecom/s1ap_14_4_0/s1ap_14_4_0.asn
```

默认 emit 为 out-of-line：`generated.hpp` 只含类型声明与编解码函数
声明，函数定义全部落在 `generated.cpp`（一个 TU 付一次，其余 TU 只解析
声明）。大模块（如 RRC 全量）消费 TU 编译时间从 ~22s 降到 ~5s。单 TU
小模块可加 `--emit-inline` 恢复全 inline 头（零链接步骤）。

## Layout

```text
include/asn1/common/    Result, Span
include/asn1/support/   SourceLocation, Diagnostics, SourceFile
include/asn1/ast/       AST nodes + printer
include/asn1/frontend/  Token, Lexer, Parser
include/asn1/ir/        Type IR
include/asn1/semantic/  SymbolTable, Analyzer
src/                    implementations
tools/asn1cxx/          compiler driver
tests/                  unit + telecom fixture tests
examples/               sample .asn files
docs/ARCHITECTURE.md    design
docs/usage_guide.md     usage guide (build/CLI quick reference)
docs/comparison_asn1c_asn1tools.md  横向对比: asn1c / Python asn1tools / 本方案
docs/analysis/          analysis & reference-project review notes
docs/analysis/architecture_review_and_optimization.md  架构优势分析与优化方案
```

Reference trees elsewhere in the parent workspace (`pasn1/`, asn1c, `Python_asn1tools/`) are not part of this build.

## License

TBD (open-source; license file will be added when the project is published).
