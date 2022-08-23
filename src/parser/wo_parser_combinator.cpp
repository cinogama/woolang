// woolang new-parser with combinators
// by mr_cino, 2022

#include "../wo_compiler_lexer.hpp"

#include <optional>
#include <variant>
#include <memory>

namespace wo::parser
{
    // 一个组合子应该是一个高阶函数
    /*
    optional<ast_empty> number(lexer& lex);
    optional<ast_value_literial<int>> int_number(lexer& lex);
    optional<ast_value_binary<add, int>> add_int(lexer& lex, );
    */
    using namespace std;

    template<typename T>
    struct Res // Resource
    {
        T* _ptr;
    private:
        Res(T* ptr) noexcept
            :_ptr(ptr)
        {
            wo_assert(ptr != nullptr);
        }
    public:
        ~Res() noexcept
        {
            _clear();
        }
        void _clear() noexcept
        {
            if (_ptr)
            {
                delete _ptr;
                _ptr = nullptr;
            }
        }
        Res(const Res&) = delete;
        Res(Res&& r) noexcept
        {
            _ptr = r._ptr;
            r._ptr = nullptr;
        }
        Res& operator = (const Res&) = delete;
        Res& operator =(Res&& r) noexcept
        {
            _clear();
            _ptr = r._ptr;
            r._ptr = nullptr;
        }
    };

    struct Type
    {
        using Pending = struct PendingTy { };
        using Void = struct VoidTy { };
        using Nil = struct NilTy { };
        using Anything = struct AnythingTy { };

        using Int = struct IntTy { };
        using Real = struct RealTy { };
        using Handle = struct HandleTy { };
        using String = struct StringTy { };

        struct MapTy { Res<Type> keyty; Res<Type> valty; }
        Map(const Type& kty, const Type& vty);

        struct ArrayTy { Res<Type> elemty; }
        Array(const Type& ety);

        struct StructTy { map<wstring, Res<Type>> membertys; } 
        Struct(...); // TODO

        struct TupleTy { vector<Res<Type>> elemtys; }
        Tuple(...); // TODO
        struct FunctionTy { vector<Res<Type>> elemtys; bool variadic; Res<Type> returnty; }
        Function(...); // TODO

        using BaseTy = variant<
            PendingTy,
            VoidTy, 
            NilTy, 
            AnythingTy,
            IntTy,
            RealTy, 
            HandleTy, 
            StringTy, 
            MapTy, ArrayTy, 
            StructTy, 
            TupleTy, 
            FunctionTy
        >;

        BaseTy basety;
    };
}