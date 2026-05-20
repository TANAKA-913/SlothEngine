#pragma once
#include "Matrix4x4.h"
#include "Vector3.h"

class Math {
public:
	// --- ベクトル演算 ---
	static Vector3 Add(const Vector3& v1, const Vector3& v2);
	static Vector3 Subtract(const Vector3& v1, const Vector3& v2);
	static Vector3 Multiply(float scalar, const Vector3& v);
	static float Dot(const Vector3& v1, const Vector3& v2);
	static float Length(const Vector3& v);
	static Vector3 Normalize(const Vector3& v);

	// --- 基本行列演算 ---
	static Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);
	static Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);
	static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
	static Matrix4x4 Transpose(const Matrix4x4& m);
	static Matrix4x4 MakeIdentity4x4();
	static Matrix4x4 Inverse(const Matrix4x4& m);

	// --- アフィン変換用行列生成 ---
	static Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
	static Matrix4x4 MakeScaleMatrix(const Vector3& scale);
	static Matrix4x4 MakeRotateXMatrix(float radian);
	static Matrix4x4 MakeRotateYMatrix(float radian);
	static Matrix4x4 MakeRotateZMatrix(float radian);
	static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	// --- 投影・ビューポート行列生成 ---
	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
	static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
	static Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

	// --- 変換・描画補助 ---
	static Vector3 Transform(const Vector3& v, const Matrix4x4& m);
	static void VectorScreenPrintf(int x, int y, const Vector3& vector, const char* label);
	static void MatrixScreenPrintf(int x, int y, const Matrix4x4& matrix, const char* label);
	static Vector3 Cross(const Vector3& v1, const Vector3& v2);
};