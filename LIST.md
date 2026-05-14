# woort.h 中调用后需要 free / woort_free 的接口汇总

约定：
- 标记 `woort_free` 的表示文档中指明使用 `woort_free()` 释放
- 标记 `free` 的表示文档中指明使用 `free()` 释放
- 标记 `free（逐元素 + 数组本身）` 的表示需要先释放每个元素再释放外层数组

## 返回值（直接返回指针）

| 接口 | 释放方式 | 备注 |
|---|---|---|
| `woort_console_readline()` | `woort_free` | 返回 malloc 的 UTF-8 字符串，NULL 表示 EOF/读错误 |
| `woort_serialize_dynbox(src, flags)` | `free` | 序列化 boxed 值为 Woolang 字面量字符串 |
| `woort_serialize_map(src, flags)` | `free` | 序列化 map 为字面量字符串 |
| `woort_serialize_vec(src, flags)` | `free` | 序列化 vec 为字面量字符串 |
| `woort_exe_path()` | `free` | 返回 malloc 的可执行文件所在目录路径 |
| `woort_work_path()` | `free` | 返回 malloc 的当前工作目录路径 |
| `woort_get_file_loc(path)` | `free` | 返回 malloc 的目录部分路径，NULL 表示入参为 NULL |
| `woort_str_to_wstr(str)` | `free` | UTF-8 → 宽字符字符串 |
| `woort_strn_to_wstr(str, size)` | `free` | UTF-8（指定长度）→ 宽字符字符串 |
| `woort_wstr_to_str(str)` | `free` | 宽字符 → UTF-8 字符串 |
| `woort_wstrn_to_str(str, size)` | `free` | 宽字符（指定长度）→ UTF-8 字符串 |
| `woort_str_to_u16str(str)` | `free` | UTF-8 → UTF-16 字符串 |
| `woort_strn_to_u16str(str, size)` | `free` | UTF-8（指定长度）→ UTF-16 字符串 |
| `woort_u16str_to_str(str)` | `free` | UTF-16 → UTF-8 字符串 |
| `woort_u16strn_to_str(str, size)` | `free` | UTF-16（指定长度）→ UTF-8 字符串 |
| `woort_str_to_u32str(str)` | `free` | UTF-8 → UTF-32 字符串 |
| `woort_strn_to_u32str(str, size)` | `free` | UTF-8（指定长度）→ UTF-32 字符串 |
| `woort_u32str_to_str(str)` | `free` | UTF-32 → UTF-8 字符串 |
| `woort_u32strn_to_str(str, size)` | `free` | UTF-32（指定长度）→ UTF-8 字符串 |

## 输出参数（通过 `out_*` 指针返回缓冲）

| 接口 | 释放方式 | 备注 |
|---|---|---|
| `woort_CodeEnv_save_binary(env, &out_buf, &out_len)` | `woort_free` | 返回分配的二进制缓冲 |
| `woort_vfs_read(path, &out_data, &out_len)` | `woort_free` | 返回 malloc 的文件内容缓冲 |
| `woort_vfs_get_all_paths(&out_paths)` | `free`（逐元素 + 数组本身） | 返回 NULL 结尾的 malloc 字符串数组，需先 free 每个字符串再 free 数组 |
| `woort_vfs_resolve_path(filepath, dirs, cnt, &out_path)` | `free` | 返回 malloc 的解析后路径 |

## 其他（不产生 malloc 缓冲，不需要释放）

- `woort_CodeEnv_restore_failed_desc()` — 返回静态字符串，不得释放
- `woort_env_locale_name()` — 返回静态字符串，注释明确注明不得释放
- 所有 `woort_set_*` / `woort_ret_*` 宏 — 操作 VM 栈，不涉及 malloc
- 所有 IR 函数（`woort_IR_*`）— 只追加 IR 指令，不产生外部可释放缓冲
- 所有 `woort_VMRuntime_*` / `woort_CodeEnv_*`（save_binary 除外）/ `woort_Dylib_*` — 使用对应的 `_destroy` / `_close` / `_drop` / `_unload` 释放，而非 free
