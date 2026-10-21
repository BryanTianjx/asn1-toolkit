# 第五批 REV-20..24 验收记录（2026-10-21）

本批是能力盘点、已确认缺陷修复、profile 和独立验证基础设施建设，
不宣称实现整个 X.680 系列或全部规范策略。保留分层、Result、纯 C11、
共享 PER Variant、零第三方运行时依赖及三种产物形态。

## REV-20：分层能力登记

新增 [标准能力及 oracle 矩阵](standard-capabilities.md)，记录 2021 基线、
未完成逐条勘误核对、语法/IR/生成/runtime/独立验证、C/C++ 区别。
条目引用实现与具体测试；21 个可重复最小 schema 探针区分接受与拒绝，
不把生成成功作为线路证据。Phase49 C emitter 的延期状态保持。

## REV-21：约束与高频结构

| 子项 | 最小 schema/原行为 | 预期与修复 |
|---|---|---|
| REV-21a | `A ::= INTEGER (unknown..10)`；归一化仅 warning，可能进入 unconstrained IR | 无法计算约束就报错，阻止生成；不是猜测一个 PER 范围。下/上界、single value 和未知形式都改为 error |
| REV-21b | `Base ::= INTEGER (0..255) A ::= Base (0..7)`；引用类型路径没有保存新增约束 | 现阶段 Unsupported，禁止生成按基类型范围编码的伪成功产物；可靠合并继承约束另立功能项 |
| REV-21c | `SEQUENCE { x INTEGER } (WITH COMPONENTS { x PRESENT })`；结构 IR 无该约束 | 对普通结构及 Boolean/Null/Enumerated 上无法保存的约束报错；不是丢掉约束后继续 |
| REV-21d | `OCTET STRING (SIZE(-1..4))`、`INTEGER ((0..1) ^ (3..4))` | 负 SIZE 不转换成巨大 unsigned 上界；空交集不能按无约束可编码类型生成，改为 error |
| REV-21e | S1AP `SIZE(0..maxProtocolIEs)`；旧值解析未追踪 import，错误降成 unconstrained | 沿模块 import 和值别名解析，检测循环；禁止挑选无关模块同名常量，补三模块回归 |
| REV-21f | `Param {INTEGER: upper} ::= SEQUENCE { value INTEGER (0..upper) }`；值实参没用于约束，实例键只含 V0 | 将值实参及调用模块来源带入解析与实例键；嵌套调用保留来源。Small{3}/Wide{7} 对值 3 分别产生 `C0`（2 bit）/`60`（3 bit），三形态真实调用验证 |
| REV-21g | `Record.count INTEGER(0..255)`；TLV/XML/JSON 等路径读取 int64 后直接 cast，300 可窄化成 44 | 五个 emitter 家族共享 native INTEGER 解码检查，转换前核查宿主范围及非扩展根区间成员；三形态验证 300 和 Small{3} 的值 4 均失败 |

这些失败输入的“预期线路”均为**不产生编码器**，而非挑选近似字节；
CLI 探针保存实际 schema、退出码、diagnostic，语义单测进入 CTest。
原有并/交区间、自动标签、参数化、对象集和组件关系测试继续回归。
严格门禁首先暴露 S1AP 导入/值实参边界错误，首次完整构建失败的证据保留在
`batch5-import-bound-repro.log`；修复原因而非恢复 warning，随后重新验收。
FROM、COMPONENTS OF、数字版本扩展组、现代时间/IRI 类型的真实拒绝已登记，
没有将它们误称为新增支持。

## REV-22：规范策略与合法替代表示

先新增 `StandardPolicy` 最小负例：修复前 7 项中 6 项失败，日志
`batch5-repro.log` 保留。具体修复：

