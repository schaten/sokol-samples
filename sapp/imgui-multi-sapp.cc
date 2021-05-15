//------------------------------------------------------------------------------
//  imgui-multi-sapp.cc
//
//  Dear ImGui multi-window sample.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "imgui-dock/imgui.h"
#include "imgui-multi-sapp.glsl.h"

#define MAX_WINDOWS (16)
#define MAX_KEY_VALUE (512)

static struct {
    sg_pass_action pass_action;
    struct {
        sg_buffer vbuf;
        sg_buffer ibuf;
        sg_image img;
        sg_pipeline pip;
        bool btn_down[SAPP_MAX_MOUSEBUTTONS];
        bool btn_up[SAPP_MAX_MOUSEBUTTONS];
        uint8_t keys_down[MAX_KEY_VALUE];
        uint8_t keys_up[MAX_KEY_VALUE];
    } imgui;
} state;

static ImDrawVert vertices[1<<16];
static ImDrawIdx indices[(1<<16) * 3];

static void imgui_init(void);
static void imgui_shutdown(void);
static void imgui_newframe(void);
static void imgui_draw(void);
static void imgui_set_modifiers(ImGuiIO& io, uint32_t mods);

void init(void) {
    sg_desc desc = { };
    desc.context = sapp_sgcontext();
    sg_setup(&desc);
    imgui_init();
}

void frame(void) {
    imgui_newframe();

    ImGui::ShowDemoWindow();

    sg_activate_context(sg_default_context());
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    imgui_draw();
    sg_end_pass();
    // FIXME: don't crash if sg_commit() is missing, needs a proper validate check!
    sg_commit();
}

