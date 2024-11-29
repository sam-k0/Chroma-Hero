// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>

// YYTK is in this now
#include "MyPlugin.h"
#include "Assets.h"
#include "LHSprites.h"
#include "LHObjects.h"
// Plugin functionality
#include <fstream>
#include <iterator>
#include "LHCore.h"
#include "Config.h"
#define _CRT_SECURE_NO_WARNINGS

CallbackAttributes_t* CodeCallbackAttr;
bool showColorPicker = false;
YYRValue colorPickerSprite;
double hue = 0;
double val = 255;
double delta = 3;
const std::string SectionName = "ChromaHero";
const std::string KeyNameColor = "Color";
const std::string KeyNameCycle = "Rainbow";
int enableRainbow = 0;

void ColorDelta(double& num, double delta)
{
    // change hue by delta, wrap around by 0 and 255
    num += delta;
    if (num > 255)
        num = 0;
    
    if (num < 0)
        num = 255;
}

// Unload function, remove callbacks here
YYTKStatus PluginUnload()
{
    LHCore::pUnregisterModule(gPluginName);
    return YYTK_OK;
}

// Game events
// This function will get called with every game event
YYTKStatus ExecuteCodeCallback(YYTKCodeEvent* codeEvent, void*)
{
    CCode* codeObj = std::get<CCode*>(codeEvent->Arguments());
    CInstance* selfInst = std::get<0>(codeEvent->Arguments());
    CInstance* otherInst = std::get<1>(codeEvent->Arguments());


    // If we have invalid data???
    if (!codeObj)
        return YYTK_INVALIDARG;

    if (!codeObj->i_pName)
        return YYTK_INVALIDARG;

    // Do event specific stuff here.
    if (Misc::StringHasSubstr(codeObj->i_pName, "o_base_button_Mouse_4"))
    {
        YYRValue objectType = Misc::InstanceGetVariable((double)(selfInst->i_spriteindex), "object_index");
        std::string cfgFilename = Filesys::GetCurrentDir() + "\\options.ini";
       
        if ((double)objectType == (double)LHObjectEnum::o_opt_herocolor)
        {
            showColorPicker = !showColorPicker;
            if (!showColorPicker) // save, hide col picker
            {
                // Save to file
                YYRValue col = Misc::CallBuiltinA("make_colour_hsv", { hue, 255.,val });

                if (!WriteIniValue(cfgFilename, SectionName, KeyNameColor, std::to_string((int)((double)col))))
                {
                    // err reading
                    Misc::Print("Error reading color from options.ini (Err. 3)");
                }

                Misc::CallBuiltinA("variable_global_set", { "opt_hero_color", col });
            }
            else //load, show col picker
            {
                //Check if it exists
                Misc::Print("Checking if section exists.");
                if (KeySectionExists(cfgFilename, SectionName, KeyNameColor))
                {
                    std::string cfgOutVal = ReadIniValue(cfgFilename, SectionName, KeyNameColor, "");

                    if (cfgOutVal != "") // actual value
                    {
                        YYRValue convertType = std::stod(cfgOutVal);
                        hue = Misc::CallBuiltinA("colour_get_hue", { convertType });
                        val = Misc::CallBuiltinA("colour_get_value", { convertType });
                        Misc::CallBuiltinA("variable_global_set", { "opt_hero_color", convertType });
                    }
                    else
                    {
                        Misc::Print("Error reading color from options.ini (Err. 2): Read default" + cfgOutVal, CLR_RED);
                    }

                    // Load boolean as int for rainbow cycle
					int enableRainbow = ReadIntFromIni(cfgFilename, SectionName,KeyNameCycle, 0);
					Misc::Print("Rainbow cycle: " + std::to_string(enableRainbow));
                    
                }
                else // !exists
                {
                    Misc::Print("Writing default value");
                    // Write a default value
                    YYRValue col = Misc::CallBuiltinA("make_colour_hsv", { 128., 255., 50. });
                    WriteIniValue(cfgFilename, SectionName, KeyNameColor, std::to_string((int)((double)col)));
					WriteIniValue(cfgFilename, SectionName, KeyNameCycle, "0");
                    Misc::Print("Writing default value done");
                }
                
            }
        }
        
    }



    if (Misc::StringHasSubstr(codeObj->i_pName, "o_opt_herocolor_Draw"))
    {
        if (showColorPicker)
        {
            YYRValue y = Misc::CallBuiltinA("variable_instance_get", { (double)selfInst->i_spriteindex, "y" });
            YYRValue x = Misc::CallBuiltinA("variable_instance_get", { (double)selfInst->i_spriteindex, "x" });
            YYRValue c = Misc::CallBuiltinA("make_colour_hsv", { hue, 255.,val });
            Misc::CallBuiltinA("draw_sprite_ext", { colorPickerSprite, 0., x ,y , 1.0, 1.0, 0.0, c , 1.0 }); // draw
            /*Misc::CallBuiltinA("draw_sprite_ext", {
                Misc::CallBuiltinA("variable_instance_get", {(double)selfInst->i_spriteindex,"sprite_index"}),
                Misc::CallBuiltinA("variable_instance_get", {(double)selfInst->i_spriteindex,"image_index"}),
            x ,y , 1.0, 1.0, 0.0, c , 1.0}); // draw*/
        }
    }

    return YYTK_OK;
}

