//------------------------------------------------------------------------------
//  multiwindow-sapp.c
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"

void frame_other(void) {
    const sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0, 1, 1, 1 } }
    };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

void init_main(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });

    sapp_open_window(&(sapp_window_desc){
        .frame_cb = frame_other,
        .x = 100,
        .y = 100,
        .width = 400,
        .height = 300,
        .title = "Other Window"
    });
}

void frame_main(void) {
    const sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1, 1, 0, 1 } }
    };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

void cleanup_main(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .window = {
            .init_cb = init_main,
            .frame_cb = frame_main,
            .cleanup_cb = cleanup_main,
            .width = 640,
            .height = 480,
            .title = "Main Window",
        },
        .icon.sokol_default = true,
        .gl.force_gles2 = true,
    };
}
