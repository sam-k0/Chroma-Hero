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

#define BUTTON_PRESS_EVENT "gml_Object_o_base_button_Mouse_4"
#define HEROCOLOR_DRAW_EVENT "gml_Object_o_opt_herocolor_Draw_0"

bool showColorPicker = false;
YYRValue colorPickerSprite;
double hue = 0;
double sat = 255;
double val = 255;
double delta = 3;
const std::string SectionName = "ChromaHero";
const std::string KeyNameColor = "Color";
const std::string KeyNameCycle = "Rainbow";
int enableRainbow = 0;
bool settingsLoaded = false;
double rainbowHue = 0;
ULONGLONG rainbowTick = 0;
const ULONGLONG RainbowInterval = 20; // ms between hue steps, a full cycle takes about 5 seconds

// On-screen picker: hue, saturation and brightness sliders next to the preview, dragged with the mouse
const double RoomWidth = 640.;
const double RoomHeight = 360.;
const double PanelPadding = 4.;
const double SliderWidth = 96.;
const double SliderHeight = 7.;
const double SliderGap = 3.;
const int SliderCount = 3;
const double SwatchSize = SliderCount * SliderHeight + (SliderCount - 1) * SliderGap;
const double PanelWidth = PanelPadding * 3 + SliderWidth + SwatchSize;
const double PanelHeight = PanelPadding * 2 + SwatchSize;
const int HueSegments = 24; // the hue slider is drawn as gradients between this many hues
const double MouseLeft = 1.; // mb_left
const double ColorBlack = 0.;
const double ColorWhite = 16777215.;
double* sliderValues[SliderCount] = { &hue, &sat, &val };
double panelX = 0;
double panelY = 0;
ULONGLONG panelDrawnTick = 0; // clicks are only blocked while the panel is on screen
int draggedSlider = -1;

void ColorDelta(double& num, double delta)
{
    // change hue by delta, wrap around by 0 and 255
    num += delta;
    if (num > 255)
        num = 0;

    if (num < 0)
        num = 255;
}

double Hsv(double h, double s, double v)
{
    return Misc::CallBuiltinA("make_colour_hsv", { h, s, v });
}

bool PointInRect(double px, double py, double x1, double y1, double x2, double y2)
{
    return px >= x1 && px <= x2 && py >= y1 && py <= y2;
}

double SliderY(int i)
{
    return panelY + PanelPadding + i * (SliderHeight + SliderGap);
}

bool MouseOverPanel()
{
    if (!showColorPicker || GetTickCount64() - panelDrawnTick > 100)
        return false;
    double mouseX = Misc::CallBuiltinA("device_mouse_x", { 0. });
    double mouseY = Misc::CallBuiltinA("device_mouse_y", { 0. });
    return PointInRect(mouseX, mouseY, panelX, panelY, panelX + PanelWidth, panelY + PanelHeight);
}

// Below the preview, or above it if there is no room
void LayoutPanel(double inst)
{
    double left = Misc::InstanceGetVariable(inst, "bbox_left");
    double top = Misc::InstanceGetVariable(inst, "bbox_top");
    double bottom = Misc::InstanceGetVariable(inst, "bbox_bottom");
    panelX = min(left, RoomWidth - PanelWidth);
    panelY = bottom + 3;
    if (panelY + PanelHeight > RoomHeight)
        panelY = top - 3 - PanelHeight;
}

void UpdateSliders()
{
    double mouseX = Misc::CallBuiltinA("device_mouse_x", { 0. });
    double mouseY = Misc::CallBuiltinA("device_mouse_y", { 0. });
    double sliderX = panelX + PanelPadding;

    if ((bool)Misc::CallBuiltinA("device_mouse_check_button_pressed", { 0., MouseLeft }))
    {
        for (int i = 0; i < SliderCount; i++)
        {
            // a bit of slack so the thin bars are easy to hit
            if (PointInRect(mouseX, mouseY, sliderX - 2, SliderY(i) - SliderGap / 2, sliderX + SliderWidth + 2, SliderY(i) + SliderHeight + SliderGap / 2))
                draggedSlider = i;
        }
    }
    if (!(bool)Misc::CallBuiltinA("device_mouse_check_button", { 0., MouseLeft }))
        draggedSlider = -1;

    if (draggedSlider >= 0)
    {
        double t = max(0., min(1., (mouseX - sliderX) / SliderWidth));
        *sliderValues[draggedSlider] = t * 255;
    }
}

void DrawGradient(double x1, double y1, double x2, double y2, double leftColor, double rightColor)
{
    Misc::CallBuiltinA("draw_rectangle_colour", { x1, y1, x2, y2, leftColor, rightColor, rightColor, leftColor, false });
}

