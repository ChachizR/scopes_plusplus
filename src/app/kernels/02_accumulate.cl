#define WF_WIDTH 580
#define WF_HEIGHT 256
#define SC_WIDTH 256
#define SC_HEIGHT 256

#define WAVEFORM_BINS 256

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

__kernel void accumulateWaveforms(__global const uchar* in_rgba,
                                  __global const uchar* in_yuv,
                                  __global uint*        out_hist_rgb,
                                  __global uint*        out_hist_yuv,
                                  uint src_width, uint src_height,
                                  uchar mask_enabled, cl_rect_2D mask_rect) {

    __local uint local_hist_rgb_r[WAVEFORM_BINS]; // 1KB
    __local uint local_hist_rgb_g[WAVEFORM_BINS]; // 1KB
    __local uint local_hist_rgb_b[WAVEFORM_BINS]; // 1KB

    __local uint local_hist_yuv_y[WAVEFORM_BINS]; // 1KB
    __local uint local_hist_yuv_u[WAVEFORM_BINS]; // 1KB
    __local uint local_hist_yuv_v[WAVEFORM_BINS]; // 1KB

    size_t x   = get_global_id(0);
    size_t y   = get_global_id(1);
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
            atomic_inc(&local_hist_rgb_r[in_rgba[rgbaPB + 0]]);
            atomic_inc(&local_hist_rgb_g[in_rgba[rgbaPB + 1]]);
            atomic_inc(&local_hist_rgb_b[in_rgba[rgbaPB + 2]]);

            // Luma WF, YUV Parade
            atomic_inc(&local_hist_yuv_y[in_yuv[yuvPB + 0]]);
            atomic_inc(&local_hist_yuv_u[in_yuv[yuvPB + 1]]);
            atomic_inc(&local_hist_yuv_v[in_yuv[yuvPB + 2]]);
        }
    }

    // wait for all threads to finish
    barrier(CLK_LOCAL_MEM_FENCE);

    // accumulate local histogram into global histogram
    if (lid < WAVEFORM_BINS) {
        uint wf_x  = (uint)floor((float)(WF_WIDTH - 1) / (float)src_width * x);
        uint wf_id = lid * WF_WIDTH + wf_x;

        atomic_add(&out_hist_rgb[wf_id * 3 + 0], local_hist_rgb_r[lid]);
        atomic_add(&out_hist_rgb[wf_id * 3 + 1], local_hist_rgb_g[lid]);
        atomic_add(&out_hist_rgb[wf_id * 3 + 2], local_hist_rgb_b[lid]);

        atomic_add(&out_hist_yuv[wf_id * 3 + 0], local_hist_yuv_y[lid]);
        atomic_add(&out_hist_yuv[wf_id * 3 + 1], local_hist_yuv_u[lid]);
        atomic_add(&out_hist_yuv[wf_id * 3 + 2], local_hist_yuv_v[lid]);
    }
}