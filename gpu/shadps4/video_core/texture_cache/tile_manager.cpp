// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef UNCHARTED_UWP
#include "boot_trace.h"
#include "depth_writeback.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#endif
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/texture_cache/image.h"
#include "video_core/texture_cache/image_info.h"
#include "video_core/texture_cache/image_view.h"
#include "video_core/texture_cache/tile_manager.h"

#include "video_core/host_shaders/tiling_macro_128_comp.h"
#include "video_core/host_shaders/tiling_macro_16_comp.h"
#include "video_core/host_shaders/tiling_macro_32_comp.h"
#include "video_core/host_shaders/tiling_macro_64_comp.h"
#include "video_core/host_shaders/tiling_macro_8_comp.h"
#include "video_core/host_shaders/tiling_macro_96_comp.h"
#include "video_core/host_shaders/tiling_micro_128_comp.h"
#include "video_core/host_shaders/tiling_micro_16_comp.h"
#include "video_core/host_shaders/tiling_micro_32_comp.h"
#include "video_core/host_shaders/tiling_micro_64_comp.h"
#include "video_core/host_shaders/tiling_micro_8_comp.h"
#include "video_core/host_shaders/tiling_micro_96_comp.h"

#include <magic_enum/magic_enum.hpp>
#include <vk_mem_alloc.h>

