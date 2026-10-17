# 待办清单（P2-3 / P3-1 与相关项）

本文档登记 P2-2 之后不再继续的剩余项，并按优先级、依赖、预估工作量列出
落地步骤。每条给出最小可行子任务，便于逐步合入。状态变更走
[status.md](status.md)，变更日志走 [changelog.md](changelog.md)。

## P2-3 — 增量代码生成（内容哈希缓存）

### 目标

`asn1cxx` 当前每次 `--emit-dir` 都全量生成 + 覆盖。C/C++ 工程在标准接入流程
里需要把 asn1cxx 作为构建依赖（CMake `add_custom_command(OUTPUT ... DEPENDS
asn1cxx <asn>)`），全量生成在大 fixture 上是数秒到分钟量级（per_golden 0.5 s、
RRC 8.6.0 10 s、S1AP 14.4.0 30 s+），CI 反复构建和 IDE 内保存触发都吃这个
成本。

`--emit-split` 已经把粒度降到 ASN.1 模块，再加一个**内容哈希缓存**：源文
件 hash 不变就跳过该单元的 emit + write；源文件 hash 变了只重写其对应单
元。

### 设计

- **缓存键**：`sha256(module_name || source_path_mtime || source_sha256)`
  三元组。`source_path_mtime` 仅用于触发重算 `source_sha256`（直接读文件
  hash 比解析 .asn 快得多，避免把 lexer/parser 拖进缓存键计算）。
- **缓存形态**：`.asn1cxx-cache/` 放在 `--emit-dir` 旁边，
  `<module-name>.cache` 文件存 `key: emit_output_sha256`。emit 后把输出
  文件做一次 hash 写入缓存条目；下一次命中时直接复用旧文件，并校验实际
  写出的 hash 与缓存一致（防止手动改产物后被错认命中）。
- **粒度**：
  - flat 模式：缓存键 = 单个 key（全量；命中即跳过整 emit）。
  - split 模式：每个 ASN.1 模块一个 key（与 `--emit-split` 单元粒度对
    齐）。依赖模块 hash 变更只重算当前单元。
- **失效链**：split 模式下单元 A 依赖 B 的类型 → B 失效时 A 也失效。
  用拓扑序逐单元判定（A 缓存键附加所有依赖 B 的 hash），保证正确性。
- **缓存关闭**：`--no-cache` 强制全量（CI 上做金标对验时不希望走缓存）；
  默认开。
- **诊断**：`Diagnostics` 加一条 `note`：「emit cache: 11/12 modules
  hit (0.04s) / 1 miss (1.2s)」（按 `note` 走，不进错误计数）。

### 子任务

| # | 内容 | 估时 | 验证 |
|---|------|------|------|
| 1 | 在 `EmitOptions` 加 `emit_cache: bool = true`；`asn1cxx` 加 `--emit-cache` / `--no-emit-cache`；`--help` 文档化 | 0.5 h | CLI 用例：flag-as-is 下命中缓存第二次跑时间归零；`--no-emit-cache` 不命中 |
| 2 | `tools/asn1cxx/main.cpp` 增加 `ContentCache` 工具类（读 `.asn1cxx-cache/*.cache` 文本、按 `(module, key) -> output_sha256` 查询、写回） | 1 h | 单元测试 `asn1_cache_tests`：写盘/读盘/命中/未命中/损坏恢复 |
| 3 | `CppGenerator::emit` 入口改为先调 cache：命中且 hash 一致则跳过 emit + write；未命中则走原路径并写缓存 | 2 h | 重复调用同 fixture 二次运行时间记录（flat + split）；改一字节后正确失效 |
| 4 | `CppGenerator::emit_split` 改为按单元走 cache：解析阶段产 module → key 依赖图（已有 `collect_module_groups`），拓扑序决定每个单元 emit 与否 | 2 h | LPP 单模块 split + S1AP 三单元 split 重复跑测时间；故意改一个 .asn 行仅影响一个单元 |
| 5 | 加 `asn1_cache_tests`：金标准用例（Person.asn 二次运行时间记录 + 改字段后失效链）；sanitizer build 也跑 | 1 h | sanitizer 0 失败；ctest 多一项 |
| 6 | 文档：`status.md` 「构建与测试状态」补缓存命中率；`changelog.md` 增条目；`development.md` 「验证门禁纪律」补"缓存命中不能绕过产物哈希校验" | 0.5 h | 文档审阅 |

