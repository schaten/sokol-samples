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

// include code-generated stamped out shader variations
#include "shdfeatures-sapp.glsl.slm.h"
#include "shdfeatures-sapp.glsl.sl.h"
#include "shdfeatures-sapp.glsl.s.h"
#include "shdfeatures-sapp.glsl.sm.h"
#include "shdfeatures-sapp.glsl.lm.h"
#include "shdfeatures-sapp.glsl.m.h"

// shader variation feature flags
#define SHD_SKIN        (1<<0)  // skinning is enabled
#define SHD_LIGHT       (1<<1)  // lighting is enabled
#define SHD_MATERIAL    (1<<2)  // material attributes are enabled
#define MAX_SHADER_VARIATIONS  (1<<3)
#define MAX_VERTEX_COMPONENTS (4)
#define MAX_UNIFORMBLOCK_SIZE (256)

// these are 'pointerized uniformblob structs' filled at runtime from shader reflection
// information, if a pointer is null, the uniform block item doesn't exist in
// this shader variation
//
// if the pointer is valid, it points into generic byte blob structs
typedef struct {
    bool valid;
    sg_shader_stage stage;
    int ub_index;
    size_t num_bytes;
    hmm_mat4* mvp;
    hmm_mat4* model;
    hmm_vec2* joint_uv;
    float* joint_pixel_width;
} vs_params_ptr_t;

typedef struct {
    bool valid;
    sg_shader_stage stage;
    int ub_index;
    size_t num_bytes;
    hmm_vec3* light_dir;
    hmm_vec3* eye_pos;
    hmm_vec3* light_color;
    hmm_vec3* mat_diffuse;
    hmm_vec3* mat_specular;
    float* mat_spec_power;
} phong_params_ptr_t;

// a struct describing a stamped out shader variation
typedef struct {
    bool valid;
    sg_pipeline pip;
    sg_bindings bind;

    // pointerized uniform block structs, filled from runtime reflection data
    vs_params_ptr_t vs_params;
    phong_params_ptr_t phong_params;

    // shader reflection functions
    const sg_shader_desc* (*shader_desc_fn)(sg_backend backend);
    int (*attr_index_fn)(const char* attr_name);
    int (*image_index_fn)(sg_shader_stage stage, const char* img_name);
    int (*uniformblock_index_fn)(sg_shader_stage stage, const char* ub_name);
    int (*uniformblock_size_fn)(sg_shader_stage stage, const char* ub_name);
    int (*uniform_offset_fn)(sg_shader_stage stage, const char* ub_name, const char* u_name);
    sg_shader_uniform_desc (*uniform_desc_fn)(sg_shader_stage stage, const char* ub_name, const char* u_name);
} shader_variation_t;

// a helper struct to describe a dynamically looked up vertex component
typedef struct {
    const char* name;
    sg_vertex_format format;
    int offset;
} vertex_component_t;