void DrawPanel()
{
    YYRValue oldColor = Misc::CallBuiltinA("draw_get_colour", {});
    YYRValue oldAlpha = Misc::CallBuiltinA("draw_get_alpha", {});
    double sliderX = panelX + PanelPadding;

    Misc::CallBuiltinA("draw_set_alpha", { 0.8 });
    Misc::CallBuiltinA("draw_set_colour", { ColorBlack });
    Misc::CallBuiltinA("draw_rectangle", { panelX, panelY, panelX + PanelWidth, panelY + PanelHeight, false });
    Misc::CallBuiltinA("draw_set_alpha", { 1. });

    for (int i = 0; i < HueSegments; i++)
    {
        double x1 = sliderX + SliderWidth * i / HueSegments;
        double x2 = sliderX + SliderWidth * (i + 1) / HueSegments;
        DrawGradient(x1, SliderY(0), x2, SliderY(0) + SliderHeight, Hsv(255. * i / HueSegments, 255., 255.), Hsv(255. * (i + 1) / HueSegments, 255., 255.));
    }
    DrawGradient(sliderX, SliderY(1), sliderX + SliderWidth, SliderY(1) + SliderHeight, Hsv(hue, 0., val), Hsv(hue, 255., val));
    DrawGradient(sliderX, SliderY(2), sliderX + SliderWidth, SliderY(2) + SliderHeight, Hsv(hue, sat, 0.), Hsv(hue, sat, 255.));

    // Markers, black around white so they show on both ends of the bars
    for (int i = 0; i < SliderCount; i++)
    {
        double markerX = sliderX + SliderWidth * *sliderValues[i] / 255;
        Misc::CallBuiltinA("draw_set_colour", { ColorBlack });
        Misc::CallBuiltinA("draw_rectangle", { markerX - 2, SliderY(i) - 2, markerX + 2, SliderY(i) + SliderHeight + 1, true });
        Misc::CallBuiltinA("draw_set_colour", { ColorWhite });
        Misc::CallBuiltinA("draw_rectangle", { markerX - 1, SliderY(i) - 1, markerX + 1, SliderY(i) + SliderHeight, true });
    }

    double swatchX = sliderX + SliderWidth + PanelPadding;
    double swatchY = panelY + PanelPadding;
    Misc::CallBuiltinA("draw_set_colour", { Hsv(hue, sat, val) });
    Misc::CallBuiltinA("draw_rectangle", { swatchX, swatchY, swatchX + SwatchSize, swatchY + SwatchSize, false });

    Misc::CallBuiltinA("draw_set_colour", { oldColor });
    Misc::CallBuiltinA("draw_set_alpha", { oldAlpha });
}

std::string CfgFilename()
{
    return Filesys::GetCurrentDir() + "\\options.ini";
}

// Reads the saved color into the picker and the rainbow setting, writes defaults if they are missing
void LoadSettings()
{
    std::string cfgFilename = CfgFilename();
    if (KeySectionExists(cfgFilename, SectionName, KeyNameColor))
    {
        std::string cfgOutVal = ReadIniValue(cfgFilename, SectionName, KeyNameColor, "");

        if (cfgOutVal != "") // actual value
        {
            YYRValue convertType = std::stod(cfgOutVal);
            hue = Misc::CallBuiltinA("colour_get_hue", { convertType });
            sat = Misc::CallBuiltinA("colour_get_saturation", { convertType });
            val = Misc::CallBuiltinA("colour_get_value", { convertType });
        }
        else
        {
            Misc::Print("Error reading color from options.ini (Err. 2): Read default" + cfgOutVal, CLR_RED);
        }
    }
    else // !exists
    {
        Misc::Print("Writing default value");
        // Write a default value
        YYRValue col = Misc::CallBuiltinA("make_colour_hsv", { 128., 255., 50. });
        WriteIniValue(cfgFilename, SectionName, KeyNameColor, std::to_string((int)((double)col)));
        Misc::Print("Writing default value done");
    }

    if (!KeySectionExists(cfgFilename, SectionName, KeyNameCycle))
        WriteIniValue(cfgFilename, SectionName, KeyNameCycle, "0");

    // Load boolean as int for rainbow cycle
    enableRainbow = ReadIntFromIni(cfgFilename, SectionName, KeyNameCycle, 0);
    Misc::Print("Rainbow cycle: " + std::to_string(enableRainbow));
}

// Cycles the hue of the hero color, keeping the picked saturation and brightness.
// Called from game events since the runner functions aren't safe to call from other threads.
void UpdateRainbow()
{
    if (!enableRainbow || showColorPicker)
        return;

    ULONGLONG now = GetTickCount64();
    if (now - rainbowTick < RainbowInterval)
        return;
    rainbowTick = now;

    ColorDelta(rainbowHue, 1);
    Misc::CallBuiltinA("variable_global_set", { "opt_hero_color", Hsv(rainbowHue, sat, val) });
}