void input(const sapp_event* ev) {
    const float dpi_scale = sapp_dpi_scale();
    ImGuiIO& io = ImGui::GetIO();
    imgui_set_modifiers(io, ev->modifiers);
    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            io.MousePos.x = ev->mouse_x / dpi_scale;
            io.MousePos.y = ev->mouse_y / dpi_scale;
            state.imgui.btn_down[ev->mouse_button] = true;
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            io.MousePos.x = ev->mouse_x / dpi_scale;
            io.MousePos.y = ev->mouse_y / dpi_scale;
            if (ev->mouse_button < 3) {
                state.imgui.btn_up[ev->mouse_button] = true;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            io.MousePos.x = ev->mouse_x / dpi_scale;
            io.MousePos.y = ev->mouse_y / dpi_scale;
            break;
        case SAPP_EVENTTYPE_MOUSE_ENTER:
        case SAPP_EVENTTYPE_MOUSE_LEAVE:
            for (int i = 0; i < SAPP_MAX_MOUSEBUTTONS; i++) {
                state.imgui.btn_down[i] = false;
                state.imgui.btn_up[i] = false;
                io.MouseDown[i] = false;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            io.MouseWheelH = ev->scroll_x;
            io.MouseWheel = ev->scroll_y;
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
            state.imgui.keys_down[ev->key_code] = 0x80 | (uint8_t)ev->modifiers;
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            state.imgui.keys_up[ev->key_code] = 0x80 | (uint8_t)ev->modifiers;
            break;
        case SAPP_EVENTTYPE_CHAR:
            /* on some platforms, special keys may be reported as
               characters, which may confuse some ImGui widgets,
               drop those, also don't forward characters if some
               modifiers have been pressed
            */
            if ((ev->char_code >= 32) &&
                (ev->char_code != 127) &&
                (0 == (ev->modifiers & (SAPP_MODIFIER_ALT|SAPP_MODIFIER_CTRL|SAPP_MODIFIER_SUPER))))
            {
                io.AddInputCharacter((ImWchar)ev->char_code);
            }
            break;
        default:
            break;
    }
}

void cleanup(void) {
    imgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sapp_desc desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = input;
    desc.width = 1024;
    desc.height = 768;
    desc.window_pool_size = MAX_WINDOWS;
    desc.window_title = "Dear ImGui Multiwindow";
    desc.icon.sokol_default = true;
    return desc;
}

void imgui_init(void) {
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.KeyMap[ImGuiKey_Tab] = SAPP_KEYCODE_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = SAPP_KEYCODE_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = SAPP_KEYCODE_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = SAPP_KEYCODE_UP;
    io.KeyMap[ImGuiKey_DownArrow] = SAPP_KEYCODE_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = SAPP_KEYCODE_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = SAPP_KEYCODE_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = SAPP_KEYCODE_HOME;
    io.KeyMap[ImGuiKey_End] = SAPP_KEYCODE_END;
    io.KeyMap[ImGuiKey_Delete] = SAPP_KEYCODE_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = SAPP_KEYCODE_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = SAPP_KEYCODE_SPACE;
    io.KeyMap[ImGuiKey_Enter] = SAPP_KEYCODE_ENTER;
    io.KeyMap[ImGuiKey_Escape] = SAPP_KEYCODE_ESCAPE;
    io.KeyMap[ImGuiKey_A] = SAPP_KEYCODE_A;
    io.KeyMap[ImGuiKey_C] = SAPP_KEYCODE_C;
    io.KeyMap[ImGuiKey_V] = SAPP_KEYCODE_V;
    io.KeyMap[ImGuiKey_X] = SAPP_KEYCODE_X;
    io.KeyMap[ImGuiKey_Y] = SAPP_KEYCODE_Y;
    io.KeyMap[ImGuiKey_Z] = SAPP_KEYCODE_Z;

    // vertex and index buffers
    sg_buffer_desc vb_desc = { };
    vb_desc.usage = SG_USAGE_STREAM;
    vb_desc.size = sizeof(vertices);
    state.imgui.vbuf = sg_make_buffer(&vb_desc);

    sg_buffer_desc ib_desc = { };
    ib_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ib_desc.usage = SG_USAGE_STREAM;
    ib_desc.size = sizeof(indices);
    state.imgui.ibuf = sg_make_buffer(&ib_desc);

    // font texture
    unsigned char* font_pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc = { };
    img_desc.width = font_width;
    img_desc.height = font_height;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.min_filter = SG_FILTER_LINEAR;
    img_desc.mag_filter = SG_FILTER_LINEAR;
    img_desc.data.subimage[0][0].ptr = font_pixels;
    img_desc.data.subimage[0][0].size = (size_t)(font_width * font_height) * sizeof(uint32_t);
    state.imgui.img = sg_make_image(&img_desc);
    io.Fonts->TexID = (ImTextureID)(uintptr_t) state.imgui.img.id;

    // shader and pipeline object
    sg_pipeline_desc pip_desc = { };
    pip_desc.shader = sg_make_shader(imgui_shader_desc(sg_query_backend()));
    pip_desc.layout.buffers[0].stride = sizeof(ImDrawVert);
    pip_desc.layout.attrs[0].offset = offsetof(ImDrawVert, pos);
    pip_desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
    pip_desc.layout.attrs[1].offset = offsetof(ImDrawVert, uv);
    pip_desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    pip_desc.layout.attrs[2].offset = offsetof(ImDrawVert, col);
    pip_desc.layout.attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.colors[0].write_mask = SG_COLORMASK_RGB;
    pip_desc.colors[0].blend.enabled = true;
    pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    state.imgui.pip = sg_make_pipeline(&pip_desc);
}

void imgui_shutdown(void) {
    ImGui::DestroyContext();
}

void imgui_set_modifiers(ImGuiIO& io, uint32_t mods) {
    io.KeyAlt = (mods & SAPP_MODIFIER_ALT) != 0;
    io.KeyCtrl = (mods & SAPP_MODIFIER_CTRL) != 0;
    io.KeyShift = (mods & SAPP_MODIFIER_SHIFT) != 0;
    io.KeySuper = (mods & SAPP_MODIFIER_SUPER) != 0;
}

void imgui_newframe(void) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = sapp_widthf();
    io.DisplaySize.y = sapp_widthf();
    io.DeltaTime = 1.0f / 60.0f;
    for (int i = 0; i < SAPP_MAX_MOUSEBUTTONS; i++) {
        if (state.imgui.btn_down[i]) {
            state.imgui.btn_down[i] = false;
            io.MouseDown[i] = true;
        }
        else if (state.imgui.btn_up[i]) {
            state.imgui.btn_up[i] = false;
            io.MouseDown[i] = false;
        }
    }
    for (int i = 0; i < MAX_KEY_VALUE; i++) {
        if (state.imgui.keys_down[i]) {
            io.KeysDown[i] = true;
            imgui_set_modifiers(io, state.imgui.keys_down[i]);
            state.imgui.keys_down[i] = 0;
        }
        else if (state.imgui.keys_up[i]) {
            io.KeysDown[i] = false;
            imgui_set_modifiers(io, state.imgui.keys_up[i]);
            state.imgui.keys_up[i] = 0;
        }
    }
    ImGui::NewFrame();
}

