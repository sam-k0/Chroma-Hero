#pragma once
#include <Windows.h>
#include "MyPlugin.h"

namespace LHCore {

    // Ready-check function
    typedef bool (*CoreReady)();
    CoreReady pCoreReady = nullptr;
    bool isCoreReady = false;

    // Saying hello to the core function
    typedef bool (*RegisterModule)(std::string, YYTKPlugin*);
    RegisterModule pRegisterModule = nullptr;

    // unregistering from the core function
    typedef bool (*UnregisterModule)(std::string);
    UnregisterModule pUnregisterModule = nullptr;

    // Pre patches run before the original event and can return YYTK_DONTCALL to skip it, post patches run after it
    typedef int (*PrePostPatchCallback)(YYTKCodeEvent*, void*);
    typedef void (*InstallPatch)(PrePostPatchCallback);
    InstallPatch pInstallPrePatch = nullptr;
    InstallPatch pInstallPostPatch = nullptr;

    typedef bool (*CoreFoundCallback_t)();

    typedef struct ResolveCoreParams_t {
        YYTKPlugin* plugin;
        CoreFoundCallback_t callback;
    } ResolveCoreParams_t;

    DWORD WINAPI ResolveCore(LPVOID lpParam)
    {
        ResolveCoreParams_t* params = static_cast<ResolveCoreParams_t*>(lpParam);
        YYTKPlugin* plugin = params->plugin;
        CoreFoundCallback_t callback = params->callback;
#ifdef DEBUG
        Misc::Print("Importing Core function");
#endif
        void* rawCoreReady;
        void* rawRegisterModule;
        void* rawUnregisterModule;
        void* rawInstallPrePatch;
        void* rawInstallPostPatch;
        while (true)
        {
            Sleep(10);
            // Loading ready function
            if (PmGetExported("CoreReady", rawCoreReady) == YYTK_OK)
            {
                pCoreReady = reinterpret_cast<CoreReady>(rawCoreReady);
                if (pCoreReady() == true)
                {
#ifdef DEBUG
                    Misc::Print("Core is present", CLR_GREEN);
#endif
                    isCoreReady = true;

                    // Loading register function
                    if (PmGetExported("RegisterModule", rawRegisterModule) == YYTK_OK)
                    {
                        pRegisterModule = reinterpret_cast<RegisterModule>(rawRegisterModule);
                        pRegisterModule(gPluginName, gThisPlugin);
#ifdef DEBUG
                        Misc::Print("Registered to Core", CLR_GREEN);
#endif
                        if (PmGetExported("API_InstallPrePatch", rawInstallPrePatch) == YYTK_OK)
                            pInstallPrePatch = reinterpret_cast<InstallPatch>(rawInstallPrePatch);
                        if (PmGetExported("API_InstallPostPatch", rawInstallPostPatch) == YYTK_OK)
                            pInstallPostPatch = reinterpret_cast<InstallPatch>(rawInstallPostPatch);

                        if (callback != NULL)
                        {
                            callback();
                        }
                    }

                    // Loading unregister function
                    if (PmGetExported("UnregisterModule", rawUnregisterModule) == YYTK_OK)
                    {
                        pUnregisterModule = reinterpret_cast<UnregisterModule>(rawUnregisterModule);
                    }

                    return TRUE;
                }
            }
            Misc::Print("Waiting for Core. Did you install LoopHeroCallbackCore.dll?", CLR_RED);
        }
    }

}