// global state
static struct {
    sg_pass_action pass_action;
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
    shader_variation_t variations[MAX_SHADER_VARIATIONS];
    vertex_component_t vertex_components[MAX_VERTEX_COMPONENTS];
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
    },

    // initialize the shader variation function table with valid shader-feature combinations
    .variations = {
        [SHD_SKIN|SHD_LIGHT|SHD_MATERIAL] = {
            .valid = true,
            .shader_desc_fn = slm_prog_shader_desc,
            .attr_index_fn = slm_prog_attr_index,
            .image_index_fn = slm_prog_image_index,
            .uniformblock_index_fn = slm_prog_uniformblock_index,
            .uniformblock_size_fn = slm_prog_uniformblock_size,
            .uniform_offset_fn = slm_prog_uniform_offset,
            .uniform_desc_fn = slm_prog_uniform_desc,
        },
        [SHD_SKIN|SHD_LIGHT] = {
            .valid = true,
            .shader_desc_fn = sl_prog_shader_desc,
            .attr_index_fn = sl_prog_attr_index,
            .image_index_fn = sl_prog_image_index,
            .uniformblock_index_fn = sl_prog_uniformblock_index,
            .uniformblock_size_fn = sl_prog_uniformblock_size,
            .uniform_offset_fn = sl_prog_uniform_offset,
            .uniform_desc_fn = sl_prog_uniform_desc,
        },
        [SHD_SKIN] = {
            .valid = true,
            .shader_desc_fn = s_prog_shader_desc,
            .attr_index_fn = s_prog_attr_index,
            .image_index_fn = s_prog_image_index,
            .uniformblock_index_fn = s_prog_uniformblock_index,
            .uniformblock_size_fn = s_prog_uniformblock_size,
            .uniform_offset_fn = s_prog_uniform_offset,
            .uniform_desc_fn = s_prog_uniform_desc,
        },
        [SHD_SKIN|SHD_MATERIAL] = {
            .valid = true,
            .shader_desc_fn = sm_prog_shader_desc,
            .attr_index_fn = sm_prog_attr_index,
            .image_index_fn = sm_prog_image_index,
            .uniformblock_index_fn = sm_prog_uniformblock_index,
            .uniformblock_size_fn = sm_prog_uniformblock_size,
            .uniform_offset_fn = sm_prog_uniform_offset,
            .uniform_desc_fn = sm_prog_uniform_desc,
        },
        [SHD_LIGHT|SHD_MATERIAL] = {
            .valid = true,
            .shader_desc_fn = lm_prog_shader_desc,
            .attr_index_fn = lm_prog_attr_index,
            .image_index_fn = lm_prog_image_index,
            .uniformblock_index_fn = lm_prog_uniformblock_index,
            .uniformblock_size_fn = lm_prog_uniformblock_size,
            .uniform_offset_fn = lm_prog_uniform_offset,
            .uniform_desc_fn = lm_prog_uniform_desc,
        },
        [SHD_MATERIAL] = {
            .valid = true,
            .shader_desc_fn = m_prog_shader_desc,
            .attr_index_fn = m_prog_attr_index,
            .image_index_fn = m_prog_image_index,
            .uniformblock_index_fn = m_prog_uniformblock_index,
            .uniformblock_size_fn = m_prog_uniformblock_size,
            .uniform_offset_fn = m_prog_uniform_offset,
            .uniform_desc_fn = m_prog_uniform_desc,
        },
    },

    // a lookup table for looking up vertex attributes by name
    .vertex_components = {
        { .name = "position",   .format = SG_VERTEXFORMAT_FLOAT3,   .offset = offsetof(ozz_vertex_t, position) },
        { .name = "normal",     .format = SG_VERTEXFORMAT_BYTE4N,   .offset = offsetof(ozz_vertex_t, normal) },
        { .name = "jindices",   .format = SG_VERTEXFORMAT_UBYTE4N,  .offset = offsetof(ozz_vertex_t, joint_indices) },
        { .name = "jweights",   .format = SG_VERTEXFORMAT_UBYTE4N,  .offset = offsetof(ozz_vertex_t, joint_weights) },
    }
};

// IO buffers (we know the max file sizes upfront)
static uint8_t skel_io_buffer[32 * 1024];
static uint8_t anim_io_buffer[96 * 1024];
static uint8_t mesh_io_buffer[3 * 1024 * 1024];

// uniform data buffers
static uint8_t vs_params_buffer[MAX_UNIFORMBLOCK_SIZE];
static uint8_t phong_params_buffer[MAX_UNIFORMBLOCK_SIZE];

