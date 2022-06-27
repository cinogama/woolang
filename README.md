# Woolang

![logo](image/woolang.png)

|平台|master分支测试结果|master分支测试覆盖率
|------|------|------|
|ubuntu 18.04 x64|[![pipeline status](https://gitlab.cinogama.com/cinogamaproject/woolang/badges/master/pipeline.svg)](https://gitlab.cinogama.com/cinogamaproject/woolang/-/commits/master)|[![coverage report](https://gitlab.cinogama.com/cinogamaproject/woolang/badges/master/coverage.svg)](https://gitlab.cinogama.com/cinogamaproject/woolang/-/commits/master)|

Woolang (原名RestorableScene) 是 Scene 系列脚本语言的第四序列，与前几代相比拥有更完善的编译时和运行时机制，性能表现也有很大提升。

```go
import woo.std;
func main()
{
    std::println("Helloworld, woo~");
}
```

Woolang 是一门强类型/静态类型脚本语言，内置比较完善的调试器，可以随时中断程序开始调试。此外语言支持GC，拥有闭包等一系列基本工具，可以简单快乐地编写所需的逻辑。

```go
import woo.std;

func main(var foo :void(string))
{
        var what = "Helloworld";
        return func()
               {
                   foo(what) 
               };
}

var f = main(func(var msg:string)
             {
                 std::println(msg)
             });

f();    // Will display "Helloworld"
```

Woolang 内置协程调度器，并向宿主提供一系列关于协程调度的API，可以简单快速地编写出高并发的程序。

```go
import woo.std;
import woo.co;

func work(var val:int)
{
    std::println(F"I'm work: {val}");
}

for (var i=0; i<1000; i+=1)
    std::co(work, i);      // Launch a coroutine.
```

作为脚本语言，Woolang 允许用户基于基本类型定义新的“自定义类型”，配合`指向调用`语法糖和`运算符重载`功能，让Woolang能够更好地配合宿主完成工作。

```go
using gameObject = gchandle;
namespace gameObject
{
    extern("libgameengine", "destroy_gameobject")
    func destroy(var self:gameObject):void;
}

// ...
var obj = foo_return_gameObject();
obj->destroy();
// ...
using vector2
{
    var x = .0;
    var y = .0;
    func create() {return new();}
    func create(var x:real, var y:real)
    {
        var self = new();
        self.x = x; self.y = y;
        return self;
    }

    func operator + (var a:vector2, var b:vector2)
    {
        return create(a.x+b.x, a.y+b.y);
    }
};
var a = vector2(1, 2);
var b = vector2(3, 4);
var c = a + b;
```

作为强类型/静态类型语言，Woolang提供了泛型机制。编译器会做力所能及的类型推导，大多数情况下不需要手动额外填写模板参数。

```go
func foo<T>(var val:T)
{
    return val;
}

foo(1);
foo(3.1415);
foo("Hello");
foo:<array<int>>([1, 2, 3]);

```
