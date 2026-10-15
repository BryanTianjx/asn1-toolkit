# 状态清单 — asn1-toolkit 修复与构建验证

状态：2026-10-15（P2-1 bit_io 批量位写 + benchmark 量化后）

## 已完成

| 项目 | 说明 | 验证 |
|---|---|---|
| span.hpp SFINAE 修复 | `std::enable_if_t<std::is_const_v<T>, Container>` 使 SFINAE 安全（非 const T 不再硬错误） | `asn1_common_tests` 编译通过 |
| uper BitReader 右值修复 | `decode_null` 临时 `BitReader` 改为命名变量 `empty_reader` | `asn1_uper_tests` 链接成功 |
| BitReader/ByteReader 悬垂 Span 修复（提交 c28da0d） | 二者只持有非拥有 `Span`，用临时量（`w.take()`、`hex({...})`）构造会 heap-use-after-free；新增按值 `std::vector` 持有构造（rvalue 绑定并保活；lvalue/`buffer()` 仍零开销借用），并修正 3 处测试 | 修复前 3 项失败；修复后 ASan 干净 |
| CI 工作流 | `.github/workflows/ci.yml`：build-test（Release）+ sanitize（ASan/UBSan）双 job | 文件已写入并在本地验证 |
| CI 分支触发 | `on.push.branches` 增加 `asn1-toolkit-phase-39-validation` | 已提交（01b1c09） |
| ASan/UBSan 构建选项（提交 ec04843） | CMake 新增 `ASN1_ENABLE_SANITIZERS`；CI 增 sanitize job；README 记录 sanitizer 构建与离线 gtest（`FETCHCONTENT_SOURCE_DIR_GOOGLETEST`） | 默认构建不受影响 |
| person_roundtrip 源码 | 旧记录称"源码缺失"，实际 `tests/codegen/person_roundtrip_test.cpp` 存在且 UPER/APER 往返通过 | `Validation.PersonUperRoundTrip` / `PersonAperRoundTrip` 通过 |
| CLI 端到端测试 | `tests/codegen/cli_test.cpp`：子进程方式跑 `asn1cxx`，验证退出码/错误处理/emit 产物（14 项，含多编解码组合） | `Cli.*` 通过 |
| 跨编码一致性测试 | `tests/codegen/cross_codec_consistency_test.cpp`：同一 Person 语义值在 UPER/APER/OER/COER/BER/DER/JER 下各自往返保真（8 项） | `CrossCodec.*` 通过 |
| 编解码性能基准 | `tests/codegen/codec_benchmark_test.cpp`：吞吐 micro-benchmark（4 项，带宽松下限防 flaky） | `CodecBenchmark.*` 通过 |
| JER 对象类开放类型编解码 | `emit.cpp`：`ObjectClassField`（&Value 开放类型，C++ 层即 byte vector）的 JER encode/decode 由 Unsupported 改为 hex OCTET STRING 编解码（与 `INSTANCE OF` value 一致）；同时补上 JER 生成头缺失的 `byte_io.hpp` include（`make_error` 未定义）；`INSTANCE OF` decode 由 Unsupported 改为 `[type-id, value]` 数组解析 | 新增 `asn1_ioc_jer_roundtrip_tests`（4 项），含空开放类型与短数组拒绝用例 |
| CLI 测试并行竞态修复 | `cli_test.cpp` 临时文件使用固定名，并行 ctest 时同一二进制多实例（`--gtest_filter` 每测试一进程）互相踩同名 out/err 文件导致随机失败；改为 pid 后缀 | 并行 `ctest -j` 连续多轮 9/9 稳定通过（修复前必挂） |
| 表约束类型化 IE 开放类型 | `analyzer.cpp`：entries 只挂开放类型字段（修复 criticality 等固定类型字段被误挂 `Decoded_` variant 的问题）；扩展枚举未显式编号时从 root 末尾值继续编号（X.680 惯例，修复 switch case 冲突）；`emit.cpp`：tc decode/encode 的 visit lambda 参数改名 `tc_alt`（避免与嵌套 CHOICE 的 `alt` 变量冲突）；OCF fixed_type 与内联 SEQUENCE OF 元素纳入 collect_named 依赖遍历（修复类型未 emit/顺序靠后）；去重按 C++ 名而非 name+module（修复参数化实例化重复定义）；内联匿名 SEQUENCE OF/SET OF 直接用裸 vector 而非 `.value`（修复 RRC/LPP 编译错误） | 新增 `asn1_tc_ie_roundtrip_tests`（4 项：typed encode 填充开放类型字节+governor id、按 id dispatch decode、固定类型字段不特化、未知 id 保留原字节） |
| 三大 3GPP fixture 编译门禁 | `tests/CMakeLists.txt`：RRC(UPER)/LPP(UPER)/S1AP(APER) 全量 emit 产物作为静态库编译门（emit 成功 ≠ 可编译） | Release + sanitizer 全量构建通过（此前从未验证过可编译性） |
| 多编解码组合生成 | `--codec` 改为位掩码：支持逗号分隔组合（如 `--codec uper,ber`）与重复 `--codec` 标志累加；`emit.cpp` 全部 19 处等值比较改位与判断；未指定 `--codec` 仍默认 UPER | 新增 `Cli.CodecComboUperBerEmitsBothRuntimes` 等 5 项冒烟测试；Release + sanitizer 296/296；组合产物 g++ -fsyntax-only 通过 |
| PER/APER 金标准字节向量（asn1c oracle） | `scripts/gen_per_golden.py` + `tests/per/per_golden_vectors_test.cpp`：从 asn1c 生成金标准向量并断言精确字节（UPER/APER 各一套，54 向量，含约束原语/长度 determinant/fragmentation/扩展 addition/版本括号组）；修复扩展 addition 编码：X.691 19.9 规定简单扩展 addition 的 open type 内容仅含组件编码（presence 由扩展 bitmap 表示），只有版本括号组 `[[...]]` 才按嵌套 SEQUENCE 编码（含 preamble）。IR 新增 `ExtensionGroup{fields, version_bracket}` 区分两种形态（analyzer 填标记，emit.cpp 的 PER/OER encode/decode 四个函数仅对版本括号组生成 preamble）；此前 UPER/APER 均把 presence bit 编入 open type（多出 1 bit+对齐）导致金标准失败。另修复版本括号组 mandatory 成员的 optional 解包（struct 中扩展组字段统一 force-optional，encode 须统一 `if` 包裹 + `.value()`——此前 mandatory 成员直接传 optional 值，类型不匹配；LPP/RRC 真实组字段全 OPTIONAL 故未暴露） | `PerGoldenVectors.UperMatchesOracleBytes` / `AperMatchesOracleBytes` 通过；Release + sanitizer 291/291 |
| P0-1 生成代码 out-of-line | CLI 默认改为 out-of-line：`generated.hpp` 只放类型声明 + 编解码函数声明，定义（包在生成命名空间内）全部落 `generated.cpp`；`--emit-inline` 恢复旧的全 inline 头（单 TU 小模块零链接）。`EmitOptions::emit_inline` 默认 true，API 调用方（`emit_header_string`）行为不变。RRC 全模块实测：头 33,697→5,998 行；消费 TU `-fsyntax-only` 22.1s→5.2s；定义 TU 一次性 62.8s（盈亏平衡 N≈3-4 TU）。顺带修复：此前 generated.cpp 是空 stub，fixture 静态库编译门禁实际未编译任何生成代码；`codec_benchmark_tests` 链接行补 `asn1::der`/`asn1::coer`（der/coer 定义 TU 此前无符号所以缺链不可见） | Release 298/298（新增 `Cli.DefaultEmitIsOutOfLine` / `Cli.EmitInlineRestoresHeaderDefinitions`）+ PER/APER 金标准向量零回归 |
| P0-2 emit.cpp 按 codec 家族拆分 | 5,208 行单文件拆为 7 个：`emit_internal.hpp`（586 行，src-local：9 个共享自由函数声明 + Emitter 类完整声明 + AliasShape）、`emit_core.cpp`（963 行）、`emit_per.cpp`（1,061）、`emit_xer.cpp`（912）、`emit_jer.cpp`（782）、`emit_oer.cpp`（769）、`emit_tlv.cpp`（783）。依赖图：共享面仅 9 个自由函数 + 11 个成员（type_cpp/emit_header/emit_type_decl/emit_struct_fields/choice_all_fields/default_val_cpp/field_has_presence_bit 等），其余 86 个成员函数全部家族内聚。函数体字节级迁移（切片脚本按闭合花括号切分），RRC UPER 产物与拆分前 diff 逐字节一致。顺带修复原文件 7 处损坏的文档注释（函数体文本混进注释行） | Release + sanitizer 298/298；单家族改动重编译 18.9s→5.5s（JER）/6.9s（PER，-j1）；金标准向量零回归 |
| 全 codec 产物编译门禁驱动的发射器缺陷修复（9 codec：uper,ber,der,oer,coer,jer,xer,exer,cxer） | 首次对三大 fixture（RRC/LPP/S1AP）+ 新 fixture tlv_oer_golden.asn 做全 codec emit + `g++ -fsyntax-only` 门禁，暴露并修复：(1) emit_xer 的 XML 运行时命名空间——`Element` 类型硬编码 `asn1::exer::Element`，Basic-XER/CXER 产物引用 E-XER 命名空间但未 include 其头文件（EXER 模式恰好自洽故此前未暴露），全部改走 `xml_ns()`；(2) `find_child` 解引用优先级 `*child.value().children` → `(*child.value())`，XER 可选字段同理 `(*value.f).bits`；(3) JER/XER 内联（匿名）SEQUENCE OF/SET OF 误用命名别名结构的 `.value` 成员（OER/TLV 已修、JER/XER 漏网），统一 `is_named(rt) ? expr + ".value" : expr`；(4) 嵌套 CHOICE 解码——替代结构体未限定（`m_` → `Nested_rec_q::m_`），JER/XER 复用 OER/TLV 的 `choice_ty` 模式；JER 选择解码误发 `return value;`（返回 `Result<void>` 的函数）→ `Result<void>::success()`；(5) 可选字段解码临时变量遮蔽（根因，五族发射器全中）——临时变量固定命名 `field`，嵌套可选 SEQUENCE 的内层临时变量遮蔽外层同名临时变量，而外层临时变量正是赋值目标 `expr`，导致嵌套可选字段全部赋错对象（RRC 真实报错 `field.ac_BarringForMO_Signalling = std::move(field)`）；新增 `Emitter::temp_counter_`，解码临时变量统一 `field<N>`，XER/JER 的 `xer_field`/`jer_field` 同步；(6) CHOICE 替代局部变量自遮蔽——`const auto& alt = std::get<N>(alt.value...)`（auto 推导前引用自身），PER/OER/TLV/XER 统一 `alt<N>`；(7) analyzer：X.680 25.3/29.2——组件列表中任一文本标签即禁用整表自动编号，未打标签组件保留自然 universal 标签（IR 实测 `plain` 保持 UNIVERSAL 2 而非 CONTEXT 0）；(8) default_val_cpp 重构为按字段取默认值：非约束 INTEGER 默认值包 `BigInteger::from_i64(...)`、字符串字面量剥离 lexer 附带的引号；(9) TLV EXPLICIT 标签与内联类型标签相同时编自然 universal 标签（X.690 8.5）；OER 选择标签不带 constructed 位（X.696 8.7.2） | 门禁：tlv_oer_golden + RRC + LPP + S1AP × 9 codec `g++ -fsyntax-only` 全部零错误（此前全部失败）；新增 `asn1_tlv_oer_golden_tests`（8 项：混合标签 DER 字节级断言 `30 0F 02 01 07 81 01 08 82 02 68 69 A3 03 01 01 FF`、DEFAULT 省略/携带 `83 01 63`/`84 03 62 79 65`、SET/SET OF/SEQUENCE OF、嵌套 CHOICE、OER/JER/BER 往返）；Release + sanitizer 双门禁 306/306，零 ASan/UBSan |

