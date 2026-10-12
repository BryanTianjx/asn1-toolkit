# 待办清单 — asn1-toolkit 修复与构建验证

状态：2026-10-08（构建验证后）

## 已完成

| 项目 | 说明 | 验证 |
|---|---|---|
| span.hpp 修复 | `std::enable_if_t<std::is_const_v<T>, Container>` 使 SFINAE 安全（非 const T 不再硬错误） | `asn1_common_tests` 编译通过 |
| uper/uper_test.cpp 修复 | `decode_null` 临时 `BitReader` 改为命名变量 `empty_reader` | `asn1_uper_tests` 链接成功 |
| CI 工作流 | `.github/workflows/ci.yml`（构建 + ctest） | 文件已写入 |

## 剩余 / 未完成

| 项目 | 阻塞原因 | 建议后续 |
|---|---|---|
| `tests/codegen/person_roundtrip_test.cpp` 源码缺失 | 仓库中仅存在 `.o`，无 `.cpp` 源码；无法修复 `PersonUperRoundTrip`、`PersonAperRoundTrip`、`PerPrimitives.NormallySmallAndLength` | 恢复源码或重新生成测试 fixture |
| 正式待办文件 | 用户中断后未完成独立文件输出 | 本文件已补做 |

## 构建状态

- 构建：`asn1_uper_tests` 通过；其余目标大部分成功
- 测试：242 项中 238 通过；4 失败含 1 项 `uper_tests_NOT_BUILT`（因构建中断），3 项与缺失源码相关
- 无残留 `Span<int>` 编译错误
