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

#define NUM_WINDOWS (3)

typedef struct {
    sg_context sgcontext;
    sg_pass_action pass_action;
} window_state_t;

static struct {
    uint64_t laptime;
    window_state_t windows[NUM_WINDOWS];
} state;

void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    stm_setup();

    // initialize default context
    state.windows[sapp_window_index(sapp_main_window())] = (window_state_t){
        .sgcontext = sg_default_context(),
        .pass_action = {
            .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0, 1, 1, 1 } }
        }
    };

    // create a new window
    const sapp_window window = sapp_open_window(&(sapp_window_desc){
        .x = 100,
        .y = 100,
        .width = 400,
        .height = 300,
        .title = "Other Window"
    });
    const sg_context_desc ctx_desc = sapp_window_sgcontext(window);
    state.windows[sapp_window_index(window)] = (window_state_t) {
        .sgcontext = sg_make_context(&ctx_desc),
        .pass_action = {
            .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1, 1, 0, 1 } }
        }
    };
}

void frame(void) {
//    double dt = stm_ms(stm_laptime(&state.laptime));
//__builtin_printf("dt: %f\n", dt);

    for (sapp_window win = sapp_first_window(); sapp_valid_window(win); win = sapp_next_window(win)) {
        window_state_t* win_state = &state.windows[sapp_window_index(win)];
        float b = win_state->pass_action.colors[0].value.b + 0.01f;
        if (b > 1.0f) {
            b = 0.0f;
        }
        win_state->pass_action.colors[0].value.b = b;
        sapp_activate_window_context(win);
        sg_activate_context(win_state->sgcontext);
        sg_begin_default_pass(&win_state->pass_action, sapp_window_width(win), sapp_window_height(win));
        sg_end_pass();
    }
    sg_commit();
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
        .cleanup_cb = cleanup,
        .window_pool_size = NUM_WINDOWS,
        .window = {
            .width = 640,
            .height = 480,
            .title = "Main Window",
        },
        .icon.sokol_default = true,
        .gl.force_gles2 = true,
    };
}
