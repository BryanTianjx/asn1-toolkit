# C11 runtime 错误与所有权契约

适用于当前 `include/asn1/c_runtime/*.h` 已实现原语。此契约不代表完整 codec
或 C emitter 已完成。标准线路支持范围仍按 Phase49/后续修正项登记。

## 状态码

原有 0..7 数值保持不变，不应把 C/C++ 错误枚举直接强制转换后假设永远对应。

| 状态 | 含义 |
|---|---|
| `ASN1_ERR_OUT_OF_MEMORY` (8) | malloc/calloc/realloc 失败 |
| `ASN1_ERR_RESOURCE_LIMIT` (9) | 已配置的字符串字节数或 BigInt limb 上限被超过 |
| `ASN1_ERR_LENGTH_OVERFLOW` (3) | 长度/容量不能表示、整数转换越界，或调用者输出缓冲区不足 |
| `ASN1_ERR_TRUNCATED` (1) | 输入缺少所需数据 |
| `ASN1_ERR_INVALID_ARGUMENT` (6) | 参数不满足接口契约 |
| `ASN1_ERR_UNSUPPORTED` (7) | 当前未支持能力 |

已有 BER length 原语对 indefinite indicator 返回 LengthOverflow 的历史行为
暂保留；这不表示 indefinite 本身是非法 BER，后续实现另行处理。

## 生命周期和输出

所有指针必须指向有效对象/足够大的存储；不得伪造公开结构的指针、计数、容量。
输入输出不得非法重叠。NULL 数据指针只允许搭配零长度。创建函数不读取旧输出
指针，不自动释放旧对象；不能把活对象的唯一指针直接传给 create 覆盖。

| API 组 | 所有权及初始化 | 失败后状态 |
|---|---|---|
| `byte_view` / `byte_buf` | 无验证的非拥有构造；使用者维持底层内存有效 | 无状态码；后续使用函数验证参数 |
| `bigint_init` / `bitstring_init` | 在新对象或已 free 对象上置零；不要用 init 覆盖活对象 | void；不分配 |
| `bigint_init_from_decimal` | 构造函数，目标必须是新对象、零对象或已 free 对象；复用先 free | 临时构造失败时不修改目标；若希望无条件 free，请先 init 为零 |
| `per_decode_unbounded_int` | 同 BigInt 构造契约，不覆盖活对象 | 输出不变；reader cursor 可能已推进 |
| `bigint_to_int64` / `bigint_to_bytes` | const 输入；返回的字节是无符号大端幅值，不是完整 BER TLV 或有符号补码 | 输出参数不变 |
| `bitstring_resize` | 目标须已 init；保留所有权并可重复使用 | 数据、指针、长度和容量不变 |
| `per_decode_bitstring` | 目标须已 init；成功释放旧存储并接管新存储 | 输出对象不变；reader cursor 可能推进 |
| `bit_writer_create` | 新建拥有缓冲区的 handle | 输出指针不变 |
| `bit_writer_put_*` / `align` | writer 须有效；每次操作先预留所需容量 | 失败不提交有效位或数据；容量不是稳定 API |
| `bit_writer_take` | 返回独立 malloc 缓冲区并清空 writer；部分尾字节零填充 | 输出与 writer 的有效位不变 |
| `bit_writer_clear` | 保留容量，清空有效数据，可重新写 0 bit | void |
| `bit_reader_create` / `byte_reader_create` | 拷贝输入，调用者之后可释放/修改原输入 | 输出指针不变 |
| `bit_reader_get_*` | 返回字节缓冲区由调用者拥有 | 输出及读位置不变；截断在分配前检查 |
| BER tag/length encode/decode | 调用者提供缓冲区或值输出 | 输出缓冲区、值及 consumed 均不提交 |
| JER/XER writer init/puts | 借用调用者输出缓冲区；puts 不做 JSON/XML 转义 | 失败 used/已提交前缀不变 |
| JER writer printf | 借用缓冲区；格式及参数须满足标准 printf 契约 | used/已提交前缀不变；未提交尾部可能被 vsnprintf 修改 |
| JER/XER reader init | 借用输入；只初始化游标，尚非完整解析器 | 参数非法时 reader 不变 |
| PER scalar decode | 标量输出成功后提交 | 输出值不变；reader cursor 可能推进 |
| PER 组合 encode | 多次调用 writer 的组合操作 | 可能已经写入前缀，不承诺整次编码回滚；失败后丢弃/clear writer |

`bigint_free` / `bitstring_free` 只接受已初始化或成功构造对象（或 NULL），释放
后置零，可重复 free。不得 free 任意未初始化结构。`*_destroy(NULL)` 安全，
但同一非 NULL handle 不得 destroy 两次；销毁后调用者应自行置 NULL。

所有返回的 `uint8_t*` 缓冲区使用与 runtime 相同 C allocator/CRT 的 `free()`
释放；空结果也可能返回非 NULL 的可释放指针。不可跨不同 Windows CRT 配置
分配/释放。JER/XER 输入、输出缓冲区始终由调用者拥有。公开结构不可浅拷贝
后分别 free；所有权移动后必须将源置为不拥有状态。

## 失败注入门禁

`Asn1CRuntimeFailureContracts` 单独编译 C 源码并启用私有测试 allocator，
逐分配点注入失败、检查状态/输出/存活分配数。生产 `asn1::c_runtime` 不包含
测试状态或 allocator 配置；测试替换接口不是公共 ABI。

第二批的 PER 补码、分片、变长 BIT STRING 与尾位语义不由此文档宣称已修复。