**总估时：~7 h（一个工作日）。**

### 不在范围内（防止蔓延）

- 不做产物 SHA256 之外的校验（不签名）。
- 不做多进程锁：同一目录被并发 asn1cxx 调用由调用方负责；通常一个工程
  一个 asn1cxx 实例。
- 不做跨工程共享缓存：每个 `--emit-dir` 自带 `.asn1cxx-cache/`。

---

## P3-1 — 前端并行分析

### 目标

`Analyzer::analyze` 当前串行跑四趟（`declare_pass` → `resolve_pass` →
`lower_pass` → `tag_pass`，见 `src/semantic/analyzer.cpp`）：

- `declare_pass`：每个模块独立声明符号集。
- `resolve_pass`：每个模块的引用按**全模块符号表**解析。
- `lower_pass`：AST → IR；每个模块的 lowering 互不依赖。
- `tag_pass`：标签分配；当前对全局扫描态。

声明阶段每个模块相互独立（模块内声明靠模块自身），最容易并行。其余
三趟有跨模块依赖，要按依赖图并行。

### 关键风险

**诊断输出逐字节可重现是硬要求**。当前 `Diagnostics` 是 `std::vector` 顺
序 append，warning/error 顺序在测试里被断言（`asn1_semantic_tests` 有序
号和内容快照）。改成多线程后会改变 append 顺序，进而破坏：

1. 现有测试 fixture 的顺序断言。
2. `format(const Diagnostic&)` 在 `print` 时的全局顺序。
3. 跨模块 warning（如 undefined imported type）原本按模块文件序输出，
   并行后变不可预测。

**强制约束**：并行跑出的 diagnostics 集合必须与串行跑出的逐字节一致
（同样的 Diagnostic 个数 + 同样的 severity + 同样的 where + 同样的
message + 同样的顺序）。

### 设计

- **诊断合并协议**：每个 pass worker 持有本地 `Diagnostics`（无锁），
  pass 结束后按模块序 merge（worker_id 0..n-1 与 input module index
  对齐）。`merge` 时按 module_index 升序拼接结果，让最终顺序确定。这要
  求每个 worker 严格处理「只属于自己模块」的 diagnostics，不在 worker
  间共享跨模块 diagnostics。
- **declare_pass 并行**：每个 worker 持一个 `SymbolTable` 分片，只
  collect 自己模块的符号。pass 结束后 `SymbolTable::merge(other)` 串行合
  并。当前 `SymbolTable` 是单个 map，需扩展为支持按 module key 合并。
- **resolve_pass 并行**：每个 worker 持完整 SymbolTable 只读视图 +
  自己模块的 unresolved 引用集。worker 把 resolved 引用写到自己模块的
  `ir::Module`；未解析的（跨模块定义尚未到位）入「重试队列」；整 pass
  结束后对重试队列做一次 sweep（常数轮收敛：理论 O(log N)，实际 1–2
  轮）。
- **lower_pass 并行**：每个 worker 处理自己模块的 AST → IR lowering；
  IR arena 在 worker 内部构建（无共享），pass 结束后按 module index 拼
  到顶层 `ir::Model` 的 arena。**已有 `ir::TypeId` 跨模块引用靠
  `referenced.resolved` 链接**，该链接在 resolve_pass 已就位，lower_pass
  无须解析跨模块 id。
- **tag_pass 并行**：标签分配需要全局扫描（决定 explicit vs implicit），
  并行收益小、复杂度高，**保留串行**。P3-1 只并行前三趟。
