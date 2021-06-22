//License note: Originally from DashFaction, may have been modified: https://github.com/rafalh/dashfaction
//DashFaction is licensed under MPL2: https://github.com/rafalh/dashfaction/blob/c2e1b1cb42b637f5876268d91e6a4edb4f4b2cdc/LICENSE.txt
//RSL2 is also licensed under MPL2 to keep things simple.

#pragma once
#include "common/patching/AsmWriter.h"
#include "common/patching/CodeBuffer.h"
#include <subhook/subhook.h>
#include <cstdint>

class BaseCodeInjection
{
private:
    uintptr_t m_addr;
    subhook::Hook m_subhook;
    CodeBuffer m_code_buf;

public:
    #define X86_GP_REG_UNION(letter) \
    union                        \
    {                            \
        int32_t e##letter##x;    \
        int16_t letter##x;       \
        struct                   \
        {                        \
            int8_t letter##l;    \
            int8_t letter##h;    \
        };                       \
    }
    struct Regs
    {
        uint32_t eflags;
        // reversed PUSHA order of registers
        int32_t edi;
        int32_t esi;
        int32_t ebp;
        int32_t reserved; // unused esp
        X86_GP_REG_UNION(b);
        X86_GP_REG_UNION(d);
        X86_GP_REG_UNION(c);
        X86_GP_REG_UNION(a);
        // real esp
        int32_t esp;
        // return address
        uint32_t eip;
    };
    #undef X86_GP_REG_UNION

    BaseCodeInjection(uintptr_t addr) : m_addr(addr), m_code_buf(256) {}
    virtual ~BaseCodeInjection() {}

    void Install()
    {
        m_subhook.Install(reinterpret_cast<void*>(m_addr), m_code_buf);
        void* trampoline = m_subhook.GetTrampoline();
        if (!trampoline)
            printf("Warning: trampoline is null for 0x%X\n", m_addr);

        AsmWriter asm_writter{m_code_buf};
        EmitCode(asm_writter, trampoline);
    }

    void SetAddr(uintptr_t addr)
    {
        m_addr = addr;
    }

protected:
    virtual void EmitCode(AsmWriter& asm_writter, void* trampoline) = 0;
};

template<typename T, typename Enable = void>
class CodeInjection2;

// CodeInjection2 specialization for handlers that take Regs struct reference
template<typename T>
class CodeInjection2<T, decltype(std::declval<T>()(std::declval<BaseCodeInjection::Regs&>()))>
 : public BaseCodeInjection
{
    T m_functor;

public:
    CodeInjection2(uintptr_t addr, T handler) :
        BaseCodeInjection(addr), m_functor(handler)
    {}

private:
    static void __thiscall wrapper(CodeInjection2& self, Regs& regs)
    {
        self.m_functor(regs);
    }

protected:
    void EmitCode(AsmWriter& asm_writter, void* trampoline) override
    {
        using namespace asm_regs;
        constexpr int esp_offset = offsetof(Regs, esp);
        constexpr int eip_offset = offsetof(Regs, eip);
        asm_writter
            .push(trampoline)               // Push default EIP = trampoline
            .push(esp)                      // push ESP before PUSHA so it can be popped manually after POPA
            .pusha()                        // push general registers
            .pushf()                        // push EFLAGS
            .add(*(esp + esp_offset), 4)    // restore ESP from before pushing the return address
            .push(esp)                      // push address of Regs struct (handler param)
            .mov(ecx, this)                 // save this pointer in ECX (thiscall)
            .call(&wrapper)                 // call handler (thiscall - calee cleans the stack)
            .add(*(esp + esp_offset), -4)   // make ESP value return address aware (again)
            .mov(eax, *(esp + eip_offset))  // read EIP from Regs struct
            .mov(ecx, *(esp + esp_offset))  // read ESP from Regs struct
            .mov(*ecx, eax)                 // copy EIP to new address of the stack pointer
            .popf()                         // pop EFLAGS
            .popa()                         // pop general registers. Note: POPA discards ESP value
            .pop(esp)                       // pop ESP pushed before PUSHA
            .ret();                         // return to address read from EIP field in Regs struct
    }
};

// CodeInjection2 specialization for handlers that does not take any arguments
template<typename T>
class CodeInjection2<T, decltype(std::declval<T>()())> : public BaseCodeInjection
{
    T m_functor;

public:
    CodeInjection2(uintptr_t addr, T handler) :
        BaseCodeInjection(addr), m_functor(handler)
    {}

private:
    static void __thiscall wrapper(CodeInjection2& self)
    {
        self.m_functor();
    }

protected:
    void EmitCode(AsmWriter& asm_writter, void* trampoline) override
    {
        using namespace asm_regs;
        asm_writter
            .push(eax)         // push caller-saved general purpose registers
            .push(ecx)         // push caller-saved general purpose registers
            .push(edx)         // push caller-saved general purpose registers
            .pushf()           // push EFLAGS
            .mov(ecx, this)    // save this pointer in ECX (thiscall)
            .call(&wrapper)    // call handler (thiscall - calee cleans the stack)
            .popf()            // pop EFLAGS
            .pop(edx)          // pop caller-saved general purpose registers
            .pop(ecx)          // pop caller-saved general purpose registers
            .pop(eax)          // pop caller-saved general purpose registers
            .jmp(trampoline);  // jump to the trampoline
    }
};

template<typename T>
class CodeInjection : public CodeInjection2<T>
{
public:
    CodeInjection(uintptr_t addr, T handler) :
        CodeInjection2<T>(addr, handler)
    {}
};
