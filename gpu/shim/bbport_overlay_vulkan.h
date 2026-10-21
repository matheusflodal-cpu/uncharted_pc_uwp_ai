// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdio>
#include <cstring>
#include "imgui_impl_vulkan.h"

namespace BbOverlay {

// The bundled backend also loads desktop surface/swapchain functions, even
// when our caller supplies its own image and uses dynamic rendering. A direct
// UWP ICD can omit those functions. Never initialize a partially loaded backend:
// its release build has no assertions and would call a null function pointer.
inline bool LoadVulkanBackendFunctions(
    const ImGui_ImplVulkan_InitInfo& info,
    PFN_vkVoidFunction (*resolve)(const char*, void*), void* user) {
    struct Loader {
        PFN_vkVoidFunction (*resolve)(const char*, void*);
        void* user;
        const char* missing = nullptr;
        PFN_vkVoidFunction begin_rendering = nullptr, end_rendering = nullptr;
        const char* begin_name = "vkCmdBeginRendering", *end_name = "vkCmdEndRendering";
    } loader{resolve, user};
    const auto checked_resolve = +[](const char* name, void* context) -> PFN_vkVoidFunction {
        auto& state = *static_cast<Loader*>(context);
        const auto function = state.resolve(name, state.user);
        if (!function && !state.missing) state.missing = name;
        if (!std::strcmp(name, "vkCmdBeginRendering") || !std::strcmp(name, "vkCmdBeginRenderingKHR")) {
            state.begin_rendering = function;
            state.begin_name = name;
        }
        if (!std::strcmp(name, "vkCmdEndRendering") || !std::strcmp(name, "vkCmdEndRenderingKHR")) {
            state.end_rendering = function;
            state.end_name = name;
        }
        return function;
    };
    const bool loaded = ImGui_ImplVulkan_LoadFunctions(info.ApiVersion, checked_resolve, &loader);
    // The backend first tries core dynamic rendering and then its KHR aliases.
    // A missing core alias is fine if the subsequent KHR lookup succeeds.
    const char* missing = !loaded ? loader.missing : nullptr;
    if (loaded && info.UseDynamicRendering) {
        if (!loader.begin_rendering) missing = loader.begin_name;
        else if (!loader.end_rendering) missing = loader.end_name;
    }
    if (!loaded || missing) {
        std::fprintf(stderr, "Overlay disabled: Vulkan function unavailable: %s; "
                     "continuing without the in-game overlay\n",
                     missing ? missing : "unknown");
        return false;
    }
    return true;
}

inline bool InitializeVulkanBackend(
    ImGui_ImplVulkan_InitInfo& info,
    PFN_vkVoidFunction (*resolve)(const char*, void*), void* user) {
    if (!LoadVulkanBackendFunctions(info, resolve, user)) return false;
    return ImGui_ImplVulkan_Init(&info);
}

} // namespace BbOverlay