| CER (X.690) 编解码器家族 | 第 12 个 codec 家族端到端落地：`src/runtime/cer/codec.cpp`（561 行，BER 原语策略层：9.1 构造值一律不定长 `30 80 … 00 00`（含 CHOICE 替代）、9.2 超 1000 内容八位组的 OCTET STRING/UTF8String 切分为 ≤1000 原始分片、8.6.4 BIT STRING 分片各自携带初始(未用位)八位组且计入 1000 预算、11.2 末八位组未用位清零、9.3 SET/SET OF 标签序排序（未打标签 CHOICE 按选择项中最小标签 emit 时静态定序 + `encode_set_ordered` 不运行时重排；SET OF 按编码字节串升序）、canonical 严格性（拒绝 CER 要求不定长处的定长形式））；`emit_tlv.cpp` 按 `TlvKind{Ber,Der,Cer}` 路由（`tlv_ns`/`tlv_fn`，`der = tlv != Ber` 覆盖 CER 的 SET/SET OF 排序路径）；CMake `asn1_cer` 静态库 + CLI `--codec cer`；`tests/fixtures/cer/cer_golden.asn` + `asn1_cer_roundtrip_tests`（10 项字节级金标准：不定长包装、SET 标签序、CHOICE 原始/构造替代派发、OCTET/BIT STRING 分片、SET OF 字节序、SET 内未打标签 CHOICE 最小标签定序、CER/DER 值等价、定长包装拒绝）。无外部 CER oracle（asn1c v0.9.29 与 asn1tools 均无 CER），金标准由 X.690 条文推导 + DER 值等价交叉验证。顺带修复全 TLV 家族 SET 解码器潜在缺陷：SET 内未打标签 CHOICE 组件的解码派发键此前生成 `universal(0, true)` 占位标签（永不匹配真实编码），改为 emit 时计算该 CHOICE 全部（递归嵌套）替代标签的并集作派发条件——BER/DER/CER 同路径受影响，由新 fixture 首次暴露 | Release + sanitizer 双门禁 316/316；五大 fixture（RRC/LPP/S1AP/rrc_slice/cer_golden）× 12 codec（uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer）`-fsyntax-only` 零错误 |

