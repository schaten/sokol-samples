//------------------------------------------------------------------------------
//  mobile-input-sapp.c
//
//  Test mobile input sokol-sapp features (virtual keyboard and clipboard).
//------------------------------------------------------------------------------
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_debugtext.h"
#include "sokol_color.h"
#include "sokol_glue.h"

static struct {
    struct {
        sapp_keycode key_code;
        uint64_t frame;
    } down;
    struct {
        sapp_keycode key_code;
        uint64_t frame;
    } up;
    struct {
        uint32_t char_code;
        uint64_t frame;
    } chr;
} state;

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    sdtx_setup(&(sdtx_desc_t){ .fonts[0] = sdtx_font_kc854() });
}

static void frame(void) {

    sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
    sdtx_origin(2.0f, 2.0f);
    sdtx_home();
    sdtx_puts(sapp_keyboard_shown() ? "Tap to close keyboard\n\n" : "Tap to open keyboard\n\n");
    sdtx_printf("char code: %04X\t\tframe: %d\n", state.chr.char_code, state.chr.frame);
    sdtx_printf("key down:   %03X\t\tframe: %d\n", state.down.key_code, state.down.frame);
    sdtx_printf("key up:     %03X\t\tframe: %d\n", state.up.key_code, state.up.frame);

    static const sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = SG_CORNFLOWER_BLUE }
    };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sdtx_draw();
    sg_end_pass();
    sg_commit();
}

static void event(const sapp_event* ev) {
    switch (ev->type) {
        case SAPP_EVENTTYPE_TOUCHES_BEGAN:
            if (ev->num_touches == 1) {
                sapp_show_keyboard(!sapp_keyboard_shown());
            }
            break;

        case SAPP_EVENTTYPE_CHAR:
            state.chr.char_code = ev->char_code;
            state.chr.frame = ev->frame_count;
            break;

        case SAPP_EVENTTYPE_KEY_DOWN:
            state.down.key_code = ev->key_code;
            state.down.frame = ev->frame_count;
            break;

        case SAPP_EVENTTYPE_KEY_UP:
            state.up.key_code = ev->key_code;
            state.up.frame = ev->frame_count;
            break;
    }
}

static void cleanup(void) {
    sdtx_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = event,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .window_title = "mobile-input-sapp.c",
        .enable_clipboard = true,
        .icon.sokol_default = true,
    };
}
