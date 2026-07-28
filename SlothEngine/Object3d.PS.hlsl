#include "object3d.hlsli"

// 【変更】enableLightingを「Lightingの方式」を表す値として使う
//   0 : Lightingなし
//   1 : Lambert反射
//   2 : Half Lambert反射
struct Material
{
    float4   color;
    int      enableLighting; // Lighting方式（0:なし 1:Lambert 2:HalfLambert）
    float3 padding; // 【修正】float[3]だと配列扱いになり要素ごとに16byte消費してC++側(96byte)とズレるためfloat3に変更
    float4x4 uvTransform;
};

// 【追加】Lighting方式を表す定数（C++側のenumと対応させる）
static const int kLightingModeNone       = 0;
static const int kLightingModeLambert    = 1;
static const int kLightingModeHalfLambert = 2;

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

    // 【変更】Lighting方式によって計算を分岐する
    //   None       : ライティングなし（Sprite等）
    //   Lambert    : 通常のランバート反射（NdotLを0でクランプ）
    //   HalfLambert: ハーフランバート反射（NdotLを0.5～1.0へなだらかに変形）
    if (gMaterial.enableLighting == kLightingModeLambert)
    {
        // NdotL: 法線とライト方向（ライト側への向き）の内積
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        // 通常のLambert反射は負の値を0にクランプするだけ
        float cos = saturate(NdotL);

        output.color = gMaterial.color * textureColor
                     * gDirectionalLight.color
                     * cos
                     * gDirectionalLight.intensity;
    }
    else if (gMaterial.enableLighting == kLightingModeHalfLambert)
    {
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
