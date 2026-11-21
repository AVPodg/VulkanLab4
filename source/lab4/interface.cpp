#include "interface.hpp"
#include "shadow_mapping.hpp"
#include "floor.hpp"
#include "../textured_renderer.hpp"
#include <imgui.h>

namespace lab4 {
texturing::State initialState() {
    texturing::State state;
    state.direction = {0.65f,-0.8f,1.f};
    state.spots[0].position = {-3.5f,4.5f,-5.f};
    state.spots[0].direction = -state.spots[0].position;
    return state;
}
void beginPanel() {
    ImGui::Begin("Labs 1 + 2 + 3 + 4",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored({0.4f,0.75f,1.f,1.f},"COMPUTER GRAPHICS / LABS 01 + 02 + 03 + 04");
}
void controls(texturing::State& state) {
    auto& shadowSettings = shadows::settings();
    if (!ImGui::CollapsingHeader("Shadows / Lab 4", ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::Checkbox("Enable shadows", &shadowSettings.enabled);
    ImGui::Checkbox("Show floor", &floor::settings().enabled);
    ImGui::Combo("Floor texture", &floor::settings().material, texturing::materialNames, texturing::materialCount);
    ImGui::SliderFloat("Sun coverage", &shadowSettings.coverage, 2.f, 40.f, "%.1f units");
    ImGui::TextDisabled("Square: %.1f x %.1f | area %.1f", shadowSettings.coverage,
        shadowSettings.coverage, shadowSettings.coverage*shadowSettings.coverage);
    ImGui::SliderFloat("Depth bias", &shadowSettings.depthBias, 0.f, 8.f);
    ImGui::SliderFloat("Slope bias", &shadowSettings.slopeBias, 0.f, 8.f);
    ImGui::Checkbox("PCF 3 x 3", &shadowSettings.pcf);
    ImGui::Checkbox("Spotlight shadow (extra task)", &shadowSettings.spotlight);
    if (state.spots.empty()) ImGui::TextDisabled("Add a spotlight in section 5.");
    else {
        shadowSettings.spotIndex = std::clamp(shadowSettings.spotIndex,0,int(state.spots.size())-1);
        int number = shadowSettings.spotIndex+1;
        if (ImGui::SliderInt("Shadow spotlight", &number, 1, int(state.spots.size()))) shadowSettings.spotIndex = number-1;
        if (!state.spots[shadowSettings.spotIndex].enabled) ImGui::TextDisabled("Selected spotlight is disabled.");
    }
    ImGui::TextDisabled("2048 x 2048 depth maps / Vulkan dynamic rendering");
    if (ImGui::Button("Sun shadow demo")) {
        floor::settings() = {};
        state = initialState(); shadowSettings = {};
        state.points.clear(); state.spots.clear();
        state.directionalIntensity = 1.2f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Spot shadow demo")) {
        floor::settings() = {};
        state = initialState(); shadowSettings = {};
        state.directionalEnabled = false; state.points.clear();
        state.spots[0].color = {1.f,0.9f,0.7f};
        state.spots[0].intensity = 100.f;
        state.spots[0].innerAngle = 25.f; state.spots[0].outerAngle = 40.f;
    }
}
}
