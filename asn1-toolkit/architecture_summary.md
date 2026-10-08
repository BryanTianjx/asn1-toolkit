================================================================================
当前项目架构分析整理总结 (asn1-toolkit / Phase 41)
路径: /mnt/f/ASN_1/asn1-toolkit/asn1-toolkit
分析基准: 实际文件内容 (CMakeLists.txt, docs/ARCHITECTURE.md, include/, src/, README.md, .git 状态)
================================================================================

一、项目身份
--------------------------------------------------------------------------------
名称:     asn1-toolkit
版本:     0.1.0 (CMake project VERSION)
状态:     Phase 41 - Review Hardening (已完成, 提交 d57dd96)
分支:     asn1-toolkit-phase-39-validation (clean)
版权:     PROCODEC (2026-2031, 作者 tina.cao, 脚本 add_file_headers.py 自动维护)
构建:     CMake 3.16+, C++17 (REQUIRED ON, EXTENSIONS OFF)
平台:     Linux/Ubuntu 24.04 (GCC/Clang 主目标), MSVC 本地可接受
依赖:     GoogleTest (FetchContent, 配置时联网)

二、架构核心原则 (来自 ARCHITECTURE.md 实际内容)
--------------------------------------------------------------------------------
1. 流水线严格分层:
   ASN.1 源码 -> Lexer (手写) -> Parser (递归下降) -> AST -> 语义分析
   -> Type IR (arena + TypeId) -> C++ 生成器 (codegen) -> 生成 .cpp/.hpp
   -> 运行时原语 (BER/DER/PER/OER/COER/XER/CXER/EXER/JER/JERI)

2. 编译器链与运行时完全隔离:
   - 编译器目标 (asn1cxx, frontend, ast, semantic, ir, constraints, codegen)
     不链接任何运行时编解码库
   - 运行时 (ber, der, per, oer, coer, xer, cxer, exer, jer, jeri, runtime)
     不包含 ast/ir/semantic 头文件
   - 生成的 .cpp 是唯一桥梁: 它调用运行时原语，不包含编解码算法本身

3. AST 是语法，IR 是语义:
   - Parser 不解析 IMPORTS、不计算自动标签、不折叠约束
   - Codecs 永不遍历 AST; 只读取已降低到 IR 的描述符 (IntegerDesc 等)

4. 编解码算法集中在运行时:
   - 生成代码为可读的薄层编排 (调用 asn1::per::encode_constrained_whole_number 等)
   - UPER/APER 共享同一 PER 实现，通过 Variant::Unaligned/Aligned 参数区分
   - DER = 规范 BER (最短确定长度、规范 SET 顺序、拒绝构造字符串)

5. 无外部重依赖:
   - 无 Boost
   - 无外部多精度库 (INTEGER > 64bit 使用内部 BigInteger)
   - 使用 std::optional / std::variant / std::string_view (C++17)

三、CMake 目标结构 (来自 CMakeLists.txt 实际内容)
--------------------------------------------------------------------------------
编译器链 (静态库):
  asn1_common (INTERFACE)   -> Result<T>, Span<T> (无外部 std::span 依赖)
  asn1_support              -> diagnostics, source_file, source_location
  asn1_ast                  -> ast/print.cpp
  asn1_frontend             -> token, lexer, parser, parser_ioc
  asn1_ir                   -> ir/type.cpp, ir/print.cpp
  asn1_constraints          -> constraints/normalize.cpp
  asn1_semantic             -> symbol_table, analyzer
  asn1_codegen              -> codegen/emit.cpp (生成 C++ 源码, 不链接 runtime)

运行时 (静态库):
  asn1_runtime              -> byte_io, bit_io, bigint
  asn1_ber                  -> ber/tlv, ber/codec
  asn1_der                  -> der/tlv, der/codec (依赖 ber)
  asn1_per                  -> per/primitives, per/codec (UPER/APER 共享)
  asn1_oer                  -> oer/codec
  asn1_coer                 -> coer/codec (依赖 oer)
  asn1_xer                  -> xer/xml, xer/codec
  asn1_cxer                 -> cxer/codec (依赖 xer)
  asn1_exer                 -> exer/codec
  asn1_jer                  -> jer/json, jer/codec
  asn1_jeri                 -> jeri/codec (依赖 jer)

可执行文件:
  asn1cxx (tools/asn1cxx/main.cpp)
  链接: frontend + semantic + codegen (不包含 runtime)

测试:
  option ASN1_BUILD_TESTS ON -> add_subdirectory(tests)

四、源码目录映射 (已验证文件存在)
--------------------------------------------------------------------------------
include/asn1/          src/                  说明
--------------------- ---------------------- ---------------------------
common/result.hpp     -                     Result<T>, Error
common/span.hpp       -                     Span<T>
common/float_conv.hpp -                     ASN.1 REAL / IEEE-754
support/              src/support/          SourceLocation, Diagnostics
ast/                   src/ast/              AST 节点 + 打印
frontend/             src/frontend/         Token, Lexer, Parser, Parser IOC
semantic/             src/semantic/         SymbolTable, Analyzer
constraints/          src/constraints/        约束归一化
ir/                    src/ir/                Type IR (TypeArena, TypeId)
codegen/              src/codegen/          CppGenerator, EmitOptions
runtime/              src/runtime/          所有编解码运行时
runtime/ber/          src/runtime/ber/      BER
runtime/der/          src/runtime/der/      DER
runtime/per/          src/runtime/per/      PER (UPER/APER)
runtime/oer/          src/runtime/oer/      OER
runtime/coer/         src/runtime/coer/     COER
runtime/xer/          src/runtime/xer/      XER
runtime/cxer/         src/runtime/cxer/     CXER
runtime/exer/         src/runtime/exer/     EXER
runtime/jer/          src/runtime/jer/      JER
runtime/jeri/         src/runtime/jeri/     JERI

