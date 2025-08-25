#pragma once

#include "pch.hpp"

namespace scpp {

enum class ScaleBehavior {
    DoNotScale,
    OnlyScaleDown,
    ScaleToFit,
};

enum class FlipBehavior {
    DoNotFlip,
    FlipHorizontally,
    FlipVertically,
    FlipBoth,
};

static inline auto ImGuiUtilGetContentScale(ImVec2 size, ScaleBehavior scaleBehavior = ScaleBehavior::OnlyScaleDown) -> float {
    ImVec2 avail  = ImGui::GetContentRegionAvail();
    float  imgW   = size.x;
    float  imgH   = size.y;
    float  scaleX = avail.x / imgW;
    float  scaleY = avail.y / imgH;
    float  scale  = 1.f;
    switch (scaleBehavior) {
    case ScaleBehavior::DoNotScale:
        break;
    case ScaleBehavior::OnlyScaleDown:
        scale = (std::min)((std::min)(scaleX, scaleY), 1.f);
        break;
    case ScaleBehavior::ScaleToFit:
        scale = (std::min)(scaleX, scaleY);
        break;
    }
    return scale;
}

static inline auto ImGuiUtilGetImageSize(ImVec2 size, ScaleBehavior scaleBehavior = ScaleBehavior::OnlyScaleDown) -> ImVec2 {
    float scale = ImGuiUtilGetContentScale(size, scaleBehavior);
    return ImVec2(size.x * scale, size.y * scale);
}

static constexpr inline auto ImGuiUtilGetUVs(FlipBehavior flipBehavior) -> std::pair<ImVec2, ImVec2> {
    if (flipBehavior == FlipBehavior::DoNotFlip) {
        return {ImVec2(0, 0), ImVec2(1, 1)};
    } else if (flipBehavior == FlipBehavior::FlipHorizontally) {
        return {ImVec2(1, 0), ImVec2(0, 1)};
    } else if (flipBehavior == FlipBehavior::FlipVertically) {
        return {ImVec2(0, 1), ImVec2(1, 0)};
    } else { // FlipBoth
        return {ImVec2(1, 1), ImVec2(0, 0)};
    }
}

static constexpr auto c_bgColor = ImVec4(0.f, 0.f, 0.f, 1.f);

static inline void ImGuiUtilImageRender(
    const CLGLTextureRGBA& texture,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);
}

static constexpr auto ImGuiUtilRelPosToImagePos(ImVec2 relPos, ImVec2 imgSize, ImVec2 offset = {0.f, 0.f}) -> ImVec2 {
    return ImVec2(
        offset.x + relPos.x * imgSize.x,
        offset.y + (1.f - relPos.y) * imgSize.y);
}

static constexpr ImU32 c_lineColor      = IM_COL32(255, 255, 255, 128);
static constexpr ImU32 c_lineColorHalf  = IM_COL32(255, 255, 255, 64);
static constexpr ImU32 c_lineColorQuart = IM_COL32(255, 255, 255, 32);
static constexpr float c_lineThickness  = 1.5f;

