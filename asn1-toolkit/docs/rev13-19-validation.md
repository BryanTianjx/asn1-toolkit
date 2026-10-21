# 第四批 REV-13..19 验收记录（2026-10-21）

本批限定缓存、并发、产品入口、构建和包消费。编译器不链接 runtime；runtime
不包含 AST/IR；Result、共享 PER Variant、自研 BigInt、三种产物形态均保留。
C11 emitter 的后续子任务没有被本批宣称完成。

## 修改与证据

| REV | 已核实问题 | 修复与可重跑证据 |
|---|---|---|
| 13 | 缓存版本写死；无 source_paths 时共用 src:empty；文件 read 错误可得到部分 hash | CMake 按 compiler 源/头、构建定义和工具链生成指纹；格式版本 2；读取失败禁用缓存；无源路径 API 禁用缓存；选项/依赖/输入顺序/输出损坏/模型变化回归 |
| 14 | workers 未使用，两个分析 pass 均按模块数启动 async | 共享 bounded_ordered 有界批次，按模块顺序合并；0/1 串行；lower/tag 保持串行；线程资源失败保留 deferred 回退 |
| 15 | 缺少 JERI CLI/掩码；误称标准 canonical JSON | Jeri 独立产品位及 CLI；共用 JER 实现，新增 encode_jeri/decode_jeri；落实 ARRAY 控制段并明确拒绝其余尚未支持控制段；[入口登记](codec-entries.md) 明确同一种 X.697 线路语法 |
| 16 | per 被重复计数；若干库映射错误；OER/XML 组合策略覆盖同族入口 | 84 个实际单选语法检查；12 个单入口及组合三形态真实链接、调用和编码结果比较；OER/COER 与 XER/CXER/E-XER 各自生成；旧 E-XER-only xer 别名保留 |
| 17 | 全局 GNU flags、硬编码 C11、新版下载参数 | 项目 target 级编译器/平台条件；C_STANDARD 11/REQUIRED/EXTENSIONS OFF；MSVC utf-8；旧 CMake 条件参数；MSVC 明确拒绝 ASan+UBSan 组合 |
| 18 | GTest 下载无 hash；宿主/目标工具混用风险 | 固定 zip SHA256；本地源码覆盖/测试关闭均离线；宿主工具路径与依赖；交叉默认关闭目标测试，显式开启报错；两阶段示例 |
| 19 | 只有 INSTALL_INTERFACE，无导出包 | install/export/config/version 与组件检查；关闭私有 GTest 安装并检查 SDK 不含 gtest/gmock 头；compiler-only/runtime-only 开关；C/C++/compiler/宿主生成四类独立消费者；纯 C 无 C++ 标准库依赖 |

代码入口包括 `src/codegen/emit_core.cpp`、`emit_{jer,xer,oer}.cpp`、
`include/asn1/support/async.hpp`、`src/semantic/analyzer.cpp`、根/测试 CMake、
`cmake/`、`scripts/validate_*.py`、`examples/package-consumers/`。

JERI 标准命名依据及局部能力边界见 [入口登记](codec-entries.md) 中的 ITU 原始来源。
本批不是完整标准语义审计，仍保留 REV-20..24；未运行新的外部 oracle 生成任务，
全量 CTest 包含已有独立 oracle 冻结向量回归，不以新增自往返冒充外部金标准。

明确家族后的最小输入曾触发 `CodecEntry.CombinationEquality` 失败：控制段
`[ARRAY] SEQUENCE` 被忽略，生成 object 而非 array。修复前日志保留为
`batch4-array-control-repro.log`；修复后正例实际输出 `[42,true,"ASN1"]`，
新增 TEXT/ENUMERATED 未实现控制段负例要求明确诊断，MSVC/GCC 针对性检查已通过。

## 工具与完整门禁

