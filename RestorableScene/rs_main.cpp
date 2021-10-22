#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <cstdint>

#include "rs_assert.h"

namespace rs
{
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
    } // namespace sfinae;

    struct gcbase
    {
        enum gctype
        {
            no_gc,
            eden,
            young,
            old,
        };
        gctype gc_type = gctype::no_gc;

        virtual ~gcbase() = default;
    };

    template<typename T>
    struct gcunit : public gcbase, public T
    {
        template<typename ... ArgTs>
        gcunit(ArgTs && ... args) : T(args...)
        {

        }

        template<typename ... ArgTs>
        static gcunit<T>* gc_new(ArgTs && ... args)
        {
            return new gcunit<T>(args...);
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
    //////////// BASE MEMORY /////////////
    //         CONST VALUE POOL


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
            mov = 2 RS_OPCODE_SPACE,    // mov(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            set = 3 RS_OPCODE_SPACE,    // set(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte

            addi = 4 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subi = 5 RS_OPCODE_SPACE,    // sub
            muli = 6 RS_OPCODE_SPACE,    // mul
            divi = 7 RS_OPCODE_SPACE,    // div
            modi = 8 RS_OPCODE_SPACE,    // mod

            addr = 9 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subr = 10 RS_OPCODE_SPACE,    // sub
            mulr = 11 RS_OPCODE_SPACE,    // mul
            divr = 12 RS_OPCODE_SPACE,    // div
            modr = 13 RS_OPCODE_SPACE,    // mod

            addh = 14 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subh = 15 RS_OPCODE_SPACE,    // sub

            adds = 16 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte

            psh = 17 RS_OPCODE_SPACE,    // psh(dr_0)            REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
            pop = 18 RS_OPCODE_SPACE,   // pop(dr_STORED?)   REGID(1BYTE)/DIFF(4BYTE)/COUNT(2BYTE)       3-5 byte
            pshr = 19 RS_OPCODE_SPACE,  // pshr(dr_0)           REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
            popr = 20 RS_OPCODE_SPACE,  // popr(dr_0)           REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte

            lds = 21 RS_OPCODE_SPACE,   // lds(dr_0)            REGID(1BYTE)/DIFF(4BYTE) DIFF(4BYTE SIGN)   4-7 byte
            ldsr = 22 RS_OPCODE_SPACE,  // ldsr(dr_0)           REGID(1BYTE)/DIFF(4BYTE) DIFF(4BYTE SIGN)   4-7 byte
            ldg = 23 RS_OPCODE_SPACE,   // ldg(dr_0)            REGID(1BYTE)/DIFF(4BYTE) DIFF(4BYTE SIGN)   4-7 byte
            ldgr = 24 RS_OPCODE_SPACE,  // ldgr(dr_0)           REGID(1BYTE)/DIFF(4BYTE) DIFF(4BYTE SIGN)   4-7 byte

            //  Logic operator, the result will store to logic_state
            equ = 25 RS_OPCODE_SPACE,   // equ(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            nequ = 26 RS_OPCODE_SPACE,  // nequ
            lt = 27 RS_OPCODE_SPACE,    // lt
            gt = 28 RS_OPCODE_SPACE,    // gt
            elt = 29 RS_OPCODE_SPACE,   // elt
            egt = 30 RS_OPCODE_SPACE,   // egt
            land = 31 RS_OPCODE_SPACE,  // land             
            lor = 32 RS_OPCODE_SPACE,   // lor

            call = 33 RS_OPCODE_SPACE,  // call(ISNATIVE?)  REGID(1BYTE)/DIFF(4BYTE)
            ret = 34 RS_OPCODE_SPACE,   // ret
            jt = 35 RS_OPCODE_SPACE,    // jt               DIFF(4BYTE)
            jf = 36 RS_OPCODE_SPACE,    // jf               DIFF(4BYTE)
            jmp = 37 RS_OPCODE_SPACE,   // jmp              DIFF(4BYTE)

            movr2i = 38 RS_OPCODE_SPACE,
            movi2r = 39 RS_OPCODE_SPACE,
            setr2i = 40 RS_OPCODE_SPACE,
            seti2r = 41 RS_OPCODE_SPACE,

            abrt = 51 RS_OPCODE_SPACE,  // abrt()  (0xcc 0xcd can use it to abort)                                                       1 byte
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

        inline value* set_val(value* _val)
        {
            type = _val->type;
            handle = _val->handle;

            return this;
        }

        inline bool is_nil()const
        {
            return handle;
        }
    };

    class ir_compiler;

    namespace opnum
    {
        struct opnumbase
        {
            virtual ~opnumbase() = default;
            virtual void generate_opnum_to_buffer(std::vector<byte_t>& buffer) const
            {
                rs_error("This type can not generate opnum.");
            }
        };

        struct stack : opnumbase
        {
            int16_t offset;

            stack(int16_t _offset) noexcept
                :offset(_offset)
            {

            }
        };

        struct global : opnumbase
        {
            int32_t offset;

            global(int32_t _offset) noexcept
                :offset(_offset)
            {

            }
        };

        struct reg :opnumbase
        {
            // REGID
            // 0B 1 1 000000
            //  isreg? stackbp? offset

            uint8_t id;

            enum spreg : uint8_t
            {
                // normal regist
                t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15,
                r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15,

                // special regist
                op_trace_result = 0b00100000, cr = op_trace_result,
                last_special_register = 0b00111111,

                // dynamic temp regist (in stack)
                s0 = 0b10000000, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15,
                s16, s17, s18, s19, s20, s21, s22, s23, s24, s25, s26, s27, s28, s29, s30, s31,
                s32, s33, s34, s35, s36, s37, s38, s39, s40, s41, s42, s43, s44, s45, s46, s47,
                s48, s49, s50, s51, s52, s53, s54, s55, s56, s57, s58, s59, s60, s61, s62, s63,
            };
            static_assert(spreg::s63 == 0b10111111);


            reg(uint8_t _id) noexcept
                :id(0b10000000 | _id)
            {
                rs_assert((_id & 0b10000000) == 0, "'_id' should less then 127.");
            }

            void generate_opnum_to_buffer(std::vector<byte_t>& buffer) const override
            {
                buffer.push_back(id);
            }
        };

        struct immbase : opnumbase
        {
            int32_t constant_index = 0;

            virtual value::valuetype type()const = 0;
            virtual bool operator < (const immbase&) const = 0;
            virtual void apply(value*) const = 0;

            void generate_opnum_to_buffer(std::vector<byte_t>& buffer) const override
            {
                byte_t* buf = (byte_t*)&constant_index;

                buffer.push_back(buf[0]);
                buffer.push_back(buf[1]);
                buffer.push_back(buf[2]);
                buffer.push_back(buf[3]);
            }
        };

        template<typename T>
        struct imm : immbase
        {
            T val;

            imm(T _val) noexcept
                :val(_val)
            {
                rs_assert(type() != value::valuetype::invalid, "Invalid immediate.");
            }

            value::valuetype type()const noexcept override
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

                // If changed this function, don't forget to modify 'apply'.
            }

            bool operator < (const immbase& _another) const override
            {
                auto t = type();
                auto t2 = _another.type();
                if (t == t2)
                {
                    return val < ((const imm&)_another).val;
                }
                return t < t2;
            }

            void apply(value* v) const override
            {
                v->type = type();
                if constexpr (std::is_same<sfinae::origin_type<T>, char*>::value)
                    v->string = val;
                else if constexpr (std::is_pointer<T>::value)
                    v->handle = val;
                else if constexpr (std::is_integral<T>::value)
                    v->integer = val;
                else if constexpr (std::is_floating_point<T>::value)
                    v->real = val;
                else
                    rs_error("Invalid immediate.");
            }
        };

        struct tag : opnumbase
        {
            string_t name;

            tag(const string_t& _name)
                : name(_name)
            {

            }
        };
    } // namespace opnum;

    class ir_compiler
    {
        // IR COMPILER:
        /*
        *
        */

        struct ir_command
        {
            instruct::opcode opcode;
            opnum::opnumbase* op1;
            opnum::opnumbase* op2;

#define RS_IS_REG(OPNUM)(dynamic_cast<opnum::reg*>(OPNUM))
            uint8_t dr()
            {
                return (RS_IS_REG(op1) ? 0b00000010 : 0) | (RS_IS_REG(op2) ? 0b00000001 : 0);
            }
#undef RS_IS_REG
        };

        std::vector<ir_command> ir_command_buffer;
        std::map<size_t, std::vector<string_t>> tag_irbuffer_offset;

        struct immless
        {
            bool operator()(const opnum::immbase* lhs, const opnum::immbase* rhs) const
            {
                return (*lhs) < (*rhs);
            }
        };
        std::set<opnum::immbase*, immless>constant_record_list;

        opnum::opnumbase* _check_and_add_const(opnum::opnumbase* _opnum)
        {
            if (auto* _immbase = dynamic_cast<opnum::immbase*>(_opnum))
            {
                if (auto fnd = constant_record_list.find(_immbase); fnd == constant_record_list.end())
                {
                    _immbase->constant_index = (int32_t)constant_record_list.size();
                    constant_record_list.insert(_immbase);
                }
                else
                {
                    // already have record.
                    _immbase->constant_index = (*fnd)->constant_index;
                }
            }

            return _opnum;
        }

    public:

#define RS_PUT_IR_TO_BUFFER(OPCODE, ...) ir_command_buffer.push_back(ir_command{OPCODE, __VA_ARGS__});
#define RS_OPNUM(OPNUM) (_check_and_add_const(new sfinae::origin_type<decltype(OPNUM)>(OPNUM)))

        template<typename OP1T, typename OP2T>
        void mov(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::mov, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void set(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::set, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void addi(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::addi, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void lt(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::lt, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void gt(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::gt, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        void jt(const opnum::tag& op1)
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::jt, RS_OPNUM(op1));
        }

        void jf(const opnum::tag& op1)
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::jf, RS_OPNUM(op1));
        }

        void jmp(const opnum::tag& op1)
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::jmp, RS_OPNUM(op1));
        }

        void abrt()
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::abrt);
        }

        void tag(const string_t& tagname)
        {
            tag_irbuffer_offset[ir_command_buffer.size()].push_back(tagname);
        }



