// SPDX-License-Identifier: GPL-2.0-or-later
// Desktop contract against the exact packaged Dozen/DXIL validator. No game data.
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <vulkan/vulkan.h>
#include "shader_recompiler/backend/spirv/integer_sample.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using Shader::Backend::SPIRV::IntegerSampling::Config;
std::vector<uint32_t> GenerateFragment(Config,bool,bool);
std::vector<uint32_t> GenerateVertex();
static unsigned validation_failures,map_failures,dred_queries;
static void Diagnostic(const char* text){
    std::fprintf(stderr,"%s\n",text);
    if(std::strstr(text,"phase=dxil_validation"))++validation_failures;
    if(std::strstr(text,"phase=command_resource_map"))++map_failures;
    if(std::strstr(text,"phase=dred_interface"))++dred_queries;
}
static void Require(bool value,const char* message){if(!value){std::fprintf(stderr,"Integer driver FAIL: %s\n",message);std::exit(1);}}
int main(int argc,char** argv){
    Require(argc==2,"layout directory required");
    const std::string dir=argv[1];
    SetDllDirectoryA(dir.c_str());
    // Match the package rather than allowing a system DXIL.dll to win the
    // normal desktop search order ahead of SetDllDirectory.
    Require(LoadLibraryA((dir+"/dxil.dll").c_str())!=nullptr,"preload packaged DXIL validator");
    Require(LoadLibraryA((dir+"/dxcompiler.dll").c_str())!=nullptr,"preload packaged DXC");
    HMODULE dll=LoadLibraryA((dir+"/vulkan_dzn.dll").c_str());Require(dll!=nullptr,"load packaged Dozen");
    auto set=reinterpret_cast<void(*)(void(*)(const char*))>(GetProcAddress(dll,"dznSetUwpDiagnosticCallback"));Require(set!=nullptr,"diagnostic export");set(Diagnostic);
    auto legacy=reinterpret_cast<void(*)(int)>(GetProcAddress(dll,"dznSetUwpDiagnosticLegacyShaderModel"));
    Require(legacy!=nullptr,"legacy shader-model test export");legacy(1);
    std::puts("Integer driver compiler contract: forced SM 6.0 for this host test only");
    auto proc=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(dll,"vk_icdGetInstanceProcAddr"));Require(proc!=nullptr,"ICD entry");
    auto create=reinterpret_cast<PFN_vkCreateInstance>(proc(nullptr,"vkCreateInstance"));
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_2;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;
    VkInstance instance{};Require(create(&ici,nullptr,&instance)==VK_SUCCESS,"create instance");
#define INSTANCE_FN(name) auto name=reinterpret_cast<PFN_##name>(proc(instance,#name));Require(name!=nullptr,#name)
    INSTANCE_FN(vkEnumeratePhysicalDevices);INSTANCE_FN(vkGetPhysicalDeviceQueueFamilyProperties);
    INSTANCE_FN(vkGetPhysicalDeviceProperties);INSTANCE_FN(vkCreateDevice);INSTANCE_FN(vkDestroyInstance);INSTANCE_FN(vkGetDeviceProcAddr);
    uint32_t n=0;Require(vkEnumeratePhysicalDevices(instance,&n,nullptr)==VK_SUCCESS&&n>0,"enumerate software/host Vulkan adapter");
    std::vector<VkPhysicalDevice> devices(n);Require(vkEnumeratePhysicalDevices(instance,&n,devices.data())==VK_SUCCESS,"device list");
    VkPhysicalDevice physical=devices[0];
    for(auto p:devices){VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(p,&props);if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)physical=p;}
    VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(physical,&props);std::printf("Integer driver adapter: %s\n",props.deviceName);
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,nullptr);std::vector<VkQueueFamilyProperties> queues(n);vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,queues.data());
    uint32_t family=0;while(family<n&&!(queues[family].queueFlags&VK_QUEUE_COMPUTE_BIT))++family;Require(family<n,"compute queue");
    float priority=1;VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
    VkDevice device{};Require(vkCreateDevice(physical,&dci,nullptr,&device)==VK_SUCCESS,"create device");
