# 项目状态

当前状态的唯一权威。状态截至 **2026-10-15**。历史变更见
[changelog.md](changelog.md)。

## 当前阶段

**Phase 45 — 大模块分文件/分命名空间 emit（`--emit-split`），单 codec × 全家族门禁已就位。**

- `--emit-split` 已落地：按 ASN.1 模块边界生成 `<Module>.hpp/.cpp` 对 + 聚合
  头；跨模块引用限定；互相递归模块（SCC）合并进同一 emit 单元；LPP/S1AP/
  RRC 逐单元编译门通过；flat 产物与拆分前逐字节一致；
- 单 codec × 全家族编译门禁已补齐：LPP × 12 codec 各跑一次独立 emit + 编译
  （`asn1_lpp_single_{uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer}_compile`）；
  此前 xer/exer/cxer 单选触发的 `make_error is not a member of asn1`
  缺口（XML 分支未 include `<asn1/runtime/byte_io.hpp>`）已修复，全 36
  case 零错误。

## 构建与测试状态

| 门禁 | 结果 |
|------|------|
| Release 构建 ctest | **320/320 通过**，0 失败 |
| Sanitizer 构建 ctest（ASan+UBSan，Debug） | **320/320 通过**，0 失败，零 ASan/UBSan 报错（含 out-of-line 定义 TU 全量编译） |
| `-Wall -Wextra` | 无新增警告 |

## 编译门禁状态

| 门禁 | 结果 |
|------|------|
| 全 codec 产物门禁 | 7 fixture（tlv_oer_golden / tlv_golden / cer_golden / RRC 8.6.0 / LPP 14.3.0 / S1AP 14.4.0 / rrc_slice）× 12 codec（uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer）`g++ -std=c++17 -fsyntax-only` 全部零错误 |
| 单 codec × 全家族门禁（新增） | LPP × 12 codec 各跑一次独立 emit + 完整编译（`asn1_lpp_single_{uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer}_compile`）：每族单独选择时产物的 runtime header 依赖自包含。该门禁捕获单 codec 场景的 include 缺口（组合门禁会把其他分支的 header 链入而掩盖） |
| 分文件 emit 门禁（UPER） | `asn1_split_lpp_split`（LPP 单模块路径）、`asn1_split_s1ap_split`（S1AP 三 emit 单元，含合并的 IEs+Containers+PDU-Contents 递归环）静态库编译通过 |
| 分文件 emit 手工矩阵 | LPP uper/aper/coer/ber/der/oer/jer 零错误；S1AP uper/ber/der/oer/jer 零错误；RRC uper 零错误；xer/exer/cxer 单 codec 零错误（XML 缺口已修复，见上）；flat 产物与改动前逐字节一致 |
| 三大 3GPP fixture emit 产物编译 | RRC/LPP/S1AP 全量 emit 产物作为 CMake 静态库编译通过（编译的是真实定义，非 stub） |

## 金标准差分验证状态

| 向量套 | Oracle | 状态 |
|--------|--------|------|
| PER/APER（`tests/per/golden/`，54 向量） | asn1c | 字节级对验通过 |
| BER/DER/OER（`tests/tlv/golden/`，16 用例 × 3 codec） | asn1tools + asn1c（DER 仲裁） | 字节级对验通过 |
| CER（`cer_golden`，10 用例） | 无外部 oracle；X.690 条文推导 + DER 值等价交叉验证 | 通过 |
| 运行时优化回归 | 三套金标准向量零回归（bit_io 批量位写前后输出逐比特一致） | 通过 |

## 剩余工作

| 项目 | 阻塞原因 / 说明 | 建议后续 |
|------|----------------|----------|
| XML 家族单 codec include 缺口（flat + split 均中招，P2-2 暴露） | ✅ 已修复：XML 分支 `emit_includes` 增补 `#include <asn1/runtime/byte_io.hpp>`（`make_error` 所在头）；补单 codec × 全家族发射矩阵的回归（每族单独选择跑一次编译门禁，LPP × 12 codec 全部零错误） | — |
| 完整模块编译 CI / 3GPP 编解码对等验证 | emit + compile 已过；BER/DER/OER 单构造金标准向量已对验（tlv_golden）；剩余为 3GPP 协议消息级完整向量（逐消息 PDU）的对验 | 扩展金标准向量覆盖面；逐步引入协议级消息向量 |
| 许可证文件 | README 标注 TBD（开放源码，发布时添加）；源码头已标注 PROCODEC 版权 2026–2031 | 发布前补 LICENSE |
| PER 位级正确性外部对验（协议级） | `per_golden.asn` 综合向量（含扩展 addition/bitmap/fragmentation/约束原语）已与 asn1c oracle 字节级对验通过；剩余为 3GPP 协议消息级完整向量的对验 | 扩展 per_golden 向量覆盖面；逐步引入协议级消息向量 |
| 增量代码生成（P2-3，未排期） | asn1cxx 每次全量 emit | 内容哈希缓存（源文件 hash → 已生成产物），与 `--emit-split` 按模块粒度天然配合 |
| 前端并行分析（P3-1，未排期） | 语义分析四趟串行 | 第一趟符号声明按模块并行，后三趟按依赖图并行；需线程安全 Diagnostics；诊断输出逐字节可重现是硬要求 |

## 已知限制

1. **emit 成功 ≠ 可编译 / 正确** —— 全部门禁定义见
   [development.md](development.md#验证门禁纪律)；
2. **组合门禁掩盖单 codec 缺口** —— 全 codec 位掩码单次发射会把其他分支的
   include 带进产物；单 codec 场景必须单独验证（XML 家族缺口即由此暴露）；
3. **电信 fixture 测试不等于 3GPP 完整对等** —— 当前覆盖 parse + analyze +
   emit 编译门禁 + 字节级金标准差分，协议级逐消息 PDU 对验在路线图（见上表）；
4. **CER 无外部 oracle** —— asn1c v0.9.29 与 asn1tools 均无 CER 实现，
   金标准由 X.690 条文推导 + DER 值等价交叉验证；
5. **许可证未定** —— 源码头已标 PROCODEC 版权，正式许可证文件发布时补齐。
