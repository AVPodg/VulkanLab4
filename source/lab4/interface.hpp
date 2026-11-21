#pragma once
namespace texturing { struct State; }

namespace lab4 {
inline constexpr const char* windowTitle = "CG Labs 1 + 2 + 3 + 4 - Shadow Mapping - Vulkan";
texturing::State initialState();
void beginPanel();
void controls(texturing::State& state);
}
