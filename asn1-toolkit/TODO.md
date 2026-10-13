# 状态清单 — asn1-toolkit 修复与构建验证

状态：2026-10-13（表约束类型化 IE 开放类型 + 三大 fixture 编译门禁后）

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
| CLI 端到端测试 | `tests/codegen/cli_test.cpp`：子进程方式跑 `asn1cxx`，验证退出码/错误处理/emit 产物（9 项） | `Cli.*` 通过 |
| 跨编码一致性测试 | `tests/codegen/cross_codec_consistency_test.cpp`：同一 Person 语义值在 UPER/APER/OER/COER/BER/DER/JER 下各自往返保真（8 项） | `CrossCodec.*` 通过 |
| 编解码性能基准 | `tests/codegen/codec_benchmark_test.cpp`：吞吐 micro-benchmark（4 项，带宽松下限防 flaky） | `CodecBenchmark.*` 通过 |
| JER 对象类开放类型编解码 | `emit.cpp`：`ObjectClassField`（&Value 开放类型，C++ 层即 byte vector）的 JER encode/decode 由 Unsupported 改为 hex OCTET STRING 编解码（与 `INSTANCE OF` value 一致）；同时补上 JER 生成头缺失的 `byte_io.hpp` include（`make_error` 未定义）；`INSTANCE OF` decode 由 Unsupported 改为 `[type-id, value]` 数组解析 | 新增 `asn1_ioc_jer_roundtrip_tests`（4 项），含空开放类型与短数组拒绝用例 |
| CLI 测试并行竞态修复 | `cli_test.cpp` 临时文件使用固定名，并行 ctest 时同一二进制多实例（`--gtest_filter` 每测试一进程）互相踩同名 out/err 文件导致随机失败；改为 pid 后缀 | 并行 `ctest -j` 连续多轮 9/9 稳定通过（修复前必挂） |
| 表约束类型化 IE 开放类型 | `analyzer.cpp`：entries 只挂开放类型字段（修复 criticality 等固定类型字段被误挂 `Decoded_` variant 的问题）；扩展枚举未显式编号时从 root 末尾值继续编号（X.680 惯例，修复 switch case 冲突）；`emit.cpp`：tc decode/encode 的 visit lambda 参数改名 `tc_alt`（避免与嵌套 CHOICE 的 `alt` 变量冲突）；OCF fixed_type 与内联 SEQUENCE OF 元素纳入 collect_named 依赖遍历（修复类型未 emit/顺序靠后）；去重按 C++ 名而非 name+module（修复参数化实例化重复定义）；内联匿名 SEQUENCE OF/SET OF 直接用裸 vector 而非 `.value`（修复 RRC/LPP 编译错误） | 新增 `asn1_tc_ie_roundtrip_tests`（4 项：typed encode 填充开放类型字节+governor id、按 id dispatch decode、固定类型字段不特化、未知 id 保留原字节） |
| 三大 3GPP fixture 编译门禁 | `tests/CMakeLists.txt`：RRC(UPER)/LPP(UPER)/S1AP(APER) 全量 emit 产物作为静态库编译门（emit 成功 ≠ 可编译） | Release + sanitizer 全量构建通过（此前从未验证过可编译性） |

## 剩余 / 未完成

| 项目 | 阻塞原因 | 建议后续 |
|---|---|---|
| 完整模块编译 CI / 3GPP 编解码对等验证 | emit+compile 已过；仍未与 3GPP 金标准字节向量对等验证（仅自往返一致） | 增加金标准向量对验证 |
| 许可证文件 | README 标注 TBD（开放源码，发布时添加） | 发布前补 LICENSE |
| PER 位级正确性外部对验 | 目前仅"自往返一致"，未与 X.691 金标准 / asn1c 交叉验证 | 引入外部金标准测试向量 |

## 构建状态

- 默认（Release）：**289/289 通过**，0 失败
- Sanitizer（ASan+UBSan，Debug）：**289/289 通过**，0 失败，零 ASan/UBSan 报错、零泄漏
- 三大 3GPP fixture（RRC/LPP/S1AP）emit 产物全量编译门禁通过
- `-Wall -Wextra`：无新增警告
- 无残留 `Span<int>` 编译错误

## 备注

- 本地领先 origin/fork 12 个提交：ec04843（sanitizer+离线）、01b1c09（CI 分支触发）、c28da0d（UAF 修复）、4aed4c8（span SFINAE + 初始 CI）、6d94856（TODO 260/260）、9ad969a（CLI/cross-codec/benchmark 测试）、398ed3b（docs 重组）、808dd05（JER OCF + CLI 竞态）+ 本轮提交（表约束类型化 + fixture 编译门禁）。
- PR #1（→ Tinacao2007/asn1-toolkit）待属主批准 fork-PR workflow 门控后由 CI 运行。