static inline void ImGuiUtilRenderLumaWF(
    const CLGLTextureRGBA& texture,
    SourceYUVRange         yuvRange,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {

    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    const auto topLeft     = ImGui::GetCursorScreenPos();

    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw Horizontal lines on the waveform in interval of 32

    if (yuvRange == SourceYUVRange::Full) {
        for (int i = 0; i <= 256; i += 32) {
            const auto p1 = ImGuiUtilRelPosToImagePos(ImVec2(0.f, i / 255.f), imgSize, topLeft);
            const auto p2 = ImGuiUtilRelPosToImagePos(ImVec2(1.f, i / 255.f), imgSize, topLeft);

            drawList->AddLine(p1, p2, c_lineColor, c_lineThickness);
        }
    } else { // Limited range
        for (int i = 0; i <= 256; i += 32) {
            const auto p1 = ImGuiUtilRelPosToImagePos(
                ImVec2(0.f, (16.f + i * (219.f / 255.f)) / 255.f), imgSize, topLeft);
            const auto p2 = ImGuiUtilRelPosToImagePos(
                ImVec2(1.f, (16.f + i * (219.f / 255.f)) / 255.f), imgSize, topLeft);
            drawList->AddLine(p1, p2, c_lineColor, c_lineThickness);
        }
    }
}

static inline void ImGuiUtilRenderRGBWF(
    const CLGLTextureRGBA& texture,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    const auto topLeft     = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (int i = 0; i <= 256; i += 32) {
        float y = topLeft.y + imgSize.y - (i / 255.f) * imgSize.y;
        drawList->AddLine(
            ImVec2(topLeft.x, y),
            ImVec2(topLeft.x + imgSize.x, y),
            c_lineColor, c_lineThickness);
    }
}

static inline void ImGuiUtilRenderParade(
    const CLGLTextureRGBA& texture,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    const auto topLeft     = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (int i = 0; i <= 256; i += 32) {
        const auto p1 = ImGuiUtilRelPosToImagePos(ImVec2(0.f, i / 255.f), imgSize, topLeft);
        const auto p2 = ImGuiUtilRelPosToImagePos(ImVec2(1.f, i / 255.f), imgSize, topLeft);
        drawList->AddLine(p1, p2, c_lineColor, c_lineThickness);
    }

    auto p1 = ImGuiUtilRelPosToImagePos(ImVec2(1.f / 3.f, 0.f), imgSize, topLeft);
    auto p2 = ImGuiUtilRelPosToImagePos(ImVec2(1.f / 3.f, 1.f), imgSize, topLeft);
    drawList->AddLine(p1, p2, c_lineColor, c_lineThickness);

    p1 = ImGuiUtilRelPosToImagePos(ImVec2(2.f / 3.f, 0.f), imgSize, topLeft);
    p2 = ImGuiUtilRelPosToImagePos(ImVec2(2.f / 3.f, 1.f), imgSize, topLeft);
    drawList->AddLine(p1, p2, c_lineColor, c_lineThickness);
}

static inline void ImGuiUtilRenderBlacklevel(
    const CLGLTextureRGBA& texture,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    const auto topLeft     = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const auto increment = 1.f / 35.f;
    for (int i = 0; i < 35; i++) {
        const auto color = (i % 5 == 0) ? c_lineColor : c_lineColorQuart;
        const auto p1    = ImGuiUtilRelPosToImagePos(
            ImVec2(0.f, i * increment), imgSize, topLeft);
        const auto p2 = ImGuiUtilRelPosToImagePos(
            ImVec2(1.f, i * increment), imgSize, topLeft);
        drawList->AddLine(p1, p2, color, c_lineThickness);
    }
}

static inline constexpr auto GetCIEPrimaries(SourceColorSpace colorSpace) -> std::array<ImVec2, 3> {
    switch (colorSpace) {
    case SourceColorSpace::BT601_525:
        return {ImVec2(0.630f, 0.340f), ImVec2(0.310f, 0.595f), ImVec2(0.155f, 0.070f)};
    case SourceColorSpace::BT601_625:
        return {ImVec2(0.640f, 0.330f), ImVec2(0.290f, 0.600f), ImVec2(0.150f, 0.060f)};
    case SourceColorSpace::BT709:
    case SourceColorSpace::SRGB:
        return {ImVec2(0.640f, 0.330f), ImVec2(0.300f, 0.600f), ImVec2(0.150f, 0.060f)};
    case SourceColorSpace::BT2020:
        return {ImVec2(0.708f, 0.292f), ImVec2(0.170f, 0.797f), ImVec2(0.131f, 0.046f)};
    }

    return {ImVec2(0.f, 0.f), ImVec2(0.f, 0.f), ImVec2(0.f, 0.f)};
}

static inline constexpr auto c_cieTriangleColorspaces =
    std::to_array<SourceColorSpace>({SourceColorSpace::BT601_525,
                                     SourceColorSpace::BT601_625,
                                     SourceColorSpace::BT709,
                                     SourceColorSpace::SRGB,
                                     SourceColorSpace::BT2020});

static inline consteval auto GetCIELocus() -> std::array<ImVec2, 100> {
    auto g = [](float x, float u, float t1, float t2) -> float {
        return (x < u)
                   ? exp_constexpr(-(t1 * t1) * (x - u) * (x - u) / 2.f)
                   : exp_constexpr(-(t2 * t2) * (x - u) * (x - u) / 2.f);
    };

    auto x = [&](float l) -> float {
        return 1.056f * g(l, 599.8f, 0.0264f, 0.0323f) +
               0.362f * g(l, 442.0f, 0.0624f, 0.0374f) -
               0.065f * g(l, 501.1f, 0.049f, 0.0382f);
    };

    auto y = [&](float l) -> float {
        return 0.821f * g(l, 568.8f, 0.0213f, 0.0247f) +
               0.286f * g(l, 530.9f, 0.0613f, 0.0322f);
    };

    auto z = [&](float l) -> float {
        return 1.217f * g(l, 437.0f, 0.0845f, 0.0278f) +
               0.681f * g(l, 459.0f, 0.0385f, 0.0725f);
    };

    std::array<ImVec2, 100> locus;

    float lStart = 440.f;
    float lEnd   = 650.f;
    for (int i = 0; i < 100; i++) {
        float l    = lStart + (lEnd - lStart) * (i / 99.f);
        float xVal = x(l);
        float yVal = y(l);
        float zVal = z(l);
        float X    = xVal / (xVal + yVal + zVal);
        float Y    = yVal / (xVal + yVal + zVal);

        locus[i] = ImVec2(X, Y);
    }

    return locus;
}

static inline constexpr auto c_cieLocus = GetCIELocus();

static inline void ImGuiUtilRenderCIE(
    const CLGLTextureRGBA& texture,
    SourceColorSpace       selectedColorSpace,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    const auto topLeft     = ImGui::GetCursorScreenPos();

    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw horizontal and vertical grid lines at .1 intervals every second one slightly less opaque
    for (int i = 0; i <= 10; i++) {
        float x     = topLeft.x + (i / 10.f) * imgSize.x;
        float y     = topLeft.y + (i / 10.f) * imgSize.y;
        ImU32 color = (i % 2 == 0) ? c_lineColorHalf : c_lineColorQuart;
        drawList->AddLine(ImVec2(x, topLeft.y), ImVec2(x, topLeft.y + imgSize.y), color, c_lineThickness);
        drawList->AddLine(ImVec2(topLeft.x, y), ImVec2(topLeft.x + imgSize.x, y), color, c_lineThickness);
    }

    // Draw the triangles for each color space

    for (const auto colorSpace : c_cieTriangleColorspaces) {
        auto primaries = GetCIEPrimaries(colorSpace);

        ImVec2 p1 = ImVec2(topLeft.x + primaries[0].x * imgSize.x, topLeft.y + (1.f - primaries[0].y) * imgSize.y);
        ImVec2 p2 = ImVec2(topLeft.x + primaries[1].x * imgSize.x, topLeft.y + (1.f - primaries[1].y) * imgSize.y);
        ImVec2 p3 = ImVec2(topLeft.x + primaries[2].x * imgSize.x, topLeft.y + (1.f - primaries[2].y) * imgSize.y);

        ImU32 color = (colorSpace == selectedColorSpace) ? c_lineColor : c_lineColorHalf;
        drawList->AddTriangle(p1, p2, p3, color);
    }

    // Draw the CIE locus

    for (size_t i = 0; i < c_cieLocus.size() - 1; i++) {
        ImVec2 p1 = ImVec2(topLeft.x + c_cieLocus[i].x * imgSize.x, topLeft.y + (1.f - c_cieLocus[i].y) * imgSize.y);
        ImVec2 p2 = ImVec2(topLeft.x + c_cieLocus[i + 1].x * imgSize.x, topLeft.y + (1.f - c_cieLocus[i + 1].y) * imgSize.y);
        drawList->AddLine(p1, p2, c_lineColorHalf, c_lineThickness);
    }
}

static constexpr auto c_diaVerts = std::to_array<ImVec2>({
    {0.5f, 0.f  },
    {1.f,  0.25f},
    {0.5f, 0.5f },
    {0.f,  0.25f},
    {0.5f, 0.f  },
    {0.5f, 0.5f },
    {1.f,  0.75f},
    {0.5f, 1.f  },
    {0.f,  0.75f},
    {0.5f, 0.5f },
    {0.5f, 1.f  }
});

static inline void ImGuiUtilRenderDia(
    const CLGLTextureRGBA& texture,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    const auto topLeft     = ImGui::GetCursorScreenPos();

    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < c_diaVerts.size() - 1; i++) {
        auto p1 = ImGuiUtilRelPosToImagePos(c_diaVerts[i], imgSize, topLeft);
        auto p2 = ImGuiUtilRelPosToImagePos(c_diaVerts[i + 1], imgSize, topLeft);
        drawList->AddLine(p1, p2, c_lineColor, c_lineThickness);
    }
}

