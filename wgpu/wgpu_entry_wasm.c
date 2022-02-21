/* WASM specific WGPU demo scaffold functions */
#if !defined(__EMSCRIPTEN__)
#error "please compile this file in Emscripten!"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "sokol_gfx.h"
#include "wgpu_entry.h"

static struct {
    int frame_state;
} emsc;

static void emsc_update_canvas_size(void) {
    double w, h;
    emscripten_get_element_css_size("canvas", &w, &h);
    emscripten_set_canvas_element_size("canvas", w, h);
    wgpu_state.width = (int) w;
    wgpu_state.height = (int) h;
    printf("canvas size updated: %d %d\n", wgpu_state.width, wgpu_state.height);
}

static EM_BOOL emsc_size_changed(int event_type, const EmscriptenUiEvent* ui_event, void* user_data) {
    (void)event_type;
    (void)ui_event;
    (void)user_data;
    emsc_update_canvas_size();
    return true;
}
static void wgpu_request_device_cb(WGPURequestDeviceStatus status, WGPUDevice dev, const char* message, void* userdata) {
    (void)userdata;
    if (message) {
        printf("wgpuInstanceRequestDevice: %s\n", message);
    }
    assert(status == WGPURequestDeviceStatus_Success);
    wgpu_state.dev = dev;
}

static void wgpu_request_adapter_cb(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata) {
    (void)userdata;
    if (message) {
        printf("wgpuInstanceRequestAdapter: %s\n", message);
    }
    if (status == WGPURequestAdapterStatus_Unavailable) {
        printf("WebGPU unavailable; exiting cleanly\n");
        exit(0);
    }
    assert(status == WGPURequestAdapterStatus_Success);
    wgpu_state.adapter = adapter;
    wgpuAdapterRequestDevice(adapter, 0, wgpu_request_device_cb, 0);
}

static void wgpu_async_setup(void) {
    wgpuInstanceRequestAdapter(0, 0, wgpu_request_adapter_cb, 0);
}

static void wgpu_create_swapchain(void) {
    const WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {
        .chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector,
        .selector = "#canvas",
    };
    const WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = &canvas_desc.chain,
    };
    WGPUSurface surf = wgpuInstanceCreateSurface(0, &surf_desc);
    assert(surf);
    wgpu_state.render_format = wgpuSurfaceGetPreferredFormat(surf, wgpu_state.adapter);

    WGPUSwapChainDescriptor swapchain_desc = {
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = wgpu_state.render_format,
        .width = (uint32_t)wgpu_state.width,
        .height = (uint32_t)wgpu_state.height,
        .presentMode = WGPUPresentMode_Fifo,
    };
    wgpu_state.swapchain = wgpuDeviceCreateSwapChain(wgpu_state.dev, surf, &swapchain_desc);
    assert(wgpu_state.swapchain); 
}

static EM_BOOL wgpu_emsc_frame(double time, void* user_data) {
    (void)time;
    (void)user_data;
    switch (emsc.frame_state) {
        case 0:
            wgpu_async_setup();
            emsc.frame_state = 1;
            break;
        case 1:
            if (wgpu_state.dev) {
                wgpu_create_swapchain();
                wgpu_swapchain_init();
                wgpu_state.desc.init_cb();
                emsc.frame_state = 2;
            }
            break;
        case 2:
            wgpu_swapchain_next_frame();
            wgpu_state.desc.frame_cb();
            break;
    }
    return EM_TRUE;
}

void wgpu_platform_start(const wgpu_desc_t* desc) {
    (void)desc;
    emsc_update_canvas_size();
    emscripten_set_resize_callback("canvas", 0, false, emsc_size_changed);
    emscripten_request_animation_frame_loop(wgpu_emsc_frame, 0);
}
