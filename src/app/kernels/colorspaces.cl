#define CS_LINEAR_RGB 0
#define CS_BT601_525 1
#define CS_BT601_625 2
#define CS_BT709 3
#define CS_BT2020 4
#define CS_SRGB 5

typedef int colorspace_t;

#define YUV_RANGE_LIMITED 0
#define YUV_RANGE_FULL 1

typedef int yuv_range_t;

inline float3 mul3x3v(constant float* m, float3 v) {
    return (float3)(m[0] * v.x + m[1] * v.y + m[2] * v.z,
                    m[3] * v.x + m[4] * v.y + m[5] * v.z,
                    m[6] * v.x + m[7] * v.y + m[8] * v.z);
}

// Transfer functions

inline float eotf_sRGB(float v) { return (v <= 0.04045f) ? (v / 12.92f) : pow((v + 0.055f) / 1.055f, 2.4f); }

inline float oetf_sRGB(float v) { return (v <= 0.0031308f) ? (v * 12.92f) : (1.055f * pow(v, 1.f / 2.4f) - 0.055f); }

inline float eotf_BT709(float v) { return (v <= 0.08145f) ? (v / 4.5f) : pow((v + 0.099f) / 1.099f, 1.0f / 0.45f); }

inline float oetf_BT709(float v) { return (v <= 0.018f) ? (v * 4.5f) : (1.099f * pow(v, 0.45f) - 0.099f); }

inline float eotf(float v, colorspace_t cs) {
    switch (cs) {
    case CS_SRGB:
        return eotf_sRGB(v);
    case CS_BT601_525:
    case CS_BT601_625:
    case CS_BT709:
    case CS_BT2020:
        return eotf_BT709(v);
    default:
        return v; // No conversion
    }
}

inline float oetf(float v, colorspace_t cs) {
    switch (cs) {
    case CS_SRGB:
        return oetf_sRGB(v);
    case CS_BT601_525:
    case CS_BT601_625:
    case CS_BT709:
    case CS_BT2020:
        return oetf_BT709(v);
    default:
        return v; // No conversion
    }
}

inline float3 eotf3(float3 v, colorspace_t cs) { return (float3)(eotf(v.x, cs), eotf(v.y, cs), eotf(v.z, cs)); }
inline float3 oetf3(float3 v, colorspace_t cs) { return (float3)(oetf(v.x, cs), oetf(v.y, cs), oetf(v.z, cs)); }

inline float4 eotf4(float4 v, colorspace_t cs) { return (float4)(eotf(v.x, cs), eotf(v.y, cs), eotf(v.z, cs), v.w); }
inline float4 oetf4(float4 v, colorspace_t cs) { return (float4)(oetf(v.x, cs), oetf(v.y, cs), oetf(v.z, cs), v.w); }

// linear RGB -> linear RGB

constant float M_RGB_601_525_to_RGB_709_linear[9] = {
    0.93954194f, 0.05018133f, 0.01027656f,
    0.01777223f, 0.96579289f, 0.01643492f,
    -0.00162160f, -0.00436975f, 1.00599146f};

constant float M_RGB_601_625_to_RGB_709_linear[9] = {
    1.04404318f, -0.04404324f, 0.00000000f,
    0.00000001f, 1.00000000f, -0.00000001f,
    -0.00000000f, 0.01179338f, 0.98820668f};

constant float M_RGB_2020_to_RGB_709_linear[9] = {
    1.66049099f, -0.58764112f, -0.07284993f,
    -0.12455052f, 1.13289988f, -0.00834943f,
    -0.01815076f, -0.10057890f, 1.11872983f};

inline float3 linearRGB_to_linearRGB_709(float3 rgb, colorspace_t src_colorspace) {
    switch (src_colorspace) {
    case CS_BT601_525:
        return mul3x3v(M_RGB_601_525_to_RGB_709_linear, rgb);
    case CS_BT601_625:
        return mul3x3v(M_RGB_601_625_to_RGB_709_linear, rgb);
    case CS_BT2020:
        return mul3x3v(M_RGB_2020_to_RGB_709_linear, rgb);
    default:
        return rgb; // No conversion
    }
}