| 子项 | 实际/预期线路 | 修复与证据 |
|---|---|---|
| REV-22a | `01 01 01` 是 BER TRUE，但不满足 CER/DER 规范 TRUE | CER 共享 DER 原语策略检查，保持 CER 构造/分片逻辑；新增 CER→DER→BER 的 runtime 目标依赖，没有 compiler/runtime 耦合 |
| REV-22b | REAL `09 03 80 FF 02` 表示 1，是 BER 的合法替代，但 mantissa 为偶数 | DER/CER 拒绝非规范 binary 表示；decimal 检查 NR3/最短词法。不是把所有 BER 表示强行规范化后接受 |
| REV-22c | `Def ::= SEQUENCE {count INTEGER DEFAULT 7}`，BER `30 03 80 01 07`；CER 对应 indefinite 构造 | BER 接受显式默认值，DER/CER 拒绝；SET 同样处理。outline/inline/split 均真实运行 |
| REV-22d | SET 两次出现同一字段，原先可能覆盖前值；短非末尾 CER 分片/非末尾 BIT STRING unused bits 未检查 | SET 所有字段增加 seen 检查；CER 检查非末尾 1000 内容字节、末尾 unused bits 和片段策略 |
| REV-22e | CER 手写 TLV 长度累积可绕过公共长度检查、递归无界 | 使用公共 TLV header 解码和 CER 最短/构造策略校验；内层递归限 64，累计内容受 kMaxCodecBytes 限制；深嵌套、超宽长度负例和 fuzz 回归 |

补充 OER 合法长长度与 COER 非规范形式的成对测试、CER 999/1000/1001 正例，
保留既有 BIT STRING 分片和嵌套 EOC 测试。BER nested indefinite helper 仍明确
Unsupported，未为通过测试而改成 CER 专用行为；COER/XML 的独立验证边界见下。

## REV-23：准确 profile 与数值失败

[REAL/字符/时间 profile](real-and-string-profile.md) 是此次的公开边界：

- REV-23a：BER binary REAL 不再用 double 累积任意 mantissa 后静默舍入；
  去除尾零后核查 53 位有效精度、范围及次正规精确性。不支持时返回 Unsupported。
  公共 content 解码接口也显式检查 kMaxCodecBytes，不能绕过 TLV 层资源上限。
  decimal 采用明确的有界精确子集。正负零区分，encoder 的负零内容修为 `43`。
- REV-23b：JER bigint 的 `number` 字段可能不是有效值，原 decode_real 会读出
  截断/默认整数；现在拒绝该路径以及不能精确表示的 int64。**完整精确 REAL 模型仍延期**，
  XML/JSON 文本转换的 host-double 语义如实公开，不宣称精确十进制支持。
- REV-23c：OER binary32 encode 对有限输入比较转换前后值，拒绝静默舍入；
  无法识别并保真的 REAL WITH COMPONENTS 不再降成 unconstrained。
- 21 项探针包含现代时间/IRI 的拒绝；传统字符串 repertoire、时间规范词法的
  待核查项分开登记，没有改成普通字符串假装支持。

## REV-24：外部对验、负例与 fuzz

新增 `tests/fixtures/standards.asn`、独立驱动、
`scripts/validate_standard_oracles.py` 和 `tests/standards/oracle-vectors.json`。
向量只从外部 asn1tools 产生；保存工具版本/commit、LF 归一化 schema SHA256、type、value、参数和字节。
普通 CTest 重放冻结向量，无 asn1tools 运行时依赖。刷新必须显式 `--write-vectors`。

- 本次 oracle：asn1tools **0.166.0**，包源码 SHA256
  `1bfc8087bedfc4071d2e0cc7452a5bab792bec25683d91febd5a4513f64b8180`。
  本地工具副本没有独立 Git commit，元数据记为 null；不把父工作区 commit 冒充 oracle 版本。
- 12 个入口的 Record 输出均由外部 decoder 解码；11 个参考输入由项目 decoder 接受。
  CXER 输入只针对本 fixture 将外部 XER 的 `<true />` 改为规范空元素拼写 `<true/>`，
  明确记录转换；不冒充独立 CXER 编码器。CER 的 definite BER 输入应拒绝。
- 7 个基础入口比较中，当前简单 Record 的 BER/DER/PER/OER 字节可比较；
  XER 比较 XML 树，JER 比较 JSON 值。无指令的 E-XER/JERI 和该 OER 子集的 COER
  外部互操作，不代表全部指令/规范策略通过。
- 复跑 C PER oracle：**234** 个外部双向 encode/decode 用例通过，**184** 个短向量；
  排除项及条文仲裁保持第二批记录。既有 C++ asn1c 冻结向量随全量回归，未重跑 asn1c 生成器。
