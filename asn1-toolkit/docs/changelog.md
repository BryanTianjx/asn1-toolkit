# 变更日志

记录约定：**追加式，最新在上**。每条含日期、变更摘要与验证口径。历史 Phase
里程碑表见下；2026-10 起的详细变更按条目记录。当前状态见 [status.md](status.md)。
未排期 / 已规划子任务见 [todo.md](todo.md)。

## 近期变更详录

### 2026-10-16

**登记 P2-3 / P3-1 待办清单**（不涉及代码改动）
- 新增 [docs/todo.md](todo.md)：把 P2-3（增量代码生成，~7 h）和 P3-1
  （前端并行分析，~15 h）从 status.md 的「未排期」一栏升级为「已规划、
  未排期」，各子任务含设计要点、估时、验证门槛、可重现性约束。
- [status.md](status.md) 增 todo.md 链接；P2-3 / P3-1 行简化为指向 todo
  的锚点。诊断输出逐字节可重现的硬要求记录在 P3-1 子任务里，作为
  强制约束。
- 没有代码 / 构建 / 测试改动。

### 2026-10-15

**P2-2 收尾 — XML 家族单 codec include 缺口修复 + 单 codec × 全家族门禁**
- 根因：`emit_includes` 的 XML 分支（xer/exer/cxer）未 include
  `<asn1/runtime/byte_io.hpp>`，而 XML 解码器在 CHOICE/ENUMERATED 等
  失败路径会调用 `asn1::make_error`（定义于 `byte_io.hpp`）。全 12-codec
  组合门禁中 PER/OER/TLV 分支都会捎带这个头，所以组合门禁一直绿；任何
  单 codec 选 XML 都必暴露。
- 修复：`emit_includes` XML 分支显式补 `#include <asn1/runtime/byte_io.hpp>`
  （一行 + 注释解释）。
- 回归门禁：`tests/CMakeLists.txt` 新增 LPP × 12 codec 各跑一次独立 emit +
  完整编译（`asn1_lpp_single_{uper,aper,per,ber,der,cer,oer,coer,jer,
  xer,exer,cxer}_compile`），覆盖单 codec 场景的 include 自包含性，
  防组合门禁再掩盖同类缺口。
- 验证：手工 3 fixture × 12 codec = 36 case 全部零错误（含 flat 单 codec
  跑 `--codec xer`/`exer`/`cxer`）；新门禁 12 个静态库编译全过；Release
  320/320 零回归。
- 状态变更：`status.md` 中"XML 家族单 codec include 缺口"由已知失败
  改为 ✅ 已修复；Phase 45 由"部分完成"改为"单 codec × 全家族门禁已就位"。

**P2-1 — bit_io 批量位写路径优化**（Phase 里程碑见下表）
- `put_bits(value,nbits)` 改先填半字节再整字节批量写；新增
  `BitWriter::put_bitmap(Span<byte-per-bit>)` 按 64 位打包一次写出；
  `put_octets` / `put_bits(Span)` 在八位对齐流上直接批量拷贝；`put_bit`
  下标除法改位移；`encode_bitmap` 与 BIT STRING 分片写路径改走批量接口。
- 验证：新增 `BitIoBenchmark.PresenceBitmapWrite`；bitmap 密集型写
  196,532 → 626,075 ops/sec（≈3.2×），UPER 往返 +12%，输出字节与逐位循环
  逐比特一致；Release + sanitizer 320/320；三套金标准向量零回归。

**P1-1 — BER/DER/OER 金标准差分向量 + OER SET 规范序**
- `scripts/gen_tlv_golden.py` + `tests/tlv/golden/` + `asn1_tlv_golden_tests`：
  16 用例 × 3 codec 字节级对验（oracle 策略：asn1tools 为 BER/OER 向量源；
  asn1c 仲裁 DER —— asn1tools 不排序 SET/SET OF，X.690 11.6 要求排序；
  asn1c 的 `oer_encode` 对 SET 值段错误（已知 bug），OER 列保留 asn1tools；
  BER 无规范 SET 序（X.690 8.14），SET 用例 BER 列记 '-'）。
- OER SET 组件改为 X.696 18.2 规范（标签）序 emit，preamble 位序与解码镜像
  同一顺序（此前按定义序，字节与 oracle 不符）。
- 验证：Release + sanitizer 319/319；tlv_golden fixture 全 12 codec
  `-fsyntax-only` 零错误；per_golden 向量零回归。

**P1-2 — CER（X.690）编解码器家族落地**（第 12 个 codec）
- `src/runtime/cer/codec.cpp`（BER 原语策略层）：9.1 构造值不定长包装、
  9.2 + 8.6.4 超 1000 内容八位组分片、11.2 末八位组未用位清零、9.3
  SET/SET OF 标签序（未打标签 CHOICE 按替代项最小标签静态定序）、canonical
  严格性；`emit_tlv.cpp` 按 `TlvKind{Ber,Der,Cer}` 路由；`asn1_cer` 静态库
  + CLI `--codec cer`；`cer_golden` fixture + 10 项字节级金标准（无外部
  CER oracle，条文推导 + DER 值等价交叉验证）。