| 工具/门禁 | 本次实际结果 |
|---|---|
| GCC/G++ 13.3.0，CMake 3.28.3，Ubuntu 24.04 WSL | 原生工具链 |
| GCC Release 全量构建 + CTest | **392/392 通过** |
| GCC Debug ASan+UBSan 全量构建 + CTest | **392/392 通过** |
| Clang 18.1.3 Release 全量构建 + CTest | **392/392 通过** |
| Clang 18.1.3 Debug ASan+UBSan 全量构建 + CTest | **392/392 通过** |
| CMake **3.16.8** 离线配置 | 通过；实际编译并运行 C11 smoke、12 入口组合，**2/2** |
| MSVC **19.44.35214**，VS 2022 x64 Release | compiler、全部 runtime 构建通过；安装后四类消费者通过 |
| MSVC 缓存/并发/codegen 回归 | **11 + 13 + 16** 用例通过；含不支持控制段负例，JERI/E-XER/12 组合程序实际调用成功 |
| MSVC ASN1_ENABLE_SANITIZERS=ON | 按预期配置失败，明确提示 ASan 不等于 UBSan |
| 公共 C++17 头独立严格编译 | **47/47** |
| 7 fixture × 12 实际产品入口单选语法矩阵 | **84/84**；使用 JERI，未计 per 别名 |
| 离线 compiler-only / runtime-only 独立构建、安装与消费者 | 通过；配置不进入 GTest 下载；C 消费者 ldd 无 libstdc++/libc++ |
| 完整 GCC/MSVC 包独立消费 | 各四类通过：C、runtime、compiler、host-generated |
| 宿主/目标连线与真实异构链接 | Generic 连线及拒绝交叉测试检查通过；x86_64 宿主生成 → Clang 18.1.3 + AArch64 sysroot 的 C++ 消费者链接通过；纯 C AArch64 消费者也链接通过，readelf 确认 AArch64，两者 **0** 个宿主运行测试 |
| Python -O 下门禁检查 | 正例通过，注入错误组合输出按预期失败；入口门禁 **16/16** 重跑通过 |
| 实际更换生成器指纹后的缓存失效 | 隔离 compiler-only 构建：同 ASN.1 hit → 改指纹重建 → miss；最后恢复指纹并重建通过 |

Clang 与 CMake 3.16.8 的开发工具只下载/解包到忽略的构建目录，未修改系统安装，
未引入项目运行时依赖。首次同时运行多份大型构建出现 WSL 内存压力，已中断该轮
Release/sanitizer 构建；最终大型 fixture 完整轮次改为串行；重新生成后采用 copy_if_different / Ninja restat 复用字节相同的编译对象；大型仅编译 fixture 使用 -g1 减少调试类型图内存，
全部 sanitizer 检查仍开启。未把中断结果记为通过。

## 可重跑命令

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build-release -j1
ctest --test-dir build-release --output-on-failure

cmake -S . -B build-san -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DASN1_ENABLE_SANITIZERS=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build-san -j1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-san --output-on-failure

# Clang 两轮使用相同配置，并显式选择 clang / clang++
# -DCMAKE_C_COMPILER=/path/to/clang -DCMAKE_CXX_COMPILER=/path/to/clang++
cmake --build build-release --target asn1_codec_matrix
python3 scripts/validate_public_headers.py
python3 scripts/validate_installed_package.py --build build-release --output build-package-check
python3 scripts/validate_generator_revision.py --build build-compiler-only
```

最后一条只用于独立且未同时构建的 compiler-only 目录；它修改并恢复该构建的
私有生成头，不修改仓库源文件。独立组件及两阶段命令见
[构建与安装消费](build-and-package.md)。

本地日志保存在忽略目录 `build-rev1-quick/batch4-*.log`：最终 GCC 构建分别为
`batch4-build-release-final.log` / `batch4-build-san-final.log`，小型驱动收口构建为
`batch4-release-final-refresh.log`；四个完整测试日志均为 `batch4-ctest-*.log`。
矩阵 `batch4-matrix.log`、最低版本 `batch4-cmake316-final.log`、安装
`batch4-package-{gcc,msvc}-final.log`、独立组件 `batch4-components.log`、交叉连线
`batch4-cross.log` / `batch4-aarch64.log`、指纹回归 `batch4-revision-persistent.log`。

## 验证边界

- 实际交叉构建验证了 Linux AArch64 的编译/链接和 ELF 架构；没有设备运行证据。
  不据此承诺裸机 libc、其他 ARM ABI 或特定电信设备已通过。
- MSVC 本批不是全部 fixture 的完整原生测试矩阵；POSIX CLI 子进程测试不在
  Windows 注册。最低 CMake 只做所列配置/代表构建，不称全部测试已在 3.16 重跑。
- 84 个语法检查不证明链接或互操作；链接证据另由代表 schema 的 CodecEntry
  程序提供。JER ARRAY 和 E-XER ATTRIBUTE 参与组合比较；CXER 使用自己的 writer。
- 缓存依赖调用者完整 source_paths；手工 IR 无来源时关闭缓存，不伪造 IR 序列化。
  同目录并发生成、原子发布/旧产物管理仍由 REV-25 处理。
- 12 是产品入口口径，完整语法/约束/规范策略和外部 oracle 空白仍按后续编号核查。