bool CoreFoundCallback() // This function is ran once the core is resolved
                         // In this case, we want to wait with registering code callback until the core has registered itself to ensure safe calling
{
    PluginAttributes_t* pluginAttributes = nullptr;
    if (PmGetPluginAttributes(gThisPlugin, pluginAttributes) == YYTK_OK)
    { // Register a game event callback handler
        PmCreateCallback(pluginAttributes, CodeCallbackAttr, reinterpret_cast<FNEventHandler>(ExecuteCodeCallback), EVT_CODE_EXECUTE, nullptr);
    }
    // load asset
    colorPickerSprite = Assets::AddSprite("Assets\\colorpicker.jpg", 0,0, 0,3, 4);
    return true;
}

DWORD WINAPI KeyControls(HINSTANCE hModule)
{
    while (true)
    {
        if (showColorPicker == false)
        {
            Sleep(100);
            continue;
        }

        if (GetAsyncKeyState(VK_UP))
        {
            ColorDelta(hue, delta);
        }
        if (GetAsyncKeyState(VK_DOWN))
        {
            ColorDelta(hue, -delta);
        }
        if (GetAsyncKeyState(VK_LEFT))
        {
            ColorDelta(val, -delta*2);
        }
        if (GetAsyncKeyState(VK_RIGHT))
        {
            ColorDelta(val, delta*2);
        }
        Sleep(200);
    }
}

DWORD WINAPI CycleColor(HINSTANCE hModule)
{
    YYRValue ccolor = Misc::CallBuiltinA("variable_global_get", { "opt_hero_color" });
    double chue = Misc::CallBuiltinA("colour_get_hue", { ccolor });
	while (true)
	{
		if (enableRainbow)
		{
			ColorDelta(chue, 10);
            YYRValue convertType = Misc::CallBuiltinA("make_colour_hsv", { hue, 255.,255. });
            Misc::CallBuiltinA("variable_global_set", { "opt_hero_color", convertType });
            Misc::Print("Cycle..");
		}
        Sleep(200);
	}
}

// Entry
DllExport YYTKStatus PluginEntry(
    YYTKPlugin* PluginObject // A pointer to the dedicated plugin object
)
{
    gThisPlugin = PluginObject;
    gThisPlugin->PluginUnload = PluginUnload;

    LHCore::ResolveCoreParams_t params;
    params.callback = CoreFoundCallback;
    params.plugin = gThisPlugin;

    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LHCore::ResolveCore, &params, 0, NULL); // Check if the Callback Core Module is loaded, and wait for it to load
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)KeyControls, NULL, 0, NULL);
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CycleColor, NULL, 0, NULL);

    return YYTK_OK; // Successful PluginEntry.
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DllHandle = hModule; // save our module handle
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

