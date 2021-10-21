#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include <cstdint>

#include "rs_assert.h"

namespace sfinae
{
    template<typename T>
    using origin_type = typename std::decay<T>::type;
    using true_type = std::true_type;
    using false_type = std::false_type;

    template<typename T>
    struct does_have_method_gc_travel
    {
        template<typename U>
        static auto checkout(int) -> decltype((&U::gc_travel), std::declval<true_type>());

        template<typename U>
        static false_type checkout(...);

        static constexpr bool value = origin_type<decltype(checkout<T>(0))>::value;
    };
}

struct gcbase
{
    virtual ~gcbase() = default;
};

template<typename T>
struct gcunit : public gcbase, public T
{
    template<typename ... ArgTs>
    gcunit(ArgTs && ... args) : T(std::forward(args)...)
    {

    }

    template<typename ... ArgTs>
    static gcunit<T>* gc_new(ArgTs && ... args)
    {
        return new gcunit<T>(std::forward(args)...);
    }

    template<typename TT>
    inline gcunit& operator = (TT&& _val)
    {
        (*(T*)this) = _val;
        return *this;
    }

    template<typename RootT>
    static void gc_scan(RootT& root)
    {
        if (sfinae)
        {

        }
    }
};

struct value;

using byte_t = uint8_t;
using real_t = double;
using hash_t = uint64_t;
using string_t = gcunit<std::string>;
using mapping_t = gcunit<std::unordered_map<hash_t, value*>>;

/*
RS MEMORY STRUCTURE

//         ???????????????
//         GLOBAL      POOL
//         CONST VALUE POOL
//////////// BASE MEMORY /////////////

*/

struct instruct
{
    // IR CODE:
    /*
    *  OPCODE(DR) [OPARGS...]
    *
    *  OPCODE 6bit  The main command of instruct (0-63)
    *  DR     2bit  Used for describing OPCODE  (00 01 10 11)
    *
    *  RS will using variable length ircode.
    *
    */

    enum opcode : uint8_t
    {
#define RS_OPCODE_SPACE <<2
        nop = 1 RS_OPCODE_SPACE,    // nop()                                                        1 byte
        mov = 2 RS_OPCODE_SPACE,    // mov()            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
        set = 3 RS_OPCODE_SPACE,    // set()            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte

        add = 4 RS_OPCODE_SPACE,    // add(TYPE)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
        sub = 5 RS_OPCODE_SPACE,    // sub
        mul = 6 RS_OPCODE_SPACE,    // mul
        div = 7 RS_OPCODE_SPACE,    // div
        mod = 8 RS_OPCODE_SPACE,    // mod

        psh = 9 RS_OPCODE_SPACE,    // psh()            REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
        pop = 10 RS_OPCODE_SPACE,    // pop(0_STORED?)   REGID(1BYTE)/DIFF(4BYTE)/COUNT(2BYTE)       3-5 byte
        pshr = 11 RS_OPCODE_SPACE,  // pshr()           REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
        popr = 12 RS_OPCODE_SPACE,  // popr()           REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte

        lds = 13 RS_OPCODE_SPACE,   // lds()            REGID(1BYTE)/DIFF(4BYTE) DIFF(2BYTE)        4-7 byte
        ldsr = 14 RS_OPCODE_SPACE,  // ldsr()           REGID(1BYTE)/DIFF(4BYTE) DIFF(2BYTE)        4-7 byte

        //  Logic operator, the result will store to logic_state
        equ = 15 RS_OPCODE_SPACE,   // equ()            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
        nequ = 16 RS_OPCODE_SPACE,  // nequ
        lt = 17 RS_OPCODE_SPACE,    // lt
        gt = 18 RS_OPCODE_SPACE,    // gt
        elt = 19 RS_OPCODE_SPACE,   // elt
        egt = 20 RS_OPCODE_SPACE,   // egt
        land = 21 RS_OPCODE_SPACE,  // land             
        lor = 22 RS_OPCODE_SPACE,   // lor
        lstate = 23 RS_OPCODE_SPACE,// lstate           REGID(1BYTE)/DIFF(4BYTE)    

        call = 24 RS_OPCODE_SPACE,  // call(ISNATIVE?)  REGID(1BYTE)/DIFF(4BYTE)
        ret = 25 RS_OPCODE_SPACE,   // ret
        jt = 26 RS_OPCODE_SPACE,    // jt               DIFF(4BYTE)
        jf = 27 RS_OPCODE_SPACE,    // jf               DIFF(4BYTE)
        jmp = 28 RS_OPCODE_SPACE,   // jmp              DIFF(4BYTE)

        movr2i = 29 RS_OPCODE_SPACE,
        movi2r = 30 RS_OPCODE_SPACE,

        abrt = 63 RS_OPCODE_SPACE,  // abrt()                                                       1 byte
#undef RS_OPCODE_SPACE
    };

    opcode opcode_dr; rs_static_assert_size(opcode, 1);

    inline constexpr instruct(opcode _opcode, uint8_t _dr)
        : opcode_dr(opcode(_opcode | _dr))
    {
        rs_assert((_opcode & 0b00000011) == 0, "illegal value for '_opcode': it's low 2-bit should be 0.");
        rs_assert((_dr & 0b11111100) == 0, "illegal value for '_dr': it should be less then 0x04.");
    }
};
rs_static_assert_size(instruct, 1);


struct vmbase
{
    // VIRTUAL MACHINE BASE:
    /*
    *
    */

    static void gc_travel() {}
};

struct value
{
    //  value
    /*
    *
    */

    union
    {
        real_t      real;
        int64_t     integer;
        uint64_t    handle;
        string_t* string;     // ADD-ABLE TYPE
        mapping_t* mapping;

        value* ref;
    };

    enum class valuetype : uint8_t
    {
        real_type,
        integer_type,
        handle_type,
        string_type,

        is_ref,

        invalid = 0xff,
    };
    valuetype type;

    inline value* get()
    {
        if (type == valuetype::is_ref)
        {
            rs_assert(ref && ref->type != valuetype::is_ref,
                "illegal reflect, 'ref' only able to store ONE layer of reflect, and should not be nullptr.");
            return ref;
        }
        return this;
    }

    inline value* set_ref(value* _ref)
    {
        if (_ref != this)
        {
            rs_assert(_ref && _ref->type != valuetype::is_ref,
                "illegal reflect, 'ref' only able to store ONE layer of reflect, and should not be nullptr.");

            type = valuetype::is_ref;
            ref = _ref;
        }
        return this;
    }

    inline bool is_nil()const
    {
        return handle;
    }
};

struct reg
{
    uint8_t id;

    reg(uint8_t _id)
        :id(_id)
    {

    }
};

template<typename T>
struct imm
{
    T val;

    imm(T _val)
        :val(_val)
    {
        rs_assert(type() != value::valuetype::invalid, "invalid immediately value.");
    }

    constexpr value::valuetype type()
    {
        if constexpr (std::is_same<sfinae::origin_type<T>, char*>::value)
            return value::valuetype::string_type;
        else if constexpr (std::is_pointer<T>::value)
            return value::valuetype::handle_type;
        else if constexpr (std::is_integral<T>::value)
            return value::valuetype::integer_type;
        else if constexpr (std::is_floating_point<T>::value)
            return value::valuetype::real_type;
        else
            return value::valuetype::invalid;
    }
};

class ir_compiler
{
    // IR COMPILER:
    /*
    *
    */
public:

    template<typename OP1T, typename OP2T>
    void mov(OP1T op1, OP2T op2)
    {

    }

    byte_t* finalize()
    {

    }
};


int main()
{
    imm{ std::vector<int>() }.type();
}