# AGENTS.md - Woolang Codebase Guide

## Project Overview

Woolang is a statically-typed scripting language with a bytecode interpreter, JIT compilation (via asmjit), and GC-based memory management. The compiler/IR layer is C++17; the runtime (woort) is C11. Tests are written as `.wo` scripts executed by the woolang binary.

## Build Commands

```bash
# Initialize submodules (required before first build)
git submodule sync --recursive && git submodule update --init --recursive

# Configure (Windows / MSVC)
cmake -B build -A x64 -DWO_MAKE_OUTPUT_IN_SAME_PATH=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=ON

# Configure (Linux / macOS)
cmake -B build -DWO_MAKE_OUTPUT_IN_SAME_PATH=ON -DCMAKE_BUILD_TYPE=RelWithDeBINFO -DBUILD_SHARED_LIBS=ON

# Build
cmake --build build --config RelWithDebInfo        # Windows
cmake --build build -j$(nproc)                     # Linux/macOS

# Debug build (for development & ASAN)
cmake -B build -DWO_MAKE_OUTPUT_IN_SAME_PATH=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -DWO_BUILD_FOR_ASAN_TEST=ON
cmake --build build --config Debug

# Disable asmjit if it fails to build
cmake -B build -DWO_SUPPORT_ASMJIT=OFF ...
```

### Build Artifacts

After build with `WO_MAKE_OUTPUT_IN_SAME_PATH=ON`, outputs go to `build/`:
- `woolang` / `woolang.exe` — the CLI driver
- `libwoo.so` / `libwoo.dll` / `libwoo.dylib` — the shared library
- Debug variants have `_debug` suffix (e.g., `woolang_debug`)

## Test Commands

Tests are Woolang `.wo` scripts. They require test dependencies to be installed first.

```bash
# Install test dependencies (requires 'baozi' package manager)
cd test && baozi install && cd ..

# Run all tests (with JIT)
./build/woolang ./test/test_all.wo --enable-ctrlc-debug 0 --enable-jit 1 --enable-halt-when-panic 1

# Run all tests (without JIT / interpreter only)
./build/woolang ./test/test_all.wo --enable-ctrlc-debug 0 --enable-jit 0 --enable-halt-when-panic 1

# Run a single test file
./build/woolang ./test/test_basic.wo --enable-ctrlc-debug 0 --enable-jit 1 --enable-halt-when-panic 1

# Run with custom memory limit (bytes)
./build/woolang ./test/test_all.wo --enable-halt-when-panic 1 --mem-chunk-size 268435456
```

### Test Framework (Woolang-side)

Tests use `test_tool/test_tool.wo` which provides:
- `test_function(name, func)` — register a named test
- `test_equal<AT, BT>(a, b)` — assert equality
- `test_assure(bool)` — assert truthiness
- `execute_all_test()` — run all registered tests

### Coverage (Linux only)

```bash
cmake -B build -DWO_BUILD_FOR_COVERAGE_TEST=ON -DWO_BUILD_FOR_ASAN_TEST=ON ...
cmake --build build
cd build/src/CMakeFiles/woolang.dir && gcov -b -l -p -c *.gcno && cd ../../../../
gcovr . -r ./src -g -k
```

## Project Structure

```
woolang/
  include/wo.h           # Public C API header
  src/                   # Compiler & library source (C++17, .hpp/.cpp)
    wo_afx.hpp           # Precompiled header
    wo_lang.hpp/cpp      # Main compiler logic
    wo_lang_ast.hpp      # AST definitions
    wo_compiler_lexer.*  # Lexer
    wo_compiler_parser.* # Parser
    wo_ir_compiler.*     # IR / bytecode compiler
    wo_assert.hpp        # Assertion macros
    wo_macro.hpp         # Preprocessor utilities
  driver/
    wo_driver.cpp        # CLI entry point
  3rd/
    woort/               # Runtime (C11, separate AGENTS.md)
    asmjit/              # JIT backend (submodule)
    mingw-std-threads/   # MinGW thread support (submodule)
  test/                  # Woolang test scripts (.wo)
    test_tool/           # Test harness library
    test_all.wo          # Master test runner
  build/                 # Build output directory
```

## Code Style (C++ — `src/` and `driver/`)

### Language & Standard

- C++17 only. No exceptions (`-fno-exceptions` on non-MSVC).
- `#pragma once` for all header guards.

### Namespaces

- All code lives in `namespace wo`.
- Nested namespaces for subsystems: `wo::ast`, `wo::platform_info`, `wo::config`.

