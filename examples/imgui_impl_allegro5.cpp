// dear imgui: Renderer + Platform Binding for Allegro 5
// (Info: Allegro 5 is a cross-platform general purpose library for handling windows, inputs, graphics, etc.)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'ALLEGRO_BITMAP*' as ImTextureID. Read the FAQ about ImTextureID in imgui.cpp.
//  [X] Platform: Clipboard support (from Allegro 5.1.12)
//  [X] Platform: Mouse cursor shape and visibility. Disable with 'io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange'.
// Issues:
//  [ ] Renderer: The renderer is suboptimal as we need to unindex our buffers and convert vertices manually.
//  [ ] Platform: Missing gamepad support.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you are new to dear imgui, read examples/README.txt and read the documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui, Original Allegro 5 code by @birthggd

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2019-07-21: Inputs: Added mapping for ImGuiKey_KeyPadEnter.
//  2019-05-11: Inputs: Don't filter character value from ALLEGRO_EVENT_KEY_CHAR before calling AddInputCharacter().
//  2019-04-30: Renderer: Added support for special ImDrawCallback_ResetRenderState callback to reset render state.
//  2018-11-30: Platform: Added touchscreen support.
//  2018-11-30: Misc: Setting up io.BackendPlatformName/io.BackendRendererName so they can be displayed in the About Window.
//  2018-06-13: Platform: Added clipboard support (from Allegro 5.1.12).
//  2018-06-13: Renderer: Use draw_data->DisplayPos and draw_data->DisplaySize to setup projection matrix and clipping rectangle.
//  2018-06-13: Renderer: Backup/restore transform and clipping rectangle.
//  2018-06-11: Misc: Setup io.BackendFlags ImGuiBackendFlags_HasMouseCursors flag + honor ImGuiConfigFlags_NoMouseCursorChange flag.
//  2018-04-18: Misc: Renamed file from imgui_impl_a5.cpp to imgui_impl_allegro5.cpp.
//  2018-04-18: Misc: Added support for 32-bits vertex indices to avoid conversion at runtime. Added imconfig_allegro5.h to enforce 32-bit indices when included from imgui.h.
//  2018-02-16: Misc: Obsoleted the io.RenderDrawListsFn callback and exposed ImGui_ImplAllegro5_RenderDrawData() in the .h file so you can call it yourself.
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2018-02-06: Inputs: Added mapping for ImGuiKey_Space.

#include <stdint.h>     // uint64_t
#include <cstring>      // memcpy
#include "imgui.h"
#include "imgui_impl_allegro5.h"

// Allegro
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#ifdef _WIN32
#include <allegro5/allegro_windows.h>
#endif
#define ALLEGRO_HAS_CLIPBOARD   (ALLEGRO_VERSION_INT >= ((5 << 24) | (1 << 16) | (12 << 8)))    // Clipboard only supported from Allegro 5.1.12

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning (disable: 4127) // condition expression is constant
#endif

// Data
static ALLEGRO_DISPLAY*         g_Display = NULL;
static ALLEGRO_BITMAP*          g_Texture = NULL;
static double                   g_Time = 0.0;
static ALLEGRO_MOUSE_CURSOR*    g_MouseCursorInvisible = NULL;
static ALLEGRO_VERTEX_DECL*     g_VertexDecl = NULL;
static char*                    g_ClipboardTextData = NULL;

struct ImDrawVertAllegro
{
    ImVec2 pos;
    ImVec2 uv;
    ALLEGRO_COLOR col;
};

// Forward Declarations
static void ImGui_ImplAllegro5_InitPlatformInterface(ALLEGRO_DISPLAY* display);
static void ImGui_ImplAllegro5_ShutdownPlatformInterface();

static void ImGui_ImplAllegro5_SetupRenderState(ImDrawData* draw_data)
{
    // Setup blending
    al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);

    // Setup orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
    {
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        ALLEGRO_TRANSFORM transform;
        al_identity_transform(&transform);
        al_use_transform(&transform);
        al_orthographic_transform(&transform, L, T, 1.0f, R, B, -1.0f);
        al_use_projection_transform(&transform);
    }
}