- 新增 `LLVMFuzzerTestOneInput` 与依赖关闭的确定性驱动；13 个种子产生 **481** 次
  截断/变异调用，注册为 `DecoderFuzz.SeedRegression`。Clang libFuzzer 使用库覆盖率插桩，
  ASan+UBSan、固定 seed、最大输入 4096，实际 **20,000** 次无崩溃。
  这是受限探索，不是安全性或完整覆盖证明。

## 门禁结果

| 门禁 | 本次结果 |
|---|---|
| 修复前最小策略复现 | 7 项中 6 项失败，已记录 |
| 21 项能力探针 | 通过（含预期拒绝） |
| 新增策略/生成三形态/约束回归 | 已运行针对性检查；最终以全量结果为准 |
| GCC 13.3 Release 全量构建/CTest | **430/430 通过** |
| GCC 13.3 Debug ASan+UBSan 全量构建/CTest | **430/430 通过**；leak detection / halt_on_error |
| 7 fixture ×12 实际入口 -fsyntax-only | **84/84 通过**；不等于互操作 |
| GCC 安装包四类消费者 | 全部通过；CER→DER 的传递目标正确导出 |
| Clang 18.1.3 sanitizer libFuzzer | 20,000 次通过，481 次确定性种子调用通过 |
| MSVC 19.44 Release 本批目标构建/针对性 CTest | 构建通过；**59/59** 回归通过，不称全 fixture 原生矩阵已运行 |
| 本次外部 asn1tools Record + C PER | 通过，范围如上 |

完整日志在忽略目录 `build-rev1-quick/batch5-*.log`。中断的旧轮次不记为通过；最终门禁使用全部修复后的源码。
本批未重跑完整 Clang fixture CTest；Clang 证据限于所列 runtime fuzz，MSVC 限于 59 项针对性门禁。

## 复跑

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build-release -j1
ctest --test-dir build-release --output-on-failure
# Debug + ASN1_ENABLE_SANITIZERS=ON 独立构建目录重复上述门禁，启用 halt_on_error/leak detection。
cmake --build build-release --target asn1_codec_matrix
python3 scripts/validate_installed_package.py --build build-release --output build-package-check
python3 scripts/validate_standard_capabilities.py --compiler build-release/asn1cxx --output build-probes
python3 scripts/validate_standard_oracles.py --driver build-release/tests/asn1_standard_oracle_driver \
  --oracle-root /absolute/path/to/Python_asn1tools
PYTHONPATH=/absolute/path/to/Python_asn1tools python3 scripts/validate_c_per_oracle.py --build-dir build-c-per-oracle
```

Fuzz 使用独立 Clang Debug 构建，开启 `ASN1_ENABLE_SANITIZERS=ON`、
`ASN1_BUILD_FUZZERS=ON`，构建 `asn1_decoder_fuzzer`。把 `tests/fuzz/seeds.txt`
的非注释 hex 行用 `bytes.fromhex` 各写入 corpus 文件后运行：

```sh
build-fuzz/tests/asn1_decoder_fuzzer /path/to/corpus -runs=20000 -max_len=4096 -seed=20261021
```

## 后续功能与验证空白

以下是本批核查后的明确后续项，不把它们算成现有完整能力；无需为本批一次实现整个标准：

1. 继承约束交集、FROM、普通 WITH COMPONENTS、COMPONENTS OF、数字版本扩展组；
   添加可靠 IR/语义规划后逐项开放，当前失败比错误线路更可控。
2. DER/CER 时间规范词法、named BIT STRING 尾零、schema 感知的 CER SET 顺序，
   COER/XER DEFAULT/未知扩展策略及全部构造替代形式需继续补独立正反例。
3. 精确 REAL 模型与文本可逆性设计（REV-23b 后续），字符 repertoire、现代时间/IRI；
   当前 double/profile 不得表述为任意精度或全标准覆盖。
4. 独立 CER/COER/CXER/E-XER 指令 oracle 仍有空白；本批 fuzz 只覆盖所列 C++ 解码入口，
   未执行长期 fuzz，未覆盖 C11 全部原语及完整生成 PDU。

新增证据与后续改动需同步 [项目 skill](skills/asn1-remediation/SKILL.md)，
历史第一至四批数字保持原样。