void imgui_draw(void) {
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGuiIO& io = ImGui::GetIO();
    if (0 == draw_data) {
        return;
    }
    if (draw_data->CmdListsCount == 0) {
        return;
    }
    size_t all_vtx_size = 0;
    size_t all_idx_size = 0;
    int cmd_list_count = 0;
    for (int cl_index = 0; cl_index < draw_data->CmdListsCount; cl_index++, cmd_list_count++) {
        ImDrawList* cl = draw_data->CmdLists[cl_index];
        const size_t vtx_size = cl->VtxBuffer.size() * sizeof(ImDrawVert);
        const size_t idx_size = cl->IdxBuffer.size() * sizeof(ImDrawIdx);
        const ImDrawVert* vtx_ptr = &cl->VtxBuffer.front();
        const ImDrawIdx* idx_ptr = &cl->IdxBuffer.front();

        /* check for buffer overflow */
        if (((all_vtx_size + vtx_size) > sizeof(vertices)) ||
            ((all_idx_size + idx_size) > sizeof(indices)))
        {
            break;
        }

        /* copy vertices and indices into common buffers */
        void* dst_vtx_ptr = (void*) (((uint8_t*)vertices) + all_vtx_size);
        void* dst_idx_ptr = (void*) (((uint8_t*)indices) + all_idx_size);
        memcpy(dst_vtx_ptr, vtx_ptr, vtx_size);
        memcpy(dst_idx_ptr, idx_ptr, idx_size);
        all_vtx_size += vtx_size;
        all_idx_size += idx_size;
    }
    if (0 == cmd_list_count) {
        return;
    }

    /* update the sokol-gfx vertex- and index-buffer */
    sg_range vtx_data = SG_RANGE(vertices);
    vtx_data.size = all_vtx_size;
    sg_range idx_data = SG_RANGE(indices);
    idx_data.size = all_idx_size;
    sg_update_buffer(state.imgui.vbuf, &vtx_data);
    sg_update_buffer(state.imgui.ibuf, &idx_data);

    /* render the ImGui command list */
    const float dpi_scale = sapp_dpi_scale();
    const int fb_width = (int) (io.DisplaySize.x * dpi_scale);
    const int fb_height = (int) (io.DisplaySize.y * dpi_scale);
    sg_apply_viewport(0, 0, fb_width, fb_height, true);
    sg_apply_scissor_rect(0, 0, fb_width, fb_height, true);

    sg_apply_pipeline(state.imgui.pip);
    vs_params_t vs_params;
    memset((void*)&vs_params, 0, sizeof(vs_params));
    vs_params.disp_size[0] = io.DisplaySize.x;
    vs_params.disp_size[1] = io.DisplaySize.y;
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(vs_params));
    sg_bindings bind = { };
    bind.vertex_buffers[0] = state.imgui.vbuf;
    bind.index_buffer = state.imgui.ibuf;
    ImTextureID tex_id = io.Fonts->TexID;
    bind.fs_images[0].id = (uint32_t)(uintptr_t)tex_id;
    int vb_offset = 0;
    int ib_offset = 0;
    for (int cl_index = 0; cl_index < cmd_list_count; cl_index++) {
        const ImDrawList* cl = draw_data->CmdLists[cl_index];

        bind.vertex_buffer_offsets[0] = vb_offset;
        bind.index_buffer_offset = ib_offset;
        sg_apply_bindings(&bind);

        int base_element = 0;
        #if defined(__cplusplus)
            const int num_cmds = cl->CmdBuffer.size();
        #else
            const int num_cmds = cl->CmdBuffer.Size;
        #endif
        uint32_t vtx_offset = 0;
        for (int cmd_index = 0; cmd_index < num_cmds; cmd_index++) {
            ImDrawCmd* pcmd = &cl->CmdBuffer.Data[cmd_index];
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cl, pcmd);
                // need to re-apply all state after calling a user callback
                sg_apply_viewport(0, 0, fb_width, fb_height, true);
                sg_apply_pipeline(state.imgui.pip);
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(vs_params));
                sg_apply_bindings(&bind);
            }
            else {
                if ((tex_id != pcmd->TextureId) || (vtx_offset != pcmd->VtxOffset)) {
                    tex_id = pcmd->TextureId;
                    vtx_offset = pcmd->VtxOffset;
                    bind.fs_images[0].id = (uint32_t)(uintptr_t)tex_id;
                    bind.vertex_buffer_offsets[0] = vb_offset + (int)(pcmd->VtxOffset * sizeof(ImDrawVert));
                    sg_apply_bindings(&bind);
                }
                const int scissor_x = (int) (pcmd->ClipRect.x * dpi_scale);
                const int scissor_y = (int) (pcmd->ClipRect.y * dpi_scale);
                const int scissor_w = (int) ((pcmd->ClipRect.z - pcmd->ClipRect.x) * dpi_scale);
                const int scissor_h = (int) ((pcmd->ClipRect.w - pcmd->ClipRect.y) * dpi_scale);
                sg_apply_scissor_rect(scissor_x, scissor_y, scissor_w, scissor_h, true);
                sg_draw(base_element, (int)pcmd->ElemCount, 1);
            }
            base_element += (int)pcmd->ElemCount;
        }
        const size_t vtx_size = cl->VtxBuffer.size() * sizeof(ImDrawVert);
        const size_t idx_size = cl->IdxBuffer.size() * sizeof(ImDrawIdx);
        vb_offset += (int)vtx_size;
        ib_offset += (int)idx_size;
    }
    sg_apply_viewport(0, 0, fb_width, fb_height, true);
    sg_apply_scissor_rect(0, 0, fb_width, fb_height, true);
}