// Unload function, remove callbacks here
YYTKStatus PluginUnload()
{
    LHCore::pUnregisterModule(gPluginName);
    return YYTK_OK;
}

// Runs before every game event, return YYTK_DONTCALL to skip the original
int CodePrePatch(YYTKCodeEvent* codeEvent, void*)
{
    CCode* codeObj = std::get<CCode*>(codeEvent->Arguments());
    CInstance* selfInst = std::get<0>(codeEvent->Arguments());
    CInstance* otherInst = std::get<1>(codeEvent->Arguments());


    // If we have invalid data???
    if (!codeObj || !selfInst)
        return YYTK_INVALIDARG;

    if (!codeObj->i_pName)
        return YYTK_INVALIDARG;

    if (!settingsLoaded)
    {
        settingsLoaded = true;
        LoadSettings();
        rainbowHue = hue;
    }
    UpdateRainbow();

    // Do event specific stuff here.
    if (strcmp(codeObj->i_pName, BUTTON_PRESS_EVENT) == 0)
    {
        YYRValue objectType = Misc::InstanceGetVariable((double)selfInst->i_id, "object_index");

        if ((double)objectType == (double)LHObjectEnum::o_opt_herocolor)
        {
            showColorPicker = !showColorPicker;
            draggedSlider = -1;
            if (!showColorPicker) // save, hide col picker
            {
                // Save to file
                YYRValue col = Misc::CallBuiltinA("make_colour_hsv", { hue, sat, val });

                if (!WriteIniValue(CfgFilename(), SectionName, KeyNameColor, std::to_string((int)((double)col))))
                {
                    // err reading
                    Misc::Print("Error reading color from options.ini (Err. 3)");
                }

                Misc::CallBuiltinA("variable_global_set", { "opt_hero_color", col });
                rainbowHue = hue;
            }
            else //load, show col picker
            {
                LoadSettings();
            }
        }
        else if (MouseOverPanel()) // don't press the buttons under the picker
        {
            return YYTK_DONTCALL;
        }

    }

    return YYTK_OK;
}

// Runs after every game event, so the picker is drawn on top of the preview
int CodePostPatch(YYTKCodeEvent* codeEvent, void*)
{
    CCode* codeObj = std::get<CCode*>(codeEvent->Arguments());
    CInstance* selfInst = std::get<0>(codeEvent->Arguments());

    if (!codeObj || !selfInst || !codeObj->i_pName)
        return YYTK_INVALIDARG;

    if (strcmp(codeObj->i_pName, HEROCOLOR_DRAW_EVENT) == 0)
    {
        if (showColorPicker)
        {
            double inst = selfInst->i_id;
            YYRValue y = Misc::CallBuiltinA("variable_instance_get", { inst, "y" });
            YYRValue x = Misc::CallBuiltinA("variable_instance_get", { inst, "x" });
            YYRValue c = Misc::CallBuiltinA("make_colour_hsv", { hue, sat, val });
            if ((double)colorPickerSprite >= 0)
                Misc::CallBuiltinA("draw_sprite_ext", { colorPickerSprite, 0., x ,y , 1.0, 1.0, 0.0, c , 1.0 }); // draw

            LayoutPanel(inst);
            UpdateSliders();
            DrawPanel();
            panelDrawnTick = GetTickCount64();
        }
    }

    return YYTK_OK;
}

bool CoreFoundCallback() // This function is ran once the core is resolved
                         // In this case, we want to wait with registering code callback until the core has registered itself to ensure safe calling
{
    if (LHCore::pInstallPrePatch && LHCore::pInstallPostPatch)
    {
        LHCore::pInstallPrePatch(CodePrePatch);
        LHCore::pInstallPostPatch(CodePostPatch);
    }
    else
    {
        Misc::Print("Your LoopHeroCallbackCore.dll is too old, please update it", CLR_RED);
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

// Entry
DllExport YYTKStatus PluginEntry(
    YYTKPlugin* PluginObject // A pointer to the dedicated plugin object
)
{
    gThisPlugin = PluginObject;
    gThisPlugin->PluginUnload = PluginUnload;

    static LHCore::ResolveCoreParams_t params; // outlives this function, ResolveCore reads it from its own thread
    params.callback = CoreFoundCallback;
    params.plugin = gThisPlugin;

    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LHCore::ResolveCore, &params, 0, NULL); // Check if the Callback Core Module is loaded, and wait for it to load
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)KeyControls, NULL, 0, NULL);

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

