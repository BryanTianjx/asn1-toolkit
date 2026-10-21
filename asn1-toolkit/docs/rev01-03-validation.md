# REV-01..03 验收记录（2026-10-20）

## 变更与证据

| 任务 | 修复前调用链证据 | 本批修复 |
|---|---|---|
| REV-01 | void ensure 在 realloc 失败后返回，部分调用方仍复制或继续写入 | ensure 返回状态；整次 writer 操作预留容量；加法、位转字节及容量倍增检查；take 失败不修改有效位；clear/take 后复用不残留旧 1 位 |
| REV-02 | decimal parse → mul_add → push_limb，最后一步失败被忽略 | 临时 BigInt 解析、成功提交；OOM/limb 上限逐层传播；幅值序列化失败不提交输出 |
| REV-03 | 分配失败混用 LengthOverflow；PER BIT STRING 解码先改输出再读；BER tag 解码提前写输出 | 保留 0..7，新增 OOM=8/ResourceLimit=9；PER 输出临时构造和失败清理；BER 输出延迟提交；reader 截断预检；文本缓冲边界安全减法及 NULL/长度验证；逐接口契约文档 |

新增 `tests/c_lang/c_runtime_failure_test.c`：逐分配点失败注入、live allocation
计数、输出不变与恢复路径、已填满/未对齐 writer、limb 上限相邻值、重复 free
以及 reader 输入复制等，共 424 个内部检查。测试专用源副本带私有 allocator
宏；生产库无全局可变 allocator 或测试 hook。初始化、复用和失败游标边界见
[c-runtime-contracts.md](c-runtime-contracts.md)。

## 实测环境与结果

WSL Ubuntu 24.04，GCC/G++ 13.3.0，CMake 3.28.3，Ninja 1.11.1。
GoogleTest 使用已有本地源码，未下载新测试依赖。

| 验证 | 结果 |
|---|---|
| 独立 gcc C11 `-Wall -Wextra -Werror -O2` failure driver | 424 个检查通过 |
| 相同编译选项、无 test allocator 的原 smoke | 通过 |
| 独立 gcc C11 ASan+UBSan failure driver（含 leaks） | 424 个检查通过，无 sanitizer 报错 |
| CMake Release 完整构建及 CTest | 346/346 通过，包含既有 345 项及新增 failure gate |
| CMake Debug ASan+UBSan 完整构建及 CTest | 346/346 通过，包含 smoke、failure、既有 golden 回归 |
| 20 个公共 C 头分别独立编译 | C11 与 C++17 共 40 次 `-fsyntax-only -Wall -Wextra -Werror` 通过 |
| 生产库 `nm` | 不含 `asn1_test_*` 符号 |
| 现有 7 fixture × 12 selector 生成语法矩阵 | 84/84 通过，每次关闭 emit cache；口径限制见下 |

第一次同时启动两组 `-j4` 全量构建时 WSL 响应超时，已停止本次构建并改为
`-j1` 顺序执行；最终两组完整构建及 CTest 均成功。没有修改全局 WSL 配置。

## 可复现命令

在仓库根目录的 Linux shell 执行；`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` 替换
为自己的 GoogleTest 1.14.0 源码目录。本次取已有 `build/_deps/googletest-src`。

```sh
cmake -S . -B build-rev1-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build-rev1-release -j 1
ctest --test-dir build-rev1-release --output-on-failure

cmake -S . -B build-rev1-san -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DASN1_ENABLE_SANITIZERS=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build-rev1-san -j 1
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build-rev1-san --output-on-failure
```

独立 C 故障测试（先创建 `build-rev1-quick`）：

```sh
gcc -std=c11 -Wall -Wextra -Werror -O2 \
  -DASN1_C_RUNTIME_TEST_ALLOCATOR -Iinclude \
  src/c_runtime/*.c tests/c_lang/c_runtime_failure_test.c \
  -o build-rev1-quick/failure
./build-rev1-quick/failure
```

独立 sanitizer 使用同一命令，将 `-O2` 替换为
`-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer`，
并带相同 ASAN/UBSAN 环境变量运行。

本地完整日志在忽略的 `build-rev1-quick/{config,build,ctest}-{release,san}.log`
及 `extra-validation.log`；本记录保留验收摘要，日志不加入源码仓库。

## 验证边界

- 运行了仓库保存的 asn1c/asn1tools 来源 golden 向量回归；未重新安装/运行外部
  oracle 生成器，未改写 golden。C PER 补码/分片等第二批问题仍待修复。
- 本批没有生成器、IR 或 C++ runtime 语义改动；既有 split 和单 codec 链接/编译
  门禁随完整 CMake 构建执行。
- 历史 12 selector 为 `uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer`。
  `per` 是组合别名，不是 JERI；此矩阵不能证明 12 个独立产品入口完整。
  JERI 与矩阵口径修正仍由 REV-15/16 处理，不作为本批已实现能力。
- Windows/MSVC ABI、完整 C codec 标准覆盖、C emitter 与第二批线路修复均不在
  本批验收范围内。

## 矩阵记录

最终结果：**84/84 通过**，驱动退出码 0。七个 fixture：

- `telecom/tlv_oer_golden.asn`
- `telecom/tlv_golden.asn`
- `cer/cer_golden.asn`
- `telecom/rrc_slice/rrc_slice.asn`
- `telecom/lpp_14_3_0/LPP-PDU-Definitions.asn`
- `telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn`
- `telecom/s1ap_14_4_0/s1ap_14_4_0.asn`

对每个 fixture 和上述 12 selector，执行：

```sh
./build-rev1-release/asn1cxx --no-emit-cache --emit-dir <dir> \
  --codec <selector> tests/fixtures/<fixture>
g++ -std=c++17 -fsyntax-only -Iinclude -I<dir> <dir>/generated.cpp
```

JERI 尚无独立生成入口，因此上述是历史矩阵的回归结果，不是 12 个独立入口
已完成的声明；既有缺口保留在 REV-15/16。REV-01..03 的本批验收完成。