static constexpr auto c_offsetR2YFull = glm::vec3(0.f, 0.5f, 0.5f);

static constexpr auto c_mR2Y601_Full = glm::mat3x3{
    0.29899999f, -0.16873589f, 0.50000000f,
    0.58700001f, -0.33126411f, -0.41868758f,
    0.11400000f, 0.50000000f, -0.08131241f};

static constexpr auto c_mR2Y709_Full = glm::mat3x3{
    0.21259999f, -0.11457211f, 0.50000000f,
    0.71520001f, -0.38542789f, -0.45415291f,
    0.07220000f, 0.50000000f, -0.04584709f};

static constexpr auto c_mR2Y2020_Full = glm::mat3x3{
    0.26269999f, -0.13963006f, 0.50000000f,
    0.67799997f, -0.36036995f, -0.45978570f,
    0.05930000f, 0.50000000f, -0.04021430f};

static constexpr auto c_offsetR2YLimited = glm::vec3(0.0627451f, 0.5019608f, 0.5019608f);

static constexpr auto c_mR2Y601_Limited = glm::mat3x3{
    0.25678822f, -0.14491436f, 0.42941177f,
    0.51563925f, -0.29099280f, -0.36778831f,
    0.10014118f, 0.43921569f, -0.07142738f};

static constexpr auto c_mR2Y709_Limited = glm::mat3x3{
    0.18258588f, -0.09839723f, 0.42941177f,
    0.62825412f, -0.33857197f, -0.39894217f,
    0.06342275f, 0.43921569f, -0.04027352f};