| BER/DER/OER 金标准差分向量 + OER SET 规范序（P1-1） | 金标准验证从 PER 扩展到 BER/DER/OER：`scripts/gen_tlv_golden.py`（oracle 策略：asn1tools 为 BER/OER 向量源；asn1c 仲裁 DER——asn1tools 不排序 SET/SET OF，X.690 11.6 要求 SET 按 X.680 8.6 标签序、SET OF 按编码八位组串升序，asn1c 的 `der_encode` 胜出并写回向量；asn1c 的 `oer_encode` 对 SET 值段错误（asn1c 已知 bug），OER 列保留 asn1tools；BER 无规范 SET 序（X.690 8.14 仅要求构造形式），SET 用例 BER 列记 '-' 由 DER 列承担排序证明）+ `tests/fixtures/telecom/tlv_golden.asn`（16 用例：IMPLICIT/EXPLICIT 上下文标签、乱序标签 SET、SET OF、DEFAULT 省略/携带、NULL/负数/REAL、可扩展 ENUMERATED、嵌套 CHOICE、扩展 addition）+ `tests/tlv/tlv_golden_vectors_test.cpp`（16 用例 × 3 codec 字节级断言 + oracle 字节解码往返；DER SET OF 往返期望值为规范序）。OER SET 组件按 X.696 18.2 规范（标签）序 emit：root 组件（root + trailing_root）在 emit 时按 `set_component_key` 稳定排序，preamble 位序与解码路径镜像同一顺序（扩展 addition 保持定义序，18.2 NOTE 1）——由新向量首次暴露（此前 OER SET 按定义序 emit，字节与 oracle 不符）。顺带修复生成器两处驱动缺陷：conv_tlv 期望 hex 文本输入（此前传原始字节全部 "odd-length hex" 失败）、stdout 已是 hex 文本（此前二次 `.hex()` 双重编码） | Release + sanitizer 319/319；tlv_golden fixture 全 12 codec `-fsyntax-only` 零错误；金标准向量零回归 |
| P2-1 bit_io 批量位写 + benchmark 量化 | 运行时写路径逐比特循环改批量：`put_bits(value,nbits)` 先填半字节再整字节写；新增 `BitWriter::put_bitmap(Span<byte-per-bit>)` 按 64 位打包一次 `put_bits`；`put_octets`/`put_bits(Span)` 在八位对齐流上直接批量拷贝；`put_bit` 下标除法改 `>>3`/`&7`；`encode_bitmap` 委托 `put_bitmap`，BIT STRING 分片写路径改走 `put_bits(Span)` 批量路径。同机同会话前后对比：bitmap 密集型写（BitIoBenchmark，10×64-bit bitmap+交错小字段）196,532→626,075 ops/sec（≈3.2×），UPER 往返 844k→~950k ops/sec（+12%）。输出字节与逐位循环逐比特一致（同一 MSB-first 语义），由 per_golden/tlv_golden/cer_golden 三套金标准零回归证明；`put_bits(Span)` 快速路径在 sub-octet tail 缺源字节时回退逐位循环（不越界，与旧行为一致）。发射器未改动，全 codec 编译门禁不受影响 | 新增 `BitIoBenchmark.PresenceBitmapWrite`；Release + sanitizer 320/320；三套金标准向量零回归 |