// Render function.
// (this used to be set in io.RenderDrawListsFn and called by ImGui::Render(), but you can now call this directly from your main loop)
void ImGui_ImplAllegro5_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    // Backup Allegro state that will be modified
    ALLEGRO_TRANSFORM last_transform = *al_get_current_transform();
    ALLEGRO_TRANSFORM last_projection_transform = *al_get_current_projection_transform();
    int last_clip_x, last_clip_y, last_clip_w, last_clip_h;
    al_get_clipping_rectangle(&last_clip_x, &last_clip_y, &last_clip_w, &last_clip_h);
    int last_blender_op, last_blender_src, last_blender_dst;
    al_get_blender(&last_blender_op, &last_blender_src, &last_blender_dst);

    // Setup desired render state
    ImGui_ImplAllegro5_SetupRenderState(draw_data);

    // Render command lists
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        // Allegro's implementation of al_draw_indexed_prim() for DX9 is completely broken. Unindex our buffers ourselves.
        // FIXME-OPT: Unfortunately Allegro doesn't support 32-bits packed colors so we have to convert them to 4 float as well..
        static ImVector<ImDrawVertAllegro> vertices;
        vertices.resize(cmd_list->IdxBuffer.Size);
        for (int i = 0; i < cmd_list->IdxBuffer.Size; i++)
        {
            const ImDrawVert* src_v = &cmd_list->VtxBuffer[cmd_list->IdxBuffer[i]];
            ImDrawVertAllegro* dst_v = &vertices[i];
            dst_v->pos = src_v->pos;
            dst_v->uv = src_v->uv;
            unsigned char* c = (unsigned char*)&src_v->col;
            dst_v->col = al_map_rgba(c[0], c[1], c[2], c[3]);
        }

        const int* indices = NULL;
        if (sizeof(ImDrawIdx) == 2)
        {
            // FIXME-OPT: Unfortunately Allegro doesn't support 16-bit indices.. You can '#define ImDrawIdx int' in imconfig.h to request Dear ImGui to output 32-bit indices.
            // Otherwise, we convert them from 16-bit to 32-bit at runtime here, which works perfectly but is a little wasteful.
            static ImVector<int> indices_converted;
            indices_converted.resize(cmd_list->IdxBuffer.Size);
            for (int i = 0; i < cmd_list->IdxBuffer.Size; ++i)
                indices_converted[i] = (int)cmd_list->IdxBuffer.Data[i];
            indices = indices_converted.Data;
        }
        else if (sizeof(ImDrawIdx) == 4)
        {
            indices = (const int*)cmd_list->IdxBuffer.Data;
        }

        // Render command lists
        int idx_offset = 0;
        ImVec2 clip_off = draw_data->DisplayPos;
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplAllegro5_SetupRenderState(draw_data);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Draw
                ALLEGRO_BITMAP* texture = (ALLEGRO_BITMAP*)pcmd->TextureId;
                al_set_clipping_rectangle(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y, pcmd->ClipRect.z - pcmd->ClipRect.x, pcmd->ClipRect.w - pcmd->ClipRect.y);
                al_draw_prim(&vertices[0], g_VertexDecl, texture, idx_offset, idx_offset + pcmd->ElemCount, ALLEGRO_PRIM_TRIANGLE_LIST);
            }
            idx_offset += pcmd->ElemCount;
        }
    }

    // Restore modified Allegro state
    al_set_blender(last_blender_op, last_blender_src, last_blender_dst);
    al_set_clipping_rectangle(last_clip_x, last_clip_y, last_clip_w, last_clip_h);
    al_use_transform(&last_transform);
    al_use_projection_transform(&last_projection_transform);
}