namespace VideoCore {

struct TilingInfo {
    u32 bank_swizzle;
    u32 num_slices;
    u32 num_mips;
    std::array<ImageInfo::MipInfo, 16> mips;
};

TileManager::TileManager(const Vulkan::Instance& instance_, Vulkan::Scheduler& scheduler_,
                         Vulkan::Runtime& runtime_, StreamBuffer& stream_buffer_)
    : instance{instance_}, scheduler{scheduler_}, runtime{runtime_}, stream_buffer{stream_buffer_} {
    const auto device = instance.GetDevice();
    const std::array<vk::DescriptorSetLayoutBinding, 3> bindings = {{
        {
            .binding = 0,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
        },
        {
            .binding = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
        },
        {
            .binding = 2,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
        },
    }};

    const vk::DescriptorSetLayoutCreateInfo desc_layout_ci = {
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    };
    auto desc_layout_result = device.createDescriptorSetLayoutUnique(desc_layout_ci);
    ASSERT_MSG(desc_layout_result.result == vk::Result::eSuccess,
               "Failed to create descriptor set layout: {}",
               vk::to_string(desc_layout_result.result));
    desc_layout = std::move(desc_layout_result.value);

    const vk::DescriptorSetLayout set_layout = *desc_layout;
    const vk::PipelineLayoutCreateInfo layout_info = {
        .setLayoutCount = 1U,
        .pSetLayouts = &set_layout,
        .pushConstantRangeCount = 0U,
        .pPushConstantRanges = nullptr,
    };
    auto [layout_result, layout] = device.createPipelineLayoutUnique(layout_info);
    ASSERT_MSG(layout_result == vk::Result::eSuccess, "Failed to create pipeline layout: {}",
               vk::to_string(layout_result));
    pl_layout = std::move(layout);
}

TileManager::~TileManager() = default;

vk::Pipeline TileManager::GetTilingPipeline(const ImageInfo& info, bool is_tiler) {
#ifdef UNCHARTED_UWP
    const auto trace_id=(u64(info.tile_mode)<<32)|(u64(info.num_bits)<<16)|(u64(info.num_samples)<<1)|u64(is_tiler);
    UnchartedBootTrace::Scope trace(UnchartedBootTrace::Kind::TilingPipeline,trace_id,reinterpret_cast<uintptr_t>(this));
#endif
    const TilingKey key{
        .tile_mode = info.tile_mode,
        .num_bits = info.num_bits,
        .num_samples = info.num_samples,
        .is_tiler = is_tiler,
    };
    if (const auto it = tiling_pipelines.find(key); it != tiling_pipelines.end()) {
        return *it->second;
    }

    const auto device = instance.GetDevice();
    const auto micro_tile_mode = AmdGpu::GetMicroTileMode(info.tile_mode);
    const bool is_macro = AmdGpu::IsMacroTiled(info.array_mode);
    std::array<u32, 12> spec_data{
        info.num_samples,
        u32(micro_tile_mode),
        AmdGpu::GetMicroTileThickness(info.array_mode),
        u32(is_tiler),
    };
    if (is_macro) {
        const auto macro_tile_mode =
            AmdGpu::CalculateMacrotileMode(info.tile_mode, info.num_bits, info.num_samples);
        spec_data[4] = u32(info.array_mode);
        spec_data[5] = u32(AmdGpu::GetPipeConfig(info.tile_mode));
        spec_data[6] = AmdGpu::GetBankWidth(macro_tile_mode);
        spec_data[7] = AmdGpu::GetBankHeight(macro_tile_mode);
        spec_data[8] = AmdGpu::GetNumBanks(macro_tile_mode);
        spec_data[9] = std::bit_width(spec_data[8]) - 1;
        spec_data[10] = AmdGpu::CalculateTileSplit(info.tile_mode, info.array_mode, micro_tile_mode,
                                                   info.num_bits);
        spec_data[11] = AmdGpu::GetMacrotileAspect(macro_tile_mode);
    }

    std::span<const u32> code;
    switch (info.num_bits) {
    case 8:
        code = is_macro ? std::span<const u32>{TILING_MACRO_8_COMP}
                        : std::span<const u32>{TILING_MICRO_8_COMP};
        break;
    case 16:
        code = is_macro ? std::span<const u32>{TILING_MACRO_16_COMP}
                        : std::span<const u32>{TILING_MICRO_16_COMP};
        break;
    case 32:
        code = is_macro ? std::span<const u32>{TILING_MACRO_32_COMP}
                        : std::span<const u32>{TILING_MICRO_32_COMP};
        break;
    case 64:
        code = is_macro ? std::span<const u32>{TILING_MACRO_64_COMP}
                        : std::span<const u32>{TILING_MICRO_64_COMP};
        break;
    case 96:
        code = is_macro ? std::span<const u32>{TILING_MACRO_96_COMP}
                        : std::span<const u32>{TILING_MICRO_96_COMP};
        break;
    case 128:
        code = is_macro ? std::span<const u32>{TILING_MACRO_128_COMP}
                        : std::span<const u32>{TILING_MICRO_128_COMP};
        break;
    default:
        UNREACHABLE_MSG("Unsupported tiling pixel width {}", info.num_bits);
    }

    const auto module = Vulkan::CompileSPV(code, device);
    static constexpr auto spec_entries = [] {
        std::array<vk::SpecializationMapEntry, 12> entries{};
        for (u32 i = 0; i < entries.size(); ++i) {
            entries[i] = vk::SpecializationMapEntry{i, i * u32(sizeof(u32)), sizeof(u32)};
        }
        return entries;
    }();

    const u32 spec_count = is_macro ? 12 : 4;
    const vk::SpecializationInfo specialization{
        .mapEntryCount = spec_count,
        .pMapEntries = spec_entries.data(),
        .dataSize = spec_count * sizeof(u32),
        .pData = spec_data.data(),
    };

    const auto module_name = fmt::format("{}_{} {}", magic_enum::enum_name(info.tile_mode),
                                         info.num_bits, is_tiler ? "tiler" : "detiler");
    LOG_INFO(Render_Vulkan, "Creating tiling pipeline {}", module_name);
    Vulkan::SetObjectName(device, module, module_name);

    const vk::PipelineShaderStageCreateInfo shader_ci = {
        .stage = vk::ShaderStageFlagBits::eCompute,
        .module = module,
        .pName = "main",
        .pSpecializationInfo = &specialization,
    };
    const vk::ComputePipelineCreateInfo compute_pipeline_ci = {
        .stage = shader_ci,
        .layout = *pl_layout,
    };

    auto [result, pipeline] = [&] {
#ifdef UNCHARTED_UWP
        UnchartedBootTrace::Scope driver_trace(UnchartedBootTrace::Kind::DriverTiling,trace_id,reinterpret_cast<uintptr_t>(this));
#endif
        return device.createComputePipelineUnique(VK_NULL_HANDLE, compute_pipeline_ci);
    }();
    ASSERT_MSG(result == vk::Result::eSuccess, "Detiler pipeline creation failed {}",
               vk::to_string(result));
    const auto handle = *pipeline;
    tiling_pipelines.emplace(key, std::move(pipeline));
    device.destroyShaderModule(module);
    return handle;
}

void TileManager::RecordTilingDispatch(vk::Pipeline pipeline, const vk::DescriptorBufferInfo& tiled,
                                       const vk::DescriptorBufferInfo& linear,
                                       const vk::DescriptorBufferInfo& params, u32 dim_x) {
    scheduler.Record([pipeline, layout = *pl_layout, tiled, linear, params,
                      dim_x](vk::CommandBuffer cmdbuf) {
        cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
        const std::array<vk::WriteDescriptorSet, 3> set_writes = {{
            {
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &tiled,
            },
            {
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &linear,
            },
            {
                .dstBinding = 2,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &params,
            },
        }};
        cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, layout, 0, set_writes);
        cmdbuf.dispatch(dim_x, 1, 1);
    });
}

