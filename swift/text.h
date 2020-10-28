#include "cimgui.h"

#if defined __cplusplus
extern "C" {
#endif
void* initTextData(void);
const char *textDataStr(void *stdString);
bool textDataChanged(void *stdString);
void deinitTextData(void* stdString);
CIMGUI_API bool igtxInputText(const char* label, void* str, ImGuiInputTextFlags flags);
CIMGUI_API bool igtxInputTextMultiline(const char* label, void* str, const ImVec2 size, ImGuiInputTextFlags flags);
#if defined __cplusplus
}
#endif
