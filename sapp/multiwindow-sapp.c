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

sg_context main_ctx;
sg_context other_ctx;

sg_pass_action other_pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0, 1, 1, 1 } }
};

sg_pass_action main_pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1, 1, 0, 1 } }
};


void init_other(void) {
    other_ctx = sg_setup_context();
}

void frame_other(void) {
    float b = other_pass_action.colors[0].value.b + 0.01f;
    if (b > 1.0f) {
        b = 0.0f;
    }
    main_pass_action.colors[0].value.b = b;
    sg_activate_context(other_ctx);
    sg_begin_default_pass(&other_pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

void cleanup_other(void) {
    sg_discard_context(other_ctx);
}

void init_main(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    main_ctx = sg_setup_context();

    sapp_open_window(&(sapp_window_desc){
        .init_cb = init_other,
        .frame_cb = frame_other,
        .cleanup_cb = cleanup_other,
        .x = 100,
        .y = 100,
        .width = 400,
        .height = 300,
        .title = "Other Window"
    });
}

void frame_main(void) {
    float b = other_pass_action.colors[0].value.b + 0.01f;
    if (b > 1.0f) {
        b = 0.0f;
    }
    other_pass_action.colors[0].value.b = b;
    sg_activate_context(main_ctx);
    sg_begin_default_pass(&main_pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

void cleanup_main(void) {
    sg_discard_context(main_ctx);
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