#undef RS_OPNUM
#undef RS_PUT_IR_TO_BUFFER

        struct runtime_env
        {
            value* constant_global_reg_rtstack;
            value* reg_begin;
            value* stack_begin;

            size_t constant_value_count;
            size_t global_value_count;
            size_t real_register_count;
            size_t runtime_stack_count;

            byte_t* rt_codes;
        };

        runtime_env finalize()
        {
            // 1. Generate constant & global & register & runtime_stack memory buffer
            size_t constant_value_count = constant_record_list.size();
            size_t global_value_count = 0;
            size_t real_register_count = 64;     // t0-t15 r0-r15 (32) special reg (32)
            size_t runtime_stack_count = 65535;  // by default

            size_t preserve_memory_size =
                constant_value_count
                + global_value_count
                + real_register_count
                + runtime_stack_count;

            const auto v = sizeof(value);

            value* preserved_memory = (value*)malloc(preserve_memory_size * sizeof(value));

            rs_assert(preserved_memory, "Alloc memory fail.");

            //  // Fill constant
            for (auto& constant_record : constant_record_list)
            {
                rs_assert(constant_record->constant_index < constant_value_count,
                    "Constant index out of range.");

                constant_record->apply(&preserved_memory[constant_record->constant_index]);
            }
            // 2. Generate code
            std::vector<byte_t> runtime_command_buffer;
            std::map<string_t, std::vector<size_t>> jmp_record_table;
            std::map<string_t, uint32_t> tag_offset_vector_table;

            for (size_t ip = 0; ip < ir_command_buffer.size(); ip++)
            {
                if (auto fnd = tag_irbuffer_offset.find(ip); fnd != tag_irbuffer_offset.end())
                {
                    for (auto& tag_name : fnd->second)
                    {
                        rs_assert(tag_offset_vector_table.find(tag_name) == tag_offset_vector_table.end()
                            , "The tag point to different place.");

                        tag_offset_vector_table[tag_name] = (uint32_t)runtime_command_buffer.size();
                    }
                }

#define RS_IR ir_command_buffer[ip]
#define RS_OPCODE(...) rs_macro_overload(RS_OPCODE,__VA_ARGS__)
#define RS_OPCODE_1(OPCODE) (instruct(instruct::opcode::OPCODE, RS_IR.dr()).opcode_dr)
#define RS_OPCODE_2(OPCODE,DR) (instruct(instruct::opcode::OPCODE, 0b000000##DR).opcode_dr)

                switch (RS_IR.opcode)
                {
                case instruct::opcode::set:
                    runtime_command_buffer.push_back(RS_OPCODE(set));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::addi:
                    runtime_command_buffer.push_back(RS_OPCODE(addi));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;

                case instruct::opcode::gt:
                    runtime_command_buffer.push_back(RS_OPCODE(gt));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;

                case instruct::opcode::jt:
                    runtime_command_buffer.push_back(RS_OPCODE(jt));

                    rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op1)->name]
                        .push_back(runtime_command_buffer.size());

                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    break;

                case instruct::opcode::jmp:
                    runtime_command_buffer.push_back(RS_OPCODE(jmp, 00));

                    rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op1)->name]
                        .push_back(runtime_command_buffer.size());

                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    break;

                case instruct::opcode::abrt:
                    runtime_command_buffer.push_back(RS_OPCODE(abrt, 00));
                    break;
                }
            }