static constexpr auto c_mR2Y2020_Limited = glm::mat3x3{
    0.22561294f, -0.11991759f, 0.42941177f,
    0.59557647f, -0.31656027f, -0.40389019f,
    0.05209098f, 0.43921569f, -0.03532550f};

static constexpr auto c_mR2R_709_to_601_525 = glm::mat3x3{
    0.93954194f, 0.05018133f, 0.01027656f,
    0.01777223f, 0.96579289f, 0.01643492f,
    -0.00162160f, -0.00436975f, 1.00599146f};

static constexpr auto c_mR2R_709_to_601_625 = glm::mat3x3{
    1.04404318f, -0.04404324f, 0.00000000f,
    0.00000001f, 1.00000000f, -0.00000001f,
    -0.00000000f, 0.01179338f, 0.98820668f};

static constexpr auto c_mR2R_709_to_2020 = glm::mat3x3{
    1.66049099f, -0.58764112f, -0.07284993f,
    -0.12455052f, 1.13289988f, -0.00834943f,
    -0.01815076f, -0.10057890f, 1.11872983f};

// static inline constexpr auto ImGuiUtilRGBtoYUV(glm::vec3 rgb, SourceColorSpace colorSpace, SourceYUVRange yuvRange) -> glm::vec3 {
//     switch (colorSpace) {
//     case SourceColorSpace::BT601_525:
//         rgb = c_mR2R_709_to_601_525 * rgb;
//     case SourceColorSpace::BT601_625:
//         rgb = c_mR2R_709_to_601_625 * rgb;
//         if (yuvRange == SourceYUVRange::Full) {
//             return c_mR2Y601_Full * rgb + c_offsetR2YFull;
//         } else {
//             return c_mR2Y601_Limited * rgb + c_offsetR2YLimited;
//         }
//     case SourceColorSpace::BT709:
//     case SourceColorSpace::SRGB:
//
//         if (yuvRange == SourceYUVRange::Full) {
//             return c_mR2Y709_Full * rgb + c_offsetR2YFull;
//         } else {
//             return c_mR2Y709_Limited * rgb + c_offsetR2YLimited;
//         }
//     case SourceColorSpace::BT2020:
//         rgb = c_mR2R_709_to_2020 * rgb;
//         if (yuvRange == SourceYUVRange::Full) {
//             return c_mR2Y2020_Full * rgb + c_offsetR2YFull;
//         } else {
//             return c_mR2Y2020_Limited * rgb + c_offsetR2YLimited;
//         }
//     }
//
//     return glm::vec3(0.f, 0.f, 0.f); // Fallback
// }

static constexpr auto c_uvVectorColors = std::to_array<glm::vec3>({
    {1.f, 0.f, 0.f},
    {1.f, 1.f, 0.f},
    {0.f, 1.f, 0.f},
    {0.f, 1.f, 1.f},
    {0.f, 0.f, 1.f},
    {1.f, 0.f, 1.f},
    {1.f, 0.f, 0.f}
});

static inline constexpr auto ConstexprMatMul3x3(const glm::mat3x3& m, glm::vec3 v) -> glm::vec3 {
    return glm::vec3(
        m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
        m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
        m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z);
}

static consteval auto GetUVVectors() -> std::array<glm::vec2, c_uvVectorColors.size()> {
    std::array<glm::vec2, c_uvVectorColors.size()> uvVectors;
    for (size_t i = 0; i < c_uvVectorColors.size(); i++) {
        const auto& color  = c_uvVectorColors[i];
        const auto  yuv709 = ConstexprMatMul3x3(c_mR2Y709_Limited, color) + c_offsetR2YLimited;
        uvVectors[i]       = glm::vec2(yuv709.y, yuv709.z);
    }
    return uvVectors;
}

