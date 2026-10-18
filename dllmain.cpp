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
#include <cmath>
#include "LHCore.h"
#include "Config.h"
#define _CRT_SECURE_NO_WARNINGS

#define BUTTON_PRESS_EVENT "gml_Object_o_base_button_Mouse_4"
#define MESSAGE_STEP_EVENT "gml_Object_o_menu_message_Step_0"
#define MESSAGE_CLOSE_EVENT "gml_Object_o_menu_message_Alarm_0" // runs 5 steps after its button is pressed, then destroys it
#define MESSAGE_DRAW_EVENT "gml_Object_o_menu_message_Draw_73"
#define LEFT_PRESS_EVENT_SUFFIX "_Mouse_4"
#define SLIDER_STEP_EVENT "gml_Object_o_slider_Step_0" // the slider polls the mouse here
#define SLIDER_DRAW_EVENT "gml_Object_o_slider_Draw_0"

bool showColorPicker = false;
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

// Picker dialog: the game's own message window (o_menu_message, with its frame and Continue button) holding
// three of the game's sliders (o_slider) for hue, saturation and brightness and a swatch of the picked color.
// The window grows upwards from the dialog's y to fit its text_message, so the text is padded with empty lines
// to make room for the sliders. The sliders draw in the Draw event, which runs before the window's Draw End, so
// their Draw is skipped there and performed again right after the window has been drawn.
const double RoomWidth = 640.;
const double RoomHeight = 360.;
const double DialogInnerLeft = -49.;   // inside of the window frame, relative to the dialog's x
const double DialogInnerWidth = 199.;
const double DialogInnerBottom = -9.;  // relative to the dialog's y
const double DialogCenterOffset = 50.; // the window is centered on x + 50
const double DialogMargin = 3.;
const double SliderWidth = 120.;       // o_slider's width
const double SliderHeight = 13.;       // o_slider takes clicks from its y to this far below
const double SliderOverhang = 5.;      // and this far past both of its ends
const double SliderCaptionGap = 1.;
const double SliderRowGap = 3.;
const double CaptionColor = 12632256.; // make_colour_rgb(192, 192, 192), like the options menu captions
const double AlignLeft = 0.;           // fa_left
const double AlignTop = 0.;            // fa_top
const double AlignMiddle = 1.;         // fa_middle, o_menu_message leaves it set
const double EventDraw = 8.;           // ev_draw
const int SliderCount = 3;
const std::string SliderCaptions[SliderCount] = { "Color", "Saturation", "Brightness" };
const double ColorBlack = 0.;
const double ButtonPressedFrame = 2.; // o_menu_message's image_index once its button was pressed
const std::string DialogTitle = "Hero color";
const double HeroPreviewScale = 2.;
double* sliderValues[SliderCount] = { &hue, &sat, &val };
double sliders[SliderCount] = { -1., -1., -1. };
CInstance* sliderInstances[SliderCount] = {}; // caught in their Draw, to perform it again later
bool drawingSliders = false;
double pickerDialog = -1.;
std::string dialogText;
ULONGLONG dialogCheckTick = 0;
// Layout, measured when the picker opens
double captionHeight = 0;
double sliderX = 0;
double sliderBlockTop = 0;
double swatchLeft = 0;
double swatchRight = 0;

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

std::string CfgFilename();
void LoadSettings();

double FontScale()
{
    return Misc::CallBuiltinA("variable_global_get", { "font_size" });
}

double SliderRowHeight()
{
    return captionHeight + SliderCaptionGap + SliderHeight + SliderRowGap;
}

double SliderY(int i)
{
    return sliderBlockTop + i * SliderRowHeight() + captionHeight + SliderCaptionGap;
}

double SliderBlockHeight()
{
    return SliderCount * SliderRowHeight();
}

// Height of the dialog's text the way o_menu_message measures it, with the current font
double DialogTextHeight(const std::string& text)
{
    double scale = FontScale();
    double sep = Misc::CallBuiltinA("variable_global_get", { "font_sep" });
    double width = Misc::InstanceGetVariable(pickerDialog, "width");
    return (double)Misc::CallBuiltinA("string_height_ext", { text, sep, width / scale }) * scale;
}

