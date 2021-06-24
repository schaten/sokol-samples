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

#include "shdfeatures-sapp.glsl.h"

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    camera_t camera;
    ozz_instance_t* ozz;
    uint64_t laptime;
    double frame_time_sec;
    double abs_time_sec;
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
    state.bind.vs_images[SLOT_joint_tex] = ozz_joint_texture();
    state.ozz = ozz_create_instance(0);

    // create shader and pipeline state object (FIXME: per shader-feature combination)
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(skinned_shader_desc(sg_query_backend())),
        .layout.attrs = {
            [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
            [ATTR_vs_normal].format = SG_VERTEXFORMAT_BYTE4N,
            [ATTR_vs_jindices].format = SG_VERTEXFORMAT_UBYTE4N,
            [ATTR_vs_jweights].format = SG_VERTEXFORMAT_UBYTE4N
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .face_winding = SG_FACEWINDING_CCW,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL
        }
    });

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
    if (ozz_all_loaded(state.ozz)) {
        state.abs_time_sec += state.frame_time_sec;
        ozz_update_instance(state.ozz, state.abs_time_sec);
        ozz_update_joint_texture(0);
        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);
        const vs_params_t vs_params = {
            .mvp = state.camera.view_proj,
            .model = HMM_Mat4d(1.0f),
            .joint_uv = HMM_Vec2(ozz_joint_texture_u(state.ozz), ozz_joint_texture_v(state.ozz)),
            .joint_pixel_width = ozz_joint_texture_pixel_width()
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, SG_RANGE_REF(vs_params));
        sg_draw(0, ozz_num_triangle_indices(state.ozz), 1);
    }
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
        state.bind.vertex_buffers[0] = ozz_vertex_buffer(state.ozz);
        state.bind.index_buffer = ozz_index_buffer(state.ozz);
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
