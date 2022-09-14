#ifndef INTERPRETER_LOCALOPCODES_H_INCLUDED
#define INTERPRETER_LOCALOPCODES_H_INCLUDED

#include "opcodes.hpp"
#include "runtime.hpp"
#include "context.hpp"

namespace Interpreter
{
    class OpStoreLocalShort : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Integer data = runtime[0].mInteger;
                int index = runtime[1].mInteger;

                runtime.getContext().setLocalShort (index, data);

                runtime.pop();
                runtime.pop();
            }
    };

    class OpStoreLocalLong : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Integer data = runtime[0].mInteger;
                int index = runtime[1].mInteger;

                runtime.getContext().setLocalLong (index, data);

                runtime.pop();
                runtime.pop();
            }
    };

    class OpStoreLocalFloat : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Float data = runtime[0].mFloat;
                int index = runtime[1].mInteger;

                runtime.getContext().setLocalFloat (index, data);

                runtime.pop();
                runtime.pop();
            }
    };

    class OpFetchIntLiteral : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Integer intValue = runtime.getIntegerLiteral (runtime[0].mInteger);
                runtime[0].mInteger = intValue;
            }
    };

    class OpFetchFloatLiteral : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Float floatValue = runtime.getFloatLiteral (runtime[0].mInteger);
                runtime[0].mFloat = floatValue;
            }
    };

    class OpFetchLocalShort : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                int index = runtime[0].mInteger;
                int value = runtime.getContext().getLocalShort (index);
                runtime[0].mInteger = value;
            }
    };

    class OpFetchLocalLong : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                int index = runtime[0].mInteger;
                int value = runtime.getContext().getLocalLong (index);
                runtime[0].mInteger = value;
            }
    };

    class OpFetchLocalFloat : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                int index = runtime[0].mInteger;
                float value = runtime.getContext().getLocalFloat (index);
                runtime[0].mFloat = value;
            }
    };

    class OpStoreGlobalShort : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Integer data = runtime[0].mInteger;
                int index = runtime[1].mInteger;

                std::string_view name = runtime.getStringLiteral (index);

                runtime.getContext().setGlobalShort (name, data);

                runtime.pop();
                runtime.pop();
            }
    };

    class OpStoreGlobalLong : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Integer data = runtime[0].mInteger;
                int index = runtime[1].mInteger;

                std::string_view name = runtime.getStringLiteral (index);

                runtime.getContext().setGlobalLong (name, data);

                runtime.pop();
                runtime.pop();
            }
    };

    class OpStoreGlobalFloat : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Float data = runtime[0].mFloat;
                int index = runtime[1].mInteger;

                std::string_view name = runtime.getStringLiteral (index);

                runtime.getContext().setGlobalFloat (name, data);

                runtime.pop();
                runtime.pop();
            }
    };

    class OpFetchGlobalShort : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                int index = runtime[0].mInteger;
                std::string_view name = runtime.getStringLiteral (index);
                Type_Integer value = runtime.getContext().getGlobalShort (name);
                runtime[0].mInteger = value;
            }
    };

    class OpFetchGlobalLong : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                int index = runtime[0].mInteger;
                std::string_view name = runtime.getStringLiteral (index);
                Type_Integer value = runtime.getContext().getGlobalLong (name);
                runtime[0].mInteger = value;
            }
    };

    class OpFetchGlobalFloat : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                int index = runtime[0].mInteger;
                std::string_view name = runtime.getStringLiteral (index);
                Type_Float value = runtime.getContext().getGlobalFloat (name);
                runtime[0].mFloat = value;
            }
    };

    template<bool TGlobal>
    class OpStoreMemberShort : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Integer data = runtime[0].mInteger;
                Type_Integer index = runtime[1].mInteger;
                std::string_view id = runtime.getStringLiteral (index);
                index = runtime[2].mInteger;
                std::string_view variable = runtime.getStringLiteral (index);

                runtime.getContext().setMemberShort (id, variable, data, TGlobal);

                runtime.pop();
                runtime.pop();
                runtime.pop();
            }
    };

    template<bool TGlobal>
    class OpStoreMemberLong : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Integer data = runtime[0].mInteger;
                Type_Integer index = runtime[1].mInteger;
                std::string_view id = runtime.getStringLiteral (index);
                index = runtime[2].mInteger;
                std::string_view variable = runtime.getStringLiteral (index);

                runtime.getContext().setMemberLong (id, variable, data, TGlobal);

                runtime.pop();
                runtime.pop();
                runtime.pop();
            }
    };

    template<bool TGlobal>
    class OpStoreMemberFloat : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Float data = runtime[0].mFloat;
                Type_Integer index = runtime[1].mInteger;
                std::string_view id = runtime.getStringLiteral (index);
                index = runtime[2].mInteger;
                std::string_view variable = runtime.getStringLiteral (index);

                runtime.getContext().setMemberFloat (id, variable, data, TGlobal);

                runtime.pop();
                runtime.pop();
                runtime.pop();
            }
    };

    template <bool TGlobal>
    class OpStoreScriptMemberShort : public Opcode0
    {

    public:
        OpStoreScriptMemberShort() {}

        virtual void execute(Runtime& runtime)
        {
            Type_Integer data = runtime[0].mInteger;
            Type_Integer index = runtime[1].mInteger;
            std::string_view id = runtime.getStringLiteral(index);
            index = runtime[2].mInteger;
            std::string_view variable = runtime.getStringLiteral(index);

            runtime.getContext().setScriptMemberShort(std::string(id), std::string(variable), data, TGlobal);

            runtime.pop();
            runtime.pop();
            runtime.pop();
        }
    };

    template <bool TGlobal>
    class OpStoreScriptMemberLong : public Opcode0
    {
    public:
        OpStoreScriptMemberLong() {}

        virtual void execute(Runtime& runtime)
        {
            Type_Integer data = runtime[0].mInteger;
            Type_Integer index = runtime[1].mInteger;
            std::string_view id = runtime.getStringLiteral(index);
            index = runtime[2].mInteger;
            std::string_view variable = runtime.getStringLiteral(index);

            runtime.getContext().setScriptMemberLong(std::string(id), std::string(variable), data, TGlobal);

            runtime.pop();
            runtime.pop();
            runtime.pop();
        }
    };

    template <bool TGlobal>
    class OpStoreScriptMemberFloat : public Opcode0
    {

    public:
        OpStoreScriptMemberFloat()  {}

        virtual void execute(Runtime& runtime)
        {
            Type_Float data = runtime[0].mFloat;
            Type_Integer index = runtime[1].mInteger;
            std::string_view id = runtime.getStringLiteral(index);
            index = runtime[2].mInteger;
            std::string_view variable = runtime.getStringLiteral(index);

            runtime.getContext().setScriptMemberFloat(std::string(id), std::string(variable), data, TGlobal);

            runtime.pop();
            runtime.pop();
            runtime.pop();
        }
    };

    template <bool TGlobal>
    class OpStoreScriptMemberRef : public Opcode0
    {

    public:
        OpStoreScriptMemberRef()  {}

        virtual void execute(Runtime& runtime)
        {
            uint32_t data = static_cast<uint32_t>(runtime[0].mInteger);
            Type_Integer index = runtime[1].mInteger;
            std::string_view id = runtime.getStringLiteral(index);
            index = runtime[2].mInteger;
            std::string_view variable = runtime.getStringLiteral(index);

            runtime.getContext().setScriptMemberRef(std::string(id), std::string(variable), data, TGlobal);

            runtime.pop();
            runtime.pop();
            runtime.pop();
        }
    };

    template<bool TGlobal>
    class OpFetchMemberShort : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Integer index = runtime[0].mInteger;
                std::string_view id = runtime.getStringLiteral (index);
                index = runtime[1].mInteger;
                std::string_view variable = runtime.getStringLiteral (index);
                runtime.pop();

                int value = runtime.getContext().getMemberShort (id, variable, TGlobal);
                runtime[0].mInteger = value;
            }
    };

    template<bool TGlobal>
    class OpFetchMemberLong : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Integer index = runtime[0].mInteger;
                std::string_view id = runtime.getStringLiteral (index);
                index = runtime[1].mInteger;
                std::string_view variable = runtime.getStringLiteral (index);
                runtime.pop();

                int value = runtime.getContext().getMemberLong (id, variable, TGlobal);
                runtime[0].mInteger = value;
            }
    };

    template<bool TGlobal>
    class OpFetchMemberFloat : public Opcode0
    {
        public:

            void execute (Runtime& runtime) override
            {
                Type_Integer index = runtime[0].mInteger;
                std::string_view id = runtime.getStringLiteral (index);
                index = runtime[1].mInteger;
                std::string_view variable = runtime.getStringLiteral (index);
                runtime.pop();

                float value = runtime.getContext().getMemberFloat (id, variable, TGlobal);
                runtime[0].mFloat = value;
            }
    };
    template <bool TGlobal>
    class OpFetchScriptMemberShort : public Opcode0
    {
        bool mGlobal;

    public:
        OpFetchScriptMemberShort() : mGlobal(TGlobal) {}

        virtual void execute(Runtime& runtime)
        {
            Type_Integer index = runtime[0].mInteger;
            std::string id(runtime.getStringLiteral(index));
            index = runtime[1].mInteger;
            std::string variable(runtime.getStringLiteral(index));
            runtime.pop();

            int value = runtime.getContext().getScriptMemberShort(id, variable, mGlobal);
            runtime.push(value);
        }
    };

    template <bool TGlobal>
    class OpFetchScriptMemberLong : public Opcode0
    {
        bool mGlobal;

    public:
        OpFetchScriptMemberLong() : mGlobal(TGlobal) {}

        virtual void execute(Runtime& runtime)
        {
            Type_Integer index = runtime[0].mInteger;
            std::string id(runtime.getStringLiteral(index));
            index = runtime[1].mInteger;
            std::string variable(runtime.getStringLiteral(index));
            runtime.pop();

            int value = runtime.getContext().getScriptMemberLong(id, variable, mGlobal);
            runtime.push(value);
        }
    };

    template <bool TGlobal>
    class OpFetchScriptMemberFloat : public Opcode0
    {
        bool mGlobal;

    public:
        OpFetchScriptMemberFloat() : mGlobal(TGlobal) {}

        virtual void execute(Runtime& runtime)
        {
            Type_Integer index = runtime[0].mInteger;
            std::string id(runtime.getStringLiteral(index));
            index = runtime[1].mInteger;
            std::string variable(runtime.getStringLiteral(index));
            runtime.pop();

            float value = runtime.getContext().getScriptMemberFloat(id, variable, mGlobal);
            runtime.push(value);
        }
    };

    template <bool TGlobal>
    class OpFetchScriptMemberRef : public Opcode0
    {
        bool mGlobal;

    public:
        OpFetchScriptMemberRef() : mGlobal(TGlobal) {}

        virtual void execute(Runtime& runtime)
        {
            Type_Integer index = runtime[0].mInteger;
            std::string id(runtime.getStringLiteral(index));
            index = runtime[1].mInteger;
            std::string variable(runtime.getStringLiteral(index));
            runtime.pop();

            uint32_t value = runtime.getContext().getScriptMemberRef(id, variable, mGlobal);
            runtime.push(static_cast<int>(value));
        }
    };
}

#endif
