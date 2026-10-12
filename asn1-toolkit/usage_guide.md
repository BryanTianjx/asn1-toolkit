================================================================================
asn1-toolkit 使用说明 (基于实际 README.md + CMakeLists.txt + 目录内容)
===========================================================================

一、环境与构建
--------------------------------------------------------------------------------
要求: CMake 3.16+, C++17 编译器 (GCC/Clang/MSVC), 构建时需要网络 (FetchContent 获取 GoogleTest)

构建命令 (在项目根目录执行):
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  ctest --test-dir build --output-on-failure

构建产物位置: build/asn1cxx (编译器驱动可执行文件)

二、编译器驱动 (build/asn1cxx) 使用
--------------------------------------------------------------------------------
支持操作 (来自 README 实际命令示例):

1. 打印 AST (语法树):
   ./build/asn1cxx --dump-ast examples/person.asn

2. 打印类型 IR (语义降低结果):
   ./build/asn1cxx --dump-ir examples/person.asn

3. 生成 C++ 代码 (指定编解码规则):
   ./build/asn1cxx --emit-dir build/gen --codec uper  examples/person.asn
   ./build/asn1cxx --emit-dir build/gen --codec aper  examples/person.asn
   ./build/asn1cxx --emit-dir build/gen --codec ber   examples/person.asn
   ./build/asn1cxx --emit-dir build/gen --codec der   examples/person.asn
   ./build/asn1cxx --emit-dir build/gen --codec oer   examples/person.asn
   ./build/asn1cxx --emit-dir build/gen --codec coer  examples/person.asn
   ./build/asn1cxx --emit-dir build/gen --codec jer   examples/person.asn
   ./build/asn1cxx --emit-dir build/gen --codec xer   examples/person.asn
   ./build/asn1cxx --emit-dir build/gen --codec cxer  examples/person.asn
   ./build/asn1cxx --emit-dir build/gen --codec exer  examples/person.asn

可用 --codec 选项: {uper, aper, both, jer, xer, exer, ber, der, oer, coer, cxer}

4. 电信 fixtures 测试 (解析 + 分析 + IR 输出验证):
   ./build/asn1cxx --dump-ir tests/fixtures/telecom/s1ap_14_4_0/s1ap_14_4_0.asn

三、示例文件
--------------------------------------------------------------------------------
位置: examples/
  person.asn    - 简单 SEQUENCE (id INTEGER, name UTF8String, age INTEGER OPTIONAL)
  smoke.asn     - 基础测试
  ioc.asn       - 信息对象类 (IOC) 相关
  phase12.asn   - 早期阶段示例

使用示例: 编译并生成 UPER 编解码代码
  ./build/asn1cxx --emit-dir build/gen --codec uper examples/person.asn

四、测试执行
--------------------------------------------------------------------------------
单位测试 + 电信 fixtures 测试 (构建后):
  ctest --test-dir build --output-on-failure

测试目录结构 (tests/):
  aper/, ber/, coer/, cxer/, der/, exer/, jer/, jeri/, oer/, per/, uper/ - 各编码规则
  codegen/         - 代码生成测试
  common/          - 共享工具测试
  constraints/     - 约束测试
  telecom/         - 3GPP 电信 fixtures 测试 (RRC/LPP/S1AP)
  runtime/         - 运行时原语测试

五、代码生成与运行时关系 (架构说明)
--------------------------------------------------------------------------------
- 编译器 (asn1cxx) 生成 .cpp/.hpp 文件，文件中包含类型定义和编解码调用
- 生成代码不包含编解码算法实现，而是调用运行时库函数 (如 asn1::per::encode_...)
- 运行时库 (build 产物中对应的静态库) 提供实际的 BER/DER/PER/OER 等实现
- 编译生成的代码需要与运行时库链接才能执行编解码

六、参考与文档
--------------------------------------------------------------------------------
- 架构设计: docs/ARCHITECTURE.md (383 行, 完整流水线、编码规则行为、阶段记录)
- 参考树说明: docs/ARCHITECTURE.md 第 1 行 (pasn1/, asn1c, Python_asn1tools/, FFASN1dump/ 不属于本构建)
- 版权头维护脚本: scripts/add_file_headers.py (自动添加 PROCODEC 头信息到 .cpp/.hpp)
- 分析报告归档 (本项目根目录已生成):
    analysis_report.md           - 全面分析
    reference_project_review.md  - 参考项目对比
    architecture_summary.md      - 架构整理总结

七、注意事项与限制
--------------------------------------------------------------------------------
1. 构建时需要网络访问 (FetchContent 获取 GoogleTest); 如离线构建需预先缓存依赖
2. 电信 fixtures 测试覆盖 parse + analyze + emit，不声称完整 3GPP 编解码完全对等
3. 许可证文件尚未添加 (README 标注 TBD); 代码头已标注 PROCODEC 版权 (2026-2031)
4. 后续工作已明确记录: 表约束类型化开放类型、完整编译 CI 为 Phase 41 之后任务

八、快速命令清单
--------------------------------------------------------------------------------
构建:     cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
测试:     ctest --test-dir build --output-on-failure
AST 打印: ./build/asn1cxx --dump-ast examples/person.asn
IR 打印:  ./build/asn1cxx --dump-ir examples/person.asn
生成 UPER: ./build/asn1cxx --emit-dir build/gen --codec uper examples/person.asn
生成 JER:  ./build/asn1cxx --emit-dir build/gen --codec jer examples/person.asn
电信 IR: ./build/asn1cxx --dump-ir tests/fixtures/telecom/s1ap_14_4_0/s1ap_14_4_0.asn
================================================================================
归档文件: /mnt/f/ASN_1/asn1-toolkit/asn1-toolkit/usage_guide.md (本文件)
================================================================================
