#include "object3d.hlsli"

struct Material
{
    float4  color;
    int32_t enableLighting;
};

// 【追加】平行光源の構造体
struct DirectionalLight
{
    float4 color;     //!< ライトの色
    float3 direction; //!< ライトの向き（正規化済み）
    float  intensity; //!< 輝度
};

ConstantBuffer<Material>        gMaterial        : register(b0);
// 【追加】平行光源用ConstantBuffer（レジスタb1）
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState      gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    if (gMaterial.enableLighting != 0)
    {
        // 【追加】ランバート反射モデルでライティング計算
        // 入力法線を再正規化（補間で長さが変わるため）
        // saturateで内積の負値を0にクランプ（裏面には光が当たらない）
        float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));

        output.color = gMaterial.color * textureColor
                     * gDirectionalLight.color
                     * cos
                     * gDirectionalLight.intensity;
    }
    else
    {
        // 【追加】Lightingしない場合（Sprite等）は前回までと同じ演算
        output.color = gMaterial.color * textureColor;
    }

    return output;
}