bool ImGui_ImplAllegro5_CreateDeviceObjects()
{
    // Build texture atlas
    ImGuiIO &io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Create texture
    int flags = al_get_new_bitmap_flags();
    int fmt = al_get_new_bitmap_format();
    al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP|ALLEGRO_MIN_LINEAR|ALLEGRO_MAG_LINEAR);
    al_set_new_bitmap_format(ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE);
    ALLEGRO_BITMAP* img = al_create_bitmap(width, height);
    al_set_new_bitmap_flags(flags);
    al_set_new_bitmap_format(fmt);
    if (!img)
        return false;

    ALLEGRO_LOCKED_REGION *locked_img = al_lock_bitmap(img, al_get_bitmap_format(img), ALLEGRO_LOCK_WRITEONLY);
    if (!locked_img)
    {
        al_destroy_bitmap(img);
        return false;
    }
    memcpy(locked_img->data, pixels, sizeof(int)*width*height);
    al_unlock_bitmap(img);

    // Convert software texture to hardware texture.
    ALLEGRO_BITMAP* cloned_img = al_clone_bitmap(img);
    al_destroy_bitmap(img);
    if (!cloned_img)
        return false;

    // Store our identifier
    io.Fonts->TexID = (void*)cloned_img;
    g_Texture = cloned_img;

    // Create an invisible mouse cursor
    // Because al_hide_mouse_cursor() seems to mess up with the actual inputs..
    ALLEGRO_BITMAP* mouse_cursor = al_create_bitmap(8,8);
    g_MouseCursorInvisible = al_create_mouse_cursor(mouse_cursor, 0, 0);
    al_destroy_bitmap(mouse_cursor);

    return true;
}

void ImGui_ImplAllegro5_InvalidateDeviceObjects()
{
    if (g_Texture)
    {
        al_destroy_bitmap(g_Texture);
        ImGui::GetIO().Fonts->TexID = NULL;
        g_Texture = NULL;
    }
    if (g_MouseCursorInvisible)
    {
        al_destroy_mouse_cursor(g_MouseCursorInvisible);
        g_MouseCursorInvisible = NULL;
    }
}

#if ALLEGRO_HAS_CLIPBOARD
static const char* ImGui_ImplAllegro5_GetClipboardText(void*)
{
    if (g_ClipboardTextData)
        al_free(g_ClipboardTextData);
    g_ClipboardTextData = al_get_clipboard_text(g_Display);
    return g_ClipboardTextData;
}

static void ImGui_ImplAllegro5_SetClipboardText(void*, const char* text)
{
    al_set_clipboard_text(g_Display, text);
}
#endif

bool ImGui_ImplAllegro5_Init(ALLEGRO_DISPLAY* display)
{
    g_Display = display;

    // Setup back-end capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;       // We can honor GetMouseCursor() values (optional)
    io.BackendPlatformName = io.BackendRendererName = "imgui_impl_allegro5";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;

    // Create custom vertex declaration.
    // Unfortunately Allegro doesn't support 32-bits packed colors so we have to convert them to 4 floats.
    // We still use a custom declaration to use 'ALLEGRO_PRIM_TEX_COORD' instead of 'ALLEGRO_PRIM_TEX_COORD_PIXEL' else we can't do a reliable conversion.
    ALLEGRO_VERTEX_ELEMENT elems[] =
    {
        { ALLEGRO_PRIM_POSITION, ALLEGRO_PRIM_FLOAT_2, (int)IM_OFFSETOF(ImDrawVertAllegro, pos) },
        { ALLEGRO_PRIM_TEX_COORD, ALLEGRO_PRIM_FLOAT_2, (int)IM_OFFSETOF(ImDrawVertAllegro, uv) },
        { ALLEGRO_PRIM_COLOR_ATTR, 0, (int)IM_OFFSETOF(ImDrawVertAllegro, col) },
        { 0, 0, 0 }
    };
    g_VertexDecl = al_create_vertex_decl(elems, sizeof(ImDrawVertAllegro));

    io.KeyMap[ImGuiKey_Tab] = ALLEGRO_KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = ALLEGRO_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = ALLEGRO_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = ALLEGRO_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = ALLEGRO_KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = ALLEGRO_KEY_PGUP;
    io.KeyMap[ImGuiKey_PageDown] = ALLEGRO_KEY_PGDN;
    io.KeyMap[ImGuiKey_Home] = ALLEGRO_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = ALLEGRO_KEY_END;
    io.KeyMap[ImGuiKey_Insert] = ALLEGRO_KEY_INSERT;
    io.KeyMap[ImGuiKey_Delete] = ALLEGRO_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = ALLEGRO_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = ALLEGRO_KEY_SPACE;
    io.KeyMap[ImGuiKey_Enter] = ALLEGRO_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = ALLEGRO_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_KeyPadEnter] = ALLEGRO_KEY_PAD_ENTER;
    io.KeyMap[ImGuiKey_A] = ALLEGRO_KEY_A;
    io.KeyMap[ImGuiKey_C] = ALLEGRO_KEY_C;
    io.KeyMap[ImGuiKey_V] = ALLEGRO_KEY_V;
    io.KeyMap[ImGuiKey_X] = ALLEGRO_KEY_X;
    io.KeyMap[ImGuiKey_Y] = ALLEGRO_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = ALLEGRO_KEY_Z;
    io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

