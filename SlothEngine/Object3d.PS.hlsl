#include "object3d.hlsli"

struct Material
{
    float4 color;
};

ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 【変更】テクスチャからUV座標を使って色をサンプリング
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // マテリアルの色とテクスチャの色を掛け合わせる（現状の赤色(1,0,0,1)のままだとテクスチャが赤くなるので、確認時はC++側で白(1,1,1,1)にすると画像がそのまま出ます）
    output.color = gMaterial.color * textureColor;

    return output;
}