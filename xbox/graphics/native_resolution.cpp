#include "video_core/renderer_vulkan/vk_temporal_upscaler.h"
#include "bbport_settings.h"
#include "video_core/renderer_vulkan/vk_frame_generation.h"
#include "video_core/renderer_vulkan/vk_scene_resolution.h"
#include "render_resolution.h"
#include <cstdlib>
#include <stdexcept>

namespace Vulkan {
TemporalUpscaler::TemporalUpscaler(const Instance& i, Scheduler& s, VideoCore::TextureCache& t,
                                 Runtime& r, CameraMotion& c, SceneTargets& targets)
    : instance(i), scheduler(s), texture_cache(t), runtime(r), camera_motion(c), scene_targets(targets) {
    BbSettings::ConfigureUpscalerSupport(false, false);
    BbSettings::Get().upscaler=BbSettings::UpscalerOff;
    BbSettings::Get().frame_gen=BbSettings::FrameGenOff;
    const char* value=std::getenv("UNCHARTED_RENDER_RESOLUTION");
    const auto resolution=UnchartedResolution::Parse(value?value:"");
    if(!resolution) throw std::runtime_error("Invalid UWP render resolution");
    scene_targets.SetSize({resolution->width,resolution->height});
    render_width=resolution->width;render_height=resolution->height;
    BbSettings::Get().active_render_width=render_width;
    BbSettings::Get().active_render_height=render_height;
}
TemporalUpscaler::~TemporalUpscaler() = default;
void TemporalUpscaler::OnDrawRecorded(u64, VideoCore::ImageId) {}
void TemporalUpscaler::OnSceneColor(VideoCore::ImageId) {}
void TemporalUpscaler::OnBlendedSceneDraw() {}
void TemporalUpscaler::OnSceneComposite() {}
void TemporalUpscaler::OnDispatch(u64) {}
bool TemporalUpscaler::OnFrameStart() { return false; }
bool TemporalUpscaler::RasterScaling() const { return scene_targets.Reduced(); }
float TemporalUpscaler::SceneMipBias() const { return 0; }
void TemporalUpscaler::OnDraw(u64, VideoCore::ImageId, VideoCore::ImageId, bool) {}
void TemporalUpscaler::OnColorTarget(VideoCore::ImageId) {}
bool TemporalUpscaler::RedirectColor(VideoCore::ImageId, const VideoCore::ImageViewInfo&, Target&) { return false; }
bool TemporalUpscaler::RedirectDepth(VideoCore::ImageId, Target&) { return false; }
bool TemporalUpscaler::RedirectSampled(VideoCore::ImageId, const VideoCore::ImageViewInfo&,
                                     vk::ImageView&, vk::ImageLayout&) { return false; }
bool TemporalUpscaler::DisplayOverride(VAddr, Display&) { return false; }
struct Fsr4Upscaler::Impl {};
Fsr4Upscaler::Fsr4Upscaler(const Instance&, Scheduler&) {}
Fsr4Upscaler::~Fsr4Upscaler() = default;
bool Fsr4Upscaler::Record(const Frame&) { return false; }
const char* Fsr4Upscaler::Problem() const noexcept { return "FSR 4 is unavailable on Xbox"; }
bool Fsr4Upscaler::Fatal() const noexcept { return true; }

struct Dlss::Impl {};
Dlss::Dlss() = default;
Dlss::~Dlss() = default;
Dlss* Dlss::Get() { return nullptr; }
void Dlss::AppendInstanceExtensions(std::vector<const char*>&) {}
void Dlss::AppendDeviceExtensions(vk::Instance, vk::PhysicalDevice, std::vector<const char*>&) {}
void Dlss::Initialize(vk::Instance, vk::PhysicalDevice, vk::Device) {}
void Dlss::Shutdown() {}
bool Dlss::Available() const { return false; }
std::string Dlss::Problem() const { return "DLSS is unavailable on Xbox"; }
int Dlss::QualityForScale(float) { return 0; }
bool Dlss::HasFeature(const FeatureDesc&) const { return false; }
bool Dlss::CreateFeature(vk::CommandBuffer, const FeatureDesc&) { return false; }
bool Dlss::Evaluate(vk::CommandBuffer, const Frame&) { return false; }
void Dlss::ReleaseFeature() {}
int Dlss::FrameGenMaxCount() const { return 0; }
bool Dlss::HasFrameGen(const FrameGenDesc&) const { return false; }
bool Dlss::CreateFrameGen(vk::CommandBuffer, const FrameGenDesc&) { return false; }
bool Dlss::EvaluateFrameGen(vk::CommandBuffer, const FrameGenFrame&) { return false; }
void Dlss::ReleaseFrameGen() {}
FrameGenerator::FrameGenerator(const Instance& i, Scheduler& s) : instance(i), scheduler(s) {}
FrameGenerator::~FrameGenerator() = default;
bool FrameGenerator::Wanted() const { return false; }
vk::ImageView FrameGenerator::BeginCapture(vk::CommandBuffer, vk::ImageView, u32, u32) { return {}; }
void FrameGenerator::EndCapture(vk::CommandBuffer, std::array<float, 2>, bool) {}
void FrameGenerator::CaptureHudless(vk::CommandBuffer, vk::Image, vk::Format, u32, u32) {}
vk::ImageView FrameGenerator::Hudless(vk::Format, vk::Extent2D& size) { size={0,0}; return {}; }
u32 FrameGenerator::Generate(vk::CommandBuffer, vk::Image, vk::ImageView, vk::ImageUsageFlags,
    vk::Image, vk::ImageView, u32, u32, std::span<const Output>) { return 0; }
void FrameGenerator::DumpIfRequested(vk::CommandBuffer, vk::Image, u32, u32, const char*) {}
}
