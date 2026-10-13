================================================================================
asn1-toolkit 架构分析与优化方案
分析时间: 2026-10-13 (Phase 41+, commit ce7d2a2)
================================================================================
本文基于本地源码实测: 模块行数、emit 产物规模、单 TU 编译耗时、
编解码器实现路径, 非设计文档转述。所有数字可复现 (见每节"实测"标注)。

--------------------------------------------------------------------------------
1. 架构总览
--------------------------------------------------------------------------------
流水线 (硬性分层, 见 docs/ARCHITECTURE.md):
  ASN.1 → Lexer → Parser → AST → Semantic(符号表/标签/约束四趟)
        → Type IR (TypeArena, TypeId 寻址) → C++ 代码生成
        → 生成值类型 + 薄编排 → 运行时原语库
        (BER/DER/PER/OER/COER/XER/CXER/E-XER/JER/JERI)

三条铁律:
  1. 编译器库从不 include 编解码算法;
  2. 运行时从不 include AST/符号表/IR 头;
  3. 生成的 .cpp 是唯一桥梁, 只调用运行时原语。

规模实测 (src + include, 75 文件):
  emit.cpp            5,208 行 / 103 个成员函数   (最大单文件)
  analyzer.cpp        2,460 行 / 45 个成员函数
  parser.cpp          2,244 行
  ber/codec.cpp       1,630 行
  per/codec.hpp       1,198 行 (header)
  ast/type.hpp        1,184 行 (header)
  其余各 500-1,200 行, 共 35,821 行 C++17
测试: 8615 行 / 21 个测试域 / 296 项 CTest

--------------------------------------------------------------------------------
2. 核心优势 (每项附证据)
--------------------------------------------------------------------------------
优势 1: 严格分层 — 编译期与运行期彻底解耦
  证据: 链接规则显式声明 (asn1cxx 只链编译链, asn1_codegen 不链运行时,
  generated TU 才链编解码库)。这是 pasn1 (解析器与 BER/UPER 生成器混在
  一个编译器) 被本项目文档明确列为反面教材后确立的规则。
  价值: 编解码器可独立单测 (Phases 6-10 不等 codegen); 运行时可被
  手写代码直接使用, 不必经过 asn1cxx。

优势 2: 编码规则覆盖最全, 且实现复用而非复制
  10 种: UPER/APER/BER/DER/OER/COER/XER/CXER/E-XER/JER/JERI
  (对比: asn1c 6 种, Python asn1tools 8 种, 见对比文档)
  - UPER 与 APER 是同一 PER 实现的两个 Variant 参数, 对齐只发生在
    X.691 要求的原语内部 (非 "UPER+末尾填充");
  - DER = BER + 规范化策略; COER = OER + 规范化策略;
  - CXER = XER + 规范化策略; E-XER/JERI 是指令效果层。
  价值: 修复一个 PER 位布局 bug 同时惠及 UPER/APER; 策略类规则
  (如最短长度) 只写一次。

优势 3: 多编码规则组合生成 (--codec uper,ber)
  本轮新增: 位掩码 + 逗号组合/重复累加, asn1c/asn1tools 均为单选。
  5 项 CLI 行为测试 + Release/sanitizer 双构建 296/296 验证。

优势 4: 信息对象类完整支持 — 电信协议的入场券
  CLASS/对象集/表约束/参数化类型/版本括号/INSTANCE OF/EXTERNAL/
  EMBEDDED PDV/CHARACTER STRING/REAL/BigInt(>64 位) 全部落地;
  Python asn1tools 明确不支持 CLASS/参数化 (其 README 自述);
  表约束 &Value 开放类型已实现 governor id dispatch + typed 视图
  (decoded_<field>)。
  价值: RRC/LPP/S1AP 这类依赖 ProtocolIE-Field 的真实协议栈可解析。

优势 5: 字节级金标准差分验证 — 唯一做位布局证明的方案
  PER/APER: 以 asn1c 为外部 oracle 生成精确字节向量, 54 向量断言;
  边缘规则 (X.691 19.9 扩展 addition preamble、版本括号组) 按 ITU
  原版 PDF 裁定, 不信"参考编译器"声誉。
  价值: 自往返一致不能证明位布局正确 (两份实现可以都错得一致);
  金标准向量是 3GPP 互操作的前提。

优势 6: Result<T> 无异常错误模型 + 位置诊断
  全链路 Result (仅 std::bad_alloc 可传播); 前端/语义诊断一律
  file:line:column; 编解码错误带 bit/byte 偏移; 禁止无信息失败。
  价值: 嵌入式/电信代码库普遍禁用异常, 该模型与目标场景匹配。

优势 7: 电信实战 + 编译门禁
  RRC Rel-8 / LPP Rel-14 / S1AP Rel-14 全量 fixtures; emit 产物作为
  CMake 静态库编译门 (emit 成功 ≠ 可编译, 这是 skill 固化的核心
  纪律); rrc_slice UPER/OER emit + 已知 hex 往返。

