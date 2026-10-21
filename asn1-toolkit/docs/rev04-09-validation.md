# REV-04..09 第二批验收记录（2026-10-20）

## 修正范围

| 任务 | 结果 |
|---|---|
| REV-04 | MSB-first 尾位取高位；source/bit-count 容量验证；scalar put_bits >64 返回 InvalidArgument；独立逐位模型覆盖 8×18 组合 |
| REV-05 | BigInt 明确区分无符号幅值与 signed minimal two's complement；负数、符号保护字节、零及 >64bit 正确编码/解码；非法空/冗余表示拒绝 |
| REV-06 | 单 determinant 显式 more；完整字符串/open type 交错处理片段与 payload，整片末尾零、多片、截断及非法 C0/C5/FF；旧 scalar helper 对片段显式 Unsupported |
| REV-07 | 长度描述符区分 fixed/constrained/semi/unconstrained/extensible；BIT STRING 实际长度；INTEGER 根区间/半约束/上界/无界及完整 int64；64K SIZE 上界走一般分片 |
| REV-08 | 唯一 `per_core.c` 以 Variant 参数化；`per.c` 是旧 ABI UPER 包装；前置 1 bit 下 APER 整数/字符串/长度/open type 的独立字节和尾字段验证 |
| REV-09 | 保留代表性 smoke，补真实 legacy 解码；新增纯 C11 正确性/大值门禁、冻结 oracle 向量与后片分配失败测试；逐声明组登记测试口径 |

没有将 compiler 链接 runtime，没有用 C++ 包装代替纯 C11，没有增加第三方
运行时依赖。C++ runtime、IR 和生成器语义未修改；本次不是完整 C emitter。

## 工具与实测

WSL Ubuntu 24.04，GCC/G++ 13.3，CMake 3.28.3。GoogleTest 使用已有本地源码。

| 检查 | 实测结果 |
|---|---|
| Release 全构建/CTest | 348/348 通过 |
| Debug ASan+UBSan 全构建/CTest | 348/348 通过，开启 leaks / halt_on_error |
| `Asn1CRuntimeFailureContracts` | 498 个内部检查，包括 signed export 和后片拼接分配失败、输出/存活分配计数 |
| `Asn1CPerCorrectness` | 4473 个内部检查；生产 64KiB 限制 |
| `Asn1CPerLargeValues` | 相同 4473 检查；单独 128KiB runtime 源副本，不修改生产默认限制 |
| 独立 asn1tools 0.166.0 | 234 个双向 encode/decode 和全字节一致用例；184 个短向量冻结进入 CTest |
| 公共头与生产符号 | 20 头 × C11/C++17 共 40 次独立编译通过；生产库无 asn1_test_* 符号 |
| C11 严格警告检查 | 最终 runtime 与三个 C 测试源 `-std=c11 -Wall -Wextra -Werror -fsyntax-only` 通过 |
| 历史 7 fixture × 12 selector | 84/84 通过；不把 per 别名当 JERI |

新 primitive 测试在 CTest 中不依赖 Python、asn1tools 或 GTest；独立 oracle
脚本只在开发验证时使用。具体声明/负例登记见
[c-runtime-test-coverage.md](c-runtime-test-coverage.md)，所有权与 ABI 兼容说明见
[c-runtime-contracts.md](c-runtime-contracts.md)。

## 重跑命令

本次复用第一批的本地构建目录及 GoogleTest override：

```sh
cmake --build build-rev1-release -j 1
ctest --test-dir build-rev1-release --output-on-failure
cmake --build build-rev1-san -j 1
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build-rev1-san --output-on-failure
PYTHONPATH=/path/to/Python_asn1tools python3 scripts/validate_c_per_oracle.py \
  --build-dir build-rev1-quick/per-oracle
```

首次配置参数见 [第一批验收记录](rev01-03-validation.md)。oracle 脚本使用
`gcc -std=c11 -Wall -Wextra -Werror -shared -fPIC` 编译纯 C 库，与独立工具对验。
`--write-vectors` 只用于明确审阅后的更新；默认重新生成预期并比较冻结头文件。
不把本实现输出回写成金标准。

## 标准仲裁和验证边界

- 新脚本明确排除 asn1tools 的 MAX 下界处理、根外 BIT STRING 未实现、根外
  OCTET STRING 整片缺最终零、APER 有界变长零内容仍对齐等边界；排除项不计入
  234 个通过用例。按标准独立字段构造验证；理由和代码证据见覆盖登记。
- 条文依据：[ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en)
  11.4/11.5/11.7/11.9、13、16、17。保留对 oracle 偏差的显式记录。
- 旧 `decode_octetstring(r,length)` 是裸 payload reader；新 `_ex` 是完整值的
  对称接口。未静默改变旧函数 ABI/参数含义，文档明确推荐新接口。
- 默认 decoder 上限 64KiB；65537 byte 可编码，但默认解码明确 ResourceLimit，
  128KiB 测试目标验证可配置上限下的完整解码。BIT STRING 上限按字节折算位数。
- 单区间 int64 描述符不表示不连续集合或 >64bit 的有界约束；无界大整数由
  BigInt API 支持。字符集原语、normally-small、完整 C 生成类型仍是未来项。
- 既有 asn1c/asn1tools 来源向量随全量 CTest 回归；没有重新运行 asn1c 生成器。
- 历史 selector 列表包含 per 组合别名，JERI 独立入口仍属 REV-15/16；没有以
  此矩阵声称 12 个独立产品入口已经齐全。

## 最终矩阵记录

最终结果 **84/84 通过**，驱动退出码 0，逐项禁用 emit cache；七个 fixture
与十二个历史 selector 同 [第一批矩阵](rev01-03-validation.md#矩阵记录)。
公共头和生产符号检查也通过。REV-04..09 已完成并勾选；第三批 REV-10 起
仍未开始。原始日志保留在忽略目录
`build-rev1-quick/batch2-{build,ctest}-{release,san}.log`、
`batch2-extra-validation.log`、`per-oracle.log`；CTest 详细内部检查输出在各
构建目录的 `Testing/Temporary/LastTest.log`。
