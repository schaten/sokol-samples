//------------------------------------------------------------------------------
//  ozzutil.cc
//------------------------------------------------------------------------------
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/util/mesh.h"

#include "ozzutil.h"

static struct {
    bool valid;
    ozz_desc_t desc;
    int joint_texture_width;    // in number of pixels
    int joint_texture_height;   // in number of pixels
    int joint_texture_pitch;    // in number of floats
    sg_image joint_texture;
} state;

struct ozz_private_t {
    int index;
    ozz::animation::Skeleton skel;
    ozz::animation::Animation anim;
    ozz::vector<uint16_t> joint_remaps;
    ozz::vector<ozz::math::Float4x4> mesh_inverse_bindposes;
    ozz::vector<ozz::math::SoaTransform> local_matrices;
    ozz::vector<ozz::math::Float4x4> local_matrixes;
    ozz::animation::SamplingCache cache;
    sg_buffer vbuf = { };
    sg_buffer ibuf = { };
    bool skel_loaded = false;
    bool anim_loaded = false;
    bool mesh_loaded = false;
    bool load_failed = false;
};

void ozz_setup(const ozz_desc_t* desc) {
    assert(!state.valid);
    assert(desc);
    assert(desc->max_palette_joints > 0);
    assert(desc->max_instances > 0);

    state.valid = true;
    state.desc = *desc;
    state.joint_texture_width = desc->max_palette_joints * 3;
    state.joint_texture_height = desc->max_instances;
    state.joint_texture_pitch = state.joint_texture_width * 4;

    sg_image_desc img_desc = { };
    img_desc.width = state.joint_texture_width;
    img_desc.height = state.joint_texture_height;
    img_desc.num_mipmaps = 1;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA32F;
    img_desc.usage = SG_USAGE_STREAM;
    img_desc.min_filter = SG_FILTER_NEAREST;
    img_desc.mag_filter = SG_FILTER_NEAREST;
    img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    state.joint_texture = sg_make_image(&img_desc);
}

void ozz_shutdown(void) {
    assert(state.valid);
    // it's ok to call sg_destroy_image with an invalid id
    sg_destroy_image(state.joint_texture);
    state.valid = false;
}

sg_image ozz_joint_texture(void) {
    assert(state.valid);
    return state.joint_texture;
}

ozz_instance_t* ozz_create_instance(int index) {
    assert(state.valid);
    ozz_private_t* self = new ozz_private_t();
    self->index = index;
    return (ozz_instance_t*) self;
}

void ozz_destroy_instance(ozz_instance_t* ozz) {
    assert(state.valid && ozz);
    ozz_private_t* self = (ozz_private_t*)ozz;
    // it's ok to call sg_destroy_buffer with an invalid id    
    sg_destroy_buffer(self->vbuf);
    sg_destroy_buffer(self->ibuf);
    delete self;
}

void ozz_load_skeleton(ozz_instance_t* ozz, const void* data, size_t num_bytes) {
    assert(state.valid && ozz && data && (num_bytes > 0));
    // FIXME
}

void ozz_load_animation(ozz_instance_t* ozz, const void* data, size_t num_bytes) {
    assert(state.valid && ozz && data && (num_bytes > 0));
    // FIXME
}

void ozz_load_mesh(ozz_instance_t* ozz, const void* data, size_t num_bytes) {
    assert(state.valid && ozz && data && (num_bytes > 0));
    // FIXME
}

void ozz_set_load_failed(ozz_instance_t* ozz) {
    assert(state.valid && ozz);
    ozz_private_t* self = (ozz_private_t*)ozz;
    self->load_failed = true;
}

bool ozz_all_loaded(ozz_instance_t* ozz) {
    assert(state.valid && ozz);
    ozz_private_t* self = (ozz_private_t*)ozz;
    return self->skel_loaded && self->anim_loaded && self->mesh_loaded && !self->load_failed;
}
