cbuffer gTransformationMatrix : register(b1)
{
    matrix gWorldMatrix;
};

struct VecterShaderInput
{
    float4 position : POSITION;
};

struct VecterShaderOutput
{
    float4 position : SV_POSITION;
};

VecterShaderOutput main(VecterShaderInput input)
{
    VecterShaderOutput output;
   
    output.position = mul(input.position, gWorldMatrix);
    
    return output;
}