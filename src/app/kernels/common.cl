inline uchar clamp8(int value) {
    return value < 0 ? 0 : value > 255 ? 255
                                       : value;
}

inline uchar preMultiply(uchar a, uchar b) {
    return (uchar)rint(((float)a * (float)b) / 255.f);
}

#define CS_BT601_525 1
#define CS_BT601_625 2
#define CS_BT709 3
#define CS_BT2020 4
#define CS_sRGB 5

void rgbToYuv(uchar r, uchar g, uchar b, uchar* y, uchar* u, uchar* v,
              int colorspace) {
    if (colorspace == CS_BT601_525) {
        *y = clamp8(16 + (int)rint(r * 0.25679f + g * 0.50413f + b * 0.09791f));
        *u = clamp8(128 + (int)rint(-0.14822f * r - 0.29099f * g + 0.43922f * b));
        *v = clamp8(128 + (int)rint(0.43922f * r - 0.36779f * g - 0.07143f * b));
    } else if (colorspace == CS_BT709) {
        // BT.709 RGB Full 0 - 255 to 16 - 235
        *y = clamp8(16 + (int)rint(r * 0.18259f + g * 0.61423f + b * 0.06201f));
        *u = clamp8(128 + (int)rint(r * -0.10064f + g * -0.33857f + b * 0.43922f));
        *v = clamp8(128 + (int)rint(r * 0.43922f + g * -0.39894f + b * -0.04027f));
    } else if (colorspace == CS_BT2020) {
        *y = clamp8(16 + (int)rint(0.22561f * r + 0.58228f * g + 0.05093f * b));
        *u = clamp8(128 + (int)rint(-0.12266f * r - 0.31656f * g + 0.43922f * b));
        *v = clamp8(128 + (int)rint(0.43922f * r - 0.40389f * g - 0.03533f * b));
    }
}

void rgbToXYZ(uchar r, uchar g, uchar b, int colorspace, float* X, float* Y, float* Z) {
    float r_f = r / 255.0f;
    float g_f = g / 255.0f;
    float b_f = b / 255.0f;

    if (colorspace == CS_BT601_525) {
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

#define WF_WIDTH 580
#define WF_HEIGHT 256
#define SC_WIDTH 256
#define SC_HEIGHT 256

#define WAVEFORM_BINS 256

inline void coordsFromIndex(uint index, uint width, uint* x, uint* y) {
    *x = index % width;
    *y = index / width;
}

inline int2 coordsVecFromIndex(uint index, uint width) {
    return (int2)(index % width, index / width);
}

inline uint indexFromCoords(uint x, uint y, uint width) {
    return y * width + x;
}

inline uint indexFromCoordsVec(int2 coords, uint width) {
    return coords.y * width + coords.x;
}

inline float4 ucharColorToFloat4(uchar4 c) {
    return (float4)(c.x / 255.f, c.y / 255.f, c.z / 255.f, c.w / 255.f);
}

typedef uint RenderFeatureFlags;

#define RENDER_FEATURE_NONE 0
#define RENDER_FEATURE_FC (1 << 0)
#define RENDER_FEATURE_WF_LUMA (1 << 1)
#define RENDER_FEATURE_WF_RGB (1 << 2)
#define RENDER_FEATURE_WF_RGBPARADE (1 << 3)
#define RENDER_FEATURE_WF_RGBBLACKS (1 << 4)
#define RENDER_FEATURE_WF_YUVPARADE (1 << 5)
#define RENDER_FEATURE_SC_UV (1 << 6)
#define RENDER_FEATURE_SC_XYZ (1 << 7)
#define RENDER_FEATURE_SC_DIA (1 << 8)

#define RENDER_FEATURE_ANY_WF_RGB \
    (RENDER_FEATURE_WF_RGB | RENDER_FEATURE_WF_RGBPARADE | RENDER_FEATURE_WF_RGBBLACKS)

#define RENDER_FEATURE_ANY_WF_YUV \
    (RENDER_FEATURE_WF_YUVPARADE | RENDER_FEATURE_WF_LUMA)

#define RENDER_FEATURE_ANY_WF \
    (RENDER_FEATURE_ANY_WF_RGB | RENDER_FEATURE_ANY_WF_YUV)

#define RENDER_FEATURE_ANY_SC \
    (RENDER_FEATURE_SC_UV | RENDER_FEATURE_SC_XYZ | RENDER_FEATURE_SC_DIA)

#define RENDER_FEATURE_ALL 0xFFFFFFFF