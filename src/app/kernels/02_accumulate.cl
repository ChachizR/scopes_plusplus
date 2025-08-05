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

__kernel void accumulateWaveforms(
    __global const uchar* in_rgba,
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

void rgbToXYZ(uchar r, uchar g, uchar b, int colorspace, float* X, float* Y, float* Z) {
    float r_f = r / 255.0f;
    float g_f = g / 255.0f;
    float b_f = b / 255.0f;

    if (colorspace == CS_BT601) {
        *X = r_f * 0.14165220f + g_f * 0.11236989f + b_f * 0.05867791f;
        *Y = r_f * 0.07303942f + g_f * 0.23248942f + b_f * 0.02347116f;
        *Z = r_f * 0.00663995f + g_f * 0.04262306f + b_f * 0.30903699f;
    } else if (colorspace == CS_BT709) {
        *X = r_f * 0.13567657f + g_f * 0.11764525f + b_f * 0.05937818f;
        *Y = r_f * 0.06995823f + g_f * 0.23529050f + b_f * 0.02375127f;
        *Z = r_f * 0.00635984f + g_f * 0.03921508f + b_f * 0.31272508f;
    } else if (colorspace == CS_BT2020) {
        *X = r_f * 0.20955920f + g_f * 0.04757896f + b_f * 0.05556184f;
        *Y = r_f * 0.08642837f + g_f * 0.22306137f + b_f * 0.01951026f;
        *Z = r_f * 0.00000000f + g_f * 0.00923592f + b_f * 0.34906408f;
    }
}

__kernel void accumulateUVScope(
    __global const uchar* in_rgba,
    __global const uchar* in_yuv,
    __global uint*        out_hist2d_uv_rgba,
    uint                  src_width,
    uint                  src_height,
    uchar                 mask_enabled,
    cl_rect_2D            mask_rect) {

    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

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

    uchar U = in_yuv[yuvPB + 1];
    uchar V = in_yuv[yuvPB + 2];

    uint sc_id = (255 - V) * SC_WIDTH + U;

    // atomic_max(&out_hist2d_uv_rgba[sc_id * 4 + 0], R);
    // atomic_max(&out_hist2d_uv_rgba[sc_id * 4 + 1], G);
    // atomic_max(&out_hist2d_uv_rgba[sc_id * 4 + 2], B);
    atomic_inc(&out_hist2d_uv_rgba[sc_id * 4 + 3]);
}

__kernel void accumulateXYZScope(
    __global const uchar* in_rgba,
    __global uint*        out_hist2d_xyz_rgba,
    uint                  src_width,
    uint                  src_height,
    uchar                 mask_enabled,
    cl_rect_2D            mask_rect,
    int                   colorspace) {

    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    if (x >= src_width || y >= src_height)
        return;

    if (mask_enabled && !isWithin(mask_rect, x, y))
        return;

    uint   src_gid = y * src_width + x;
    size_t rgbaPB  = src_gid * 4;

    uchar R = in_rgba[rgbaPB + 0];
    uchar G = in_rgba[rgbaPB + 1];
    uchar B = in_rgba[rgbaPB + 2];

    float X, Y, Z;
    rgbToXYZ(R, G, B, colorspace, &X, &Y, &Z);

    float XX = X / (X + Y + Z);
    float YY = Y / (X + Y + Z);

    uint sc_x = (uint)floor((float)(SC_WIDTH - 1) * XX);
    uint sc_y = (uint)floor((float)(SC_HEIGHT - 1) * YY);

    uint sc_id = sc_y * SC_WIDTH + sc_x;

    // atomic_max(&out_hist2d_xyz_rgba[sc_id * 4 + 0], R);
    // atomic_max(&out_hist2d_xyz_rgba[sc_id * 4 + 1], G);
    // atomic_max(&out_hist2d_xyz_rgba[sc_id * 4 + 2], B);
    atomic_inc(&out_hist2d_xyz_rgba[sc_id * 4 + 3]);
}

__kernel void accumulateDiaScope(
    __global const uchar* in_rgba,
    __global uint*        out_hist2d_dia_rgba,
    uint                  src_width,
    uint                  src_height,
    uchar                 mask_enabled,
    cl_rect_2D            mask_rect) {
    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    if (x >= src_width || y >= src_height)
        return;

    if (mask_enabled && !isWithin(mask_rect, x, y))
        return;

    uint   src_gid = y * src_width + x;
    size_t rgbaPB  = src_gid * 4;

    uchar R = in_rgba[rgbaPB + 0];
    uchar G = in_rgba[rgbaPB + 1];
    uchar B = in_rgba[rgbaPB + 2];

    uchar xGB = clamp8((uint)rint(-.5f * (float)G + .5f * (float)B + 128.f));
    uchar yGB = 255 - clamp8((uint)rint(.25f * (float)G + .25f * (float)B + 128.f));

    uint sc_id_gb = yGB * SC_WIDTH + xGB;

    // atomic_max(&out_hist2d_dia_rgba[sc_id_gb * 4 + 1], G);
    // atomic_max(&out_hist2d_dia_rgba[sc_id_gb * 4 + 2], B);
    atomic_inc(&out_hist2d_dia_rgba[sc_id_gb * 4 + 3]);

    uchar xGR = clamp8((uint)rint(-.5f * (float)G + .5f * (float)R + 128.f));
    uchar yGR = 255 - clamp8((uint)rint(-.25f * (float)G - .25f * (float)R + 128.f));

    uint sc_id_gr = yGR * SC_WIDTH + xGR;

    // atomic_max(&out_hist2d_dia_rgba[sc_id_gr * 4 + 0], R);
    // atomic_max(&out_hist2d_dia_rgba[sc_id_gr * 4 + 1], G);
    atomic_inc(&out_hist2d_dia_rgba[sc_id_gr * 4 + 3]);
}

#define BINS_X 256                  // U bins
#define TILE_V 16                   // V rows per pass
#define BINS_TILE (BINS_X * TILE_V) // 4096

// instead of accumulating the whole UV scope at once using atomics on global memory,
// we accumulate stripes of the UV scope in local memory and then write them to global memory
// this is done to reduce the contention on the global memory and improve performance
__kernel void accumulateUVScopeV2(
    __global const uchar* in_yuv,
    __global uint*        out_hist2d_uv_rgba,
    uint                  src_w,
    uint                  src_h,
    uchar                 mask_on,
    cl_rect_2D            mask_rect) {

    const uint x    = get_global_id(0);
    const uint y    = get_global_id(1);
    const uint pass = get_global_id(2); // 0 … 15

    const uint lid = get_local_id(0);
    const uint lsz = get_local_size(0);

    __local uint hist[BINS_TILE];

    /* 1. Clear local histogram ------------------------------------------ */
    for (uint i = lid; i < BINS_TILE; i += lsz)
        hist[i] = 0u;

    barrier(CLK_LOCAL_MEM_FENCE);

    /* 2. Accumulate if this pixel falls into the current V stripe -------- */
    uchar active = 0;
    uchar U = 0, V = 0;

    if (x < src_w && y < src_h) {

        if (!mask_on || isWithin(mask_rect, x, y)) {

            const uint p = (y * src_w + x) * 3u;
            U            = in_yuv[p + 1];
            V            = in_yuv[p + 2];

            const uchar stripe_min = pass * TILE_V;

            if (V >= stripe_min && V < stripe_min + TILE_V) {
                const uint idx = (V - stripe_min) * BINS_X + U;
                atomic_inc(&hist[idx]);
                active = 1;
            }
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* 3. Flush the tile to global memory -------------------------------- */
    for (uint i = lid; i < BINS_TILE; i += lsz) {

        uint count = hist[i];
        if (count == 0)
            continue; // skip zeros to save atomics

        uint v_bin = i / BINS_X; // 0 … 15
        uint u_bin = i % BINS_X; // 0 … 255

        uint global_v = pass * TILE_V + v_bin; // 0 … 255

        /* store “upside-down” so V=0 (blue) is at the top of the scope */
        uint sc_row = 255u - global_v;

        uint dst = (sc_row * BINS_X + u_bin) * 4u + 3u; // A-channel
        atomic_add(&out_hist2d_uv_rgba[dst], count);
    }
}