//------------------------------------------------------------------------------
//  uvwrap-wgpu.c
//  Demonstrates and tests texture coordinate wrapping modes.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "wgpu_entry.h"

#define SAMPLE_COUNT (4)

static struct {
    sg_buffer vbuf;
    sg_image img[_SG_WRAP_NUM];
    sg_pipeline pip;
    sg_pass_action pass_action;
} state = {
    .pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = {0.0f, 0.5f, 0.7f, 1.0f } }
    }
};

typedef struct {
    float offset[2];
    float scale[2];
} vs_params_t;

static void init(void) {
    sg_setup(&(sg_desc){ .context = wgpu_get_context() });

    // a quad vertex buffer
    const float quad_vertices[] = {
        -1.0f, +1.0f,
        +1.0f, +1.0f,
        -1.0f, -1.0f,
        +1.0f, -1.0f,
    };
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quad_vertices)
    });

    // one test image per UV-wrap mode
    const uint32_t o = 0xFF555555;
    const uint32_t W = 0xFFFFFFFF;
    const uint32_t R = 0xFF0000FF;
    const uint32_t G = 0xFF00FF00;
    const uint32_t B = 0xFFFF0000;
    const uint32_t test_pixels[8][8] = {
        { R, R, R, R, G, G, G, G },
        { R, o, o, o, o, o, o, G },
        { R, o, o, o, o, o, o, G },
        { R, o, o, W, W, o, o, G },
        { B, o, o, W, W, o, o, R },
        { B, o, o, o, o, o, o, R },
        { B, o, o, o, o, o, o, R },
        { B, B, B, B, R, R, R, R },
    };
    for (int i = SG_WRAP_REPEAT; i <= SG_WRAP_MIRRORED_REPEAT; i++) {
        state.img[i] = sg_make_image(&(sg_image_desc){
            .width = 8,
            .height = 8,
            .wrap_u = (sg_wrap) i,
            .wrap_v = (sg_wrap) i,
            .border_color = SG_BORDERCOLOR_OPAQUE_BLACK,
            .data.subimage[0][0] = SG_RANGE(test_pixels)
        });
    }

    // a shader object
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs = {
            .uniform_blocks[0].size = sizeof(vs_params_t),
            .source =
                "struct vs_params_t {\n"
                "  offset: vec2<f32>;\n"
                "  scale: vec2<f32>;\n"
                "};\n"
                "@group(0) @binding(0) var<uniform> params: vs_params_t;\n"
                "struct vs_out_t {\n"
                "  @builtin(position) pos: vec4<f32>;\n"
                "  @location(0) uv: vec2<f32>;\n"
                "};\n"
                "@stage(vertex) fn main(@location(0) pos: vec4<f32>) -> vs_out_t {\n"
                "  var vs_out: vs_out_t;\n"
                "  vs_out.pos = vec4<f32>(pos.xy * params.scale + params.offset, 0.5, 1.0);\n"
                "  vs_out.uv = (pos.xy + vec2<f32>(1.0)) - vec2<f32>(0.5);\n"
                "  return vs_out;\n"
                "}\n"
        },
        .fs = {
            .images[0] = {
                .image_type = SG_IMAGETYPE_2D,
                .sampler_type = SG_SAMPLERTYPE_FLOAT
            },
            .source =
                "@group(2) @binding(0) var tex: texture_2d<f32>;\n"
                "@group(2) @binding(1) var smp: sampler;\n"
                "@stage(fragment) fn main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {\n"
                "  return textureSample(tex, smp, uv);\n"
                "}\n",
        }
    });

    // a pipeline state object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {
            .attrs[0].format = SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
    });
}

static void frame(void) {
    sg_begin_default_pass(&state.pass_action, wgpu_width(), wgpu_height());
    sg_apply_pipeline(state.pip);
    for (int i = SG_WRAP_REPEAT; i <= SG_WRAP_MIRRORED_REPEAT; i++) {
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = state.vbuf,
            .fs_images[0] = state.img[i]
        });
        float x_offset = 0, y_offset = 0;
        switch (i) {
            case SG_WRAP_REPEAT:            x_offset = -0.5f; y_offset = 0.5f; break;
            case SG_WRAP_CLAMP_TO_EDGE:     x_offset = +0.5f; y_offset = 0.5f; break;
            case SG_WRAP_CLAMP_TO_BORDER:   x_offset = -0.5f; y_offset = -0.5f; break;
            case SG_WRAP_MIRRORED_REPEAT:   x_offset = +0.5f; y_offset = -0.5f; break;
        }
        vs_params_t vs_params = {
            .offset = { x_offset, y_offset },
            .scale = { 0.4f, 0.4f }
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
        sg_draw(0, 4, 1);
    }
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .sample_count = SAMPLE_COUNT,
        .width = 640,
        .height = 480,
        .title = "uvwrap-wgpu"
    });
    return 0;
}