static void skel_data_loaded(const sfetch_response_t* response);
static void anim_data_loaded(const sfetch_response_t* response);
static void mesh_data_loaded(const sfetch_response_t* response);
static void update_light(void);
static void draw_light_debug(void);
static void draw_ui(void);
static sg_layout_desc vertex_layout_for_variation(const shader_variation_t* var);
static void fill_vs_params(const shader_variation_t* var);
static void fill_phong_params(const shader_variation_t* var);
static hmm_mat4* uniform_ptr_mat4(const shader_variation_t* var, uint8_t* base_ptr, sg_shader_stage stage, const char* ub_name, const char* u_name);
static hmm_vec2* uniform_ptr_vec2(const shader_variation_t* var, uint8_t* base_ptr, sg_shader_stage stage, const char* ub_name, const char* u_name);
static hmm_vec3* uniform_ptr_vec3(const shader_variation_t* var, uint8_t* base_ptr, sg_shader_stage stage, const char* ub_name, const char* u_name);
static float* uniform_ptr_float(const shader_variation_t* var, uint8_t* base_ptr, sg_shader_stage stage, const char* ub_name, const char* u_name);

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
    state.ozz = ozz_create_instance(0);

    // create per-shader-variation objects
    for (int i = 0; i < MAX_SHADER_VARIATIONS; i++) {
        shader_variation_t* var = &state.variations[i];
        if (!var->valid) {
            continue;
        }

        // check if the shader variation needs the joint texture
        const int tex_slot = var->image_index_fn(SG_SHADERSTAGE_VS, "joint_tex");
        if (tex_slot >= 0) {
            var->bind.vs_images[tex_slot] = ozz_joint_texture();
        }

        // fill the pointerized uniform-block structs, a pointer will be null
        // if the shader variation doesn't have that uniform block item
        if (var->uniformblock_index_fn(SG_SHADERSTAGE_VS, "vs_params") >= 0) {
            vs_params_ptr_t* p = &var->vs_params;
            uint8_t* base_ptr = vs_params_buffer;
            p->valid = true;
            p->stage = SG_SHADERSTAGE_VS;
            p->ub_index = var->uniformblock_index_fn(p->stage, "vs_params");
            p->num_bytes = (size_t)var->uniformblock_size_fn(p->stage, "vs_params");
            assert(p->num_bytes <= MAX_UNIFORMBLOCK_SIZE);
            p->mvp = uniform_ptr_mat4(var, base_ptr, p->stage, "vs_params", "mvp");
            p->model = uniform_ptr_mat4(var, base_ptr, p->stage, "vs_params", "model");
            p->joint_uv = uniform_ptr_vec2(var, base_ptr, p->stage, "vs_params", "joint_uv");
            p->joint_pixel_width = uniform_ptr_float(var, base_ptr, p->stage, "vs_params", "joint_pixel_width");
        }
        if (var->uniformblock_index_fn(SG_SHADERSTAGE_FS, "phong_params") >= 0) {
            phong_params_ptr_t* p = &var->phong_params;
            uint8_t* base_ptr = phong_params_buffer;
            p->valid = true;
            p->stage = SG_SHADERSTAGE_FS;
            p->ub_index = var->uniformblock_index_fn(p->stage, "phong_params");
            p->num_bytes = (size_t)var->uniformblock_size_fn(SG_SHADERSTAGE_FS, "phong_params");
            assert(p->num_bytes <= MAX_UNIFORMBLOCK_SIZE);
            p->light_dir = uniform_ptr_vec3(var, base_ptr, SG_SHADERSTAGE_FS, "phong_params", "light_dir");
            p->eye_pos = uniform_ptr_vec3(var, base_ptr, SG_SHADERSTAGE_FS, "phong_params", "eye_pos");
            p->light_color = uniform_ptr_vec3(var, base_ptr, SG_SHADERSTAGE_FS, "phong_params", "light_color");
            p->mat_diffuse = uniform_ptr_vec3(var, base_ptr, SG_SHADERSTAGE_FS, "phong_params", "mat_diffuse");
            p->mat_specular = uniform_ptr_vec3(var, base_ptr, SG_SHADERSTAGE_FS, "phong_params", "mat_specular");
            p->mat_spec_power = uniform_ptr_float(var, base_ptr, SG_SHADERSTAGE_FS, "phong_params", "mat_spec_power");
        }


        // create shader and pipeline object, note that the shader and
        // vertex layout depend on the current shader variation
        var->pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader = sg_make_shader(var->shader_desc_fn(sg_query_backend())),
            .layout = vertex_layout_for_variation(var),
            .index_type = SG_INDEXTYPE_UINT16,
            .face_winding = SG_FACEWINDING_CCW,
            .cull_mode = SG_CULLMODE_BACK,
            .depth = {
                .write_enabled = true,
                .compare = SG_COMPAREFUNC_LESS_EQUAL
            }
        });
    }

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
        ozz_update_joint_texture();

        uint8_t var_mask = 0;
        if (state.animation.enabled) {
            var_mask |= SHD_SKIN;
        }
        if (state.light.enabled) {
            var_mask |= SHD_LIGHT;
        }
        if (state.material.enabled) {
            var_mask |= SHD_MATERIAL;
        }

        const shader_variation_t* var = &state.variations[var_mask];
        assert(var->valid);

        sg_apply_pipeline(var->pip);
        sg_apply_bindings(&var->bind);

        // update uniform block data as required by the current shader variation
        if (var->vs_params.valid) {
            fill_vs_params(var);
            sg_apply_uniforms(var->vs_params.stage, var->vs_params.ub_index, &(sg_range){vs_params_buffer, var->vs_params.num_bytes});
        }
        if (var->phong_params.valid) {
            fill_phong_params(var);
            sg_apply_uniforms(var->phong_params.stage, var->phong_params.ub_index, &(sg_range){phong_params_buffer, var->phong_params.num_bytes});
        }

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
        for (int i = 0; i < MAX_SHADER_VARIATIONS; i++) {
            if (state.variations[i].valid) {
                state.variations[i].bind.vertex_buffers[0] = ozz_vertex_buffer(state.ozz);
                state.variations[i].bind.index_buffer = ozz_index_buffer(state.ozz);
            }
        }
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

