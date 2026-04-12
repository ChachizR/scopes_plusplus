#include "common.cl"

typedef struct __attribute__((packed)) cl_rect_2D {
    int  x;
    int  y;
    uint width;
    uint height;
} cl_rect_2D;

bool isWithin(cl_rect_2D rect, int x, int y) {
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y &&
           y < rect.y + rect.height;
}

inline uchar bin(float value) {
    return (uchar)floor(value * 255.f);
}

__kernel void accumulateWaveforms(
    __global const float* in_rgba,
    __global const float* in_yuv,
    __global uint*        out_hist_rgb,
    __global uint*        out_hist_yuv,
    uint src_width, uint src_height,
    uchar mask_enabled, cl_rect_2D mask_rect,
    RenderFeatureFlags features,
    uint analysis_step) {

    __local uint local_hist_rgb_r[WAVEFORM_BINS]; // 1KB
    __local uint local_hist_rgb_g[WAVEFORM_BINS]; // 1KB
    __local uint local_hist_rgb_b[WAVEFORM_BINS]; // 1KB

    __local uint local_hist_yuv_y[WAVEFORM_BINS]; // 1KB
    __local uint local_hist_yuv_u[WAVEFORM_BINS]; // 1KB
    __local uint local_hist_yuv_v[WAVEFORM_BINS]; // 1KB

    size_t x   = get_global_id(0) * analysis_step;
    size_t y   = get_global_id(1) * analysis_step;
    size_t gid = y * src_width + x;

    size_t yuvPB  = gid * 3;
    size_t rgbaPB = gid * 4;

    size_t local_x = get_local_id(0);
    size_t local_y = get_local_id(1);
    size_t lid     = local_y * get_local_size(0) + local_x;

    // initialize local histogram if thread is responsible for it

    if (lid < WAVEFORM_BINS) {
        local_hist_rgb_r[lid] = 0;
        local_hist_rgb_g[lid] = 0;
        local_hist_rgb_b[lid] = 0;

        local_hist_yuv_y[lid] = 0;
        local_hist_yuv_u[lid] = 0;
        local_hist_yuv_v[lid] = 0;
    }

    // other threads should only start once the local histogram is initialized
    barrier(CLK_LOCAL_MEM_FENCE);

    if (x < src_width && y < src_height) {
        // check if the pixel is within the mask rectangle
        if (mask_enabled == 0 || isWithin(mask_rect, x, y)) {
            // RGB WF, RGB Parade WF, RGB Blacks WF
            if (features & RENDER_FEATURE_ANY_WF_RGB) {
                atomic_inc(&local_hist_rgb_r[bin(in_rgba[rgbaPB + 0])]);
                atomic_inc(&local_hist_rgb_g[bin(in_rgba[rgbaPB + 1])]);
                atomic_inc(&local_hist_rgb_b[bin(in_rgba[rgbaPB + 2])]);
            }

            // Luma WF, YUV Parade
            if (features & RENDER_FEATURE_ANY_WF_YUV) {
                atomic_inc(&local_hist_yuv_y[bin(in_yuv[yuvPB + 0])]);
                atomic_inc(&local_hist_yuv_u[bin(in_yuv[yuvPB + 1])]);
                atomic_inc(&local_hist_yuv_v[bin(in_yuv[yuvPB + 2])]);
            }
        }
    }

    // wait for all threads to finish
    barrier(CLK_LOCAL_MEM_FENCE);

    // accumulate local histogram into global histogram
    if (lid < WAVEFORM_BINS) {
        uint wf_x  = (uint)floor((float)(WF_WIDTH - 1) / (float)src_width * x);
        uint wf_id = lid * WF_WIDTH + wf_x;

        if (features & RENDER_FEATURE_ANY_WF_RGB) {
            atomic_add(&out_hist_rgb[wf_id * 3 + 0], local_hist_rgb_r[lid]);
            atomic_add(&out_hist_rgb[wf_id * 3 + 1], local_hist_rgb_g[lid]);
            atomic_add(&out_hist_rgb[wf_id * 3 + 2], local_hist_rgb_b[lid]);
        }

        if (features & RENDER_FEATURE_ANY_WF_YUV) {
            atomic_add(&out_hist_yuv[wf_id * 3 + 0], local_hist_yuv_y[lid]);
            atomic_add(&out_hist_yuv[wf_id * 3 + 1], local_hist_yuv_u[lid]);
            atomic_add(&out_hist_yuv[wf_id * 3 + 2], local_hist_yuv_v[lid]);
        }
    }
}