## 剩余 / 未完成

| 项目 | 阻塞原因 | 建议后续 |
|---|---|---|
| 完整模块编译 CI / 3GPP 编解码对等验证 | emit+compile 已过；BER/DER/OER 单构造金标准向量已对验（tlv_golden）；剩余为 3GPP 协议消息级完整向量（逐消息 PDU）的对验 | 扩展金标准向量覆盖面；逐步引入协议级消息向量 |
| 许可证文件 | README 标注 TBD（开放源码，发布时添加） | 发布前补 LICENSE |
| PER 位级正确性外部对验 | `per_golden.asn` 综合向量（含扩展 addition/bitmap/fragmentation/约束原语）已与 asn1c oracle 字节级对验通过；剩余为 3GPP 协议消息级完整向量（逐消息 PDU）的对验 | 扩展 per_golden 向量覆盖面；逐步引入协议级消息向量 |

## 构建状态

- 默认（Release）：**320/320 通过**，0 失败
- Sanitizer（ASan+UBSan，Debug）：**320/320 通过**，0 失败，零 ASan/UBSan 报错（含 out-of-line 定义 TU 全量编译）
- 全 codec 编译门禁：tlv_oer_golden.asn + tlv_golden.asn + cer_golden.asn + RRC(8.6.0) + LPP(14.3.0) + S1AP(14.4.0) + rrc_slice 七 fixture × 12 codec（uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer）`g++ -std=c++17 -fsyntax-only` 全部零错误
- 金标准差分验证：PER/APER（per_golden，asn1c oracle）+ BER/DER/OER（tlv_golden，asn1tools + asn1c oracle）双套字节级对验通过
- 三大 3GPP fixture（RRC/LPP/S1AP）emit 产物全量编译门禁通过（现在编译的是真实定义，此前为 stub）
- `-Wall -Wextra`：无新增警告
- 无残留 `Span<int>` 编译错误

## 备注

- 本地领先 origin/fork 12 个提交：ec04843（sanitizer+离线）、01b1c09（CI 分支触发）、c28da0d（UAF 修复）、4aed4c8（span SFINAE + 初始 CI）、6d94856（TODO 260/260）、9ad969a（CLI/cross-codec/benchmark 测试）、398ed3b（docs 重组）、808dd05（JER OCF + CLI 竞态）+ 本轮提交（表约束类型化 + fixture 编译门禁）。
- PR #1（→ Tinacao2007/asn1-toolkit）待属主批准 fork-PR workflow 门控后由 CI 运行。
