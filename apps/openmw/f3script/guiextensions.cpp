#include "guiextensions.hpp"

#include <components/focompiler/opcodes.hpp>


#include <components/interpreter/interpreter.hpp>
#include <components/interpreter/runtime.hpp>
#include <components/interpreter/opcodes.hpp>

#include <components/esm4/records.hpp>

#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/mechanicsmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "interpretercontext.hpp"
#include "usings.hpp"

namespace FOScript
{
    namespace Gui
    {
        class OpShowMessage : public Interpreter::Opcode1
        {
        public:
            void execute(Interpreter::Runtime& runtime, unsigned int arg0) override
            {
                std::string_view msgRef = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();
                
                const ESM4::Message* msg = MWBase::Environment::get().getWorld()->
                                            getStore().get<ESM4::Message>().search(msgRef);
                
                if (msg)
                {
                    MWBase::WindowManager *winMgr = MWBase::Environment::get().getWindowManager();
                    if (msg->mButtons.size() > 0)
                    {
                        // TODO: buttons
                    }
                    else
                    {
                        winMgr->interactiveMessageBox(msg->mDescription, { "OK" });
                    }
                }
            }
        };
        

        void installOpcodes (Interpreter::Interpreter& interpreter)
        {
            interpreter.installSegment3<OpShowMessage>(FOCompiler::Gui::opcodeShowMessage);
        }
    }
}
