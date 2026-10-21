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
std::vector<uint32_t> Generate(Config,bool,bool);
static unsigned validation_failures;
static void Diagnostic(const char* text){
    std::fprintf(stderr,"%s\n",text);
    if(std::strstr(text,"phase=dxil_validation"))++validation_failures;
}
static void Require(bool value,const char* message){if(!value){std::fprintf(stderr,"Integer driver FAIL: %s\n",message);std::exit(1);}}
int main(int argc,char** argv){
    Require(argc==2,"layout directory required");
    const std::string dir=argv[1];
    SetDllDirectoryA(dir.c_str());
    HMODULE dll=LoadLibraryA((dir+"/vulkan_dzn.dll").c_str());Require(dll!=nullptr,"load packaged Dozen");
    auto set=reinterpret_cast<void(*)(void(*)(const char*))>(GetProcAddress(dll,"dznSetUwpDiagnosticCallback"));Require(set!=nullptr,"diagnostic export");set(Diagnostic);
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
    DEVICE_FN(vkCreatePipelineLayout);DEVICE_FN(vkDestroyPipelineLayout);DEVICE_FN(vkCreateComputePipelines);DEVICE_FN(vkDestroyPipeline);DEVICE_FN(vkDestroyDevice);
    VkDescriptorSetLayoutBinding bindings[3]={{0,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
        {1,VK_DESCRIPTOR_TYPE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo sci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sci.bindingCount=3;sci.pBindings=bindings;
    VkDescriptorSetLayout descriptors{};Require(vkCreateDescriptorSetLayout(device,&sci,nullptr,&descriptors)==VK_SUCCESS,"descriptor layout");
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};lci.setLayoutCount=1;lci.pSetLayouts=&descriptors;
    VkPipelineLayout layout{};Require(vkCreatePipelineLayout(device,&lci,nullptr,&layout)==VK_SUCCESS,"pipeline layout");
    unsigned accepted=0;
    auto pipeline=[&](Config c,bool sign,bool original){
        auto code=Generate(c,sign,original);VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};smci.codeSize=code.size()*4;smci.pCode=code.data();
        VkShaderModule module{};Require(vkCreateShaderModule(device,&smci,nullptr,&module)==VK_SUCCESS,"shader module");
        VkComputePipelineCreateInfo pci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pci.stage.module=module;pci.stage.pName="main";pci.layout=layout;
        VkPipeline p{};auto result=vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&pci,nullptr,&p);
        if(p)vkDestroyPipeline(device,p,nullptr);vkDestroyShaderModule(device,module,nullptr);return result;
    };
    Require(pipeline(Config{},false,true)!=VK_SUCCESS&&validation_failures>0,"original integer sample must reproduce DXIL validation failure");
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
    vkDestroyPipelineLayout(device,layout,nullptr);vkDestroyDescriptorSetLayout(device,descriptors,nullptr);vkDestroyDevice(device,nullptr);vkDestroyInstance(instance,nullptr);
    std::printf("Integer driver contract: PASS original rejected, %u lowered pipelines accepted; no game/GPU execution tested\n",accepted);
}
