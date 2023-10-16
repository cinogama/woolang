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

<del>推荐使用 [Chief_Reloaded](https://github.com/BiDuang/Chief_Reloaded) 来自动化管理和安装 Woolang 编译器。  </del>

<del>注意: [Chief](https://github.com/BiDuang/Chief) 已不赞成使用, 其源码已过时且有较大性能问题, 请尽快迁移至 [Chief_Reloaded](https://github.com/BiDuang/Chief_Reloaded)。</del>

新版本 Chief 开发中，在此之前请先手动构建或下载现成产物。

### 手动构建

对于 `Windows` 平台（构建 `.exe` 文件）:

**必须** 具有以下工具:  

- Visual Studio 2019 及以上
    - MSBuild 构建工具
    - CMake 工具

1. 拉取本仓库源代码并更新依赖

```bash
git clone https://git.cinogama.net/cinogamaproject/woolang.git
cd ./woolang
git submodule update --init --recursive --force
```

2. 使用 `PowerShell` 将提交信息写入到 `wo_info.hpp`

```bash
echo $(git rev-parse HEAD) > ./woolang/wo_info.hpp
```

3. 使用 `CMake` 编译

```bash
mkdir build
cd build
del CMakeCache.txt
cmake .. -DWO_MAKE_OUTPUT_IN_SAME_PATH=ON -DCMAKE_BUILD_TYPE=RELWITHDEBINFO -DBUILD_SHARED_LIBS=ON
```

如果在尝试构建时出错，可能是由于系统平台不支持 `JIT`（目前仅支持 x86_64 和 aarch64）, 导致构建失败, 详见 [#145](https://git.cinogama.net/cinogamaproject/woolang/-/issues/145)。

若要禁用 `JIT` 支持, 只需将最后一句命令改为:

```bash
cmake .. -DWO_MAKE_OUTPUT_IN_SAME_PATH=ON -DCMAKE_BUILD_TYPE=RELWITHDEBINFO -DBUILD_SHARED_LIBS=ON -DWO_SUPPORT_ASMJIT=OFF
```

*禁用 JIT 仅导致性能损失*

1. 使用 `MSBuild` 构建 `.exe` 文件

打开 `Developer PowerShell for VS`, 一般情况下它位于 `开始菜单` -> `程序` -> `Visual Studio`

输入以下命令开始构建:

```bash
MSBuild driver/woodriver.vcxproj -p:Configuration=Release -maxCpuCount -m
```

等待构建完成后, 产物即在 `./build/Release/` 呈现。

---

对于 `Linux` 平台 (构建二进制文件):

**必须** 具有以下工具:  

- CMake 工具
- Make 工具

步骤 1-3 与 `Windows` 平台一致。

4. 使用 `Make` 构建二进制文件

在命令行输入以下命令:

```bash
make -j 4
```

等待构建完成后, 产物即在 `./build/` 呈现。

---

其它平台, 可以尝试使用 `CMake` + `Make` 工具进行编译构建, 欢迎提交相关测试样例。

---

## 鸣谢（Acknowledgments）

感谢 [asmjit](https://asmjit.com/) 提供的jit支持，虽然woolang对jit的支持尚未全部完成，但是asmjit让我拥有了愉快的开发体验。

Thanks to [asmjit](https://asmjit.com/) for the jit support. Although Woolang's support for jit has not been fully completed, asmjit has given me a pleasant development experience.

感谢 [@BiDuang](https://github.com/BiDuang) 提供的工具，这能让安装 Woolang 更方便快捷。

Thanks to [@BiDuang](https://github.com/BiDuang) for providing tools for Woolang, which makes installing Woolang easier and faster.

另外感谢家里的猫和狗，尽管我没有养猫，也没有养狗。

Also thanks for my cat and dog, even though I don't own a cat, nor a dog.
