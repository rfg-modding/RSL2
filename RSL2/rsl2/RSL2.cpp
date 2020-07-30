﻿#include "common/windows/WindowsWrapper.h"
#include "common/patching/FunHook.h"
#include "hooks/PlayerDoFrame.h"
#include "functions/Functions.h"
#include "hooks/RenderHooks.h"
#include "misc/GlobalState.h"
#include "common/Typedefs.h"
#include "common/Common.h"
#include "hooks/WndProc.h"
#ifdef COMPILE_IN_PROFILER
#include "tracy/Tracy.hpp"
#endif
#include <kiero/kiero.h>
#include <cstdio>

//Todo: Have common lib with RFG types instead of tossing them everywhere

//Note: When a plugin is reloaded the host calls RSL2_PluginUnload and then RSL2_PluginInit again
//      as if it were the first time the host were loading the plugin.

//Need to use extern "C" to avoid C++ export name mangling. Lets us use the exact name RSL2_XXXX with GetProcAddress in the host
extern "C"
{
    //Called when the host dll loads this plugin
    DLLEXPORT bool __cdecl RSL2_PluginInit()
    {
        printf("RSL2.dll RSL2_PluginInit() called!\n");
        RSL2_GlobalState* globalState = GetGlobalState();

        globalState->ModuleBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
        printf("ModuleBase: %d\n", globalState->ModuleBase);

        if (kiero::init(kiero::RenderType::D3D11) != kiero::Status::Success)
        {
            printf("Error! Failed to init kiero in RSL2.dll!\n");
            return false;
        }

        //Todo: Find the offset needed for these patches so this old technique can be removed
        globalState->MouseGenericPollMouseVisibleAddress = globalState->ModuleBase + 0x001B88DC;
        globalState->CenterMouseCursorCallAddress = globalState->ModuleBase + 0x878D90;

        //Todo: Maybe set ModuleBase in FuncHook and provide option to offset function address from it for convenience & less mistakes
        PlayerDoFrame_hook.SetAddr(globalState->ModuleBase + 0x6E6290);
        PlayerDoFrame_hook.Install();
        keen_graphics_beginFrame.SetAddr(globalState->ModuleBase + 0x0086A8A0);
        keen_graphics_beginFrame.Install();

        //Todo: Wait for valid game state then get window handle
        RegisterFunction(rfg::GameseqGetState, 0x003BFC70);
        RegisterFunction(rfg::GameseqSetState, 0x003D8730);

        //D3D11 hooks
        D3D11_ResizeBuffersHook.SetAddr(kiero::getMethodsTable()[13]);
        D3D11_ResizeBuffersHook.Install();
        D3D11_PresentHook.SetAddr(kiero::getMethodsTable()[8]);
        D3D11_PresentHook.Install();

        return true;
    }

    //Called when the host dll unloads this plugin
    DLLEXPORT bool __cdecl RSL2_PluginUnload()
    {
        printf("RSL2.dll RSL2_PluginUnload() called!\n");
        RSL2_GlobalState* globalState = GetGlobalState();

        //Remove hooks
        PlayerDoFrame_hook.Remove();
        keen_graphics_beginFrame.Remove();
        D3D11_ResizeBuffersHook.Remove();
        D3D11_PresentHook.Remove();

        //Relock mouse so game has full control of it
        LockMouse();

        //Remove custom WndProc
        SetWindowLongPtr(globalState->gGameWindowHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(globalState->RfgWndProc));

        return true;
    }
}