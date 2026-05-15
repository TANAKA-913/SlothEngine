
struct VecterShaderOutput
{
    float4 position : SV_POSITION;
};
struct VecterShaderInput
{
    float4 position : POSITION;
};
VecterShaderOutput main( VecterShaderInput input )
{
    VecterShaderOutput output;
    output.position = input.position;
    return output;
}