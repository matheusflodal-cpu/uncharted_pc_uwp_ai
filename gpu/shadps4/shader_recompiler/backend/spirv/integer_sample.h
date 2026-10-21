// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <sirit/sirit.h>

namespace Shader::Backend::SPIRV::IntegerSampling {
struct Config {
    unsigned dimensions{2};
    bool array{};
    std::array<unsigned,3> wrap{}; // AMD clamp mode values, 0..7
    unsigned border{}; // transparent black, opaque black, white; custom excluded
    std::array<unsigned,4> swizzle{4,5,6,7}; // AMD: 0/1 constants, 4..7 RGBA
};

constexpr std::array<uint32_t,4> Border(const Config& c) {
    const uint32_t rgb=c.border==2?1:0,alpha=c.border==0?0:1;
    const std::array<uint32_t,4> original{rgb,rgb,rgb,alpha};
    std::array<uint32_t,4> result{};
    for(unsigned j=0;j<4;++j) result[j]=c.swizzle[j]==1?1:c.swizzle[j]>=4?original[c.swizzle[j]-4]:0;
    return result;
}

// Shared by the production shader and the host contract. B is either the
// SPIR-V scalar emitter below or an evaluator used by the numerical tests.
template<class B>
auto Coordinate(B& b,unsigned mode,typename B::Value coord,typename B::Value size,
                typename B::Value offset) {
    if(mode==3 || mode==5 || mode==7) coord=b.abs(coord);
    auto p=b.add(b.floor(b.mul(coord,size)),offset);
    if(mode==0) return b.mod(p,size);
    if(mode==1) {
        auto period=b.mul(size,b.constant(2));
        auto q=b.mod(p,period);
        return b.min(q,b.sub(b.sub(period,b.constant(1)),q));
    }
    return b.clamp(p,b.constant(0),b.sub(size,b.constant(1)));
}

struct Scalar {
    using Value=Sirit::Id;
    Sirit::Module& m;
    Value f,i;
    Value constant(float x) {return m.Constant(f,x);}
    Value abs(Value x) {return m.OpFAbs(f,x);}
    Value mul(Value a,Value b) {return m.OpFMul(f,a,b);}
    Value add(Value a,Value b) {return m.OpFAdd(f,a,b);}
    Value sub(Value a,Value b) {return m.OpFSub(f,a,b);}
    Value floor(Value a) {return m.OpFloor(f,a);}
    Value min(Value a,Value b) {return m.OpFMin(f,a,b);}
    Value clamp(Value a,Value lo,Value hi) {return m.OpFClamp(f,a,lo,hi);}
    Value mod(Value a,Value b) {return m.OpFMod(f,a,b);}
};

inline Sirit::Id Emit(Sirit::Module& m,const Config& c,Sirit::Id image,
                     Sirit::Id result_type,Sirit::Id coords,Sirit::Id offset={}) {
    using Sirit::Id;
    const Id f=m.TypeFloat(32),i=m.TypeInt(32,true),u=m.TypeUInt(32),boolean=m.TypeBool();
    Scalar b{m,f,i};
    const unsigned count=c.dimensions+unsigned(c.array);
    const Id size_type=count==1?i:m.TypeVector(i,count);
    const Id zero=m.Constant(i,0);
    const Id sizes=m.OpImageQuerySizeLod(size_type,image,zero);
    const auto extract=[&](Id type,Id value,unsigned index,unsigned n) {
        return n==1?value:m.OpCompositeExtract(type,value,index);
    };
    std::array<Id,4> out{};
    Id border=m.ConstantFalse(boolean);
    for(unsigned axis=0;axis<c.dimensions;++axis) {
        const Id coord=extract(f,coords,axis,count);
        const Id size=m.OpConvertSToF(f,extract(i,sizes,axis,count));
        const Id off=Sirit::ValidId(offset)?m.OpConvertSToF(f,extract(i,offset,axis,c.dimensions)):b.constant(0);
        out[axis]=m.OpConvertFToS(i,Coordinate(b,c.wrap[axis],coord,size,off));
        if(c.wrap[axis]>=4) {
            Id q=(c.wrap[axis]==5 || c.wrap[axis]==7)?b.abs(coord):coord;
            const Id p=b.add(b.floor(b.mul(q,size)),off);
            border=m.OpLogicalOr(boolean,border,m.OpLogicalOr(boolean,
                m.OpFOrdLessThan(boolean,p,b.constant(0)),m.OpFOrdGreaterThanEqual(boolean,p,size)));
        }
    }
    if(c.array) {
        const Id layer=m.OpRoundEven(f,extract(f,coords,c.dimensions,count));
        const Id layers=m.OpConvertSToF(f,extract(i,sizes,c.dimensions,count));
        out[c.dimensions]=m.OpConvertFToS(i,b.clamp(layer,b.constant(0),b.sub(layers,b.constant(1))));
    }
    const Id position=count==1?out[0]:m.OpCompositeConstruct(m.TypeVector(i,count),std::span<const Id>(out.data(),count));
    const Id texel=m.OpImageFetch(result_type,image,position,spv::ImageOperandsMask::Lod,zero);
    // Construct border bits in the actual signed/unsigned result type. Bitcast
    // later preserves all 32-bit values, including values above float precision.
    const auto rgba=Border(c);
    const Id color=m.OpBitcast(result_type,m.ConstantComposite(m.TypeVector(u,4),
        m.Constant(u,rgba[0]),m.Constant(u,rgba[1]),m.Constant(u,rgba[2]),m.Constant(u,rgba[3])));
    const Id condition=m.OpCompositeConstruct(m.TypeVector(boolean,4),border,border,border,border);
    return m.OpSelect(result_type,condition,color,texel);
}
} // namespace Shader::Backend::SPIRV::IntegerSampling