#define DEVICE_FN(name) auto name=reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device,#name));Require(name!=nullptr,#name)
    DEVICE_FN(vkCreateShaderModule);DEVICE_FN(vkDestroyShaderModule);DEVICE_FN(vkCreateDescriptorSetLayout);DEVICE_FN(vkDestroyDescriptorSetLayout);
    DEVICE_FN(vkCreatePipelineLayout);DEVICE_FN(vkDestroyPipelineLayout);DEVICE_FN(vkCreateGraphicsPipelines);DEVICE_FN(vkCreateRenderPass);DEVICE_FN(vkDestroyRenderPass);DEVICE_FN(vkDestroyPipeline);DEVICE_FN(vkDestroyDevice);
    // Exercise the actual patched commands with a failed resource Map.
    // EndCommandBuffer must retain the error; no write/unmap/copy may follow.
    auto inject=reinterpret_cast<void(*)(int32_t)>(GetProcAddress(dll,"dznSetUwpDiagnosticMapFailure"));
    auto status=reinterpret_cast<void(*)(VkDevice)>(GetProcAddress(dll,"dznReportUwpDeviceStatus"));
    Require(inject&&status,"map injection and device status exports");status(device);
    Require(dred_queries==1,"optional DRED query on the real driver device");
    DEVICE_FN(vkCreateBuffer);DEVICE_FN(vkDestroyBuffer);DEVICE_FN(vkGetBufferMemoryRequirements);
    DEVICE_FN(vkAllocateMemory);DEVICE_FN(vkFreeMemory);DEVICE_FN(vkBindBufferMemory);
    DEVICE_FN(vkCreateCommandPool);DEVICE_FN(vkDestroyCommandPool);DEVICE_FN(vkAllocateCommandBuffers);
    DEVICE_FN(vkBeginCommandBuffer);DEVICE_FN(vkEndCommandBuffer);DEVICE_FN(vkResetCommandBuffer);
    DEVICE_FN(vkCmdFillBuffer);DEVICE_FN(vkCmdUpdateBuffer);
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bci.size=256;bci.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer buffer{};Require(vkCreateBuffer(device,&bci,nullptr,&buffer)==VK_SUCCESS,"transfer buffer");
    VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,buffer,&req);
    unsigned memory_type=0;while(memory_type<32&&!(req.memoryTypeBits&(1u<<memory_type)))++memory_type;
    Require(memory_type<32,"transfer memory type");VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};mai.allocationSize=req.size;mai.memoryTypeIndex=memory_type;
    VkDeviceMemory memory{};Require(vkAllocateMemory(device,&mai,nullptr,&memory)==VK_SUCCESS,"transfer memory");
    Require(vkBindBufferMemory(device,buffer,memory,0)==VK_SUCCESS,"bind transfer buffer");
    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cpci.queueFamilyIndex=family;cpci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool{};Require(vkCreateCommandPool(device,&cpci,nullptr,&pool)==VK_SUCCESS,"command pool");
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};cbai.commandPool=pool;cbai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cbai.commandBufferCount=1;
    VkCommandBuffer command{};Require(vkAllocateCommandBuffers(device,&cbai,&command)==VK_SUCCESS,"command buffer");
    const uint32_t words[16]={0x12345678};
    for(unsigned fail=0;fail<2;++fail)for(unsigned update=0;update<2;++update){
        Require(vkResetCommandBuffer(command,0)==VK_SUCCESS,"reset transfer command");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Require(vkBeginCommandBuffer(command,&begin)==VK_SUCCESS,"begin transfer command");
        inject(fail?int32_t(0x80004005):0);
        if(update)vkCmdUpdateBuffer(command,buffer,0,sizeof(words),words);else vkCmdFillBuffer(command,buffer,0,64,0x12345678);
        const auto result=vkEndCommandBuffer(command);inject(0);
        Require(result==(fail?VK_ERROR_UNKNOWN:VK_SUCCESS),"failed map preserved / successful transfer recording");
    }
    Require(map_failures==2,"both transfer maps guarded");
    vkDestroyCommandPool(device,pool,nullptr);vkDestroyBuffer(device,buffer,nullptr);vkFreeMemory(device,memory,nullptr);
    std::puts("Dozen transfer map contract: PASS two injected failures and two successful recordings; no GPU execution");
    VkDescriptorSetLayoutBinding bindings[3]={{0,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_ALL,nullptr},
        {1,VK_DESCRIPTOR_TYPE_SAMPLER,1,VK_SHADER_STAGE_ALL,nullptr},{2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_ALL,nullptr}};
    VkDescriptorSetLayoutCreateInfo sci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sci.bindingCount=3;sci.pBindings=bindings;
    VkDescriptorSetLayout descriptors{};Require(vkCreateDescriptorSetLayout(device,&sci,nullptr,&descriptors)==VK_SUCCESS,"descriptor layout");
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};lci.setLayoutCount=1;lci.pSetLayouts=&descriptors;
    VkPipelineLayout layout{};Require(vkCreatePipelineLayout(device,&lci,nullptr,&layout)==VK_SUCCESS,"pipeline layout");
    VkRenderPass passes[2]{};
    for(unsigned sign=0;sign<2;++sign){
        VkAttachmentDescription attachment{};attachment.format=sign?VK_FORMAT_R32G32B32A32_SINT:VK_FORMAT_R32G32B32A32_UINT;
        attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ref;
        VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rpci.attachmentCount=1;rpci.pAttachments=&attachment;rpci.subpassCount=1;rpci.pSubpasses=&sub;
        Require(vkCreateRenderPass(device,&rpci,nullptr,&passes[sign])==VK_SUCCESS,"integer render pass");
    }
    auto vertex_code=GenerateVertex();VkShaderModuleCreateInfo vsci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};vsci.codeSize=vertex_code.size()*4;vsci.pCode=vertex_code.data();
    VkShaderModule vertex{};Require(vkCreateShaderModule(device,&vsci,nullptr,&vertex)==VK_SUCCESS,"vertex module");
    unsigned accepted=0;
    auto pipeline=[&](Config c,bool sign,bool original){
        auto code=GenerateFragment(c,sign,original);VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};smci.codeSize=code.size()*4;smci.pCode=code.data();
        VkShaderModule module{};Require(vkCreateShaderModule(device,&smci,nullptr,&module)==VK_SUCCESS,"fragment module");
        VkPipelineShaderStageCreateInfo stages[2]{};
        for(auto& s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vertex;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=module;
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0,0,1,1,0,1};VkRect2D scissor{{0,0},{1,1}};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=1;vp.pViewports=&viewport;vp.scissorCount=1;vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState attachment{};attachment.colorWriteMask=15;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&attachment;
        VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pci.stageCount=2;pci.pStages=stages;pci.pVertexInputState=&vi;pci.pInputAssemblyState=&ia;
        pci.pViewportState=&vp;pci.pRasterizationState=&rs;pci.pMultisampleState=&ms;pci.pColorBlendState=&blend;pci.layout=layout;pci.renderPass=passes[unsigned(sign)];
        VkPipeline p{};auto result=vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pci,nullptr,&p);
        if(p)vkDestroyPipeline(device,p,nullptr);vkDestroyShaderModule(device,module,nullptr);return result;
    };
    const auto original=pipeline(Config{},false,true);
    std::printf("Integer driver original VkResult=%d validation_failures=%u\n",int(original),validation_failures);std::fflush(stdout);
    const bool reproduced=original!=VK_SUCCESS&&validation_failures>0;
    const unsigned prior=validation_failures;
    for(bool sign:{false,true})for(unsigned dim:{1u,2u,3u})for(bool array:{false,true}){
        if(dim==3&&array)continue;
        for(unsigned wrap=0;wrap<8;++wrap){
            const auto result=pipeline(Config{dim,array,{wrap,wrap,wrap},wrap%3},sign,false);
            if(result!=VK_SUCCESS)std::fprintf(stderr,"Rejected sign=%u dim=%u array=%u wrap=%u VkResult=%d\n",unsigned(sign),dim,unsigned(array),wrap,int(result));
            Require(result==VK_SUCCESS,"lowered pipeline accepted by packaged driver");++accepted;
        }
    }
    Require(validation_failures==prior,"no lowered DXIL validation failure");
    Require(reproduced,"original integer fragment sample must reproduce DXIL validation failure");
    vkDestroyShaderModule(device,vertex,nullptr);for(auto pass:passes)vkDestroyRenderPass(device,pass,nullptr);
    vkDestroyPipelineLayout(device,layout,nullptr);vkDestroyDescriptorSetLayout(device,descriptors,nullptr);vkDestroyDevice(device,nullptr);vkDestroyInstance(instance,nullptr);
    std::printf("Integer driver contract: PASS original rejected, %u lowered pipelines accepted; no game/GPU execution tested\n",accepted);
}
