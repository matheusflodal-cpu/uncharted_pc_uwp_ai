// SPDX-License-Identifier: GPL-2.0-or-later
#include "shader_recompiler/backend/spirv/integer_sample.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
using namespace Shader::Backend::SPIRV::IntegerSampling;
struct Eval {
    using Value=double;
    double constant(float x){return x;}
    double abs(double x){return std::abs(x);}
    double mul(double x,double y){return x*y;}
    double add(double x,double y){return x+y;}
    double sub(double x,double y){return x-y;}
    double floor(double x){return std::floor(x);}
    double min(double x,double y){return x<y?x:y;}
    double clamp(double x,double a,double b){return x<a?a:x>b?b:x;}
    double mod(double x,double y){return x-y*std::floor(x/y);}
};
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Integer sample FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
std::vector<uint32_t> Generate(Config config,bool sign,bool original) {
    Sirit::Module m(0x00010500);using Sirit::Id;
    m.AddCapability(spv::Capability::Shader);m.AddCapability(spv::Capability::ImageQuery);
    if(config.dimensions==1)m.AddCapability(spv::Capability::Sampled1D);
    m.SetMemoryModel(spv::AddressingModel::Logical,spv::MemoryModel::GLSL450);
    const Id void_t=m.TypeVoid(),f=m.TypeFloat(32),u=m.TypeUInt(32),data=sign?m.TypeInt(32,true):u;
    const Id v4=m.TypeVector(data,4);
    const Id image_t=m.TypeImage(data,config.dimensions==1?spv::Dim::Dim1D:config.dimensions==2?spv::Dim::Dim2D:spv::Dim::Dim3D,false,config.array,false,1,spv::ImageFormat::Unknown);
    const Id image_var=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::UniformConstant,image_t),spv::StorageClass::UniformConstant);
    m.Decorate(image_var,spv::Decoration::DescriptorSet,0u);m.Decorate(image_var,spv::Decoration::Binding,0u);
    const Id sampler_t=m.TypeSampler();
    const Id sampler=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::UniformConstant,sampler_t),spv::StorageClass::UniformConstant);
    m.Decorate(sampler,spv::Decoration::DescriptorSet,0u);m.Decorate(sampler,spv::Decoration::Binding,1u);
    const Id output_t=m.TypeImage(data,spv::Dim::Dim2D,false,false,false,2,sign?spv::ImageFormat::Rgba32i:spv::ImageFormat::Rgba32ui);
    const Id output=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::UniformConstant,output_t),spv::StorageClass::UniformConstant);
    m.Decorate(output,spv::Decoration::DescriptorSet,0u);m.Decorate(output,spv::Decoration::Binding,2u);
    const Id fn=m.OpFunction(void_t,spv::FunctionControlMask::MaskNone,m.TypeFunction(void_t));m.AddLabel();
    const Id image=m.OpLoad(image_t,image_var);
    const unsigned n=config.dimensions+unsigned(config.array);
    std::vector<Id> c(n,m.Constant(f,-0.25f));
    const Id coord=n==1?c[0]:m.ConstantComposite(m.TypeVector(f,n),c);
    Id value;
    if(original){
        const Id sampled=m.OpSampledImage(m.TypeSampledImage(image_t),image,m.OpLoad(sampler_t,sampler));
        value=m.OpImageSampleExplicitLod(v4,sampled,coord,spv::ImageOperandsMask::Lod,m.Constant(f,0.0f));
    }else value=Emit(m,config,image,v4,coord);
    const Id i=m.TypeInt(32,true),zero=m.Constant(i,0);
    m.OpImageWrite(m.OpLoad(output_t,output),m.ConstantComposite(m.TypeVector(i,2),zero,zero),value);
    m.OpReturn();m.OpFunctionEnd();
    const std::array interfaces{image_var,sampler,output};
    m.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",interfaces);
    m.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,1u,1u,1u);
    return m.Assemble();
}
#ifndef UNCHARTED_INTEGER_TEST_NO_MAIN
int main(int argc,char** argv){
    Eval b;
    // Independent hand-calculated cases exercise texel boundaries, negative
    // repeat/mirror, offsets, non-power-of-two sizes and mirror-once modes.
    struct Case{unsigned mode;double c,size,offset,want;};
    const Case cases[]={
      {0,-.01,8,0,7},{0,1.,8,0,0},{0,2.25,8,0,2},{0,0.,7,-1,6},
      {1,-.01,8,0,0},{1,-.25,8,0,1},{1,1.,8,0,7},{1,2.,8,0,0},{1,1.5,7,0,3},
      {2,-1.,8,0,0},{2,1.,8,0,7},{2,.5,8,2,6},{2,.125,8,-2,0},
      {3,-.01,8,0,0},{3,-.25,8,0,2},{3,-2.,8,0,7},
      {4,-.1,8,0,0},{4,1.1,8,0,7},{5,-.25,8,0,2},
      {6,-.1,8,0,0},{6,1.1,8,0,7},{7,-.25,8,0,2},
      {0,.5,1,0,0},{1,-100.5,1,0,0},{2,100.,1,0,0}
    };
    for(auto c:cases)CHECK(Coordinate(b,c.mode,c.c,c.size,c.offset)==c.want);
    for(bool sign:{false,true})for(unsigned dim:{1u,2u,3u})for(bool array:{false,true}){
        if(dim==3&&array)continue;
        for(unsigned wrap=0;wrap<8;++wrap){
            Config c{dim,array,{wrap,wrap,wrap},wrap%3};
            auto words=Generate(c,sign,false);CHECK(words.size()>5);
            unsigned fetch=0,sample=0;
            for(size_t pos=5;pos<words.size();){
                auto op=spv::Op(words[pos]&0xffff);const auto count=words[pos]>>16;CHECK(count>0);
                fetch+=op==spv::Op::OpImageFetch;
                sample+=op==spv::Op::OpImageSampleExplicitLod||op==spv::Op::OpImageSampleImplicitLod;
                pos+=count;
            }
            CHECK(fetch==1&&sample==0);
            if(argc>1){std::string path=std::string(argv[1])+"/integer-"+std::to_string(sign)+"-"+std::to_string(dim)+"-"+std::to_string(array)+"-"+std::to_string(wrap)+".spv";std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(words.data()),words.size()*4);CHECK(bool(out));}
        }
    }
    if(argc>1){auto words=Generate(Config{},false,true);std::ofstream out(std::string(argv[1])+"/integer-original.spv",std::ios::binary);out.write(reinterpret_cast<const char*>(words.data()),words.size()*4);CHECK(bool(out));}
    std::printf("Integer sampling contract: PASS checks=%u; addressing math and 80 production SPIR-V modules\n",checks);
}

#endif
