#include"object3d.hlsli"
cbuffer gTransformationMatrix : register(b1)
{
    matrix gWorldMatrix;
};

struct VecterShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VecterShaderInput input)
{
    VertexShaderOutput output;
   
    output.position = mul(input.position, gWorldMatrix);
    output.texcoord = input.texcoord;
    
    return output;
}