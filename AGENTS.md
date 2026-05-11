# AGENTS.md - Woolang Codebase Guide

## Project Overview

Woolang is a statically-typed scripting language with a bytecode interpreter, JIT compilation (via asmjit), and GC-based memory management. The compiler/IR layer is C++17 (`src/`); the runtime (woort) is C11 (`3rd/woort/`). Tests are `.wo` scripts executed by the woolang binary.

## Build Commands

```bash
# Initialize submodules (required before first build)
git submodule sync --recursive && git submodule update --init --recursive

# Configure (Windows / MSVC)
cmake -B build -A x64 -DWO_MAKE_OUTPUT_IN_SAME_PATH=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=ON

# Configure (Linux / macOS)
cmake -B build -DWO_MAKE_OUTPUT_IN_SAME_PATH=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=ON

# Build
cmake --build build --config RelWithDebInfo        # Windows (multi-config: outputs to build/RelWithDebInfo/)
cmake --build build -j$(nproc)                     # Linux/macOS (single-config: outputs to build/)

# Debug build (for development & ASAN)
cmake -B build -DWO_MAKE_OUTPUT_IN_SAME_PATH=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -DWO_BUILD_FOR_ASAN_TEST=ON
cmake --build build --config Debug

# Disable asmjit if it fails to build
cmake -B build -DWO_SUPPORT_ASMJIT=OFF ...
```

### Build Artifacts

With `WO_MAKE_OUTPUT_IN_SAME_PATH=ON`:
- **Single-config generators** (Ninja, Make): outputs go directly to `build/`
- **Multi-config generators** (MSVC): outputs go to `build/Release/`, `build/Debug/`, etc.
- `woolang` / `woolang.exe` — CLI driver (target name `woodriver`, output name `woolang`)
- `libwoo.so` / `libwoo.dll` / `libwoo.dylib` — shared library (target name `woolang`, output name `libwoo`)
- Debug variants have `_debug` suffix (e.g., `woolang_debug`, `libwoo_debug.so`)

### Embedded Standard Library

Files in `woo/` (e.g. `woo/std.wo`) are **embedded into the binary at build time** via codegen in `src/CMakeLists.txt` → `wo_stdlib_embedded.inc`. Any change to `woo/*.wo` requires a rebuild for the binary to pick it up.

### Generated Files

- `src/wo_commit_sha.hpp` — generated at cmake-configure time (git commit hash)
- `src/wo_lang_grammar_lr1_autogen.hpp` — auto-generated LR(1) grammar table; **do not edit manually**

## Test Commands

Tests require the `baozi` package manager to install dependencies (`test/package.json` lists required packages).

```bash
# Install test dependencies (one-time setup)
cd test && baozi install && cd ..

# Run all tests (with JIT)
./build/woolang ./test/test_all.wo --enable-ctrlc-debug 0 --enable-jit 1 --enable-halt-when-panic 1

# Run all tests (without JIT / interpreter only)
./build/woolang ./test/test_all.wo --enable-ctrlc-debug 0 --enable-jit 0 --enable-halt-when-panic 1

# Run a single test file
./build/woolang ./test/test_basic.wo --enable-ctrlc-debug 0 --enable-jit 1 --enable-halt-when-panic 1

# Compile to bytecode then run (pre-compiled .woo files)
./build/woolang ./test/test_all.wo --enable-jit 1 -o test_out.woo
./build/woolang ./test_out.woo --enable-jit 0 --enable-halt-when-panic 1

# macOS ARM64: codesigning required before running tests
codesign -s - -f --entitlements test/test.entitlements build/woolang_debug
```

### Test Framework (Woolang-side)

Tests use `test_tool/test_tool.wo`:
- `test_function(name, func)` — register a named test
- `test_equal<AT, BT>(a, b)` — assert equality
- `test_assure(bool)` — assert truthiness
- `execute_all_test()` — run all registered tests

