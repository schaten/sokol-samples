//------------------------------------------------------------------------------
//  multiwindow-sapp.c
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "sokol_time.h"
#include "dbgui/dbgui.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"

#define NUM_WINDOWS (3)

typedef struct {
    sapp_window win;
    sg_context sg_ctx;
    sdtx_context sdtx_ctx;
    sg_pass_action pass_action;
} window_state_t;

static struct {
    uint64_t laptime;
    window_state_t windows[NUM_WINDOWS];
} state;

const char* window_titles[NUM_WINDOWS] = {
    "Main Window",
    "Window 1",
    "Window 2"
};

sg_color clear_colors[NUM_WINDOWS] = {
    { 0.0f, 0.5f, 0.5f, 1.0f },
    { 0.5f, 0.5f, 0.0f, 1.0f },
    { 0.5f, 0.0f, 0.5f, 1.0f }
};

void open_window(int index) {
    assert((index >= 0) && (index < NUM_WINDOWS));
    window_state_t* win_state = &state.windows[index];
    assert(SAPP_INVALID_ID == win_state->win.id);

    sapp_window window_handle = sapp_open_window(&(sapp_window_desc) {
        .x = 100,
        .y = 100,
        .width = 400,
        .height = 300,
        .title = window_titles[index],
        .user_data = &state.windows[index],
    });

    win_state->win = window_handle;
    sg_context_desc ctx_desc = sapp_window_sgcontext(win_state->win);
    win_state->sg_ctx = sg_make_context(&ctx_desc);
    sg_activate_context(win_state->sg_ctx);
    win_state->sdtx_ctx = sdtx_make_context(&(sdtx_context_desc_t){ 0 });
    win_state->pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = clear_colors[index] }
    };
}

void window_closed(sapp_window win) {
    // don't close the main window
    SOKOL_ASSERT(win.id != sapp_main_window().id);
    window_state_t* win_state = (window_state_t*) sapp_window_userdata(win);
    SOKOL_ASSERT(win_state);
    assert(SAPP_INVALID_ID != win_state->win.id);
    sg_activate_context(win_state->sg_ctx);
    sdtx_destroy_context(win_state->sdtx_ctx);
    sg_destroy_context(win_state->sg_ctx);
    *win_state = (window_state_t) { 0 };
}

void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_kc853()
    });
    stm_setup();

    window_state_t* win_state = &state.windows[0];
    win_state->win = sapp_main_window();
    win_state->sg_ctx = sg_default_context();
    win_state->sdtx_ctx = SDTX_DEFAULT_CONTEXT;
    win_state->pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = clear_colors[0] }
    };
}

void frame(void) {
    double dt = stm_ms(stm_laptime(&state.laptime));

    for (sapp_window win = sapp_first_window(); sapp_valid_window(win); win = sapp_next_window(win)) {
        window_state_t* win_state = (window_state_t*) sapp_window_userdata(win);
        SOKOL_ASSERT(0 != win_state);
        assert(win_state->win.id != SAPP_INVALID_ID);

        sdtx_set_context(win_state->sdtx_ctx);
        sdtx_canvas(sapp_window_widthf(win) * 0.5f, sapp_window_heightf(win) * 0.5f);
        sdtx_origin(1.0f, 2.0f);
        sdtx_printf("frame time: %.3f\n\n", dt);

        if (sapp_main_window().id == win.id) {
            sdtx_puts("Press key to:\n\n");
            for (int i = 1; i < NUM_WINDOWS; i++) {
                window_state_t* ws = &state.windows[i];
                sdtx_printf("  %d: %s '%s'\n", i, sapp_valid_window(ws->win) ? "close":"open", window_titles[i]);
            }
        }

        sapp_activate_window_context(win);
        sg_activate_context(win_state->sg_ctx);
        sg_begin_default_pass(&win_state->pass_action, sapp_window_width(win), sapp_window_height(win));
        sdtx_draw();
        sg_end_pass();
    }
    sg_commit();
}

void event(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        for (int i = 1; i < NUM_WINDOWS; i++) {
            if (ev->key_code == (sapp_keycode)(SAPP_KEYCODE_1 + (i-1))) {
                window_state_t* win_state = &state.windows[i];
                if (win_state->win.id == SAPP_INVALID_ID) {
                    open_window(i);
                }
                else {
                    sapp_close_window(win_state->win);
                }
            }
        }
    }
    else if (ev->type == SAPP_EVENTTYPE_WINDOW_CLOSED) {
        window_closed(ev->window);
    }
}

void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = event,
        .cleanup_cb = cleanup,
        .window_pool_size = NUM_WINDOWS,
        .width = 640,
        .height = 480,
        .window_title = "Main Window",
        .icon.sokol_default = true,
        .gl.force_gles2 = true,
        .user_data = &state.windows[0],
    };
}