// Pads the dialog text with empty lines until the sliders fit under the title
void FitDialogText()
{
    double needed = DialogTextHeight(DialogTitle) + DialogMargin * 2 + SliderBlockHeight();
    for (int lines = 0; DialogTextHeight(dialogText) < needed && lines < 20; lines++)
        dialogText += "\n ";
    Misc::CallBuiltinA("variable_instance_set", { pickerDialog, "text_message", dialogText });
}

void CreateSliders()
{
    for (int i = 0; i < SliderCount; i++)
    {
        sliders[i] = Misc::CallBuiltinA("instance_create_depth", { 0., 0., 0., (double)LHObjectEnum::o_slider });
        sliderInstances[i] = nullptr;
        Misc::CallBuiltinA("variable_instance_set", { sliders[i], "width", SliderWidth });
        Misc::CallBuiltinA("variable_instance_set", { sliders[i], "steps", -1. });
        // trueval is the value, val the knob position that follows it
        Misc::CallBuiltinA("variable_instance_set", { sliders[i], "trueval", *sliderValues[i] / 255 });
        Misc::CallBuiltinA("variable_instance_set", { sliders[i], "val", *sliderValues[i] / 255 });
    }
}

// Measures the captions, with the font they are drawn with
void MeasureCaptions()
{
    captionHeight = 0;
    for (int i = 0; i < SliderCount; i++)
        captionHeight = max(captionHeight, (double)Misc::CallBuiltinA("string_height", { SliderCaptions[i] }) * FontScale());
}

// Lays out the sliders on the left of the window's bottom, the swatch goes right of them
void PlaceSliders(double dialogX, double dialogY)
{
    sliderX = dialogX + DialogInnerLeft + DialogMargin + SliderOverhang;
    sliderBlockTop = dialogY + DialogInnerBottom - DialogMargin - SliderBlockHeight();
    swatchLeft = sliderX + SliderWidth + SliderOverhang + DialogMargin * 2;
    swatchRight = dialogX + DialogInnerLeft + DialogInnerWidth - DialogMargin;

    for (int i = 0; i < SliderCount; i++)
    {
        Misc::CallBuiltinA("variable_instance_set", { sliders[i], "x", sliderX });
        Misc::CallBuiltinA("variable_instance_set", { sliders[i], "y", SliderY(i) });
    }
}

void DestroySliders()
{
    for (int i = 0; i < SliderCount; i++)
    {
        if ((bool)Misc::CallBuiltinA("instance_exists", { sliders[i] }))
            Misc::CallBuiltinA("instance_destroy", { sliders[i] });
        sliders[i] = -1.;
        sliderInstances[i] = nullptr;
    }
}

int SliderIndex(int id)
{
    for (int i = 0; i < SliderCount; i++)
        if (id == (int)sliders[i])
            return i;
    return -1;
}

// Takes the color from the sliders, they stop taking input once the window's button was pressed
void ReadSliders(bool allowInput)
{
    for (int i = 0; i < SliderCount; i++)
    {
        if (!(bool)Misc::CallBuiltinA("instance_exists", { sliders[i] }))
            continue;
        Misc::CallBuiltinA("variable_instance_set", { sliders[i], "active", allowInput });
        *sliderValues[i] = (double)Misc::InstanceGetVariable(sliders[i], "trueval") * 255;
    }
}

void DrawSwatch()
{
    YYRValue oldColor = Misc::CallBuiltinA("draw_get_colour", {});
    YYRValue oldAlpha = Misc::CallBuiltinA("draw_get_alpha", {});
    double left = swatchLeft;
    double top = SliderY(0);
    double right = swatchRight;
    double bottom = SliderY(SliderCount - 1) + SliderHeight;
    Misc::CallBuiltinA("draw_set_alpha", { 1. });

    double sprite = double(LHSpriteEnum::s_hero);
    double width = (double)Misc::CallBuiltinA("sprite_get_width", { sprite }) * HeroPreviewScale;
    double height = (double)Misc::CallBuiltinA("sprite_get_height", { sprite }) * HeroPreviewScale;
    double x = floor((left + right - width) / 2);
    double y = floor((top + bottom - height) / 2);
    Misc::CallBuiltinA("draw_sprite_ext", { sprite, 0.0, x, y, HeroPreviewScale, HeroPreviewScale, 0.0, Hsv(hue, sat, val), 1.0 });

    Misc::CallBuiltinA("draw_set_colour", { oldColor });
    Misc::CallBuiltinA("draw_set_alpha", { oldAlpha });
}