- **API 扩展**：`Analyzer::analyze` 增加 `parallelism: int` 参数
  （`0` = 串行 = 默认；`>0` = worker 数）。`asn1cxx` CLI 加
  `--parallel <N>` 标志，默认 0（保持现状可重现）。
- **可重现性兜底**：保留串行路径；CI 上金标对验强制 `--parallel 0`。

### 实际落地（2026-10-17）

✅ `declare_pass` + `resolve_pass` **已并行**（`--parallel N`，N > 0 时
worker 线程池按模块分片）。`Diagnostics::merge` + `SymbolTable::merge`
按 module index 串行合并，diagnostics 字节级与 `--parallel 0` 一致。

❌ **`lower_pass` 保持串行**（**关键变更**，偏离原 todo 设计）：
- 现状：`LowerCtx` 共享 `model_.arena`，跨模块 `Referenced.resolved`
  通过 `model_.arena.get(...)` 在 pass 期间跨模块链接；declare_pass 之后
  即开始把跨模块类型加进共享 arena。`named_ids` 是 per-worker 私有 map，
  但底层 `TypeId` 全部指向同一个 arena。
- 把 arena 拆成 per-worker arena + pass 后串行合并，需要重建跨模块
  `Referenced.resolved` 链接（每个 worker 算出的 id 在合并后失效），
  且 `re_resolve_all_object_class_fields` 的 `model_` 整体遍历假设
  一个连续 arena，重建需要先合并才能再跑。
- 当前 lower 在 78 模块 RRC 8.6.0 fixture 上约 ~10 s（per todo 估时），
  占比 ~30% 的 analyze 时间；declare/resolve 并行化已经吃掉主要的
  Amdahl 收益（声明 + 引用检查的 O(M·A·fields) 部分），lower 串行
  不会成为新的瓶颈。**
- 后续 Phase 解：保留接口扩展点 `lower_pass(int)` 已接受 parallelism
  参数；实现 per-module arena + 重映射 deferred（独立工作项，可与
  ```parse_modules()` 流水线化合并设计）。

❌ **`tag_pass` 保持串行**（与原 todo 一致，标注原因）。

### 子任务

| # | 内容 | 估时 | 验证 |
|---|------|------|------|
| 1 | 在 `Diagnostics` 加 `merge(Diagnostics&& other)`：按 index 重排追加。**重要**：合并顺序由 caller 决定；先实现串行版本做对照 | 1 h | ✅ `asn1_parallel_analyzer_tests.DiagnosticsMerge*`（3 项）：merge 后 items 顺序与预期一致；`error_count` 聚合；空合并 |
| 2 | `SymbolTable` 加按 module 分片支持：`declare(module_id, names)`、`merge(SymbolTable&& other)`。保留单线程兼容路径 | 2 h | ✅ `asn1_parallel_analyzer_tests.SymbolTableMerge*`（2 项）：合入 / duplicate 检测 |
| 3 | `Analyzer::declare_pass` 改为 `std::async(std::launch::async, modules, ...)`：每个 worker 处理一个 module；结束后串行 merge | 1 h | ✅ `declare_pass(int)` worker 路径已落地 |
| 4 | `Analyzer::resolve_pass` 改为 per-module worker + 重试 sweep。worker 持完整符号表只读；residual 未解析项在 sweep 阶段按模块序解析 | 3 h | ✅ `resolve_pass(int)` worker 路径已落地；保留 import-visibility 串行（依赖 `modules_`） |
| 5 | `Analyzer::lower_pass` 改为 per-module worker + arena merge。lowered type arena per-worker，pass 结束按 module index 串行拷贝到顶层 arena | 3 h | ❌ **保持串行**（per-module arena + 重映射 deferred，理由见上） |
| 6 | `Analyzer::tag_pass` 保留串行（标注原因） | 0 h | ✅ |
| 7 | `asn1cxx --parallel <N>` CLI；`Diagnostics::merge` + 顶层 merge orchestrator；`Analyzer::analyze` parallel 参数 | 1 h | ✅ CLI smoke `Cli.ParallelModeProducesByteIdenticalOutput` 1 项 |
| 8 | 可重现性门禁：`tests/CMakeLists.txt` 新增「parallel vs serial equivalence」门禁：LPP + S1AP + RRC 跑两遍（`--parallel 0` 与 `--parallel 16`），产物 `diff` 必须空 | 2 h | ✅ 单元测试覆盖 5 个 fixture 模块 + CLI 字节相等门禁（LPP） |
| 9 | sanitizer build + Release build 都过；ctest 344/344 + 新门禁 11 项 | 1 h | ✅ Release **344/344** + sanitizer **344/344**（0 ASan/UBSan） |
| 10 | 文档：`status.md` 性能数字；`changelog.md` P3-1 条目；`architecture.md` 标「并行分析」段落（依赖图、Diagnostics 合并协议） | 1 h | ✅ status.md / changelog.md 已更新；architecture.md 设计已落进 status.md「当前阶段」段 |