#if ALLEGRO_HAS_CLIPBOARD
    io.SetClipboardTextFn = ImGui_ImplAllegro5_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplAllegro5_GetClipboardText;
    io.ClipboardUserData = NULL;
#endif

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        ImGui_ImplAllegro5_InitPlatformInterface(display);

    return true;
}

void ImGui_ImplAllegro5_Shutdown()
{
    ImGui_ImplAllegro5_InvalidateDeviceObjects();

    g_Display = NULL;
    g_Time = 0.0;

    if (g_VertexDecl)
        al_destroy_vertex_decl(g_VertexDecl);
    g_VertexDecl = NULL;

    if (g_ClipboardTextData)
        al_free(g_ClipboardTextData);
    g_ClipboardTextData = NULL;
}

// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
bool ImGui_ImplAllegro5_ProcessEvent(ALLEGRO_EVENT *ev)
{
    ImGuiIO& io = ImGui::GetIO();

    switch (ev->type)
    {
    case ALLEGRO_EVENT_MOUSE_AXES:
        if (ev->mouse.display == g_Display)
        {
            io.MouseWheel += ev->mouse.dz;
            io.MouseWheelH += ev->mouse.dw;
            io.MousePos = ImVec2(ev->mouse.x, ev->mouse.y);
        }
        return true;
    case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
    case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
        if (ev->mouse.display == g_Display && ev->mouse.button <= 5)
            io.MouseDown[ev->mouse.button - 1] = (ev->type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN);
        return true;
    case ALLEGRO_EVENT_TOUCH_MOVE:
        if (ev->touch.display == g_Display)
            io.MousePos = ImVec2(ev->touch.x, ev->touch.y);
        return true;
    case ALLEGRO_EVENT_TOUCH_BEGIN:
    case ALLEGRO_EVENT_TOUCH_END:
    case ALLEGRO_EVENT_TOUCH_CANCEL:
        if (ev->touch.display == g_Display && ev->touch.primary)
            io.MouseDown[0] = (ev->type == ALLEGRO_EVENT_TOUCH_BEGIN);
        return true;
    case ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY:
        if (ev->mouse.display == g_Display)
            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        return true;
    case ALLEGRO_EVENT_KEY_CHAR:
        if (ev->keyboard.display == g_Display)
            io.AddInputCharacter((unsigned int)ev->keyboard.unichar);
        return true;
    case ALLEGRO_EVENT_KEY_DOWN:
    case ALLEGRO_EVENT_KEY_UP:
        if (ev->keyboard.display == g_Display)
            io.KeysDown[ev->keyboard.keycode] = (ev->type == ALLEGRO_EVENT_KEY_DOWN);
        return true;
    }
    return false;
}