// RGB -> XYZ

constant float M_RGB_709_Lin_to_XYZ[9] = {
    0.41239080f, 0.35758433f, 0.18048079f,
    0.21263900f, 0.71516865f, 0.07219232f,
    0.01933082f, 0.11919478f, 0.95053214f};

inline float3 linearRGB_709_to_XYZ(float3 rgb, colorspace_t src_colorspace) {
    return mul3x3v(M_RGB_709_Lin_to_XYZ, rgb);
}

// XYZ -> RGB

constant float M_XYZ_to_RGB_709_Lin[9] = {
    3.24096990f, -1.53738320f, -0.49861076f,
    -0.96924365f, 1.87596750f, 0.04155505f,
    0.05563009f, -0.20397697f, 1.05697155f};

inline float3 XYZ_to_linearRGB_709(float3 xyz) {
    return mul3x3v(M_XYZ_to_RGB_709_Lin, xyz);
}

inline float3 XYZ_to_RGB_709(float3 xyz) {
    return oetf3(XYZ_to_linearRGB_709(xyz), CS_BT709);
}

inline float3 xy_to_RGB_709_linear(float x, float y, float Y) {
    if (y <= 0.0f || x <= 0.0f || y >= 1.0f || x + y >= 1.0f)
        return (float3)(0.0f, 0.0f, 0.0f);

    float X = (x / y) * Y;
    float Z = ((1.0f - x - y) / y) * Y;

    float3 xyz = (float3)(X, Y, Z);
    return XYZ_to_linearRGB_709(xyz); // may be <0 or >1 if outside 709 gamut
}

inline float3 xy_to_RGB_709(float x, float y, float Y) {
    float3 lin = xy_to_RGB_709_linear(x, y, Y);
    lin        = fmax(lin, (float3)(0.0f, 0.0f, 0.0f));
    return oetf3(lin, CS_BT709);
}

// RGB -> YUV

constant float3 YUV_LIMITED_OFFSET = (float3)(16.f / 255.f, 128.f / 255.f, 128.f / 255.f);

constant float M_RGB_709_to_YUV_709_LIMITED[9] = {
    0.18258588f, 0.61423057f, 0.06200706f,
    -0.10064373f, -0.33857197f, 0.43921569f,
    0.43921569f, -0.39894217f, -0.04027352f};

inline float3 rgb_709_to_YUV_709_Limited(float3 rgb) {
    return mul3x3v(M_RGB_709_to_YUV_709_LIMITED, rgb) + YUV_LIMITED_OFFSET;
}

constant float3 YUV_FULL_OFFSET = (float3)(0.0f, 0.5f, 0.5f);

constant float M_RGB_709_to_YUV_709_FULL[9] = {
    0.21259999f, 0.71520001f, 0.07220000f,
    -0.11457211f, -0.38542789f, 0.50000000f,
    0.50000000f, -0.45415291f, -0.04584709f};

inline float3 rgb_709_to_YUV_709_Full(float3 rgb) {
    return mul3x3v(M_RGB_709_to_YUV_709_FULL, rgb) + YUV_FULL_OFFSET;
}

inline float3 rgb_709_to_YUV_709(float3 rgb, yuv_range_t yuv_range) {
    if (yuv_range == YUV_RANGE_FULL)
        return rgb_709_to_YUV_709_Full(rgb);
    else
        return rgb_709_to_YUV_709_Limited(rgb);
}

// YUV -> RGB

constant float M_YUV_601_LIMITED_to_RGB_601[9] = {
    1.16438353f, 0.00000000f, 1.59602666f,
    1.16438353f, -0.39176229f, -0.81296766f,
    1.16438353f, 2.01723194f, 0.00000000f};

constant float M_YUV_709_LIMITED_to_RGB_709[9] = {
    1.16438353f, 0.00000000f, 1.79274106f,
    1.16438353f, -0.21324860f, -0.53290927f,
    1.16438353f, 2.11240172f, 0.00000000f};

