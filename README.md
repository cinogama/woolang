# Woolang

![logo](image/woolang_logo.png)

[![pipeline status](https://git.cinogama.net/cinogamaproject/woolang/badges/master/pipeline.svg)](https://gitlab.cinogama.com/cinogamaproject/woolang/-/commits/master)
[![coverage report](https://git.cinogama.net/cinogamaproject/woolang/badges/master/coverage.svg)](https://gitlab.cinogama.com/cinogamaproject/woolang/-/commits/master)

---

如果需要获取Woolang的二进制文件，欢迎使用 [Chief](https://github.com/BiDuang/Chief)，这是由Biduang编写的Woolang安装器，可以一并获取包管理器Baozi。

---

Woolang（原名 RestorableScene）是第四代 scene 系列语言，拥有静态类型系统，性能也相对不错（在脚本语言中比较而言）。

Woolang (origin name is RestorableScene) is fourth generation of my 'scene' programing language. It has a static type system and good performance (compared to other scripting languages).

```rust
import woo.std;
func main()
{
    std::println("Helloworld, woo~");
}

main(); // Execute `main`
```

---

## 鸣谢（Acknowledgments）

感谢 @Biduang 为woolang提供的 [Chief](https://github.com/BiDuang/Chief)，这能让安装woolang和baozi更方便快捷。

Thanks to @Biduang for providing [Chief](https://github.com/BiDuang/Chief) for woolang, which makes installing woolang and baozi easier and faster.

感谢 [asmjit](https://asmjit.com/) 提供的jit支持，虽然woolang对jit的支持尚未全部完成，但是asmjit让我拥有了愉快的开发体验。

Thanks to [asmjit](https://asmjit.com/) for the jit support. Although Woolang's support for jit has not been fully completed, asmjit has given me a pleasant development experience.

另外感谢家里的猫和狗，尽管我没有养猫，也没有养狗。

Also thanks for my cat and dog, even though I don't own a cat, nor a dog.