static void ImGui_ImplAllegro5_UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None)
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        al_set_mouse_cursor(g_Display, g_MouseCursorInvisible);
    }
    else
    {
        ALLEGRO_SYSTEM_MOUSE_CURSOR cursor_id = ALLEGRO_SYSTEM_MOUSE_CURSOR_DEFAULT;
        switch (imgui_cursor)
        {
        case ImGuiMouseCursor_TextInput:    cursor_id = ALLEGRO_SYSTEM_MOUSE_CURSOR_EDIT; break;
        case ImGuiMouseCursor_ResizeAll:    cursor_id = ALLEGRO_SYSTEM_MOUSE_CURSOR_MOVE; break;
        case ImGuiMouseCursor_ResizeNS:     cursor_id = ALLEGRO_SYSTEM_MOUSE_CURSOR_RESIZE_N; break;
        case ImGuiMouseCursor_ResizeEW:     cursor_id = ALLEGRO_SYSTEM_MOUSE_CURSOR_RESIZE_E; break;
        case ImGuiMouseCursor_ResizeNESW:   cursor_id = ALLEGRO_SYSTEM_MOUSE_CURSOR_RESIZE_NE; break;
        case ImGuiMouseCursor_ResizeNWSE:   cursor_id = ALLEGRO_SYSTEM_MOUSE_CURSOR_RESIZE_NW; break;
        }
        al_set_system_mouse_cursor(g_Display, cursor_id);
    }
}

void ImGui_ImplAllegro5_NewFrame()
{
    if (!g_Texture)
        ImGui_ImplAllegro5_CreateDeviceObjects();

    ImGuiIO &io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    w = al_get_display_width(g_Display);
    h = al_get_display_height(g_Display);
    io.DisplaySize = ImVec2((float)w, (float)h);

    // Setup time step
    double current_time = al_get_time();
    io.DeltaTime = g_Time > 0.0 ? (float)(current_time - g_Time) : (float)(1.0f/60.0f);
    g_Time = current_time;

    // Setup inputs
    ALLEGRO_KEYBOARD_STATE keys;
    al_get_keyboard_state(&keys);
    io.KeyCtrl = al_key_down(&keys, ALLEGRO_KEY_LCTRL) || al_key_down(&keys, ALLEGRO_KEY_RCTRL);
    io.KeyShift = al_key_down(&keys, ALLEGRO_KEY_LSHIFT) || al_key_down(&keys, ALLEGRO_KEY_RSHIFT);
    io.KeyAlt = al_key_down(&keys, ALLEGRO_KEY_ALT) || al_key_down(&keys, ALLEGRO_KEY_ALTGR);
    io.KeySuper = al_key_down(&keys, ALLEGRO_KEY_LWIN) || al_key_down(&keys, ALLEGRO_KEY_RWIN);

    ImGui_ImplAllegro5_UpdateMouseCursor();
}

void ImGui_ImplAllegro5_NewFrame(ALLEGRO_DISPLAY* display)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.Fonts->IsBuilt() && "Font atlas not built! It is generally built by the renderer back-end. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame().");

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    w = al_get_display_width(display);
    h = al_get_display_height(display);

    //SDL_GL_GetDrawableSize(window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    if (w > 0 && h > 0)
        io.DisplayFramebufferScale = ImVec2((float)w / w, (float)h / h);

    /*
    // Setup time step (we don't use SDL_GetTicks() because it is using millisecond resolution)
    static Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 current_time = SDL_GetPerformanceCounter();
    io.DeltaTime = g_Time > 0 ? (float)((double)(current_time - g_Time) / frequency) : (float)(1.0f / 60.0f);
    g_Time = current_time;

    ImGui_ImplSDL2_UpdateMousePosAndButtons();
    ImGui_ImplSDL2_UpdateMouseCursor();

    // Update game controllers (if enabled and available)
    ImGui_ImplSDL2_UpdateGamepads();
    */
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the back-end to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

#define IMGUI_ALLEGRO_MAX_VIEWPORTS 256
static ImGuiViewport *ImGuiAllegroViewports[IMGUI_ALLEGRO_MAX_VIEWPORTS];

struct ImGuiViewportDataAllegro5
{
    ALLEGRO_DISPLAY*    Display;
    bool                WindowOwned;
    bool                Focused;
    bool                Minimized;

