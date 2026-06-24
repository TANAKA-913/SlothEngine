#include "object3d.hlsli"

// 【変更】WVPとWorldの両方を持つ構造体に変更
cbuffer gTransformationMatrix : register(b1)
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal   : NORMAL0; // 【追加】法線の入力
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // 【変更】WVP行列で座標変換
    output.position = mul(input.position, WVP);
    output.texcoord = input.texcoord;

    // 【追加】法線をワールド座標系に変換し正規化してPixelShaderへ渡す
    // WorldMatrixの左上3x3（拡縮・回転成分）だけを使う
    output.normal = normalize(mul(input.normal, (float32_t3x3)World));

    return output;
}