void DrawSliders()
{
    YYRValue oldFont = Misc::CallBuiltinA("draw_get_font", {});
    double scale = FontScale();
    Misc::CallBuiltinA("draw_set_font", { Misc::CallBuiltinA("variable_global_get", { "font_big" }) });
    Misc::CallBuiltinA("draw_set_halign", { AlignLeft });
    Misc::CallBuiltinA("draw_set_valign", { AlignTop });
    for (int i = 0; i < SliderCount; i++)
    {
        double captionY = SliderY(i) - SliderCaptionGap - captionHeight;
        Misc::CallBuiltinA("draw_text_transformed_colour", { sliderX, captionY, SliderCaptions[i], scale, scale, 0., CaptionColor, CaptionColor, CaptionColor, CaptionColor, 1. });
    }
    Misc::CallBuiltinA("draw_set_valign", { AlignMiddle });
    Misc::CallBuiltinA("draw_set_font", { oldFont });

    drawingSliders = true;
    for (int i = 0; i < SliderCount; i++)
    {
        if (sliderInstances[i] && (bool)Misc::CallBuiltinA("instance_exists", { sliders[i] }))
            Misc::CallBuiltin("event_perform", sliderInstances[i], sliderInstances[i], { EventDraw, 0. });
    }
    drawingSliders = false;
}

void OpenPicker()
{
    LoadSettings();
    pickerDialog = Misc::CallBuiltinA("instance_create_depth", { RoomWidth / 2 - DialogCenterOffset, RoomHeight / 2, 0., (double)LHObjectEnum::o_menu_message });
    dialogText = DialogTitle;
    CreateSliders();

    // Measure with the font the dialog draws with, it is only set while drawing
    YYRValue oldFont = Misc::CallBuiltinA("draw_get_font", {});
    Misc::CallBuiltinA("draw_set_font", { Misc::CallBuiltinA("variable_global_get", { "font_big" }) });
    MeasureCaptions();
    FitDialogText();
    double textHeight = DialogTextHeight(dialogText);
    Misc::CallBuiltinA("draw_set_font", { oldFont });

    // Center the window and the button under it vertically
    double dialogY = floor(RoomHeight / 2 + textHeight / 2);
    Misc::CallBuiltinA("variable_instance_set", { pickerDialog, "y", dialogY });
    PlaceSliders(RoomWidth / 2 - DialogCenterOffset, dialogY);
    showColorPicker = true;
}

void SavePicker()
{
    YYRValue col = Misc::CallBuiltinA("make_colour_hsv", { hue, sat, val });

    if (!WriteIniValue(CfgFilename(), SectionName, KeyNameColor, std::to_string((int)((double)col))))
    {
        // err reading
        Misc::Print("Error reading color from options.ini (Err. 3)");
    }

    Misc::CallBuiltinA("variable_global_set", { "opt_hero_color", col });
    rainbowHue = hue;
}

void ClosePicker()
{
    showColorPicker = false;
    pickerDialog = -1.;
    DestroySliders();
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
        // Write a default value
        YYRValue col = Misc::CallBuiltinA("make_colour_hsv", { 128., 255., 50. });
        WriteIniValue(cfgFilename, SectionName, KeyNameColor, std::to_string((int)((double)col)));
    }

    if (!KeySectionExists(cfgFilename, SectionName, KeyNameCycle))
        WriteIniValue(cfgFilename, SectionName, KeyNameCycle, "0");

    // Load boolean as int for rainbow cycle
    enableRainbow = ReadIntFromIni(cfgFilename, SectionName, KeyNameCycle, 0);
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