五、Phase 41 硬化内容 (README.md + ARCHITECTURE.md + 提交 d57dd96)
--------------------------------------------------------------------------------
- 对象类字段重新解析 (typed IE id/criticality; &Value 保持开放类型)
- DEFAULT 作为普通字段 (非 optional)
- 共享 BitStringValue
- BER/DER 扩展组
- PER 约束 INTEGER 拒绝超范围 (无钳制)
- 编解码长度上限 (安全限制)
- 完整 RRC/LPP/S1AP emit gates (header smoke)
- Person + rrc_slice 编译往返验证

六、已知未完成 / 后续 (文档明确标注, 非隐藏缺陷)
--------------------------------------------------------------------------------
- 表约束类型化开放类型 (table-constraint typed IE open types) - TBD
- 完整模块编译 CI (emit 成功 ≠ 3GPP 编解码完全对等) - 后续工作
- 许可证文件: README 标注 TBD (开放源码, 发布时添加)

七、测试与电信验证
--------------------------------------------------------------------------------
Fixtures (tests/fixtures/telecom/):
  RRC Rel-8  (EUTRA-RRC-Definitions.asn)  -> parse + analyze + UPER emit
  LPP Rel-14 (LPP-PDU-Definitions.asn)      -> parse + analyze + UPER emit
  S1AP Rel-14 (s1ap_14_4_0.asn)           -> parse + analyze + APER emit (ProtocolIE-Field)
  RRC slice  (rrc_slice.asn)              -> UPER/OER emit + UPER 往返对照已知 hex

关键测试二进制: asn1_telecom_tests, asn1_telecom_roundtrip_tests, asn1_codegen_roundtrip_tests

八、与参考项目关系 (已审查目录实际内容)
--------------------------------------------------------------------------------
参考树 (ARCHITECTURE.md 提及):
  pasn1/            -> 早期 Procodec 实现 (混合解析器+生成器, 反面参考)
  Python_asn1tools/ -> Python 开源实现 (功能受限, 无 CLASS/参数化/完整约束)
  asn1c             -> 经典 ASN.1 编译器 (外部参考)
  FFASN1dump/        -> 仅二进制分发 (无源码)

本项目与参考的核心差异:
  - 严格分离的编译器流水线 (AST/IR/codegen/runtime)
  - 完整的编码规则覆盖 (包括 CXER/EXER/JER/JERI)
  - 电信 fixtures 完整覆盖 (3GPP RRC/LPP/S1AP)
  - 商业化版权和自动维护脚本 (add_file_headers.py)

九、风险与限制 (基于实际文件提取)
--------------------------------------------------------------------------------
1. 编解码完整性: 电信测试仅为 parse+analyze+emit + 部分往返; 不声称完整 3GPP 对等
2. 许可证: 源码头已标 PROCODEC 版权, 但正式许可证文件尚未添加 (README TBD)
3. 构建依赖: 需要网络访问 (FetchContent GoogleTest); 离线构建需预缓存
4. 后续工作已在文档中明确标注 (表约束类型化、完整 CI), 不构成隐藏缺陷

十、工程实践观察
--------------------------------------------------------------------------------
- 构建系统规范 (CMake 3.16+, C++17 严格标准)
- 模块边界清晰 (每个库有独立 include + src, 无混合头文件依赖)
- 测试覆盖全面 (每编码规则独立目录 + 电信 fixtures + 往返测试)
- 文档完整 (README + ARCHITECTURE.md 383 行 + 每源文件头部描述 + 自动脚本维护版权)
- 版本控制清洁 (分支 clean, 提交历史清晰)
- 生成代码可读性优先 (codegen 不生成私有实现, 而是调用共享原语)

================================================================================
总结
================================================================================
asn1-toolkit 是一个结构严谨、分层清晰、文档完整、已进入商业化阶段 (PROCODEC)
的 C++17 ASN.1 编译器与编解码运行时。Phase 41 硬化已完成核心编码规则、代码生成、
电信 fixtures 发射、对象类重解析和约束强化。剩余后续工作 (表约束类型化开放类型、
完整编解码对等验证、许可证文件) 已在文档中明确标注，可作为下一阶段计划依据。

归档文件:
  /mnt/f/ASN_1/asn1-toolkit/asn1-toolkit/analysis_report.md        (全面分析)
  /mnt/f/ASN_1/asn1-toolkit/asn1-toolkit/reference_project_review.md (参考项目对比)
  /mnt/f/ASN_1/asn1-toolkit/asn1-toolkit/architecture_summary.md     (本文件)
================================================================================
