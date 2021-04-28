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

sg_context other_context;
sapp_window other_window;
uint64_t laptime;

sg_pass_action other_pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0, 1, 1, 1 } }
};

sg_pass_action default_pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1, 1, 0, 1 } }
};

void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    stm_setup();
    other_window = sapp_open_window(&(sapp_window_desc){
        .x = 100,
        .y = 100,
        .width = 400,
        .height = 300,
        .title = "Other Window"
    });

    const sg_context_desc ctx_desc = sapp_window_sgcontext(other_window);
    other_context = sg_make_context(&ctx_desc);
}

void frame(void) {
    //double dt = stm_ms(stm_laptime(&laptime));

    float b = default_pass_action.colors[0].value.b + 0.01f;
    if (b > 1.0f) {
        b = 0.0f;
    }
    default_pass_action.colors[0].value.b = b;

    b = other_pass_action.colors[0].value.b + 0.01f;
    if (b > 1.0f) {
        b = 0.0f;
    }
    other_pass_action.colors[0].value.b = b;

    // draw in main window
    sg_activate_context(sg_default_context());
    sg_begin_default_pass(&default_pass_action, sapp_width(), sapp_height());
    sg_end_pass();

    // draw in other window
    sg_activate_context(other_context);
    sg_begin_default_pass(&other_pass_action, sapp_window_width(other_window), sapp_window_height(other_window));
    sg_end_pass();

    // one commit per frame
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
        .window = {
            .width = 640,
            .height = 480,
            .title = "Main Window",
        },
        .icon.sokol_default = true,
        .gl.force_gles2 = true,
    };
}