    ImGuiViewportDataAllegro5() { Display = NULL; WindowOwned = false; }
    ~ImGuiViewportDataAllegro5() { IM_ASSERT(Display == NULL); }
};

static void ImGui_ImplAllegro5_CreateWindow(ImGuiViewport* viewport)
{
    ImGuiViewportDataAllegro5* data = IM_NEW(ImGuiViewportDataAllegro5)();
    viewport->PlatformUserData = data;

    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGuiViewportDataAllegro5* main_viewport_data = (ImGuiViewportDataAllegro5*)main_viewport->PlatformUserData;

    // Share GL resources with main context
    /*bool use_opengl = (main_viewport_data->GLContext != NULL);
    SDL_GLContext backup_context = NULL;
    if (use_opengl)
    {
        backup_context = SDL_GL_GetCurrentContext();
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        SDL_GL_MakeCurrent(main_viewport_data->Window, main_viewport_data->GLContext);
    }

    Uint32 sdl_flags = 0;
    sdl_flags |= use_opengl ? SDL_WINDOW_OPENGL : SDL_WINDOW_VULKAN;
    sdl_flags |= SDL_GetWindowFlags(g_Window) & SDL_WINDOW_ALLOW_HIGHDPI;
    sdl_flags |= SDL_WINDOW_HIDDEN;
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? SDL_WINDOW_BORDERLESS : 0;
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? 0 : SDL_WINDOW_RESIZABLE;
#if SDL_HAS_ALWAYS_ON_TOP
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_TopMost) ? SDL_WINDOW_ALWAYS_ON_TOP : 0;
#endif
    */
    //data->Window = SDL_CreateWindow("No Title Yet", (int)viewport->Pos.x, (int)viewport->Pos.y, (int)viewport->Size.x, (int)viewport->Size.y, sdl_flags);
    data->Display = al_create_display(1280, 720);
    data->WindowOwned = true;

    //al_set_window_title(data->Display, "Test1");

    /*
    if (use_opengl)
    {
        data->GLContext = SDL_GL_CreateContext(data->Window);
        SDL_GL_SetSwapInterval(0);
    }
    if (use_opengl && backup_context)
        SDL_GL_MakeCurrent(data->Window, backup_context);
    */

    viewport->PlatformHandle = (void*)data->Display;

#if defined(_WIN32)
    viewport->PlatformHandleRaw = al_get_win_window_handle(data->Display);
#endif

    // Store viewport for lookup during certain callbacks
    for(int i = 0; i < IMGUI_ALLEGRO_MAX_VIEWPORTS; i++)
    {
        if(ImGuiAllegroViewports[i] == NULL) {
            ImGuiAllegroViewports[i] = viewport;
            break;
        }
    }
}

static void ImGui_ImplAllegro5_DestroyWindow(ImGuiViewport* viewport)
{
    if (ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData)
    {
        if(data->Display && data->WindowOwned)
            al_destroy_display(data->Display);

        data->Display = NULL;
        IM_DELETE(data);
    }
    viewport->PlatformUserData = viewport->PlatformHandle = NULL;
}

static void ImGui_ImplAllegro5_ShowWindow(ImGuiViewport* viewport)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;

#if defined(_WIN32)
    HWND hwnd = (HWND)viewport->PlatformHandleRaw;

    // TODO: Do we still need these in Allegro?
    // SDL hack: Hide icon from task bar
    // Note: SDL 2.0.6+ has a SDL_WINDOW_SKIP_TASKBAR flag which is supported under Windows but the way it create the window breaks our seamless transition.
    if (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon)
    {
        LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
        ex_style &= ~WS_EX_APPWINDOW;
        ex_style |= WS_EX_TOOLWINDOW;
        ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
    }

    // SDL hack: SDL always activate/focus windows :/
    if (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing)
    {
        ::ShowWindow(hwnd, SW_SHOWNA);
        return;
    }
#endif

    //TODO: Allegro show window function?
    //SDL_ShowWindow(data->Window);
}

