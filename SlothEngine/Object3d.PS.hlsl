#include "object3d.hlsli"

struct Material
{
    float4   color;
    int      enableLighting;
    float3 padding; // 【修正】float[3]だと配列扱いになり要素ごとに16byte消費してC++側(96byte)とズレるためfloat3に変更
    float4x4 uvTransform;
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

    // 【追加】UVTransformを適用してからサンプリングする
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor  = gTexture.Sample(gSampler, transformedUV.xy);

    if (gMaterial.enableLighting != 0)
    {
        // 【変更】Half Lambert反射モデルでライティング計算
        // NdotL: 法線とライト方向（ライト側への向き）の内積 [-1, 1]
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        // [-1,1] を [0,1] へなだらかに変形し、さらに2乗でよりそれっぽく見せる
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);

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