// helper function to draw the light vector
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

// helper functions to build a matching vertex layout for a shader variation
static sg_layout_desc vertex_layout_for_variation(const shader_variation_t* var) {
    assert(var);

    // buffer stride must be provided, because the vertex layout may have gaps
    sg_layout_desc desc = {
        .buffers[0].stride = sizeof(ozz_vertex_t)
    };

    // populate the vertex attribute description depending on what
    // vertex attributes the shader variation requires
    for (int i = 0; i < MAX_VERTEX_COMPONENTS; i++) {
        const vertex_component_t* comp = &state.vertex_components[i];
        if (comp->name) {
            const int slot = var->attr_index_fn(comp->name);
            if (slot >= 0) {
                desc.attrs[slot] = (sg_vertex_attr_desc) {
                    .format = comp->format,
                    .offset = comp->offset
                };
            }
        }
    }
    return desc;
}

// helper functions to fill uniform block data into generic byte buffer
static void fill_vs_params(const shader_variation_t* var) {
    assert(var);
    const vs_params_ptr_t* p = &var->vs_params;
    if (p->mvp) {
        *p->mvp = state.camera.view_proj;
    }
    if (p->model) {
        *p->model = HMM_Mat4d(1.0f);
    }
    if (p->joint_uv) {
        *p->joint_uv = HMM_Vec2(ozz_joint_texture_u(state.ozz), ozz_joint_texture_v(state.ozz));
    }
    if (p->joint_pixel_width) {
        *p->joint_pixel_width = ozz_joint_texture_pixel_width();
    }
}

static void fill_phong_params(const shader_variation_t* var) {
    const phong_params_ptr_t* p = &var->phong_params;
    if (p->light_dir) {
        *p->light_dir = state.light.dir;
    }
    if (p->eye_pos) {
        *p->eye_pos = state.camera.eye_pos;
    }
    if (p->light_color) {
        *p->light_color = HMM_MultiplyVec3f(state.light.color, state.light.intensity);
    }
    if (p->mat_diffuse) {
        *p->mat_diffuse = state.material.diffuse;
    }
    if (p->mat_specular) {
        *p->mat_specular = state.material.specular;
    }
    if (p->mat_spec_power) {
        *p->mat_spec_power = state.material.spec_power;
    }
}