### Naming Conventions

| Type | Pattern | Example |
|------|---------|---------|
| Files | `wo_module_name.hpp/.cpp` | `wo_ir_compiler.hpp` |
| Classes/Structs | `PascalCase` | `IRCompiler`, `LangContext` |
| Enums (scoped) | `enum class Name { VALUE }` | `enum class compile_result { PROCESS_OK }` |
| Member variables | `m_prefix_name` | `m_ircompiler`, `m_symbol_kind` |
| Functions/Methods | `snake_case` | `init_lang_processers()`, `commit()` |
| Macros/Constants | `WO_UPPER_CASE` | `WO_FORCE_INLINE`, `WO_PLATFORM_64` |
| Type aliases | `snake_case_t` | `wo_pstring_t`, `lex_type_base_t` |

### Include Order (in `.cpp` and `.hpp`)

1. Precompiled header: `#include "wo_afx.hpp"` (only in `.cpp` files)
2. Project headers: `#include "wo_lang.hpp"`
3. Sub-project headers: `#include "wo_lang_ast.hpp"`
4. Standard library: `#include <vector>`, `#include <optional>`
5. C headers: `#include <cstring>`, `#include <cstdio>`

### Formatting

- 4-space indentation.
- Opening braces on new lines for functions, classes, structs, namespaces, and enums.
- Opening braces on same line for `if`, `for`, `while`, `switch`.
- Spaces around operators: `a + b`, `m_name = name`.
- Trailing commas in initializer lists / enum values are not used.
- Chinese comments are acceptable.

### Error Handling

- Use assertion macros from `wo_assert.hpp`:
  - `wo_assert(condition)` / `wo_assert(condition, reason)` — debug-only assert
  - `wo_error(reason)` — always-fatal runtime error
  - `wo_warning(reason)` — non-fatal warning
  - `wo_unreachable(reason)` — marks unreachable code
- Return `std::optional` or `bool` for fallible operations.
- Use `enum class` result codes for multiple outcomes (e.g., `compile_result::PROCESS_OK`).
- No C++ exceptions. The project compiles with `-fno-exceptions`.

### Pointer Nullability

- Use `/* OPTIONAL */` annotation for nullable pointers (inherited from woort convention):
  ```cpp
  /* OPTIONAL */ woort_IRCompiler* m_ircompiler;
  ```

### Deleted Copy/Move

- Classes owning resources should delete copy and move:
  ```cpp
  IRCompiler(const IRCompiler&) = delete;
  IRCompiler(IRCompiler&&) = delete;
  IRCompiler& operator = (const IRCompiler&) = delete;
  IRCompiler& operator = (IRCompiler&&) = delete;
  ```

### Key Macros

- `WO_API` — marks exported/imported C-linkage functions
- `WO_FORCE_INLINE` — forces inlining in release builds
- `WO_UNREACHABLE()` — platform-specific unreachable hint
- `wo_static_assert_size(TYPE, SIZE)` — static assert on type size

## Code Style (woort Runtime — `3rd/woort/`)

See `3rd/woort/AGENTS.md` for full details. Key points:

- C11 only. Use `/* */` comments.
- `woort_` prefix for types and functions.
- `WOORT_NODISCARD` on all non-void functions.
- `/* OPTIONAL */` on all nullable parameters/returns/members.
- Error handling: return `bool` for success/failure, output via `Type** out_result`.

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | ON | Build woo as shared lib |
| `WO_MAKE_OUTPUT_IN_SAME_PATH` | OFF | Output to `build/` dir |
| `WO_SUPPORT_ASMJIT` | ON | Enable JIT via asmjit |
| `WO_BUILD_FOR_COVERAGE_TEST` | OFF | Enable coverage (gcov) |
| `WO_BUILD_FOR_ASAN_TEST` | OFF | Enable AddressSanitizer |
| `WO_DISABLE_COMPILER` | OFF | Build runtime only (smaller) |
| `WO_FORCE_GC_OBJ_THREAD_SAFETY` | OFF | Force GC thread safety |
| `WO_DISABLE_FUNCTION_FOR_WASM` | OFF | Disable features for WASM |

## Supported Platforms

Windows (MSVC x64), Linux (GCC/Clang x64/ARM64), macOS (ARM64).

## CI

CI runs on GitLab (`.gitlab-ci.yml` renamed to `___.gitlab-ci.yml`). Pipelines:
1. Build release on each platform
2. Run test suite with JIT enabled and disabled
3. Coverage + ASAN on Ubuntu
4. Valgrind memory checks on tagged releases
