
#include "imgui.h"
#include "imgui_internal.h"
#include "text.h"

#include <string>

struct TextUserData {
    bool changed;
    std::string str;
};

void* initTextData(void)
{
    return (void *)(new TextUserData());
}

const char *textDataStr(void *stdString)
{
    return ((TextUserData *)stdString)->str.c_str();
}

bool textDataChanged(void *stdString)
{
    TextUserData* user_data = (TextUserData*)stdString;
    const bool changed = user_data->changed;
    user_data->changed = false;
    return changed;
}

void deinitTextData(void *stdString)
{
    delete (TextUserData *)stdString;
}

static int InputTextCallback(ImGuiInputTextCallbackData* data)
{
    TextUserData* user_data = (TextUserData*)data->UserData;
    user_data->changed = true;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        // Resize string callback
        // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
        IM_ASSERT(data->Buf == user_data->str.c_str());
        user_data->str.resize(data->BufTextLen);
        data->Buf = (char*)user_data->str.c_str();
    }
    return 0;
}

bool igtxInputText(const char* label, void* str, ImGuiInputTextFlags flags)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    return ImGui::InputText(label, (char*)((TextUserData *)str)->str.c_str(), ((TextUserData *)str)->str.capacity() + 1, flags, InputTextCallback, str);
}

bool igtxInputTextMultiline(const char* label, void* str, const ImVec2 size, ImGuiInputTextFlags flags)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    return ImGui::InputTextMultiline(label, (char*)((TextUserData *)str)->str.c_str(), ((TextUserData *)str)->str.capacity() + 1, size, flags, InputTextCallback, str);
}