优势 8: 零外部依赖
  无 Boost、无 multiprecision (BigInt 自研)、C++17 only
  (不用 std::span/std::expected, 自备 Result/Span)。
  价值: 交叉编译到工具链受限的目标平台无障碍。

--------------------------------------------------------------------------------
3. 量化现状 (实测, 2026-10-13)
--------------------------------------------------------------------------------
指标                              数值
--------------------------------- ----------------------------------------
RRC 全模块 emit 耗时              0.425s (EUTRA-RRC-Definitions.asn)
RRC 生成产物                      1 个 generated.hpp, 33,697 行, 全 inline
生成的 generated.cpp              2 行 stub (仅 #include "generated.hpp")
单 TU 全量编译 (含该头)           23.0s  ← 实测
单 TU 语法检查 (-fsyntax-only)    19.3s  ← 实测
asn1_codegen 库重编译 (改 emit.cpp 后, -j1)  18.9s  ← 实测
测试                              296 项, Release 6.0s / sanitizer 32.5s
金标准差分覆盖                    仅 PER/APER (54 向量); 其余 codec 仅自往返

--------------------------------------------------------------------------------
4. 问题与优化方案 (按优先级)
--------------------------------------------------------------------------------

[P0-1] 生成代码全 inline 于单头文件 — 集成成本数量级问题
  现状证据: 33,697 行的 generated.hpp, 每个类型的 encode_uper/
  decode_uper 都是 inline 自由函数; companion TU (generated.cpp)
  只 include 头、不产出任何符号。任何包含该头的 TU 支付 23s 编译;
  多 TU 工程 = N × 23s。
  方案: 生成头只放类型声明 + 编解码函数声明 (非 inline),
  函数定义全部落到 generated.cpp (一次性 23s), 后续各 TU 只付
  声明解析 (~百 ms 级)。保留开关 --emit-inline 兼容现状 (单 TU
  小模块仍可全 inline, 零链接步骤)。
  预期收益: 多 TU 电信集成编译时间数量级下降; 产物头从 33.7k 行
  缩到声明级 (约 1/5); IDE 索引同样受益。
  风险: ODR 行为变化 (当前 inline 是每 TU 一份, 改后单一定义);
  CMake 编译门需确认 generated.cpp 真正参与编译 (当前 stub 意味着
  门禁实际在编译 .hpp, 改后需保证门禁仍覆盖 .cpp)。
  验证: 以 RRC fixture 重测单 TU 编译耗时; 296 测试 + 金标准向量
  不回归。

[P0-2] emit.cpp 单文件 5,208 行 / 103 成员函数 — 编译与维护瓶颈
  现状证据: 改动 emit.cpp 后单线程重建 asn1_codegen 需 18.9s;
  单文件承载 PER/OER/BER/TLV/JER/XER 全家族的发射逻辑。
  方案: 按 codec 家族拆为 emit_per.cpp / emit_jer.cpp / emit_xer.cpp /
  emit_tlv.cpp / emit_core.cpp (类型声明/工具函数), Emitter 类拆为
  partial class 或 改为按家族的自由函数组 + 共享上下文结构。
  预期收益: 单家族改动的重编译从 18.9s 降到 ~5s; 并行编译收益;
  评审半径缩小 (JER 改动不再动 PER 代码路径)。
  风险: 成员函数互相引用密集 (inline_type_cpp 等被全家族使用),
  拆分边界需先梳理依赖; 无功能风险。
  验证: 各 codec round-trip 测试 + fixture 编译门不回归。

[P1-1] 金标准差分验证从 PER 扩展到其余 codec
  现状证据: tests/per/golden 是唯一 oracle 差分; OER/BER/DER/JER/
  XER 等只有自往返测试 (自一致 ≠ 位布局正确, 见 skill 纪律)。
  方案: 以 asn1c (BER/DER/OER/XER) + asn1tools (JER/GSER) 为 oracle,
  按 per_golden_vectors_test.cpp 的成熟模式扩展到至少 OER/BER/DER;
  JER 文本形态可用 asn1tools 差分。
  预期收益: OER 系 (3GPP R17+ 新 IE 方向) 获得字节级正确性证明。
  风险: oracle 自身偏差需按 ITU PDF 逐条裁定 (asn1c 已知
  BIT STRING 尾零剥离问题), 工作量主要在裁定而非代码。
  验证: 每新增一族金标准向量, 先跑通再合入; 冲突条款记录到
  references/per-differential-validation.md 同类文档。

[P1-2] 补齐 CER (X.690)
  现状证据: --codec 无 cer; asn1c 支持 CER (对比文档第 1 节)。
  方案: DER 的策略层已具备最短长度/规范 SET 序, CER = BER + 规范化
  策略 (DER 的姊妹), 实现模式与 DER=BER 策略完全同构, 增量小。
  预期收益: 编码规则覆盖 10 → 11, 追平 asn1c 全集;
  价值: 某些证书/协议仍要求 CER。
  风险: CER 的 SET OF 序规则与 DER 不同 (编码值序而非标签序),
  需按 X.690 实现; 工作量中等。
  验证: 金标准向量差分 (asn1c oracle 有 CER)。

[P2-1] bit_io 逐位写路径优化
  现状证据: BitWriter::put_bit 每比特做除法取字节下标 + 分支 + OR;
  UPER 的 presence bitmap/扩展 bitmap 大量逐位调用。
  方案: put_bits 已有; 让 presence bitmap 走一次 put_bits(打包值)
  或在 BitWriter 内缓存当前字节, 消除逐位重复计算。
  预期收益: PER 编码吞吐提升 (bitmap 密集的消息类型显著);
  对 BIT STRING 内容路径 (put_bits(Span)) 无影响。
  风险: 对齐语义 (align_to_octet 时机) 不能变, 改动需金标准向量
  零回归守门。
  验证: per_golden_vectors_test 全部通过 + benchmark 对比
  (tests/codegen/codec_benchmark_test.cpp 已有框架)。

[P2-2] 大模块分文件/分命名空间 emit
  现状证据: RRC 单头 33,697 行 (即便 P0-1 拆声明后仍可观);
  Phase 40 已有 synthetic nested names 基础。
  方案: --emit-split 按 ASN.1 模块边界生成 <Module>.hpp/.cpp 对 +
  聚合头; 头内类型命名空间与模块名对齐 (利用已有 nested names)。
  预期收益: 单文件可读性、增量编译 (改一个模块只重编该 .cpp)、
  IDE 性能。
  风险: 跨模块类型引用需聚合 include 管理, emitter 依赖收集
  (collect_named) 已具备遍历基础, 但 include 生成逻辑是新增面。
  验证: 用 LPP (多模块) fixture 验证拆分正确性。

[P2-3] 增量代码生成 (src 未变则跳过)
  现状证据: 用户既有工作流偏好 (src 未变不重跑 generate/preprocess);
  当前 asn1cxx 每次全量 emit。
  方案: asn1cxx 增加内容哈希缓存 (源文件 hash → 已生成产物),
  命中则跳过; 与 --emit-split 天然配合 (按模块粒度缓存)。
  预期收益: 大模块重复 emit (0.425s) 归零; 集成构建系统收益。
  风险: 低 (纯性能优化, 产物内容不变)。
  验证: 同一输入二次运行跳过 + 修改后正确失效。

[P3-1] 前端并行分析 (多模块)
  现状证据: 语义分析四趟串行, 大规范 (S1AP 多模块) 可感知。
  方案: 第一趟符号声明按模块并行, 后三趟 (引用解析/标签/约束)
  按依赖图并行; 需线程安全的 Diagnostics (加锁聚合)。
  预期收益: 大规范分析阶段提速; 改动面大, 收益待测。
  风险: 中 (符号表并发插入顺序影响诊断可重现性)。
  验证: 诊断输出逐字节不变 (可重现性是硬要求)。

--------------------------------------------------------------------------------
5. 明确不做的优化 (守住优势)
--------------------------------------------------------------------------------
- 不动分层铁律: 不为了性能把 codec 算法塞进编译器或让运行时
  依赖 IR (优势 1 的根基);
- 不引入解释式/反射式运行时: 与 "生成薄编排 + 共享原语" 的
  架构决策冲突, 且牺牲类型安全与性能;
- 不引入 Boost/multiprecision: 优势 8 的根基;
- 不把金标准向量换成"信任 oracle": 边缘规则永远按 ITU PDF 裁定。

--------------------------------------------------------------------------------
6. 实施路线建议
--------------------------------------------------------------------------------
第 1 步 (本周): P0-1 生成代码 out-of-line (收益最大, 风险可控),
  以 RRC fixture 做前后编译耗时对比 + 全量 296 测试守门;
  同步更新 CMake 门禁确保 generated.cpp 真正被编译。
第 2 步: P0-2 emit.cpp 按 codec 家族拆分 (先画依赖图再动手)。
第 3 步: P1-1 OER/BER/DER 金标准扩展 (正确性投资, 服务 3GPP R17+
  OER 方向)。
第 4 步: P2-1 bit_io 批量位写 + benchmark 量化。
后续: P1-2 CER、P2-2 分文件 emit、P2-3 增量生成按需排期。

每步验证口径统一: ctest 296/296 (Release + sanitizer) + 受影响
fixture 的产物 g++ -fsyntax-only 0 error + 金标准向量零回归。
================================================================================