static ImVec2 ImGui_ImplAllegro5_GetWindowPos(ImGuiViewport* viewport)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    int x = 0, y = 0;
    al_get_window_position(data->Display, &x, &y);
    return ImVec2((float)x, (float)y);
}

static void ImGui_ImplAllegro5_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    al_set_window_position(data->Display, (int)pos.x, (int)pos.y);
}

static ImVec2 ImGui_ImplAllegro5_GetWindowSize(ImGuiViewport* viewport)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    int w = 0, h = 0;
    w = al_get_display_width(data->Display);
    h = al_get_display_height(data->Display);
    return ImVec2((float)w, (float)h);
}

static void ImGui_ImplAllegro5_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    al_resize_display(data->Display, (int)size.x, (int)size.y);
}

static void ImGui_ImplAllegro5_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    al_set_window_title(data->Display, title);
}

/*
#if SDL_HAS_WINDOW_ALPHA
static void ImGui_ImplAllegro5_SetWindowAlpha(ImGuiViewport* viewport, float alpha)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    SDL_SetWindowOpacity(data->Window, alpha);
}
#endif
*/

static ImGuiViewport *ImGui_ImplAllegro5_FindViewport(ALLEGRO_DISPLAY *display) {
    for(int i = 0; i < IMGUI_ALLEGRO_MAX_VIEWPORTS; i++)
    {
        if(ImGuiAllegroViewports[i] != NULL && ImGuiAllegroViewports[i]->PlatformHandle == display) {
            return ImGuiAllegroViewports[i];
        }
    }
    return NULL;
}

void ImGui_ImplAllegro5_HandleEvent(ALLEGRO_EVENT *ev) {
    ImGuiViewport *viewport = ImGui_ImplAllegro5_FindViewport(ev->display.source);
    if(NULL != viewport) {
        ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
        switch(ev->type) {
        case ALLEGRO_EVENT_DISPLAY_SWITCH_IN:
            for(int i = 0; i < IMGUI_ALLEGRO_MAX_VIEWPORTS; i++)
            {
                if(NULL == ImGuiAllegroViewports[i]) break;
                ((ImGuiViewportDataAllegro5*)ImGuiAllegroViewports[i]->PlatformUserData)->Focused = false;
            }
            data->Focused = true;
            break;
        }

    }
}

static void ImGui_ImplAllegro5_SetWindowFocus(ImGuiViewport* viewport)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    // TODO: Allegro method to set window focus
    data->Focused = true;
}

static bool ImGui_ImplAllegro5_GetWindowFocus(ImGuiViewport* viewport)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    return data->Focused;
}

static bool ImGui_ImplAllegro5_GetWindowMinimized(ImGuiViewport* viewport)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    // TODO: Allegro/platform methods to get minimized flag
    return false;
}

static void ImGui_ImplAllegro5_RenderWindow(ImGuiViewport* viewport, void*)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
//al_draw_line(0, 0, 1000, 1000, al_map_rgb(0,0,0), 2);
    al_set_target_bitmap(al_get_backbuffer(data->Display));

}

static void ImGui_ImplAllegro5_SwapBuffers(ImGuiViewport* viewport, void*)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    al_set_target_bitmap(al_get_backbuffer(data->Display));
    al_flip_display();
}

// Vulkan support (the Vulkan renderer needs to call a platform-side support function to create the surface)
// SDL is graceful enough to _not_ need <vulkan/vulkan.h> so we can safely include this.
#if SDL_HAS_VULKAN
#include <SDL_vulkan.h>
static int ImGui_ImplAllegro5_CreateVkSurface(ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface)
{
    ImGuiViewportDataAllegro5* data = (ImGuiViewportDataAllegro5*)viewport->PlatformUserData;
    (void)vk_allocator;
    SDL_bool ret = SDL_Vulkan_CreateSurface(data->Window, (VkInstance)vk_instance, (VkSurfaceKHR*)out_vk_surface);
    return ret ? 0 : 1; // ret ? VK_SUCCESS : VK_NOT_READY
}
#endif // SDL_HAS_VULKAN

