---
name: asn1-remediation
description: 执行 asn1-toolkit 的 REV 修正任务，并同步代码、验证证据和项目文档。用于本仓库 TODO 分批修复与回归维护。
---

# ASN.1 工程修正

本文件维护可复用的操作约束；任务完成状态以
[remediation-todo.md](../../remediation-todo.md) 为准，当前能力以
[status.md](../../status.md) 为准。这里不固定测试数量，也不把历史缺陷当作当前事实。
路径均相对于本文件；执行脚本时从仓库根目录运行。

## 接手任务

读取仓库适用的 AGENTS.md（如存在）、指定 REV、相关接口/实现/测试和当前 diff。
按用户指定的编号或批次处理必要依赖，保留已有未提交修改。先核实调用链或复现，
再做修复；若代码已变更，记录证据，避免重复修改。

只读取与本次任务相关的契约与验收记录：

- C11 内存、状态、所有权：[C runtime 契约](../../c-runtime-contracts.md)、
  [声明覆盖登记](../../c-runtime-test-coverage.md)、
  [REV-01..03](../../rev01-03-validation.md)。
- 位操作、PER Variant、分片与独立向量：[REV-04..09](../../rev04-09-validation.md)。
- C++ Result、诊断与 E-XER 属性：[C++ 错误契约](../../cpp-error-contracts.md)、
  [REV-10..12](../../rev10-12-validation.md)。
- 缓存、并发、codec 组合与构建：[REV-13..19](../../rev13-19-validation.md)、
  [产品入口登记](../../codec-entries.md)、[安装与交叉构建](../../build-and-package.md)。

## 必须保留的设计

- compiler 流水线禁止链接 runtime；runtime 禁止包含 AST/IR/前端头。
  生成产物是桥梁，单头模式的桥梁在生成头中。
- C++ 业务失败使用 Result/Diagnostics，只有 std::bad_alloc 向外传播。
  检查数字转换、文件系统、线程启动及写文件的失败路径；不要机械添加 noexcept。
  C11 使用状态码与输出参数，按逐 API 契约处理失败状态、初始化和释放。
- UPER/APER 共用 Variant 参数化核心；保留 DER/BER、COER/OER、CXER/XER 策略关系。
  多入口同时生成时，各入口必须使用自己的策略，不让全局掩码覆盖同族行为。
- 保留自研 BigInt、零第三方运行时依赖、C++17、纯 C11 和三种生成产物形态。
- 12 是产品入口口径；per 是组合别名。JERI 共享 X.697 JER 线路语法，
  不称为标准 canonical JSON；未实现的 encoding instruction 必须明确诊断。

## 按改动选择回归

### 内存、线路和错误传播

分配失败必须逐层传播，检查容量加乘溢出和资源上限；遵守各 API 的输出提交、
游标及生命周期契约，不假定所有失败都会回滚游标或所有 out 都自动清空。
测试覆盖失败后释放/复用及截断输入。测试 hook 不进入生产库。

PER 改动同时验证两种 Variant，覆盖非零起始位、整数符号/补码、长度阈值、
片段与末尾零 determinant。使用适用的 asn1c/asn1tools 独立向量与交叉解码；
遇到 oracle 能力差异，记录版本、限制及标准仲裁依据。自身往返不能证明线路合规。

生成代码的解析失败应传播到调用方；必填缺失与非法值分别处理，数值完整消费并
核查目标范围。Result 的访问前置条件和源码/线路位置单位遵循错误契约。

### 缓存、并发和生成入口

- 缓存键保留生成器内容指纹、格式版本、完整输出选项、依赖内容和输入顺序。
  无 source_paths 的手工 IR 与不可读输入禁用缓存。对比缓存开/关及 miss/hit
  产物字节；修改 emitter 后覆盖相关 inline/out-of-line/split。
- 指纹集成回归使用独立、未同时构建的 compiler-only 目录：
  `python3 scripts/validate_generator_revision.py --build build-compiler`。
  此脚本会临时修改并恢复构建目录的私有生成头，不与该目录其他构建并发。
- 并发保持有界、按模块序合并诊断；0/1 串行，lower/tag 保持串行。
  验证峰值与确定性，不能仅比较运行时间。
- codec 改动核查 CLI、位掩码、include、runtime 依赖与函数名；比较单入口和组合
  的实际编码结果。用 CodecEntry 测试覆盖真实链接/调用，保留三种产物验证。

### 构建与验收口径

编译选项限定到 target 和适用编译器；C 目标声明 C11/REQUIRED/EXTENSIONS OFF。
维持最低 CMake 兼容、固定 GTest 下载 hash 和离线路径。
MSVC ASan 不冒充 ASan+UBSan。交叉构建区分宿主生成器和目标库，避免在宿主执行目标程序。

影响 runtime 或生成语义时，按 [开发纪律](../../development.md) 完成针对性回归、
Release 与 ASan+UBSan 全量门禁，以及 7 fixture ×12 实际入口语法矩阵：
`cmake --build build --target asn1_codec_matrix`。
语法通过不等于链接、运行或互操作通过，分别记录证据。

安装改动运行 `python3 scripts/validate_installed_package.py --build build --output build/package-check`，
使用新的输出目录，覆盖纯 C、runtime、compiler 和宿主生成消费者；
私有 GTest 不进入 SDK，纯 C 消费者不引入 C++ 标准库。
具体离线/交叉命令与支持范围查阅安装文档，不从历史通过记录推断本次已验证。

大型 fixture 构建根据资源控制并行度；中断/未跑的门禁不记为通过。
只改文档时检查表述与链接，无需重复全量编译。

## 同步收尾

每个 REV 记录实际命令、结果、环境及未验证范围；缺少必需验证时保留“验证待补”。
更新对应契约/使用/架构文档、TODO、status 与 changelog。
历史验收和分析快照保留原数字；只同步当前状态数字。

每批完成时检查本 skill：把已验证且可复用的流程变化同步到对应段落，详细证据
链接到验收文档，不复制整个任务清单。后续 REV-20..28 的待办不能提前写成已实现能力，
尤其不能把 C11 原语与 smoke 通过表述为 Phase49 C emitter 已完成。

该文件是仓库内的 skill 源文件；置于 docs 不代表已安装到 Codex 的技能发现目录。
