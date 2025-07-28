#include <type_traits>

namespace wo
{
    struct runtime_env;
    struct value;

    /*
    NOTE: IJitCodeGenerator
    IJitCodeGenerator 是代码生成器的抽象，在JIT实现中，JITv2的JIT代理
    将使用满足 IJitCodeGenerator 的生成器实现来生成代码。

    在设计上：
        *（不可行）JIT 将把局部变量（当前栈帧）保存在寄存器或本机栈上（psh
    系列指令仍然会对 vm->sp 进行移动等操作）。当遇到中断请求时，此时JIT
    才把值写回栈上以便检查（不可行！假设调用栈帧 A-> B-> C，中断在C处生效
    时，A/B的写回难以实现，JIT栈回滚？调用时亦写回？）

        * 
    */
    template<class T>
    concept IJitCodeGenerator = requires(T x)
    {
        T(
            std::declval<const byte_t*>(), 
            std::declval<size_t>(), 
            std::declval<runtime_env*>());    // Constructable.

        typename T::jit_value_t;
        typename T::jit_value_val_t;
        typename T::jit_value_type_t;

        x.mov(std::declval<T::jit_value_t>(), std::declval<T::jit_value_t>());
        x.mov_const(std::declval<T::jit_value_t>(), std::declval<const wo::value*>());
        x.mov_val(std::declval<T::jit_value_t>(), std::declval<T::jit_value_t>());
    };
}