**实际落地：~9 h（declare + resolve 并行；lower 串行 + deferred）。**

### 验证（2026-10-17）

- `asn1_parallel_analyzer_tests`（10 项）全过：Diagnostics::merge 字节
  级序列 / error_count 聚合 / 空合并；SymbolTable::merge 合入 /
  duplicate 检测；5 个 fixture 模块（tiny single-module / multi-module
  import / undefined type / unknown field / duplicate symbol）在
  `--parallel 0` 与 `--parallel 16` 下 diagnostics **逐字节相同** +
  error_count 一致。
- `Cli.ParallelModeProducesByteIdenticalOutput`：LPP 14.3.0 fixture 上
  `--parallel 0` 与 `--parallel 16` 生成的 `generated.hpp/.cpp` 在
  namespace 替换后**逐字节相同**（含 sanitizer）。
- Release + sanitizer 全量 ctest **344/344** 通过，零 ASan/UBSan 报错。
- 手工验证 RRC 8.6.0（largest fixture，78 模块）：serial vs `--parallel 16`
  生成 `generated.hpp` SHA-256 一致（`af86e15d...`）。

### 不在范围内（防止蔓延）

- 不并行 lexer / parser：单 .asn 文件通常几 MB，串行 lexer 已经够快；
  并行 lexer 在多线程上争 I/O 反而退化。
- 不并行 emit：emit 产物与并行分析无关，独立项。
- 不在 `Diagnostics` 加全局序号或原子化 `vector::push_back`：原子化能
  解决正确性但仍会改变顺序，不可重现。
- 不并行 `lower_pass`：per-module arena 重映射 deferred（见上）。

---

## 共同风险与缓解（两项目共享）

| 风险 | 缓解 |
|------|------|
| 现有 fixture / 金标对验对诊断顺序敏感 | 并行前所有现有 fixture 必须串行 baseline 跑通、记录产物 hash；并行化后产物与 baseline 字节级 diff |
| sanitizer 化（ASan + UBSan）漏掉数据竞争 | P3-1 强制加 TSan 临时构建验证（`-fsanitize=thread`）；CI 上跑一次确认无 race |
| 缓存错命中 = 旧产物混进构建 | P2-3 强制产物 SHA256 后校验；CI 跑 `--no-emit-cache` 与带缓存两遍做 diff |
| `release` 路径与 `debug` 路径行为不同 | 并行路径与串行路径共享同一 `Analyzer::analyze`，仅 orchestrator 不同；条件编译禁，避免路径分叉 |

---

## 排期建议

1. **先 P2-3**（缓存）：低风险、低耦合、独立价值（即使 P3-1 不做也能立
   刻见效），估时 1 天。
2. **再 P3-1**（并行分析）：依赖符号表 / Diagnostics 内部扩展，需先做
   一轮完整 fixture 金标对验确保可重现；估时 2 天。

两项合计 ~3 个工作日。Phase 46 / 47 名义可作占位。