std::pair<const Buffer*, u64> TileManager::DetileImage(const VideoCore::Buffer* in_buffer,
                                                       u64 in_offset, const ImageInfo& info) {
    if (!info.props.is_tiled) {
        return {in_buffer, in_offset};
    }

    TilingInfo params{};
    params.bank_swizzle = info.bank_swizzle;
    params.num_slices = info.props.is_volume ? info.size.depth : info.resources.layers;
    params.num_mips = info.resources.levels;
    for (u32 mip = 0; mip < params.num_mips; ++mip) {
        auto& mip_info = params.mips[mip];
        mip_info = info.mips_layout[mip];
        if (info.props.is_block) {
            mip_info.pitch = std::max((mip_info.pitch + 3) / 4, 1U);
            mip_info.height = std::max((mip_info.height + 3) / 4, 1U);
        }
    }

    const vk::DescriptorBufferInfo params_buffer_info{
        .buffer = stream_buffer.Handle(),
        .offset = stream_buffer.Copy(&params, sizeof(params), instance.UniformMinAlignment()),
        .range = sizeof(params),
    };

    const auto staging = runtime.GetStagingPool().Request(info.guest_size, MemoryType::DeviceLocal);

    scheduler.EndRendering();
    runtime.FlushBarriers();

    const vk::DescriptorBufferInfo tiled_buffer_info{
        .buffer = in_buffer->Handle(),
        .offset = in_offset,
        .range = info.guest_size,
    };
    const vk::DescriptorBufferInfo linear_buffer_info{
        .buffer = staging.buffer->Handle(),
        .offset = staging.offset,
        .range = info.guest_size,
    };
    const auto dim_x = static_cast<u32>((info.guest_size / (info.num_bits / 8)) / 64);
    // bbport: recorded with copies of the descriptor infos (threaded recording).
    RecordTilingDispatch(GetTilingPipeline(info, false), tiled_buffer_info, linear_buffer_info,
                         params_buffer_info, dim_x);

    runtime.AccessBuffer(staging.buffer, staging.offset, info.guest_size,
                         vk::PipelineStageFlagBits2::eComputeShader,
                         vk::AccessFlagBits2::eShaderWrite);

    return {staging.buffer, staging.offset};
}

