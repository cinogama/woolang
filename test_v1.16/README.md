# dev_1.16 冒烟测试套件

本目录针对 `dev_1.16` 分支相对 `master` 的语法/语义变更提供冒烟测试和回归测试。
`pkg/` 为 dev_1.16 语法编译的预编译依赖（仅 x64）。

## 构建

在仓库根目录单独配置一个构建目录（不要复用已有的 `build/` 配置目录）：

```bash
cmake -S . -B build_test_v1.16 -A x64 -DWO_MAKE_OUTPUT_IN_SAME_PATH=ON
cmake --build build_test_v1.16 --config RelWithDebInfo --parallel
```

`WO_MAKE_OUTPUT_IN_SAME_PATH=ON` 会把 `woolang.exe`、`libwoo.dll`、`libwoort.dll`
统一输出到 `build/RelWithDebInfo/`，与仓库原有的输出布局一致。

## 运行

```bash
cd test_v1.16
../build/RelWithDebInfo/woolang.exe test.wo
```

全部通过时输出 `failed cases: 0` 并以 0 退出；有失败时打印失败用例名并 panic。

## 覆盖范围与文件

| 文件 | 覆盖内容 |
| --- | --- |
| `smoke_v116_tool.wo` | 测试工具（1.16 语法的 `type` / `pub` / `local`） |
| `smoke_v116_basic.wo` | 基础语言冒烟：`type Name: ...`、`alias Name: ...`、struct/enum/union、模板、`match`、`as`、`for`、函数内类型声明、enum 分组自动递增 |
| `smoke_v116_local_a.wo` / `_b.wo` | local 符号局部化（3243a3b1）：不同文件可在同一命名空间/根作用域声明同名 `local`/无修饰符符号，并各自解析 |
| `smoke_v116_internal_a.wo` / `_b.wo` | `internal` 可见性：同命名空间跨文件、子命名空间可访问；`internal` 名字 + `pub` 构造可跨文件构造 |
| `smoke_v116_construct_a.wo` / `_b.wo` | 构造修饰符分离（1ee99df8 / a89146a6 / 3f0ccdf9）：`A type Name: B Base`、`A enum B Name`、`A union B Name`；默认仅声明文件可构造、`internal` 仅类型自身命名空间可构造、`pub` 全局可构造；enum/union item 级修饰符；泛型类型 + 命名空间块 |
| `smoke_v116_negative.wo` | 负向/回归测试（dumpling 内存编译）：旧语法 `using X = ...`、`type X = ...` / `alias X = ...`（旧 `=` 连接符）、`public` 等不再接受；同文件 local 重定义、local 与非 local 冲突；跨文件 local/pub 冲突与两文件同名 local 共存；构造权限矩阵；枚举/联合 item 权限；REPL 失败求值回滚（ec2b3cdc） |
| `smoke_v116_macro.wo` | 宏子系统冒烟（`#macro` 展开 + lexer token 类型与 `wo::lex_type` 的数值对齐） |

当前共 9 个用例、164 个断言。

## 历史问题（已修复）

测试编写过程中发现 `woo/macro.wo` 未随 dev_1.16 语法迁移（`public`/`using`
残留、`token_type` 与 `lex_type` 顺序不一致），导致所有 `#macro` 无法展开。
该文件已于 2026-11-21 修复，`smoke_v116_macro.wo` 现在校验修复后的
数值契约：`l_import=75`、`l_internal=76`、`l_is=77`、`l_local=80`、
`l_pub=86`、`l_type=92`、`l_unknown_token=103`、`lex_type_COUNT=104`。

