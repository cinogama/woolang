<div align="center">
<img src="./image/woolang_logo.png" />
<h1>Woolang</h1>
<a href="https://git.cinogama.net/cinogamaproject/woolang/-/commits/master">
<img src="https://git.cinogama.net/cinogamaproject/woolang/badges/master/pipeline.svg" />
<img src="https://git.cinogama.net/cinogamaproject/woolang/badges/master/coverage.svg" />
<img src="https://git.cinogama.net/cinogamaproject/woolang/-/badges/release.svg" />
</a>
</div>
  
  
Woolang（原名 RestorableScene）是第四代 scene 系列语言，拥有静态类型系统，性能也相对不错（在脚本语言中比较而言）。

Woolang (origin name is RestorableScene) is fourth generation of my 'scene' programing language. It has a static type system and good performance (compared to other scripting languages).

```rust
import woo::std;
func main()
{
    std::println("Helloworld, woo~");
}

main(); // Execute `main`
```

---

## 构建指南

### 自动构建与安装

使用 `PowerShell` 运行:

```powershell
powershell -ExecutionPolicy ByPass -c "irm https://install.woolang.net/install.ps1 | iex"
```

### 手动构建

准备工作：
  * CMake 3.8+
  * 适合的构建工具，至少能被 cmake 愉快地识别到，使用 `Visual Studio Toolchain` 或者 `Make` 等都可以
  * 适合的编译器，Woolang 使用了部分 C++17 的特性，至少确保这些特性得到支持

拉取源代码，获取依赖的子模块。使用 `CMake` 构建配置：

```bash
git clone https://git.cinogama.net/cinogamaproject/woolang.git
cd ./woolang
git submodule update --init --recursive --force
mkdir cmakebuild && cd cmakebuild
cmake .. -DWO_MAKE_OUTPUT_IN_SAME_PATH=ON -DCMAKE_BUILD_TYPE=RELWITHDEBINFO -DBUILD_SHARED_LIBS=ON
```

考虑到 `asmjit` 可能在部分平台无法通过构建，如果遇到 `asmjit` 构建失败的情况，可以尝试使用 `-DWO_SUPPORT_ASMJIT=OFF` 选项明确禁用 `asmjit`。

接着即可构建：

```bash
cmake --build . --config RELWITHDEBINFO
```

等待构建完成后, 产物生成在源码目录的 `build/` 中。

---

## 鸣谢（Acknowledgments）

感谢 [asmjit](https://asmjit.com/) 提供的 jit 支持，asmjit 让我拥有了愉快的开发体验。

Thanks to [asmjit](https://asmjit.com/) for the jit support. Although Woolang's support for jit has not been fully completed, asmjit has given me a pleasant development experience.

感谢 [@BiDuang](https://github.com/BiDuang) 提供的工具，这能让安装 Woolang 更方便快捷。

Thanks to [@BiDuang](https://github.com/BiDuang) for providing tools for Woolang, which makes installing Woolang easier and faster.

另外感谢家里的猫和狗，尽管我没有养猫，也没有养狗。

Also thanks for my cat and dog, even though I don't own a cat, nor a dog.