// Option buttons belong to the options tab in their razdel, o_menu.opt_menu_razdel is the selected one.
// The other buttons' razdel_check() turns their active off on other tabs, but o_opt_herocolor replaces
// it with an active_check() that only reads the hero color, so it takes clicks on every tab.
bool OnSelectedTab(double button)
{
    double menu = (double)LHObjectEnum::o_menu;
    if (!(bool)Misc::CallBuiltinA("instance_exists", { menu }))
        return false;
    if (!(bool)Misc::CallBuiltinA("variable_instance_exists", { button, "razdel" }))
        return true;
    return (double)Misc::InstanceGetVariable(button, "razdel") == (double)Misc::InstanceGetVariable(menu, "opt_menu_razdel");
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

    // The dialog is gone without its button being pressed, e.g. when the room changed
    if (showColorPicker && GetTickCount64() - dialogCheckTick > 250)
    {
        dialogCheckTick = GetTickCount64();
        if (!(bool)Misc::CallBuiltinA("instance_exists", { pickerDialog }))
            ClosePicker();
    }

    int sliderIndex = showColorPicker ? SliderIndex(selfInst->i_id) : -1;

    // The dialog is modal: nothing under it gets clicks, e.g. the buttons and the sliders of the options menu
    if (showColorPicker && selfInst->i_id != (int)pickerDialog && sliderIndex < 0)
    {
        const char* name = codeObj->i_pName;
        size_t length = strlen(name);
        size_t suffixLength = strlen(LEFT_PRESS_EVENT_SUFFIX);
        if (length >= suffixLength && strcmp(name + length - suffixLength, LEFT_PRESS_EVENT_SUFFIX) == 0)
            return YYTK_DONTCALL;
        if (strcmp(name, SLIDER_STEP_EVENT) == 0)
            return YYTK_DONTCALL;
    }

    // The picker's sliders would be drawn under the dialog, DrawSliders draws them again after it
    if (sliderIndex >= 0 && !drawingSliders && strcmp(codeObj->i_pName, SLIDER_DRAW_EVENT) == 0)
    {
        sliderInstances[sliderIndex] = selfInst;
        return YYTK_DONTCALL;
    }

    // Do event specific stuff here.
    if (strcmp(codeObj->i_pName, BUTTON_PRESS_EVENT) == 0)
    {
        double button = (double)selfInst->i_id;
        YYRValue objectType = Misc::InstanceGetVariable(button, "object_index");
        if ((double)objectType == (double)LHObjectEnum::o_opt_herocolor && OnSelectedTab(button))
            OpenPicker();
    }

    if (strcmp(codeObj->i_pName, MESSAGE_CLOSE_EVENT) == 0 && showColorPicker && selfInst->i_id == (int)pickerDialog)
    {
        SavePicker();
        ClosePicker();
    }

    return YYTK_OK;
}

// Runs after every game event, the swatch and sliders are drawn on top of the dialog
int CodePostPatch(YYTKCodeEvent* codeEvent, void*)
{
    CCode* codeObj = std::get<CCode*>(codeEvent->Arguments());
    CInstance* selfInst = std::get<0>(codeEvent->Arguments());

    if (!codeObj || !selfInst || !codeObj->i_pName)
        return YYTK_INVALIDARG;

    if (!showColorPicker || selfInst->i_id != (int)pickerDialog)
        return YYTK_OK;

    // o_menu_message only shows itself while o_menu.move == 1, which may not hold in the options menu
    if (strcmp(codeObj->i_pName, MESSAGE_STEP_EVENT) == 0 && !(bool)Misc::InstanceGetVariable(pickerDialog, "visible"))
    {
        Misc::CallBuiltinA("variable_instance_set", { pickerDialog, "visible", true });
        Misc::CallBuiltinA("variable_instance_set", { pickerDialog, "fade", Misc::InstanceGetVariable(pickerDialog, "fade_max") });
    }

    if (strcmp(codeObj->i_pName, MESSAGE_DRAW_EVENT) == 0)
    {
        FitDialogText(); // the dialog's font may be taller than font_big, e.g. for other languages
        ReadSliders((double)Misc::InstanceGetVariable(pickerDialog, "image_index") != ButtonPressedFrame);
        DrawSwatch();
        DrawSliders();
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
    return true;
}

/*
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
}*/

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
    //CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)KeyControls, NULL, 0, NULL);

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

