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

#define SOKOL_GL_IMPL
#include "sokol_gl.h"

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
    struct {
        bool enabled;
        bool paused;
        float time_factor;
        double time_sec;
    } animation;
    struct {
        bool enabled;
        bool dbg_draw;
        float latitude;
        float longitude;
        hmm_vec3 dir;   // computed from lat/long
        float intensity;
        hmm_vec3 color;
    } light;
    struct {
        bool enabled;
        hmm_vec3 diffuse;
        hmm_vec3 specular;
        float spec_power;
    } material;
} state = {
    .animation = {
        .enabled = true,
        .time_factor = 1.0f
    },
    .light = {
        .enabled = true,
        .latitude = 45.0f,
        .longitude = -45.0f,
        .intensity = 1.0f,
        .color = {{ 1.0f, 1.0f, 1.0f }},
    },
    .material = {
        .enabled = true,
        .diffuse = {{ 1.0f, 1.0f, 0.5f }},
        .specular = {{ 1.0f, 1.0f, 1.0f }},
        .spec_power = 32.0f
    }
};

// IO buffers (we know the max file sizes upfront)
static uint8_t skel_io_buffer[32 * 1024];
static uint8_t anim_io_buffer[96 * 1024];
static uint8_t mesh_io_buffer[3 * 1024 * 1024];

static void skel_data_loaded(const sfetch_response_t* respone);
static void anim_data_loaded(const sfetch_response_t* respone);
static void mesh_data_loaded(const sfetch_response_t* respone);
static void update_light(void);
static void draw_light_debug(void);
static void draw_ui(void);

static void init(void) {
    // setup sokol-gfx
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });

    // setup sokol-gl
    sgl_setup(&(sgl_desc_t){ .sample_count = sapp_sample_count() });

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
    // viewport is sligth offcenter
    const int vp_x     = (int) (fb_width * 0.3f);
    const int vp_y     = 0;
    const int vp_width = (int) (fb_width * 0.7f);
    const int vp_height = (int) fb_height;

    uint64_t frame_ticks = stm_round_to_common_refresh_rate(stm_laptime(&state.laptime));
    state.frame_time_sec = stm_sec(frame_ticks);
    cam_update(&state.camera, vp_width, vp_height);
    simgui_new_frame(fb_width, fb_height, state.frame_time_sec);

    if (state.light.enabled) {
        update_light();
        if (state.light.dbg_draw) {
            draw_light_debug();
        }
    }

    draw_ui();

    sg_begin_default_pass(&state.pass_action, fb_width, fb_height);
    sg_apply_viewport(vp_x, vp_y, vp_width, vp_height, true);
    if (ozz_all_loaded(state.ozz)) {
        if (state.animation.enabled && !state.animation.paused) {
            state.animation.time_sec += state.frame_time_sec * state.animation.time_factor;
        }
        ozz_update_instance(state.ozz, state.animation.time_sec);
        ozz_update_joint_texture(0);
        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);
        const vs_params_t vs_params = {
            .mvp = state.camera.view_proj,
            .model = HMM_Mat4d(1.0f),
            .joint_uv = HMM_Vec2(ozz_joint_texture_u(state.ozz), ozz_joint_texture_v(state.ozz)),
            .joint_pixel_width = ozz_joint_texture_pixel_width()
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
        const phong_params_t phong_params = {
            .light_dir = state.light.dir,
            .eye_pos = state.camera.eye_pos,
            .light_color = HMM_MultiplyVec3f(state.light.color, state.light.intensity),
            .mat_diffuse = state.material.diffuse,
            .mat_specular = state.material.specular,
            .mat_spec_power = state.material.spec_power
        };
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_phong_params, &SG_RANGE(phong_params));
        sg_draw(0, ozz_num_triangle_indices(state.ozz), 1);
    }
    sgl_draw();
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    ozz_destroy_instance(state.ozz);
    ozz_shutdown();
    simgui_shutdown();
    sfetch_shutdown();
    sgl_shutdown();
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

static void update_light(void) {
    const float lat = HMM_ToRadians(state.light.latitude);
    const float lng = HMM_ToRadians(state.light.longitude);
    state.light.dir = HMM_Vec3(cosf(lat) * sinf(lng), sinf(lat), cosf(lat) * cosf(lng));
}

static void draw_light_debug(void) {
    const float l = 1.0f;
    const float y = 1.0f;
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_load_matrix((const float*)&state.camera.proj);
    sgl_matrix_mode_modelview();
    sgl_load_matrix((const float*)&state.camera.view);
    sgl_c3f(state.light.color.X, state.light.color.Y, state.light.color.Z);
    sgl_begin_lines();
    sgl_v3f(0.0f, y, 0.0f);
    sgl_v3f(state.light.dir.X * l, y + (state.light.dir.Y * l), state.light.dir.Z * l);
    sgl_end();
}

static void draw_ui(void) {
    igSetNextWindowPos((ImVec2){20,20}, ImGuiCond_Once, (ImVec2){0,0});
    igSetNextWindowSize((ImVec2){220,150 }, ImGuiCond_Once);
    if (igBegin("Controls", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ozz_load_failed(state.ozz)) {
            igText("Failed loading character data!");
        }
        else {
            igText("Camera Controls:");
            igText("  LMB + Mouse Move: Look");
            igText("  Mouse Wheel: Zoom");
            igPushIDStr("camera");
            igSliderFloat("Distance", &state.camera.distance, state.camera.min_dist, state.camera.max_dist, "%.1f", 1.0f);
            igSliderFloat("Latitude", &state.camera.latitude, state.camera.min_lat, state.camera.max_lat, "%.1f", 1.0f);
            igSliderFloat("Longitude", &state.camera.longitude, 0.0f, 360.0f, "%.1f", 1.0f);
            igPopID();
            igSeparator();
            igCheckbox("Enable Animation", &state.animation.enabled);
            if (state.animation.enabled) {
                igSeparator();
                igCheckbox("Paused", &state.animation.paused);
                igSliderFloat("Factor", &state.animation.time_factor, 0.0f, 10.0f, "%.1f", 1.0f);
            }
            igSeparator();
            igCheckbox("Enable Lighting", &state.light.enabled);
            if (state.light.enabled) {
                igPushIDStr("light");
                igSeparator();
                igCheckbox("Draw Light Vector", &state.light.dbg_draw);
                igSliderFloat("Latitude", &state.light.latitude, -85.0f, 85.0f, "%.1f", 1.0f);
                igSliderFloat("Longitude", &state.light.longitude, 0.0f, 360.0f, "%.1f", 1.0f);
                igSliderFloat("Intensity", &state.light.intensity, 0.0f, 10.0f, "%.1f", 1.0f);
                igColorEdit3("Color", &state.light.color.X, ImGuiColorEditFlags_None);
                igPopID();
            }
            igSeparator();
            igCheckbox("Enable Material", &state.material.enabled);
            if (state.material.enabled) {
                igPushIDStr("material");
                igSeparator();
                igColorEdit3("Diffuse", &state.material.diffuse.X, ImGuiColorEditFlags_None);
                igColorEdit3("Specular", &state.material.specular.X, ImGuiColorEditFlags_None);
                igSliderFloat("Spec Pwr", &state.material.spec_power, 1.0f, 64.0f, "%.1f", 1.0f);
                igPopID();
            }
        }
    }
    igEnd();
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