void TileManager::TileImage(Image& in_image, std::span<vk::BufferImageCopy> buffer_copies,
                            const VideoCore::Buffer* out_buffer, u64 out_offset) {
    const auto& info = in_image.info;
    if (!info.props.is_tiled) {
        for (auto& copy : buffer_copies) {
            copy.bufferOffset += out_offset;
        }
        runtime.DownloadImage(&in_image, out_buffer, buffer_copies);
        return;
    }

#ifdef UNCHARTED_UWP
    static const bool trace_enabled = [] {
        const char* text = std::getenv("UNCHARTED_DEPTH_WRITEBACK");
        return UnchartedDepthWriteback::Parse(text ? text : "off") ==
               UnchartedDepthWriteback::Mode::Stages;
    }();
    static std::atomic<u64> traced{0};
    const u64 trace_id = trace_enabled && info.tile_mode == AmdGpu::TileMode::Depth2DThin64 &&
                                 info.num_bits == 32
                            ? traced.fetch_add(1) + 1 : 0;
    // Bound intrusive submissions/logging if the title progresses into gameplay.
    const bool trace = trace_id && trace_id <= 32;
    if (trace_id == 33) {
        std::puts("Depth writeback: stage checkpoint budget exhausted after 32 operations");
        std::fflush(stdout);
    }
    if (trace) {
        std::printf("Depth writeback #%llu begin guest=%#llx bytes=%u image=%ux%ux%u "
                    "mips=%u layers=%u samples=%u format=%s depth=%u stencil=%u "
                    "copies=%zu out_offset=%llu out_capacity=%llu bank_swizzle=%u\n",
                    trace_id, info.guest_address, info.guest_size, info.size.width,
                    info.size.height, info.size.depth, info.resources.levels,
                    info.resources.layers, info.num_samples, vk::to_string(info.pixel_format).c_str(),
                    info.props.is_depth, info.props.has_stencil, buffer_copies.size(), out_offset,
                    static_cast<u64>(out_buffer->SizeBytes()), info.bank_swizzle);
        std::fflush(stdout);
        if (buffer_copies.empty() || buffer_copies.size() > 16 ||
            !UnchartedDepthWriteback::Contains(out_buffer->SizeBytes(), out_offset, info.guest_size))
            throw std::runtime_error("Depth writeback: invalid mip count or output descriptor range");
        runtime.FlushBarriers();
        scheduler.CheckpointDepthWriteback(trace_id, "prior");
    }
#endif

    TilingInfo params{};
    params.bank_swizzle = info.bank_swizzle;
    params.num_slices = info.props.is_volume ? info.size.depth : info.resources.layers;
    params.num_mips = static_cast<u32>(buffer_copies.size());
    for (u32 mip = 0; mip < params.num_mips; ++mip) {
        auto& mip_info = params.mips[mip];
        mip_info = info.mips_layout[mip];
        if (info.props.is_block) {
            mip_info.pitch = std::max((mip_info.pitch + 3) / 4, 1U);
            mip_info.height = std::max((mip_info.height + 3) / 4, 1U);
        }
    }

    const vk::DescriptorBufferInfo params_buffer_info{
        .buffer = stream_buffer.Handle(),
        .offset = stream_buffer.Copy(&params, sizeof(params), instance.UniformMinAlignment()),
        .range = sizeof(params),
    };

    const auto staging = runtime.GetStagingPool().Request(info.guest_size, MemoryType::DeviceLocal, 0,
#ifdef UNCHARTED_UWP
                                                         trace
#else
                                                         false
#endif
    );
#ifdef UNCHARTED_UWP
    if (trace) {
        std::printf("Depth writeback #%llu staging_offset=%llu staging_capacity=%llu "
                    "descriptor_bytes=%u dispatch_x=%u params_offset=%llu params_bytes=%zu\n",
                    trace_id, staging.offset, static_cast<u64>(staging.buffer->SizeBytes()),
                    info.guest_size, (info.guest_size / 4) / 64, params_buffer_info.offset,
                    sizeof(params));
        for (u32 mip = 0; mip < buffer_copies.size(); ++mip) {
            const auto& c = buffer_copies[mip];
            const auto& layout = info.mips_layout[mip];
            // This footprint is exact only for the 32-bit depth plane; other
            // aspects/formats are logged without asserting that interpretation.
            const bool plane32 = c.imageSubresource.aspectMask == vk::ImageAspectFlagBits::eDepth &&
                (info.pixel_format == vk::Format::eD32Sfloat ||
                 info.pixel_format == vk::Format::eD32SfloatS8Uint ||
                 info.pixel_format == vk::Format::eD24UnormS8Uint ||
                 info.pixel_format == vk::Format::eX8D24UnormPack32);
            const auto footprint = UnchartedDepthWriteback::CopyBytes32(
                c.imageExtent.width, c.imageExtent.height, c.imageExtent.depth,
                c.imageSubresource.layerCount, c.bufferRowLength, c.bufferImageHeight);
            const bool fits = footprint && UnchartedDepthWriteback::Contains(
                info.guest_size, c.bufferOffset, *footprint);
            std::printf("Depth writeback #%llu copy=%u mip=%u base_layer=%u layers=%u "
                        "extent=%ux%ux%u row=%u height=%u offset=%llu aspect=%u "
                        "plane32=%u footprint=%llu fits_guest=%u "
                        "layout_size=%u layout_pitch=%u layout_height=%u layout_offset=%u\n",
                        trace_id, mip, c.imageSubresource.mipLevel,
                        c.imageSubresource.baseArrayLayer, c.imageSubresource.layerCount,
                        c.imageExtent.width, c.imageExtent.height, c.imageExtent.depth,
                        c.bufferRowLength, c.bufferImageHeight, c.bufferOffset,
                        static_cast<u32>(c.imageSubresource.aspectMask), plane32,
                        footprint.value_or(0), fits, layout.size, layout.pitch,
                        layout.height, layout.offset);
            std::fflush(stdout);
            if (plane32 && !fits)
                throw std::runtime_error("Depth writeback: copy exceeds guest-sized staging range");
        }
        if (!UnchartedDepthWriteback::Contains(staging.buffer->SizeBytes(), staging.offset, info.guest_size))
            throw std::runtime_error("Depth writeback: staging descriptor exceeds buffer");
    }
#endif
    for (auto& copy : buffer_copies) {
        copy.bufferOffset += staging.offset;
    }

    runtime.DownloadImage(&in_image, staging.buffer, buffer_copies);
    runtime.FlushBarriers();

#ifdef UNCHARTED_UWP
    if (trace) scheduler.CheckpointDepthWriteback(trace_id, "copy");
#endif

    const vk::DescriptorBufferInfo tiled_buffer_info{
        .buffer = out_buffer->Handle(),
        .offset = out_offset,
        .range = info.guest_size,
    };
    const vk::DescriptorBufferInfo linear_buffer_info{
        .buffer = staging.buffer->Handle(),
        .offset = staging.offset,
        .range = info.guest_size,
    };
    const auto dim_x = static_cast<u32>((info.guest_size / (info.num_bits / 8)) / 64);
    // bbport: recorded with copies of the descriptor infos (threaded recording).
#ifdef UNCHARTED_UWP
    if (trace) {
        std::printf("Depth writeback #%llu tiler_pipeline_begin\n", trace_id);
        std::fflush(stdout);
    }
#endif
    RecordTilingDispatch(GetTilingPipeline(info, true), tiled_buffer_info, linear_buffer_info,
                         params_buffer_info, dim_x);

    runtime.AccessBuffer(out_buffer, out_offset, info.guest_size,
                         vk::PipelineStageFlagBits2::eComputeShader,
                         vk::AccessFlagBits2::eShaderWrite);
#ifdef UNCHARTED_UWP
    if (trace) {
        runtime.FlushBarriers();
        scheduler.CheckpointDepthWriteback(trace_id, "tiler");
        runtime.GetStagingPool().FreeDeferred(staging);
        std::printf("Depth writeback #%llu complete\n", trace_id);
        std::fflush(stdout);
    }
#endif
}

} // namespace VideoCore
