# 状态清单 — asn1-toolkit 修复与构建验证

状态：2026-10-12（全部测试通过 + sanitizer 门禁就位后）

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

## 剩余 / 未完成

| 项目 | 阻塞原因 | 建议后续 |
|---|---|---|
| 表约束类型化 IE 开放类型（table-constraint typed IE open types） | 设计事项，未实现 | 下一 Phase |
| 完整模块编译 CI / 3GPP 编解码对等验证 | emit 成功 ≠ 完整 3GPP 编解码对等；仅 parse+analyze+emit + 部分往返 | 增加金标准向量对验证 |
| 许可证文件 | README 标注 TBD（开放源码，发布时添加） | 发布前补 LICENSE |
| PER 位级正确性外部对验 | 目前仅"自往返一致"，未与 X.691 金标准 / asn1c 交叉验证 | 引入外部金标准测试向量 |

## 构建状态

- 默认（Release）：**260/260 通过**，0 失败
- Sanitizer（ASan+UBSan，Debug）：**260/260 通过**，0 失败，零 ASan/UBSan 报错、零泄漏
- `-Wall -Wextra`：无新增警告
- 无残留 `Span<int>` 编译错误

## 备注

- 本地领先 origin/fork 4 个提交：ec04843（sanitizer+离线）、01b1c09（CI 分支触发）、c28da0d（UAF 修复）、4aed4c8（span SFINAE + 初始 CI）。
- PR #1（→ Tinacao2007/asn1-toolkit）待属主批准 fork-PR workflow 门控后由 CI 运行。