- 顺带修复全 TLV 家族 SET 解码器缺陷：SET 内未打标签 CHOICE 组件的解码派发
  键此前为 `universal(0, true)` 占位标签（永不匹配），改为 emit 时计算该
  CHOICE 全部替代标签的并集。
- 验证：Release + sanitizer 316/316；五 fixture × 12 codec `-fsyntax-only`
  零错误。

**P2-2 — 大模块分文件/分命名空间 emit（`--emit-split`）**（2026-10-15 主体，
10-15 收尾：XML include 缺口 + 单 codec × 全家族门禁，详见上条目）
- 按 ASN.1 模块边界生成 `<Module>.hpp/.cpp` 对 + 聚合 `generated.hpp`；头内
  类型命名空间与模块名对齐，跨模块引用按 `<Module>::Name` 限定
  （`Emitter::named_ref`，flat 产物逐字节不变）；互相递归模块（依赖图 SCC）
  合并进同一 TU；每模块头自带 `detail::Null`；模块依赖 include 按结构依赖
  计算。多模块验证对象为 S1AP（6 模块，LPP 实为单模块 fixture）。
- 验证：LPP/S1AP/RRC 逐单元 `g++ -fsyntax-only` 手工矩阵通过（uper 全过；
  LPP 另过 aper/coer/ber/der/oer/jer）；新增 `asn1_split_lpp_split` /
  `asn1_split_s1ap_split` 编译门禁；flat 产物与改动前 diff 为空；
  Release 320/320（无新增 gtest）。
