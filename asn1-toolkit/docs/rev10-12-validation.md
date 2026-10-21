# REV-10..12 第三批验收记录（2026-10-20）

## 修正范围

| 任务 | 实现与证据 |
|---|---|
| REV-10 | 属性先判存在；mandatory 缺失报错，optional 清空，default 恢复 ASN.1 默认；非法值立即传播；整数通过 BigInteger Result 完整解析，检查规范化根区间及宿主范围后转换 |
| REV-11 | SourceFile / IR BigInt 返回 Result；OID / CLI 数字 from_chars；目录操作 error_code；写入 flush 检查；线程创建失败 deferred 回退；容量异常在入口转换诊断；移除 catch(...) 和业务 throw；PER restricted-string 编码失败立即传播 |
| REV-12 | Result nodiscard、错误访问 assert/终止；Error 增加 offset_unit/field_path；BitReader/ByteReader 标单位；源位置明确 UTF-8 字节列；公共头 C++17 独立编译脚本（含 -Wall/-Wextra/-Werror/-pedantic-errors） |

新增 E-XER fixture 以 out-of-line、inline、split 三种模式各自生成、编译、
链接、运行。用例涵盖缺 mandatory、非法 bool、12x、数值溢出、约束越界、
非法 BigInteger、optional/default 缺失与非法值、逻辑字段名。全部显式禁用
emit cache；不会靠旧缓存通过生成器回归。

另有 SourceFile 不存在/目录读取、BigInt int64 两端及负零、OID 溢出源范围、
CLI 部分数字/溢出/输出为文件、`/dev/full` 写失败、线程资源失败注入及
bad_alloc 不转换、严格消费者 Error 聚合初始化、Result move-only/const/错误访问死亡测试、bit/byte 单位。
识别 int64 边界后同步消除 ENUMERATED 后继编号的有符号溢出；无法隐式表示的
后继报诊断，不悄悄变成零。

`nodiscard` 还揭示 PER 生成器丢弃 restricted-string 的 Result：已修复，新增
UPER/APER 非法字母表输入的实际生成代码测试。两个构建目录的 emit cache
在该补丁后清除，避免旧产物掩盖修正；缓存键的永久失效策略仍是 REV-13。

## 验证命令

本次使用 WSL Ubuntu 24.04、GCC/G++ 13.3、CMake 3.28.3、Ninja、本地已有
GoogleTest 源码。Windows MSVC / Clang 本批未执行。

```sh
cmake --build build-rev1-release -j 1
ctest --test-dir build-rev1-release --output-on-failure
cmake --build build-rev1-san -j 1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-rev1-san --output-on-failure
python3 scripts/validate_public_headers.py --cxx g++
```

| 门禁 | 最终实测 |
|---|---|
| Release 全构建 / CTest | 370/370，0 失败 |
| Debug ASan+UBSan 全构建 / CTest | 370/370，leaks/halt_on_error 开启，0 失败 |
| 公共 C++17 头独立编译 | 47/47，含严格警告与 pedantic |
| Error 聚合初始化 / nodiscard 负向编译 | 正向严格编译通过；丢弃 Result 按预期被 -Werror=unused-result 拒绝 |
| C runtime 公共头 | 全部 C11 与 C++17 严格语法编译通过 |
| 历史 7 fixture × 12 selector | 84/84，显式 --no-emit-cache |
| 生产 C runtime 符号 | nm 无 asn1_test_ 故障注入符号 |

原始构建、CTest、头检查和矩阵日志位于忽略目录 `build-rev1-quick/batch3-*`；
矩阵驱动为本地 `verify3.py`，逐项命令/fixture/selector 保留在 extra-validation
日志。CTest 包含 C smoke、failure、PER 冻结向量与大值、所有既有 golden 回归。

## 验证边界与兼容性

- 历史语法矩阵仍使用 `uper aper per ber der cer oer coer jer xer exer cxer`，
  `per` 是组合别名，不等于已有 JERI 生成入口；真实 12 入口由 REV-15/16
  继续处理，不把 84 项称为 12 种独立线路规则。
- C11 与 C++ PER 原语算法本批未更改；PER 生成器补 restricted-string 失败传播；全量运行既有冻结 oracle 门禁。第二批独立
  asn1tools 234 用例为历史证据，本批不冒充重新执行其 live 差分。
- REV-13 的缓存修订键、REV-14 的并发上限仍待办；本批仅处理相关错误路径。
- 旧 codec 未标明的偏移保留 Unknown；本批未承诺完整嵌套字段路径或 XML
  DOM 错误的原始文本字节定位。
- 新 C++ 返回类型和 Error 布局需要重编译消费者，迁移方式和访问前置条件
  见 [C++ 错误契约](cpp-error-contracts.md)。C11 ABI 与分层铁律保持。