// FIXME-PLATFORM: SDL doesn't have an event to notify the application of display/monitor changes
static void ImGui_ImplAllegro5_UpdateMonitors()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ALLEGRO_MONITOR_INFO al_monitor;
    platform_io.Monitors.resize(0);
    int display_count = al_get_num_video_adapters();
    for (int n = 0; n < display_count; n++)
    {
        al_get_monitor_info(n, &al_monitor);

        // Warning: the validity of monitor DPI information on Windows depends on the application DPI awareness settings, which generally needs to be set in the manifest or at runtime.
        ImGuiPlatformMonitor monitor;

        monitor.MainPos = monitor.WorkPos = ImVec2((float)al_monitor.x1, (float)al_monitor.y1);
        monitor.MainSize = monitor.WorkSize = ImVec2((float)al_monitor.x2 - al_monitor.x1, (float)al_monitor.y2 - al_monitor.y1);
/*
#if SDL_HAS_USABLE_DISPLAY_BOUNDS
        SDL_GetDisplayUsableBounds(n, &r);
        monitor.WorkPos = ImVec2((float)r.x, (float)r.y);
        monitor.WorkSize = ImVec2((float)r.w, (float)r.h);
#endif
#if SDL_HAS_PER_MONITOR_DPI
        float dpi = 0.0f;
        if (!SDL_GetDisplayDPI(n, &dpi, NULL, NULL))
            monitor.DpiScale = dpi / 96.0f;
#endif
*/
        platform_io.Monitors.push_back(monitor);
    }
}

static void ImGui_ImplAllegro5_InitPlatformInterface(ALLEGRO_DISPLAY* display)
{
    // Register platform interface (will be coupled with a renderer interface)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateWindow = ImGui_ImplAllegro5_CreateWindow;
    platform_io.Platform_DestroyWindow = ImGui_ImplAllegro5_DestroyWindow;
    platform_io.Platform_ShowWindow = ImGui_ImplAllegro5_ShowWindow;
    platform_io.Platform_SetWindowPos = ImGui_ImplAllegro5_SetWindowPos;
    platform_io.Platform_GetWindowPos = ImGui_ImplAllegro5_GetWindowPos;
    platform_io.Platform_SetWindowSize = ImGui_ImplAllegro5_SetWindowSize;
    platform_io.Platform_GetWindowSize = ImGui_ImplAllegro5_GetWindowSize;
    platform_io.Platform_SetWindowFocus = ImGui_ImplAllegro5_SetWindowFocus;
    platform_io.Platform_GetWindowFocus = ImGui_ImplAllegro5_GetWindowFocus;
    platform_io.Platform_GetWindowMinimized = ImGui_ImplAllegro5_GetWindowMinimized;
    platform_io.Platform_SetWindowTitle = ImGui_ImplAllegro5_SetWindowTitle;
    platform_io.Platform_RenderWindow = ImGui_ImplAllegro5_RenderWindow;
    platform_io.Platform_SwapBuffers = ImGui_ImplAllegro5_SwapBuffers;
    /*
#if SDL_HAS_WINDOW_ALPHA
    platform_io.Platform_SetWindowAlpha = ImGui_ImplAllegro5_SetWindowAlpha;
#endif
#if SDL_HAS_VULKAN
    platform_io.Platform_CreateVkSurface = ImGui_ImplAllegro5_CreateVkSurface;
#endif
    */
    // Allegro5 by default doesn't pass mouse clicks to the application when the click focused a window. This is getting in the way of our interactions and we disable that behavior.
#if SDL_HAS_MOUSE_FOCUS_CLICKTHROUGH
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#endif

    ImGui_ImplAllegro5_UpdateMonitors();

    // Register main window handle (which is owned by the main application, not by us)
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGuiViewportDataAllegro5* data = IM_NEW(ImGuiViewportDataAllegro5)();
    data->Display = display;
    data->WindowOwned = false;
    main_viewport->PlatformUserData = data;
    main_viewport->PlatformHandle = data->Display;
    ImGuiAllegroViewports[0] = main_viewport;
}

static void ImGui_ImplAllegro5_ShutdownPlatformInterface()
{
}