#undef RS_OPCODE_2
#undef RS_OPCODE_1
#undef RS_OPCODE
#undef RS_IR
            // 3. Generate tag

            for (auto& [tag, offsets] : jmp_record_table)
            {
                uint32_t offset_val = tag_offset_vector_table[tag];

                for (auto offset : offsets)
                {
                    *(uint32_t*)(runtime_command_buffer.data() + offset) = offset_val;
                }
            }

            runtime_env env;
            env.constant_global_reg_rtstack = preserved_memory;
            env.reg_begin = preserved_memory + constant_value_count + global_value_count;
            env.stack_begin = env.reg_begin + real_register_count;

            env.constant_value_count = constant_value_count;
            env.global_value_count = global_value_count;
            env.real_register_count = real_register_count;
            env.runtime_stack_count = runtime_stack_count;

            env.rt_codes = (byte_t*)malloc(runtime_command_buffer.size() * sizeof(byte_t));
            rs_assert(env.rt_codes, "Alloc memory fail.");

            memcpy(env.rt_codes, runtime_command_buffer.data(), runtime_command_buffer.size() * sizeof(byte_t));

            return env;
        }
    };


    class vm
    {
        ir_compiler::runtime_env rt_env;
        byte_t* ip;

        value* cr;

    public:
        void set_runtime(ir_compiler::runtime_env _rt_env)
        {
            rt_env = _rt_env;
            ip = rt_env.rt_codes;

            cr = rt_env.reg_begin + opnum::reg::spreg::cr;
        }

        void run()
        {
            byte_t* rtip = ip;
            value* bp, * sp;
            bp = sp = rt_env.stack_begin;
            value* const_global_begin = rt_env.constant_global_reg_rtstack;
            value* reg_begin = rt_env.reg_begin;

            // addressing macro
#define RS_IPVAL (*(rtip))
#define RS_IPVAL_MOVE_1 (*(rtip++))
#define RS_IPVAL_MOVE_4 (*(uint32_t*)((rtip+=4)-4))
#define RS_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)

#define RS_ADDRESSING_N1\
                    value * opnum1 = (dr >> 1) ?\
                        (\
                            (RS_IPVAL & (1 << 7)) ?\
                            (bp + RS_SIGNED_SHIFT(RS_IPVAL_MOVE_1))\
                            :\
                            (RS_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            RS_IPVAL_MOVE_4 + const_global_begin\
                        )

#define RS_ADDRESSING_N2\
                    value * opnum2 = (dr & 0b01) ?\
                        (\
                            (RS_IPVAL & (1 << 7)) ?\
                            (bp + RS_SIGNED_SHIFT(RS_IPVAL_MOVE_1))\
                            :\
                            (RS_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            RS_IPVAL_MOVE_4 + const_global_begin\
                        )

            // used for restoring IP
            struct ip_restore_raii
            {
                byte_t*& oip;
                byte_t*& rtip;
                ip_restore_raii(byte_t*& _oip, byte_t*& _rtip)
                    : oip(_oip)
                    , rtip(_rtip)
                {
                }
                ~ip_restore_raii()
                {
                    oip = rtip;
                }
            }
            _o1(ip, rtip);

            for (;;)
            {
                byte_t opcode_dr = *(rtip++);
                unsigned opcode = opcode_dr & 0b11111100u;
                unsigned dr = opcode_dr & 0b00000011u;

                switch (opcode)
                {
                case instruct::opcode::abrt:
                    rs_error("executed 'abrt'.");
                    break;

                case instruct::opcode::set:
                    {
                        RS_ADDRESSING_N1;
                        RS_ADDRESSING_N2;

                        cr->set_ref(opnum1->set_val(opnum2));
                    }
                    break;

                case instruct::opcode::jmp:
                    {

                    }
                    break;

                }
            }

        }
    };

} //namespace rs;

#include<ctime>
int main()
{
    using namespace rs;
    using namespace rs::opnum;

    ir_compiler c;
    c.set(reg(reg::cr), opnum::imm(233333));         //      set t0,  0
    c.abrt();

    //c.set(reg(reg::cr), opnum::imm(0));         //      set t0,  0
    //c.tag("loop_begin");                        //  :loop_begin
    //c.gt(reg(reg::t0), opnum::imm(100000000));  //      gt t0,   100000000
    //c.jt(tag("loop_end"));                      //      jt loop_end
    //c.addi(reg(reg::t0), opnum::imm(1));        //      add t0,  1
    //c.jmp(tag("loop_begin"));                   //      jmp loop_begin
    //c.tag("loop_end");                          //  :loop_end
    //c.abrt();                                   //      abrt;

    auto env = c.finalize();

    vm vmm;
    vmm.set_runtime(env);
    vmm.run();

    0b10000000;
    const auto v = 1 << 7;
}