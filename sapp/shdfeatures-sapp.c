//------------------------------------------------------------------------------
//  shdfeatures-sapp.c
//
//  FIXME: explanation
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_fetch.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "ozzutil/ozzutil.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "util/camera.h"

static struct {
    sg_pass_action pass_action;
    camera_t camera;
    ozz_instance_t* ozz;
    uint64_t laptime;
    double frame_time_sec;
} state;

// IO buffers (we know the max file sizes upfront)
static uint8_t skel_io_buffer[32 * 1024];
static uint8_t anim_io_buffer[96 * 1024];
static uint8_t mesh_io_buffer[3 * 1024 * 1024];

static void skel_data_loaded(const sfetch_response_t* respone);
static void anim_data_loaded(const sfetch_response_t* respone);
static void mesh_data_loaded(const sfetch_response_t* respone);

static void init(void) {
    // setup sokol-gfx
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });

    // setup sokol-time
    stm_setup();

    // setup sokol-fetch
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 1,
        .num_lanes = 3
    });

    // setup sokol-imgui
    simgui_setup(&(simgui_desc_t){0});

    // initialize clear color
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    // initialize camera controller
    cam_init(&state.camera, &(camera_desc_t){
        .min_dist = 2.0f,
        .max_dist = 10.0f,
        .center.Y = 1.1f,
        .distance = 3.0f,
        .latitude = 20.0f,
        .longitude = 20.0f
    });

    // setup ozz-utility wrapper and create a character instance
    ozz_setup(&(ozz_desc_t){
        .max_palette_joints = 64,
        .max_instances = 1
    });
    state.ozz = ozz_create_instance(0);

    // start loading data
    sfetch_send(&(sfetch_request_t){
        .path = "ozz_skin_skeleton.ozz",
        .callback = skel_data_loaded,
        .buffer_ptr = skel_io_buffer,
        .buffer_size = sizeof(skel_io_buffer)
    });
    sfetch_send(&(sfetch_request_t){
        .path = "ozz_skin_animation.ozz",
        .callback = anim_data_loaded,
        .buffer_ptr = anim_io_buffer,
        .buffer_size = sizeof(anim_io_buffer)
    });
    sfetch_send(&(sfetch_request_t){
        .path = "ozz_skin_mesh.ozz",
        .callback = mesh_data_loaded,
        .buffer_ptr = mesh_io_buffer,
        .buffer_size = sizeof(mesh_io_buffer)
    });
}

static void frame(void) {
    sfetch_dowork();

    const int fb_width = sapp_width();
    const int fb_height = sapp_height();
    uint64_t frame_ticks = stm_round_to_common_refresh_rate(stm_laptime(&state.laptime));
    state.frame_time_sec = stm_sec(frame_ticks);
    cam_update(&state.camera, fb_width, fb_height);
    simgui_new_frame(fb_width, fb_height, state.frame_time_sec);

    sg_begin_default_pass(&state.pass_action, fb_width, fb_height);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    ozz_destroy_instance(state.ozz);
    ozz_shutdown();
    simgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();
}

static void input(const sapp_event* ev) {
    if (simgui_handle_event(ev)) {
        return;
    }
    cam_handle_event(&state.camera, ev);
}

static void skel_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        ozz_load_skeleton(state.ozz, response->buffer_ptr, response->fetched_size);
    }
    else if (response->failed) {
        ozz_set_load_failed(state.ozz);
    }
}

static void anim_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        ozz_load_animation(state.ozz, response->buffer_ptr, response->fetched_size);
    }
    else if (response->failed) {
        ozz_set_load_failed(state.ozz);
    }
}

static void mesh_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        ozz_load_mesh(state.ozz, response->buffer_ptr, response->fetched_size);
    }
    else if (response->failed) {
        ozz_set_load_failed(state.ozz);
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .window_title = "shdfeatures-sapp.c",
        .icon.sokol_default = true
    };
}