static constexpr auto c_uvVectors = GetUVVectors();

static constexpr auto ImGuiUtilGLMVec3ToImU32(glm::vec3 vec) -> ImU32 {
    return IM_COL32(static_cast<ImU8>(vec.x * 255.f), static_cast<ImU8>(vec.y * 255.f), static_cast<ImU8>(vec.z * 255.f), 255);
}

static inline void ImGuiUtilRenderUV(
    const CLGLTextureRGBA& texture,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    const auto topLeft     = ImGui::GetCursorScreenPos();

    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const auto p0 = ImGuiUtilRelPosToImagePos(ImVec2(.5f, .5f), imgSize, topLeft);

    for (size_t i = 0; i < c_uvVectors.size() - 1; i++) {
        const auto uva = c_uvVectors[i];
        const auto uvb = c_uvVectors[i + 1];

        const auto pa = ImGuiUtilRelPosToImagePos(ImVec2(uva.x, uva.y), imgSize, topLeft);
        const auto pb = ImGuiUtilRelPosToImagePos(ImVec2(uvb.x, uvb.y), imgSize, topLeft);

        drawList->AddLine(pa, pb, c_lineColor, c_lineThickness);

        drawList->AddLine(p0, pa, c_lineColor, c_lineThickness);
    }
}

struct WindowAspectData {
    float  targetAspectRatio; // width / height
    ImVec2 offset;            // e.g. title bar offset
};

struct WindowSizeConstraints {

    static inline ImVec2 ProjectToAspect(ImVec2 desiredWH, float aspect /* w/h */) {
        // valid sizes lie on h = w / aspect  => direction d = (1, 1/aspect)
        const glm::vec2 v(desiredWH.x, desiredWH.y);
        const glm::vec2 d(1.0f, 1.0f / aspect);
        const float     t = glm::dot(v, d) / glm::dot(d, d);
        glm::vec2       p = t * d;

        // clamp to non-negative and pixel snap (round to nearest int)
        ImVec2 out((float)(int)(p.x + 0.5f), (float)(int)(p.y + 0.5f));
        if (out.x < 0)
            out.x = 0;
        if (out.y < 0)
            out.y = 0;
        return out;
    }

    static inline ImVec2 SubtractOffsetNonNegative(ImVec2 a, ImVec2 off) {
        ImVec2 r = ImVec2(a.x - off.x, a.y - off.y);
        if (r.x < 0)
            r.x = 0;
        if (r.y < 0)
            r.y = 0;
        return r;
    }
    // Maintain a fixed aspect ratio (no offset)
    static void AspectRatio(ImGuiSizeCallbackData* data) {
        const float  aspect = *(float*)data->UserData; // width / height
        const ImVec2 wh     = ProjectToAspect(data->DesiredSize, aspect);
        data->DesiredSize   = wh;
    }

    // Force square (no offset)
    static void Square(ImGuiSizeCallbackData* data) {
        const ImVec2 wh   = ProjectToAspect(data->DesiredSize, 1.0f);
        data->DesiredSize = wh;
    }

    // Snap size to grid step
    static void Step(ImGuiSizeCallbackData* data) {
        float step          = *(float*)data->UserData;
        data->DesiredSize.x = roundf(data->DesiredSize.x / step) * step;
        data->DesiredSize.y = roundf(data->DesiredSize.y / step) * step;
    }

    // Maintain aspect ratio with an offset (e.g. title bar height)
    static void AspectWithOffset(ImGuiSizeCallbackData* data) {
        const WindowAspectData* a      = (const WindowAspectData*)data->UserData;
        const float             aspect = a->targetAspectRatio;
        const ImVec2            off    = a->offset;

        // operate in "content" space (minus chrome offset)
        ImVec2 desired_content = SubtractOffsetNonNegative(data->DesiredSize, off);

        // project to aspect in content space
        ImVec2 wh_content = ProjectToAspect(desired_content, aspect);

        // restore total window size
        data->DesiredSize.x = wh_content.x + off.x;
        data->DesiredSize.y = wh_content.y + off.y;
    }

    // Force square with an offset (e.g. title bar height)
    static void SquareWithOffset(ImGuiSizeCallbackData* data) {
        const ImVec2* offp = (const ImVec2*)data->UserData;

        ImVec2 desired_content = SubtractOffsetNonNegative(data->DesiredSize, *offp);

        ImVec2 wh_content = ProjectToAspect(desired_content, 1.0f);

        data->DesiredSize.x = wh_content.x + offp->x;
        data->DesiredSize.y = wh_content.y + offp->y;
    }
};

} // namespace scpp