Individual test files (e.g. `test_basic.wo`) are imported via `import test_basic` in `test_all.wo`. The test framework panics on failures, so `--enable-halt-when-panic 1` is required.

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
  include/wo.h                  # Public C API (C++17 wrapper around woort)
  src/                          # Compiler & library (C++17, .hpp/.cpp)
    wo_afx.hpp                  # Precompiled header (included first in every .cpp)
    wo_lang.hpp/cpp             # Main compiler context & symbol table
    wo_lang_ast.hpp/cpp         # AST node definitions
    wo_lang_ast_builder.hpp/cpp # AST construction helpers
    wo_compiler_lexer.*         # Lexer
    wo_compiler_parser.*        # Parser (LR(1) table-driven)
    wo_lang_grammar.*           # Grammar definition
    wo_lang_grammar_loader.*    # Grammar table loader
    wo_lang_grammar_lr1_autogen.hpp  # Auto-generated grammar table (don't edit)
    wo_lang_pass0.cpp            # Pass 0: name resolution, import resolution
    wo_lang_pass1.cpp            # Pass 1: type checking, template instantiation
    wo_lang_passir.cpp           # Pass IR: bytecode generation
    wo_lang_pass_template.cpp    # Template pass framework
    wo_ir_compiler.*             # IR / bytecode compiler
    wo_assert.hpp                # Assertion/error macros
    wo_macro.hpp                 # Preprocessor metaprogramming macros
    wo_const_string_pool.hpp     # Interned string pool
    wo_source_file_manager.*     # Virtual file system
    wo_api_impl.cpp              # Implementation of wo.h C API
    wo_api_lspv2_impl.cpp        # LSP server implementation
  driver/
    wo_driver.cpp                # CLI entry point (main())
  woo/                           # Embedded standard library (.wo files bundled at build)
    std.wo                       # Core standard library
  3rd/
    woort/                       # Runtime (C11, see 3rd/woort/AGENTS.md)
    asmjit/                      # JIT backend (git submodule)
    mingw-std-threads/           # MinGW thread support (git submodule)
  test/                          # Test scripts (.wo) + test_tool harness
    test_tool/test_tool.wo       # Test harness
    test_all.wo                  # Master test runner
  build/                         # Build output directory
```

### Compile Pass Pipeline

The compiler runs passes in this order:
1. **Pass 0** (`wo_lang_pass0.cpp`) — name resolution, import resolution, symbol registration
2. **Pass 1** (`wo_lang_pass1.cpp`) — type checking, template instantiation (most compile errors surface here)
3. **Pass IR** (`wo_lang_passir.cpp`) — bytecode generation from type-checked AST

A `compile_result` enum tracks the outcome: if grammar or pass 0 fails, later passes are skipped. Pass 1 can fail with `PROCESS_FAILED_BUT_PASS_1_OK` meaning some errors were found but enough survived to continue.

## Code Style (C++ — `src/` and `driver/`)

### Language & Standard

- C++17 only. No exceptions (`-fno-exceptions` on non-MSVC).
- `#pragma once` for all header guards.

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
- Opening braces on new lines for all constructs (`if`, `for`, `while`, `switch`, functions, classes, structs, namespaces, enums).
- Spaces around operators: `a + b`, `m_name = name`.
- No trailing commas in initializer lists or enum values.
- Chinese comments are acceptable.

### Error Handling

Use macros from `wo_assert.hpp`:
- `wo_assert(condition)` / `wo_assert(condition, reason)` — debug-only assert, no-op in release
- `wo_error(reason)` — always-fatal runtime error
- `wo_warning(reason)` — non-fatal warning
- `wo_unreachable(reason)` — unreachable code (debug: fatal; release: `__builtin_unreachable()`)

Return `std::optional` or `bool` for fallible operations. Use `enum class` result codes for multiple outcomes. No C++ exceptions.

### Pointer Nullability

Use `/* OPTIONAL */` annotation for nullable pointers (inherited from woort convention):

```cpp
/* OPTIONAL */ woort_IRCompiler* m_ircompiler;
```

### Deleted Copy/Move

Classes owning resources should delete copy and move:

```cpp
IRCompiler(const IRCompiler&) = delete;
IRCompiler(IRCompiler&&) = delete;
IRCompiler& operator = (const IRCompiler&) = delete;
IRCompiler& operator = (IRCompiler&&) = delete;
```

## Code Style (woort Runtime — `3rd/woort/`)

See `3rd/woort/AGENTS.md` for full details. Key points when working across both layers:

- woort is **C11 only**, `/* */` comments only, `//` is not used.
- `woort_` prefix for all types and functions.
- `WOORT_NODISCARD` on every non-void function.
- `/* OPTIONAL */` on all nullable parameters/returns/members.
- Error handling: return `bool` for success/failure (`true` = success), output via `Type** out_result`.
- Write barriers are mandatory when writing GC pointer fields.
- The library output name is `libwoort` (not `woort`).

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

CI runs on GitLab (`.gitlab-ci.yml`). Pipelines: build release on each platform, test with JIT on/off, coverage + ASAN on Ubuntu, valgrind memory checks on tags.
