// Exercise the actual bundled ImGui Vulkan loader in its release configuration.
// No GPU is needed: missing ICD/WSI entry points must prevent backend init.
#include "imgui.h"
#include "bbport_overlay_vulkan.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <initializer_list>

static int checks;
#define CHECK(value) do { ++checks; if (!(value)) { \
    std::fprintf(stderr, "Overlay contract failed at line %d: %s\n", __LINE__, #value); \
    std::exit(1); } } while (0)

static VKAPI_ATTR void VKAPI_CALL UnexpectedGpuCall() {
    std::fprintf(stderr, "Partial Vulkan loader reached GPU initialization\n");
    std::abort();
}

struct Resolver {
    const char* missing;
    bool core_dynamic;
    unsigned calls = 0;
};

static PFN_vkVoidFunction Resolve(const char* name, void* user) {
    auto& state = *static_cast<Resolver*>(user);
    ++state.calls;
    if ((state.missing && !std::strcmp(name, state.missing)) ||
        (!state.core_dynamic && (!std::strcmp(name, "vkCmdBeginRendering") ||
                                 !std::strcmp(name, "vkCmdEndRendering")))) return nullptr;
    return UnexpectedGpuCall;
}

int main() {
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = VK_API_VERSION_1_2;
    info.UseDynamicRendering = true;
    // Include functions before and after the WSI entries in ImGui's table.
    // A late failure leaves more pointers populated, but still must not init.
    for (const char* missing : {"vkAllocateCommandBuffers", "vkCreateSwapchainKHR",
         "vkDestroySurfaceKHR", "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
         "vkGetPhysicalDeviceSurfaceFormatsKHR", "vkGetPhysicalDeviceSurfacePresentModesKHR",
         "vkWaitForFences", "vkCmdBeginRenderingKHR", "vkCmdEndRenderingKHR"}) {
        ImGui::CreateContext();
        Resolver resolver{missing, false};
        CHECK(!BbOverlay::InitializeVulkanBackend(info, Resolve, &resolver));
        CHECK(resolver.calls > 0);
        CHECK(ImGui::GetIO().BackendRendererUserData == nullptr);
        ImGui::DestroyContext();
    }
    // Vulkan 1.2 with KHR aliases is valid; absent 1.3 core aliases must not
    // disable the overlay. Loading alone does not invoke any GPU functions.
    Resolver khr{nullptr, false};
    CHECK(BbOverlay::LoadVulkanBackendFunctions(info, Resolve, &khr));
    Resolver core{nullptr, true};
    info.ApiVersion = VK_API_VERSION_1_3;
    CHECK(BbOverlay::LoadVulkanBackendFunctions(info, Resolve, &core));
    // A previously complete load must not allow a subsequent partial one
    // to initialize using stale global backend state.
    ImGui::CreateContext();
    Resolver retry{"vkDestroySurfaceKHR", true};
    CHECK(!BbOverlay::InitializeVulkanBackend(info, Resolve, &retry));
    CHECK(ImGui::GetIO().BackendRendererUserData == nullptr);
    ImGui::DestroyContext();
    std::printf("Overlay loader contract: PASS (%d checks; no GPU calls)\n", checks);
}