// typesafe helper function to dynamically resolve a pointer to a uniform-block item,
// returns a nullptr if the item doesn't exist, assert if the type doesn't match
static uint8_t* uniform_ptr(const shader_variation_t* var, uint8_t* base_ptr, sg_uniform_type expected_type, sg_shader_stage stage, const char* ub_name, const char* u_name) {
    assert(var && base_ptr);
    int offset = var->uniform_offset_fn(stage, ub_name, u_name);
    if (offset < 0) {
        return 0;
    }
    assert(var->uniform_desc_fn(stage, ub_name, u_name).type == expected_type);
    return base_ptr + offset;
}

static hmm_mat4* uniform_ptr_mat4(const shader_variation_t* var, uint8_t* base_ptr, sg_shader_stage stage, const char* ub_name, const char* u_name) {
    return (hmm_mat4*) uniform_ptr(var, base_ptr, SG_UNIFORMTYPE_MAT4, stage, ub_name, u_name);
}

static hmm_vec2* uniform_ptr_vec2(const shader_variation_t* var, uint8_t* base_ptr, sg_shader_stage stage, const char* ub_name, const char* u_name) {
    return (hmm_vec2*) uniform_ptr(var, base_ptr, SG_UNIFORMTYPE_FLOAT2, stage, ub_name, u_name);
}

static hmm_vec3* uniform_ptr_vec3(const shader_variation_t* var, uint8_t* base_ptr, sg_shader_stage stage, const char* ub_name, const char* u_name) {
    return (hmm_vec3*) uniform_ptr(var, base_ptr, SG_UNIFORMTYPE_FLOAT3, stage, ub_name, u_name);
}

static float* uniform_ptr_float(const shader_variation_t* var, uint8_t* base_ptr, sg_shader_stage stage, const char* ub_name, const char* u_name) {
    return (float*) uniform_ptr(var, base_ptr, SG_UNIFORMTYPE_FLOAT, stage, ub_name, u_name);
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
            igSliderFloat("Distance", &state.camera.distance, state.camera.min_dist, state.camera.max_dist, "%.1f", ImGuiSliderFlags_None);
            igSliderFloat("Latitude", &state.camera.latitude, state.camera.min_lat, state.camera.max_lat, "%.1f", ImGuiSliderFlags_None);
            igSliderFloat("Longitude", &state.camera.longitude, 0.0f, 360.0f, "%.1f", ImGuiSliderFlags_None);
            igPopID();
            igSeparator();
            igCheckbox("Enable Animation", &state.animation.enabled);
            if (state.animation.enabled) {
                igSeparator();
                igCheckbox("Paused", &state.animation.paused);
                igSliderFloat("Factor", &state.animation.time_factor, 0.0f, 10.0f, "%.1f", ImGuiSliderFlags_None);
            }
            igSeparator();
            igCheckbox("Enable Lighting", &state.light.enabled);
            if (state.light.enabled) {
                igPushIDStr("light");
                igSeparator();
                igCheckbox("Draw Light Vector", &state.light.dbg_draw);
                igSliderFloat("Latitude", &state.light.latitude, -85.0f, 85.0f, "%.1f", ImGuiSliderFlags_None);
                igSliderFloat("Longitude", &state.light.longitude, 0.0f, 360.0f, "%.1f", ImGuiSliderFlags_None);
                igSliderFloat("Intensity", &state.light.intensity, 0.0f, 10.0f, "%.1f", ImGuiSliderFlags_None);
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
                igSliderFloat("Spec Pwr", &state.material.spec_power, 1.0f, 64.0f, "%.1f", ImGuiSliderFlags_None);
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
