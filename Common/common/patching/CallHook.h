//License note: Originally from DashFaction, may have been modified: https://github.com/rafalh/dashfaction
//DashFaction is licensed under MPL2: https://github.com/rafalh/dashfaction/blob/c2e1b1cb42b637f5876268d91e6a4edb4f4b2cdc/LICENSE.txt
//RSL2 is also licensed under MPL2 to keep things simple.

#pragma once
#include "common/Typedefs.h"
#include "common/patching/Traits.h"
#include "common/patching/AsmOpcodes.h"
#include "common/patching/MemUtils.h"
#include <cstdint>

class CallHookImpl
{
protected:
    uintptr_t m_call_op_addr;
    void* m_target_fun_ptr;
    void* m_hook_fun_ptr;

    CallHookImpl(uintptr_t call_op_addr, void* hook_fun_ptr) :
        m_call_op_addr{call_op_addr}, m_hook_fun_ptr{hook_fun_ptr}
    {}

public:
    void Install()
    {
        uint8_t Opcode = *reinterpret_cast<uint8_t*>(m_call_op_addr);
        if (Opcode != asm_opcodes::call_rel_long) {
            ERR("not a call at 0x%X", m_call_op_addr);
            return;
        }

        intptr_t call_offset = *reinterpret_cast<intptr_t*>(m_call_op_addr + 1);
        int call_op_size = 1 + sizeof(uintptr_t);
        m_target_fun_ptr = reinterpret_cast<void*>(m_call_op_addr + call_op_size + call_offset);
        intptr_t new_offset = reinterpret_cast<intptr_t>(m_hook_fun_ptr) - m_call_op_addr - call_op_size;
        WriteMem<i32>(m_call_op_addr + 1, new_offset);
    }
};

template<class T>
class CallHook;

template<class R, class... A>
class CallHook<R __cdecl(A...)> : public CallHookImpl
{
private:
    typedef R __cdecl FunType(A...);

public:
    CallHook(uintptr_t call_op_addr, FunType* hook_fun_ptr) :
        CallHookImpl(call_op_addr, reinterpret_cast<void*>(hook_fun_ptr))
    {}

    R CallTarget(A... a) const
    {
        auto target_fun = reinterpret_cast<FunType*>(m_target_fun_ptr);
        return target_fun(a...);
    }
};

template<class R, class... A>
class CallHook<R __fastcall(A...)> : public CallHookImpl
{
private:
    typedef R __fastcall FunType(A...);

public:
    CallHook(uintptr_t call_op_addr, FunType* hook_fun_ptr) :
        CallHookImpl(call_op_addr, reinterpret_cast<void*>(hook_fun_ptr))
    {}

    R CallTarget(A... a) const
    {
        auto target_fun = reinterpret_cast<FunType*>(m_target_fun_ptr);
        return target_fun(a...);
    }
};

#ifdef __cpp_deduction_guides
// deduction guide for lambda functions
template<class T>
CallHook(uintptr_t addr, T)->CallHook<typename function_traits<T>::f_type>;
#endif
