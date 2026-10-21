// Adapted from Cosmo/gow3_xbox d0975c91 (GPL-2.0-or-later).
#include "present_bridge.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/blit_helper.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/texture_cache/tile_manager.h"
#include "video_core/host_shaders/uwp_texture_sample_comp.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <stdexcept>
namespace UnchartedUwp {
static void ReadbackBarrier(vk::CommandBuffer command) {
    const vk::MemoryBarrier2 barrier{
        .srcStageMask=vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask=vk::AccessFlagBits2::eMemoryWrite,
        .dstStageMask=vk::PipelineStageFlagBits2::eAllCommands|vk::PipelineStageFlagBits2::eHost,
        .dstAccessMask=vk::AccessFlagBits2::eMemoryRead|vk::AccessFlagBits2::eMemoryWrite|vk::AccessFlagBits2::eHostRead};
    command.pipelineBarrier2(vk::DependencyInfo{.memoryBarrierCount=1,.pMemoryBarriers=&barrier});
}
template <typename T> static T ProbeCheck(vk::ResultValue<T>&& result) {
    if(result.result!=vk::Result::eSuccess) {
        std::fprintf(stderr,"Texture probe Vulkan error: %d\n",int(result.result));
        throw std::runtime_error("Texture probe resource/pipeline creation failed");
    }
    return std::move(result.value);
}
static bool SampleProbe(const Vulkan::Instance& instance,Vulkan::Scheduler& scheduler,vk::Image image,const VideoCore::Buffer& input,
                        vk::Format format,std::span<const u8> reference) {
    const auto device=instance.GetDevice();
    const std::array bindings{
        vk::DescriptorSetLayoutBinding{0,vk::DescriptorType::eStorageBuffer,128,vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{128,vk::DescriptorType::eStorageBuffer,1,vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{129,vk::DescriptorType::eUniformBuffer,1,vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{130,vk::DescriptorType::eSampledImage,1,vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{131,vk::DescriptorType::eSampler,1,vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{132,vk::DescriptorType::eSampledImage,1,vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{133,vk::DescriptorType::eSampler,1,vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{134,vk::DescriptorType::eStorageImage,1,vk::ShaderStageFlagBits::eCompute}};
    auto descriptors=ProbeCheck(device.createDescriptorSetLayoutUnique({.flags=vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
        .bindingCount=u32(bindings.size()),.pBindings=bindings.data()}));
    const auto set=*descriptors;
    auto layout=ProbeCheck(device.createPipelineLayoutUnique({.setLayoutCount=1,.pSetLayouts=&set}));
    auto module=ProbeCheck(device.createShaderModuleUnique({.codeSize=sizeof(UWP_TEXTURE_SAMPLE_COMP),.pCode=UWP_TEXTURE_SAMPLE_COMP}));
    auto pipeline=ProbeCheck(device.createComputePipelineUnique(VK_NULL_HANDLE,{
        .stage={.stage=vk::ShaderStageFlagBits::eCompute,.module=*module,.pName="main"},.layout=*layout}));
    auto view=ProbeCheck(device.createImageViewUnique({.image=image,.viewType=vk::ImageViewType::e2D,
        .format=format,.subresourceRange={vk::ImageAspectFlagBits::eColor,0,3,0,1}}));
    auto sampler=ProbeCheck(device.createSamplerUnique({.magFilter=vk::Filter::eNearest,.minFilter=vk::Filter::eNearest,
        .mipmapMode=vk::SamplerMipmapMode::eNearest,.maxLod=2}));
    VideoCore::Buffer output(instance,0,68*52*3*4*sizeof(float),VideoCore::MemoryType::HostCached);
    VideoCore::UniqueImage stored(device,instance.GetAllocator());
    stored.Create({.imageType=vk::ImageType::e2D,.format=vk::Format::eR8G8B8A8Unorm,.extent={68,52,1},
        .mipLevels=1,.arrayLayers=1,.samples=vk::SampleCountFlagBits::e1,.tiling=vk::ImageTiling::eOptimal,
        .usage=vk::ImageUsageFlagBits::eStorage|vk::ImageUsageFlagBits::eTransferSrc,.sharingMode=vk::SharingMode::eExclusive});
    auto stored_view=ProbeCheck(device.createImageViewUnique({.image=stored.image,.viewType=vk::ImageViewType::e2D,
        .format=vk::Format::eR8G8B8A8Unorm,.subresourceRange={vk::ImageAspectFlagBits::eColor,0,1,0,1}}));
    VideoCore::Buffer stored_readback(instance,0,68*52*4,VideoCore::MemoryType::HostCached);
    std::array<vk::DescriptorBufferInfo,128> inputs;
    inputs.fill({input.Handle(),0,input.SizeBytes()});
    const vk::DescriptorBufferInfo result{output.Handle(),0,output.SizeBytes()};
    const vk::DescriptorImageInfo sampled{*sampler,*view,vk::ImageLayout::eShaderReadOnlyOptimal};
    const vk::DescriptorImageInfo storage{VK_NULL_HANDLE,*stored_view,vk::ImageLayout::eGeneral};
    const std::array writes{
        vk::WriteDescriptorSet{.dstBinding=0,.descriptorCount=128,.descriptorType=vk::DescriptorType::eStorageBuffer,.pBufferInfo=inputs.data()},
        vk::WriteDescriptorSet{.dstBinding=128,.descriptorCount=1,.descriptorType=vk::DescriptorType::eStorageBuffer,.pBufferInfo=&result},
        vk::WriteDescriptorSet{.dstBinding=130,.descriptorCount=1,.descriptorType=vk::DescriptorType::eSampledImage,.pImageInfo=&sampled},
        vk::WriteDescriptorSet{.dstBinding=131,.descriptorCount=1,.descriptorType=vk::DescriptorType::eSampler,.pImageInfo=&sampled},
        vk::WriteDescriptorSet{.dstBinding=132,.descriptorCount=1,.descriptorType=vk::DescriptorType::eSampledImage,.pImageInfo=&sampled},
        vk::WriteDescriptorSet{.dstBinding=133,.descriptorCount=1,.descriptorType=vk::DescriptorType::eSampler,.pImageInfo=&sampled},
        vk::WriteDescriptorSet{.dstBinding=134,.descriptorCount=1,.descriptorType=vk::DescriptorType::eStorageImage,.pImageInfo=&storage}};
    scheduler.Record([&](vk::CommandBuffer cmd) {
        const vk::ImageMemoryBarrier2 barrier{.srcStageMask=vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask=vk::AccessFlagBits2::eMemoryWrite,.dstStageMask=vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask=vk::AccessFlagBits2::eShaderRead,.oldLayout=vk::ImageLayout::eTransferSrcOptimal,
            .newLayout=vk::ImageLayout::eShaderReadOnlyOptimal,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=image,.subresourceRange={vk::ImageAspectFlagBits::eColor,0,3,0,2}};
        cmd.pipelineBarrier2(vk::DependencyInfo{.imageMemoryBarrierCount=1,.pImageMemoryBarriers=&barrier});
        vk::ImageMemoryBarrier2 storage_barrier{.dstStageMask=vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask=vk::AccessFlagBits2::eShaderWrite,.oldLayout=vk::ImageLayout::eUndefined,
            .newLayout=vk::ImageLayout::eGeneral,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=stored.image,
            .subresourceRange={vk::ImageAspectFlagBits::eColor,0,1,0,1}};
        cmd.pipelineBarrier2(vk::DependencyInfo{.imageMemoryBarrierCount=1,.pImageMemoryBarriers=&storage_barrier});
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute,*pipeline);
        cmd.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute,*layout,0,writes);
        cmd.dispatch((68*52+63)/64,1,1);
        storage_barrier.srcStageMask=vk::PipelineStageFlagBits2::eComputeShader;
        storage_barrier.srcAccessMask=vk::AccessFlagBits2::eShaderWrite;
        storage_barrier.dstStageMask=vk::PipelineStageFlagBits2::eCopy;
        storage_barrier.dstAccessMask=vk::AccessFlagBits2::eTransferRead;
        storage_barrier.oldLayout=vk::ImageLayout::eGeneral;
        storage_barrier.newLayout=vk::ImageLayout::eTransferSrcOptimal;
        cmd.pipelineBarrier2(vk::DependencyInfo{.imageMemoryBarrierCount=1,.pImageMemoryBarriers=&storage_barrier});
        const vk::BufferImageCopy copy{.imageSubresource={vk::ImageAspectFlagBits::eColor,0,0,1},.imageExtent={68,52,1}};
        cmd.copyImageToBuffer(stored.image,vk::ImageLayout::eTransferSrcOptimal,stored_readback.Handle(),copy);
        ReadbackBarrier(cmd);
    });
    scheduler.Finish();
    output.Invalidate(0,output.SizeBytes());
    stored_readback.Invalidate(0,stored_readback.SizeBytes());
    auto values=reinterpret_cast<const float*>(output.mapped_data.data());
    u32 errors=0,first=0;
    for(u32 i=0;i<68*52;++i) for(u32 sample=0;sample<2;++sample) for(u32 channel=0;channel<4;++channel) {
        const float expected=float(reference[i*4+channel])/255;
        const u32 slot=i*12+sample*4+channel;
        if(std::abs(values[slot]-expected)>0.01f||!std::isfinite(values[slot])) { if(!errors) first=slot; ++errors; }
    }
    std::printf("Texture sampling %s with 128 SSBOs and binding gaps: %s (%u components wrong, first %u got %g)\n",
        vk::to_string(format).c_str(),errors?"FAIL":"PASS",errors,first,values[first]);
    u32 store_errors=0;
    for(u32 i=0;i<68*52*4;++i) store_errors+=std::abs(int(stored_readback.mapped_data[i])-int(reference[i]))>2;
    std::printf("Texture GENERAL storage-image writes: %s (%u components wrong)\n",store_errors?"FAIL":"PASS",store_errors);
    errors+=store_errors;
    return errors==0;
}
static bool CopyProbe(const Vulkan::Instance& instance,Vulkan::Scheduler& scheduler,vk::Format format,u32 block,u32 bytes) {
    const auto device=instance.GetDevice();
    const vk::ImageCreateInfo ci{
        .imageType=vk::ImageType::e2D,.format=format,.extent={68,52,1},.mipLevels=3,.arrayLayers=2,
        .samples=vk::SampleCountFlagBits::e1,.tiling=vk::ImageTiling::eOptimal,
        .usage=vk::ImageUsageFlagBits::eTransferSrc|vk::ImageUsageFlagBits::eTransferDst|vk::ImageUsageFlagBits::eSampled,
        .sharingMode=vk::SharingMode::eExclusive};
    vk::UniqueDeviceMemory memory;
    vk::UniqueImage image;
    auto created_image=ProbeCheck(device.createImageUnique(ci));
    auto req=device.getImageMemoryRequirements(*created_image);
    u32 memory_type=0;
    while(!(req.memoryTypeBits&(1U<<memory_type))) ++memory_type;
    memory=ProbeCheck(device.allocateMemoryUnique(vk::MemoryAllocateInfo{.allocationSize=req.size,.memoryTypeIndex=memory_type}));
    image=std::move(created_image);
    if(device.bindImageMemory(*image,*memory,0)!=vk::Result::eSuccess) return false;
    std::vector<vk::BufferImageCopy> uploads,downloads;
    std::vector<u8> expected;
    u32 input_size=512,output_size=0;
    for(u32 mip=0;mip<3;++mip) {
        const u32 w=68>>mip,h=52>>mip;
        const u32 row=((w+block-1)/block)*bytes,rows=(h+block-1)/block;
        const u32 pitch=((80>>mip)+block-1)/block,padded_h=((64>>mip)+block-1)/block;
        uploads.push_back({input_size,pitch*block,padded_h*block,{vk::ImageAspectFlagBits::eColor,mip,0,2},{0,0,0},{w,h,1}});
        downloads.push_back({output_size,0,0,{vk::ImageAspectFlagBits::eColor,mip,0,2},{0,0,0},{w,h,1}});
        input_size+=pitch*padded_h*bytes*2;
        output_size+=row*rows*2;
    }
    VideoCore::Buffer input(instance,0,input_size+512,VideoCore::MemoryType::HostUncached);
    VideoCore::Buffer output(instance,0,output_size+512,VideoCore::MemoryType::HostCached);
    std::fill(input.mapped_data.begin(),input.mapped_data.end(),0xCD);
    for(u32 mip=0;mip<3;++mip) {
        const auto c=uploads[mip];
        const u32 row=((c.imageExtent.width+block-1)/block)*bytes,rows=(c.imageExtent.height+block-1)/block;
        const u32 pitch=c.bufferRowLength/block*bytes,stride=pitch*(c.bufferImageHeight/block);
        for(u32 layer=0;layer<2;++layer) for(u32 y=0;y<rows;++y) for(u32 x=0;x<row;++x) {
            u8 value=u8(x*19+y*31+layer*83+mip*47);
            if(format==vk::Format::eBc1RgbaUnormBlock) {
                constexpr u16 colors[]={0xf800,0x07e0,0x001f,0xffff,0};
                const u64 encoded=u64(colors[(x/8+y+layer+mip)%5])|
                    (u64(colors[(x/8*3+y+1)%5])<<16)|(u64(0x44444444U)<<32);
                value=u8(encoded>>(8*(x%8)));
            }
            input.mapped_data[c.bufferOffset+layer*stride+y*pitch+x]=value;
            expected.push_back(value);
        }
    }
    input.Flush(0,input_size);
    scheduler.Record([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier2 barrier{
            .srcStageMask=vk::PipelineStageFlagBits2::eAllCommands,
            .dstStageMask=vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask=vk::AccessFlagBits2::eTransferWrite,
            .oldLayout=vk::ImageLayout::eUndefined,.newLayout=vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .image=*image,.subresourceRange={vk::ImageAspectFlagBits::eColor,0,3,0,2}};
        cmd.pipelineBarrier2(vk::DependencyInfo{.imageMemoryBarrierCount=1,.pImageMemoryBarriers=&barrier});
        cmd.copyBufferToImage(input.Handle(),*image,vk::ImageLayout::eTransferDstOptimal,uploads);
        barrier.srcAccessMask=vk::AccessFlagBits2::eTransferWrite;
        barrier.dstAccessMask=vk::AccessFlagBits2::eTransferRead;
        barrier.oldLayout=vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout=vk::ImageLayout::eTransferSrcOptimal;
        cmd.pipelineBarrier2(vk::DependencyInfo{.imageMemoryBarrierCount=1,.pImageMemoryBarriers=&barrier});
        cmd.copyImageToBuffer(*image,vk::ImageLayout::eTransferSrcOptimal,output.Handle(),downloads);
        ReadbackBarrier(cmd);
    });
    scheduler.Finish();
    output.Invalidate(0,output_size);
    u32 errors=0,first=0;
    for(u32 i=0;i<output_size;++i) if(output.mapped_data[i]!=expected[i]) { if(!errors) first=i; ++errors; }
    std::printf("Texture copy %s, padded rows/layers, 3 mips: %s (%u/%u bytes wrong; first %u expected %02x got %02x)\n",
        vk::to_string(format).c_str(),errors?"FAIL":"PASS",errors,output_size,first,expected[first],output.mapped_data[first]);
    if(format==vk::Format::eR8G8B8A8Unorm&&!SampleProbe(instance,scheduler,*image,input,format,expected)) ++errors;
    if(format==vk::Format::eBc1RgbaUnormBlock) {
        std::vector<u8> pixels(68*52*4);
        for(u32 y=0;y<52;++y) for(u32 x=0;x<68;++x) {
            const auto block_data=expected.data()+((y/4)*17+x/4)*8;
            u16 c0,c1;u32 indices;
            std::memcpy(&c0,block_data,2);std::memcpy(&c1,block_data+2,2);std::memcpy(&indices,block_data+4,4);
            u8 colors[4][4]{};
            for(u32 c=0;c<2;++c) {
                const u16 packed=c?c1:c0;
                colors[c][0]=u8(((packed>>11)*255+15)/31);
                colors[c][1]=u8((((packed>>5)&63)*255+31)/63);
                colors[c][2]=u8(((packed&31)*255+15)/31);
                colors[c][3]=255;
            }
            for(u32 c=0;c<3;++c) {
                colors[2][c]=c0>c1?(2*colors[0][c]+colors[1][c])/3:(colors[0][c]+colors[1][c])/2;
                colors[3][c]=c0>c1?(colors[0][c]+2*colors[1][c])/3:0;
            }
            colors[2][3]=255;colors[3][3]=c0>c1?255:0;
            const u32 index=(indices>>(2*((y%4)*4+x%4)))&3;
            std::copy_n(colors[index],4,pixels.data()+(y*68+x)*4);
        }
        if(!SampleProbe(instance,scheduler,*image,input,format,pixels)) ++errors;
    }
    image.reset();
    return errors==0;
}
int TextureProbe() {
    Vulkan::Instance instance(s32(-1),false);
    Vulkan::Scheduler scheduler(instance);
    Vulkan::Runtime runtime(instance,scheduler);
    VideoCore::StreamBuffer params(instance,scheduler,VideoCore::MemoryType::HostUncached,65536);
    VideoCore::TileManager tiles(instance,scheduler,runtime,params);
    unsigned failures=0;
    for(u32 bits : {8U,16U,32U,64U,128U}) {
        VideoCore::ImageInfo info{};
        info.props.is_tiled=1;
        info.num_bits=bits;
        info.tile_mode=AmdGpu::TileMode::Thin1DThin;
        info.array_mode=AmdGpu::ArrayMode::Array1DTiledThin1;
        info.resources={3,2};
        const u32 bytes=bits/8;
        u32 total=0;
        for(u32 mip=0;mip<3;++mip) {
            const u32 pitch=32>>mip,height=32>>mip;
            info.mips_layout[mip]={pitch*height*bytes*2,pitch,height,total};
            total+=info.mips_layout[mip].size;
        }
        info.guest_size=total;
        std::vector<u8> expected(total),tiled(total);
        for(u32 i=0;i<total;++i) expected[i]=u8((i*131+(i>>8)*17)^((i>>4)*29));
        for(u32 mip=0;mip<3;++mip) {
            const auto m=info.mips_layout[mip];
            for(u32 layer=0;layer<2;++layer) for(u32 y=0;y<m.height;++y) for(u32 x=0;x<m.pitch;++x) {
                u32 morton=0;
                for(u32 b=0;b<3;++b) morton|=((x>>b)&1)<<(2*b)|((y>>b)&1)<<(2*b+1);
                const u32 src=m.offset+(layer*m.pitch*m.height+y*m.pitch+x)*bytes;
                const u32 dst=m.offset+(layer*m.pitch*m.height+((y/8)*(m.pitch/8)+x/8)*64+morton)*bytes;
                std::copy_n(expected.data()+src,bytes,tiled.data()+dst);
            }
        }
        VideoCore::Buffer input(instance,0,total+512,VideoCore::MemoryType::HostUncached);
        VideoCore::Buffer output(instance,0,total,VideoCore::MemoryType::HostCached);
        std::copy(tiled.begin(),tiled.end(),input.mapped_data.begin()+512);
        input.Flush(512,total);
        auto [linear,offset]=tiles.DetileImage(&input,512,info);
        const vk::BufferCopy copy{offset,0,total};
        runtime.CopyBuffer(linear,&output,std::span(&copy,1));
        runtime.FlushBarriers();
        scheduler.Finish();
        output.Invalidate(0,total);
        u32 errors=0,first=0;
        for(u32 i=0;i<total;++i) if(output.mapped_data[i]!=expected[i]) { if(!errors) first=i; ++errors; }
        std::printf("Texture detile %u-bit, 3 mips, 2 layers, offset 512: %s (%u/%u bytes wrong; first %u expected %02x got %02x)\n",
            bits,errors?"FAIL":"PASS",errors,total,first,expected[first],output.mapped_data[first]);
        failures+=errors!=0;
    }
    failures+=!CopyProbe(instance,scheduler,vk::Format::eR8Unorm,1,1);
    failures+=!CopyProbe(instance,scheduler,vk::Format::eR8G8B8A8Unorm,1,4);
    failures+=!CopyProbe(instance,scheduler,vk::Format::eBc1RgbaUnormBlock,4,8);
    failures+=!CopyProbe(instance,scheduler,vk::Format::eBc3UnormBlock,4,16);
    return failures?1:0;
}
}