__kernel void accumulateUVScope(
    __global const float* in_rgba,
    __global const float* in_yuv,
    __global uint*        out_hist2d_uv_rgba,
    uint                  src_width,
    uint                  src_height,
    uchar                 mask_enabled,
    cl_rect_2D            mask_rect,
    uint                  analysis_step) {

    size_t x = get_global_id(0) * analysis_step;
    size_t y = get_global_id(1) * analysis_step;

    if (x >= src_width || y >= src_height)
        return;

    if (mask_enabled && !isWithin(mask_rect, x, y))
        return;

    uint   src_gid = y * src_width + x;
    size_t rgbaPB  = src_gid * 4;
    size_t yuvPB   = src_gid * 3;

    // uchar R = in_rgba[rgbaPB + 0];
    // uchar G = in_rgba[rgbaPB + 1];
    // uchar B = in_rgba[rgbaPB + 2];

    float U = in_yuv[yuvPB + 1]; // 0-1
    float V = in_yuv[yuvPB + 2]; // 0-1

    uint sc_id = bin(1.f - V) * SC_WIDTH + bin(U);

    // atomic_max(&out_hist2d_uv_rgba[sc_id * 4 + 0], R);
    // atomic_max(&out_hist2d_uv_rgba[sc_id * 4 + 1], G);
    // atomic_max(&out_hist2d_uv_rgba[sc_id * 4 + 2], B);
    atomic_inc(&out_hist2d_uv_rgba[sc_id * 4 + 3]);
}

__kernel void accumulateXYZScope(
    __global const float* in_xyz,
    __global uint*        out_hist2d_xyz_rgba,
    uint                  src_width,
    uint                  src_height,
    uchar                 mask_enabled,
    cl_rect_2D            mask_rect,
    int                   colorspace,
    uint                  analysis_step) {

    size_t x = get_global_id(0) * analysis_step;
    size_t y = get_global_id(1) * analysis_step;

    if (x >= src_width || y >= src_height)
        return;

    if (mask_enabled && !isWithin(mask_rect, x, y))
        return;

    uint   src_gid = y * src_width + x;
    size_t xyzPB   = src_gid * 3;

    float X = in_xyz[xyzPB + 0];
    float Y = in_xyz[xyzPB + 1];
    float Z = in_xyz[xyzPB + 2];

    float XX = X / (X + Y + Z);
    float YY = Y / (X + Y + Z);

    uchar sc_x = bin(XX);
    uchar sc_y = bin(YY);

    uint sc_id = sc_y * SC_WIDTH + sc_x;

    // atomic_max(&out_hist2d_xyz_rgba[sc_id * 4 + 0], R);
    // atomic_max(&out_hist2d_xyz_rgba[sc_id * 4 + 1], G);
    // atomic_max(&out_hist2d_xyz_rgba[sc_id * 4 + 2], B);
    atomic_inc(&out_hist2d_xyz_rgba[sc_id * 4 + 3]);
}

__kernel void accumulateDiaScope(
    __global const float* in_rgba,
    __global uint*        out_hist2d_dia_rgba,
    uint                  src_width,
    uint                  src_height,
    uchar                 mask_enabled,
    cl_rect_2D            mask_rect,
    uint                  analysis_step) {
    size_t x = get_global_id(0) * analysis_step;
    size_t y = get_global_id(1) * analysis_step;

    if (x >= src_width || y >= src_height)
        return;

    if (mask_enabled && !isWithin(mask_rect, x, y))
        return;

    uint   src_gid = y * src_width + x;
    size_t rgbaPB  = src_gid * 4;

    float R = in_rgba[rgbaPB + 0];
    float G = in_rgba[rgbaPB + 1];
    float B = in_rgba[rgbaPB + 2];

    float xGB = -.5f * G + .5f * B + 0.5f;
    float yGB = .25f * G + .25f * B + 0.5f;

    yGB = 1.f - yGB;

    uint sc_id_gb = bin(yGB) * SC_WIDTH + bin(xGB);

    atomic_inc(&out_hist2d_dia_rgba[sc_id_gb * 4 + 3]);

    float xGR = -.5f * G + .5f * R + .5f;
    float yGR = -.25f * G - .25f * R + 0.5f;

    yGR = 1.f - yGR;

    uint sc_id_gr = bin(yGR) * SC_WIDTH + bin(xGR);

    atomic_inc(&out_hist2d_dia_rgba[sc_id_gr * 4 + 3]);
}