constant float M_YUV_2020_LIMITED_to_RGB_2020[9] = {
    1.16438353f, 0.00000000f, 1.67867398f,
    1.16438353f, -0.18732609f, -0.65042430f,
    1.16438353f, 2.14177227f, 0.00000000f};

inline float3 yuv_601_Limited_to_RGB_601(float3 yuv) {
    float3 yuv_full = yuv - YUV_LIMITED_OFFSET;

    return mul3x3v(M_YUV_601_LIMITED_to_RGB_601, yuv_full);
}

inline float3 yuv_709_Limited_to_RGB_709(float3 yuv) {

    float3 yuv_full = yuv - YUV_LIMITED_OFFSET;

    return mul3x3v(M_YUV_709_LIMITED_to_RGB_709, yuv_full);
}

inline float3 yuv_2020_Limited_to_RGB_2020(float3 yuv) {
    float3 yuv_full = yuv - YUV_LIMITED_OFFSET;

    return mul3x3v(M_YUV_2020_LIMITED_to_RGB_2020, yuv_full);
}

inline float3 yuv_limited_to_RGB(float3 yuv, colorspace_t cs) {
    switch (cs) {
    case CS_BT601_525:
    case CS_BT601_625:
        return yuv_601_Limited_to_RGB_601(yuv);
    case CS_BT709:
        return yuv_709_Limited_to_RGB_709(yuv);
    case CS_BT2020:
        return yuv_2020_Limited_to_RGB_2020(yuv);
    default:
        return yuv_709_Limited_to_RGB_709(yuv);
    }
}

constant float M_YUV_601_FULL_to_RGB_601[9] = {
    1.00000000f, 0.00000000f, 1.40199995f,
    1.00000000f, -0.34413630f, -0.71413630f,
    1.00000000f, 1.77199996f, 0.00000000f};

constant float M_YUV_709_FULL_to_RGB_709[9] = {
    1.00000000f, 0.00000000f, 1.57480001f,
    1.00000000f, -0.18732427f, -0.46812427f,
    1.00000000f, 1.85560000f, 0.00000000f};

constant float M_YUV_2020_FULL_to_RGB_2020[9] = {
    1.00000000f, 0.00000000f, 1.47459996f,
    1.00000000f, -0.16455312f, -0.57135314f,
    1.00000000f, 1.88139999f, 0.00000000f};

inline float3 yuv_601_Full_to_RGB_601(float3 yuv) {
    float3 yuv_full = yuv - YUV_FULL_OFFSET;
    return mul3x3v(M_YUV_601_FULL_to_RGB_601, yuv_full);
}

inline float3 yuv_709_Full_to_RGB_709(float3 yuv) {
    float3 yuv_full = yuv - YUV_FULL_OFFSET;
    return mul3x3v(M_YUV_709_FULL_to_RGB_709, yuv_full);
}

inline float3 yuv_2020_Full_to_RGB_2020(float3 yuv) {
    float3 yuv_full = yuv - YUV_FULL_OFFSET;
    return mul3x3v(M_YUV_2020_FULL_to_RGB_2020, yuv_full);
}

inline float3 yuv_full_to_RGB(float3 yuv, colorspace_t cs) {
    switch (cs) {
    case CS_BT601_525:
    case CS_BT601_625:
        return yuv_601_Full_to_RGB_601(yuv);
    case CS_BT709:
        return yuv_709_Full_to_RGB_709(yuv);
    case CS_BT2020:
        return yuv_2020_Full_to_RGB_2020(yuv);
    default:
        return yuv_709_Full_to_RGB_709(yuv);
    }
}

inline float3 yuv_to_RGB(float3 yuv, colorspace_t cs, yuv_range_t yuv_range) {
    if (yuv_range == YUV_RANGE_FULL)
        return yuv_full_to_RGB(yuv, cs);
    else
        return yuv_limited_to_RGB(yuv, cs);
}