- ⚠️ 已知失败（暂停修复）：`--codec xer`/`exer`/`cxer` 单独选择时 split 产物
  编译失败（`make_error` is not a member of `asn1`）—— 根因是 `emit_includes`
  XML 分支未 include `<asn1/runtime/byte_io.hpp>`，flat 单跑同样复现，属既有
  缺陷；详见 [status.md 剩余工作](status.md#剩余工作)。

### 2026-10-14

**P0-1 — 生成代码默认 out-of-line**
- CLI 默认改为 out-of-line：`generated.hpp` 只放类型声明 + 编解码函数声明，
  定义全部落 `generated.cpp`；`--emit-inline` 恢复全 inline 头。RRC 全模块
  实测：头 33,697 → 5,998 行；消费 TU 编译 ~22s → ~5s；定义 TU 一次性
  ~63s（盈亏平衡 N≈3–4 TU）。顺带修复：此前 `generated.cpp` 是空 stub，
  fixture 静态库编译门禁实际未编译任何生成代码。
- 验证：Release 298/298（新增 2 项 CLI 测试）+ sanitizer 全量 + PER/APER
  金标准向量零回归。

**P0-2 — emit.cpp 按 codec 家族拆分**
- 5,208 行单文件拆为 7 个（`emit_internal.hpp` 共享声明 + `Emitter` 类接口；
  `emit_core` / `emit_per` / `emit_tlv` / `emit_xer` / `emit_jer` /
  `emit_oer`）。依赖图结论：共享面仅 9 个自由函数 + 11 个成员，其余 86 个
  成员全部家族内聚。函数体字节级迁移，RRC UPER 产物与拆分前逐字节一致。
- 验证：Release + sanitizer 298/298；单家族改动重编译 18.9s → 5.5s（JER）/
  6.9s（PER，-j1）；金标准向量零回归。

**P0-3 — 全 codec 产物编译门禁驱动的发射器缺陷修复**
- 首次对三大 fixture + 新 tlv_oer_golden 做 12 codec emit + `-fsyntax-only`
  门禁，暴露并修复 6 类根因（XML 运行时命名空间硬编码、解引用优先级、
  inline 列表 `.value` 误用、嵌套 CHOICE 替代未限定、解码临时变量遮蔽
  —— 新增 `Emitter::temp_counter_`、CHOICE 替代局部变量自遮蔽）及顺带修复
  （X.680 25.3 自动编号规则、default_val_cpp 按字段取默认值、X.690 8.5
  显式标签自然编码、X.696 8.7.2 OER 选择标签 constructed 位）。
- 验证：四 fixture × 12 codec `-fsyntax-only` 零错误；Release + sanitizer
  316/316，零 ASan/UBSan。

### 2026-10-08 起（文档重组前）

**表约束类型化 IE 开放类型**
- `analyzer.cpp`：entries 只挂开放类型字段；扩展枚举未显式编号时从 root
  末尾继续（X.680 惯例）；OCF fixed_type 与内联 SEQUENCE OF 纳入依赖遍历；
  去重按 C++ 名。`emit.cpp`：tc decode/encode 参数改名避免嵌套 CHOICE 冲突。
- 验证：新增 `asn1_tc_ie_roundtrip_tests`（4 项）。

**多编解码组合生成**
- `--codec` 改位掩码：逗号分隔组合与重复标志累加；未指定默认 UPER。
- 验证：新增 5 项 CLI 冒烟测试；组合产物 `-fsyntax-only` 通过。

**PER/APER 金标准字节向量（asn1c oracle）**
- `scripts/gen_per_golden.py` + `asn1_per_golden_tests`：UPER/APER 各一套，
  54 向量（约束原语/长度 determinant/分片/扩展 addition/版本括号组）。
  修复扩展 addition 编码（X.691 19.9：简单扩展 addition 的 open type 仅含
  组件编码，版本括号组 `[[...]]` 才按嵌套 SEQUENCE 编码含 preamble）——
  IR 新增 `ExtensionGroup{fields, version_bracket}` 区分两种形态。
- 验证：`PerGoldenVectors.UperMatchesOracleBytes` /
  `AperMatchesOracleBytes` 通过。

**JER 对象类开放类型编解码 / CLI 测试并行竞态修复 / 三大 3GPP fixture 编译
门禁**（均于 2026-10 上旬完成，细节见归档
[analysis/architecture-review-and-optimization.md](analysis/architecture-review-and-optimization.md)）

## Phase 里程碑表

| Phase | 内容 | 状态 |
|-------|------|------|
| 1 | 项目骨架与构建系统 | done |
| 2 | Lexer | done |
| 3 | 基础 parser | done |
| 4 | AST 与语义分析器 | done |
| 5 | 约束系统 | done |
| 6 | BER | done |
| 7 | DER | done |
| 8 | BitReader/BitWriter 与 PER 原语 | done |
| 9 | UPER | done |
| 10 | APER | done |
| 11 | 代码生成器 | done |
| 12 | 高级 ASN.1 构造（ENUMERATED / OID / RELATIVE-OID / SET / SET OF / 类型化 REAL） | done |
| 13 | 信息对象类（class fields、开放类型 `&Value`、表约束类型化 IE dispatch） | done |
| 14 | 真实电信 ASN.1 验证 | done |
| 15 | OER | done |
| 16 | COER | done |
| 17 | XER | done |
| 18 | CXER | done |
| 19 | EXTENDED-XER | done |
| 20 | JER | done |
| 21 | JER 编码指令运行时 | done |
| 22 | JER 编码指令前端 + codegen | done |
| 23 | INSTANCE OF + XER 编码指令前端/codegen | done |
| 24 | 全量 E-XER codegen 指令矩阵 | done |
| 25 | REAL 编解码（IEEE-754 二进制 ↔ ASN.1 base-2） | done |
| 26 | OER REAL WITH COMPONENTS IEEE binary32/64 分类 | done |
| 27 | EXTERNAL / EMBEDDED PDV / CHARACTER STRING | done |
| 28 | >64 位 INTEGER 编解码（`asn1::BigInteger`） | done |
| 29 | EMBEDDED PDV / CHARACTER STRING 深化 BER | done |
| 30 | BER 构造/不定长 + DER + 现代 EXTERNAL | done |
| 31 | PER 扩展 addition 序列 | done |
| 32 | ABSTRACT-SYNTAX + 现代 EXTERNAL 识别形式 IR | done |
| 33 | JER SequenceOf/SetOf 解码 codegen | done |
| 34 | Codegen BER / DER / OER / CXER | done |
| 35 | EXER LIST character-encodable 元素类型 | done |
| 36 | OER/COER 扩展 addition | done |
| 37 | COER `--codec coer` codegen | done |
| 38 | 电信 UPER 编解码往返 | done |
| 39 | 验证 —— 多 codec Person + 电信 emit 门禁 | done |
| 40 | RRC/LPP/S1AP 全量 emit —— 嵌套命名 + 参数化 IE 字段 | done |
| 41 | 评审硬化 —— OCF 重新解析、DEFAULT、PER 约束、BitString、长度上限 | done |
| 42 | 全 codec 产物硬化 —— 12 codec 编译门禁（P0-3）；`tlv_oer_golden` fixture + 8 往返测试 | done |
| 43 | CER（X.690）编解码家族（P1-2）；`cer_golden` fixture + 10 字节级金标准 | done |
| 44 | BER/DER/OER 金标准差分验证 + OER SET 规范序（P1-1） | done |
| 45 | 大模块分文件/分命名空间 emit（`--emit-split`，P2-2） | ✅ 已完成（XML include 缺口修复 + 单 codec × 全家族门禁就位，2026-10-15 收尾） |
| 46 | 增量代码生成（P2-3） | ⏳ 已规划，未排期；子任务清单见 [todo.md](todo.md#p2-3--增量代码生成内容哈希缓存) |
| 47 | 前端并行分析（P3-1） | ⏳ 已规划，未排期；子任务清单见 [todo.md](todo.md#p3-1--前端并行分析) |
