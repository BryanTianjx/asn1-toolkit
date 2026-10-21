# C++ 错误与诊断契约

编译器与 runtime 共享 `asn1/common/result.hpp` 的数据定义，不互相链接。
业务失败通过 `Result<T>` 或编译阶段已有的 `Diagnostics` 返回；只有
`std::bad_alloc` 可以向外传播。不要给可能分配内存的入口机械添加 `noexcept`。

## Result 访问

`Result<T>` 和 `Result<void>` 为 C++17 `[[nodiscard]]` 类型。先检查成功状态，
再调用 `value()`；失败状态才能调用 `error()`。违反前置条件是程序员错误：
Debug assert，Release 也终止进程，不抛 `bad_variant_access`。
左值/const 左值访问返回引用；右值 `value()` 支持移动所有权。

`TypeArena::get/get_value` 同样要求有效且属于当前 arena 的 ID；无效 ID
属于程序员错误。它不承担验证外部输入的职责。

## 源位置与线路位置

| 信息 | 表示 | 单位与范围 |
|---|---|---|
| 编译诊断 | `Diagnostic::where` / `SourceRange` | 文件名，1-based 行和 UTF-8 **字节列**；不是 Unicode 字符列 |
| 线路错误 | `Error::offset` + `offset_unit` | 0-based Byte / Bit；基于产生错误的当前 reader，不自动换算为外层完整报文偏移 |
| 未知线路位置 | `OffsetUnit::Unknown` | 旧调用点或没有可定位的线性输入；offset=0 不足以证明“报文起点” |
| 逻辑字段 | `Error::field_path` | 可空；本批 E-XER 属性错误记录属性名，不承诺所有 codec 已构建完整嵌套路径 |

BitReader 和 ByteReader 的直接错误分别标 Bit、Byte。`make_error` 的新增
单位参数默认 Unknown，不能根据 codec 名称猜测或将旧错误默认为 byte。
后续 codec 的定位增强须在知道实际单位时逐调用点补充，不能批量误标。
runtime Error 不包含 AST、IR 或编译器源范围。

## 编译器入口与资源失败

- `SourceFile::from_path` 返回 `Result<SourceFile>`，打开失败和读取失败显式
  返回错误；分块读取避免 streambuf iterator 的 I/O 异常路径。
- `ir::BigInt::from_decimal` 返回 `Result<BigInt>`，空值、非数字拒绝，规范化
  负零，正确识别 int64 两端；调用者必须传播失败或记录源诊断。
- CLI `--parallel` 与 OID arc 用 `from_chars`，拒绝尾随内容和溢出。OID 错误
  保留原 token 源范围。并发上限本身仍属 REV-14。
- 输出目录与缓存目录使用 `filesystem(error_code)`；输出流 flush 后检查
  失败，失败不能记录成功缓存。缓存不可用可退化为重新生成。
- OS 线程创建 `system_error` 回退 deferred 执行，按模块顺序 get/合并，结果
  保持确定。任务内部异常不由此捕获，bad_alloc 不被吞掉。
- 标准容器容量失败 `length_error` 在 Analyzer/CppGenerator/CLI 入口转换
  为诊断；这不是业务异常模型，也不是捕获所有 `std::exception` 的兜底。

## 集成迁移

`SourceFile::from_path`、`ir::BigInt::from_decimal` 的返回类型发生源兼容变化，
必须先检查 Result。Error 增加单位与字段路径会改变 C++ 对象布局；编译器、
runtime、生成代码和消费者应一起重新编译，不承诺旧静态库二进制兼容。
C11 status/out 参数 ABI 本批未修改。

本批生成器修复验证显式使用 `--no-emit-cache`。REV-13 完成前，升级生成器后
应禁用旧 emit cache 并重新生成，避免旧产物掩盖修复。
