#include "Math.h"
#include <cmath>

// --- ベクトル演算 ---
Vector3 Math::Add(const Vector3& v1, const Vector3& v2) { return {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z}; }
Vector3 Math::Subtract(const Vector3& v1, const Vector3& v2) { return {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z}; }
Vector3 Math::Multiply(float scalar, const Vector3& v) { return {v.x * scalar, v.y * scalar, v.z * scalar}; }
float Math::Dot(const Vector3& v1, const Vector3& v2) { return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z; }
float Math::Length(const Vector3& v) { return std::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }
Vector3 Math::Normalize(const Vector3& v) {
	float len = Length(v);
	if (len == 0)
		return {0, 0, 0};
	return {v.x / len, v.y / len, v.z / len};
}

// --- 基本行列演算 ---
Matrix4x4 Math::Add(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			result.m[i][j] = m1.m[i][j] + m2.m[i][j];
	return result;
}

Matrix4x4 Math::Subtract(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			result.m[i][j] = m1.m[i][j] - m2.m[i][j];
	return result;
}

Matrix4x4 Math::Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			for (int k = 0; k < 4; ++k)
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
	return result;
}

Matrix4x4 Math::Transpose(const Matrix4x4& m) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			result.m[i][j] = m.m[j][i];
	return result;
}

Matrix4x4 Math::MakeIdentity4x4() {
	Matrix4x4 result = {0};
	for (int i = 0; i < 4; ++i)
		result.m[i][i] = 1.0f;
	return result;
}

// --- 逆行列 (以前のロジックを保持) ---
Matrix4x4 Math::Inverse(const Matrix4x4& m) {
	float inv[16];
	float mat[16];
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			mat[i * 4 + j] = m.m[i][j];

	inv[0] = mat[5] * mat[10] * mat[15] - mat[5] * mat[11] * mat[14] - mat[9] * mat[6] * mat[15] + mat[9] * mat[7] * mat[14] + mat[13] * mat[6] * mat[11] - mat[13] * mat[7] * mat[10];
	inv[4] = -mat[4] * mat[10] * mat[15] + mat[4] * mat[11] * mat[14] + mat[8] * mat[6] * mat[15] - mat[8] * mat[7] * mat[14] - mat[12] * mat[6] * mat[11] + mat[12] * mat[7] * mat[10];
	inv[8] = mat[4] * mat[9] * mat[15] - mat[4] * mat[11] * mat[13] - mat[8] * mat[5] * mat[15] + mat[8] * mat[7] * mat[13] + mat[12] * mat[5] * mat[11] - mat[12] * mat[7] * mat[9];
	inv[12] = -mat[4] * mat[9] * mat[14] + mat[4] * mat[10] * mat[13] + mat[8] * mat[5] * mat[14] - mat[8] * mat[6] * mat[13] - mat[12] * mat[5] * mat[10] + mat[12] * mat[6] * mat[9];
	inv[1] = -mat[1] * mat[10] * mat[15] + mat[1] * mat[11] * mat[14] + mat[9] * mat[2] * mat[15] - mat[9] * mat[3] * mat[14] - mat[13] * mat[2] * mat[11] + mat[13] * mat[3] * mat[10];
	inv[5] = mat[0] * mat[10] * mat[15] - mat[0] * mat[11] * mat[14] - mat[8] * mat[2] * mat[15] + mat[8] * mat[3] * mat[14] + mat[12] * mat[2] * mat[11] - mat[12] * mat[3] * mat[10];
	inv[9] = -mat[0] * mat[9] * mat[15] + mat[0] * mat[11] * mat[13] + mat[8] * mat[1] * mat[15] - mat[8] * mat[3] * mat[13] - mat[12] * mat[1] * mat[11] + mat[12] * mat[3] * mat[9];
	inv[13] = mat[0] * mat[9] * mat[14] - mat[0] * mat[10] * mat[13] - mat[8] * mat[1] * mat[14] + mat[8] * mat[2] * mat[13] + mat[12] * mat[1] * mat[10] - mat[12] * mat[2] * mat[9];
	inv[2] = mat[1] * mat[6] * mat[15] - mat[1] * mat[7] * mat[14] - mat[5] * mat[2] * mat[15] + mat[5] * mat[3] * mat[14] + mat[13] * mat[2] * mat[7] - mat[13] * mat[3] * mat[6];
	inv[6] = -mat[0] * mat[6] * mat[15] + mat[0] * mat[7] * mat[14] + mat[4] * mat[2] * mat[15] - mat[4] * mat[3] * mat[14] - mat[12] * mat[2] * mat[7] + mat[12] * mat[3] * mat[6];
	inv[10] = mat[0] * mat[5] * mat[15] - mat[0] * mat[7] * mat[13] - mat[4] * mat[1] * mat[15] + mat[4] * mat[3] * mat[13] + mat[12] * mat[1] * mat[7] - mat[12] * mat[3] * mat[5];
	inv[14] = -mat[0] * mat[5] * mat[14] + mat[0] * mat[6] * mat[13] + mat[4] * mat[1] * mat[14] - mat[4] * mat[2] * mat[13] - mat[12] * mat[1] * mat[6] + mat[12] * mat[2] * mat[5];
	inv[3] = -mat[1] * mat[6] * mat[11] + mat[1] * mat[7] * mat[10] + mat[5] * mat[2] * mat[11] - mat[5] * mat[3] * mat[10] - mat[9] * mat[2] * mat[7] + mat[9] * mat[3] * mat[6];
	inv[7] = mat[0] * mat[6] * mat[11] - mat[0] * mat[7] * mat[10] - mat[4] * mat[2] * mat[11] + mat[4] * mat[3] * mat[10] + mat[8] * mat[2] * mat[7] - mat[8] * mat[3] * mat[6];
	inv[11] = -mat[0] * mat[5] * mat[11] + mat[0] * mat[7] * mat[9] + mat[4] * mat[1] * mat[11] - mat[4] * mat[3] * mat[9] - mat[8] * mat[1] * mat[7] + mat[8] * mat[3] * mat[5];
	inv[15] = mat[0] * mat[5] * mat[10] - mat[0] * mat[6] * mat[9] - mat[4] * mat[1] * mat[10] + mat[4] * mat[2] * mat[9] + mat[8] * mat[1] * mat[6] - mat[8] * mat[2] * mat[5];

	float det = mat[0] * inv[0] + mat[1] * inv[4] + mat[2] * inv[8] + mat[3] * inv[12];
	if (std::abs(det) < 0.0001f)
		return MakeIdentity4x4();

	det = 1.0f / det;
	Matrix4x4 result;
	for (int i = 0; i < 16; i++)
		result.m[i / 4][i % 4] = inv[i] * det;
	return result;
}

// --- アフィン変換用行列生成 ---
Matrix4x4 Math::MakeTranslateMatrix(const Vector3& t) {
	Matrix4x4 res = MakeIdentity4x4();
	res.m[3][0] = t.x;
	res.m[3][1] = t.y;
	res.m[3][2] = t.z;
	return res;
}
Matrix4x4 Math::MakeScaleMatrix(const Vector3& s) {
	Matrix4x4 res = {0};
	res.m[0][0] = s.x;
	res.m[1][1] = s.y;
	res.m[2][2] = s.z;
	res.m[3][3] = 1.0f;
	return res;
}
Matrix4x4 Math::MakeRotateXMatrix(float rad) {
	Matrix4x4 res = MakeIdentity4x4();
	res.m[1][1] = std::cos(rad);
	res.m[1][2] = std::sin(rad);
	res.m[2][1] = -std::sin(rad);
	res.m[2][2] = std::cos(rad);
	return res;
}
Matrix4x4 Math::MakeRotateYMatrix(float rad) {
	Matrix4x4 res = MakeIdentity4x4();
	res.m[0][0] = std::cos(rad);
	res.m[0][2] = -std::sin(rad);
	res.m[2][0] = std::sin(rad);
	res.m[2][2] = std::cos(rad);
	return res;
}
Matrix4x4 Math::MakeRotateZMatrix(float rad) {
	Matrix4x4 res = MakeIdentity4x4();
	res.m[0][0] = std::cos(rad);
	res.m[0][1] = std::sin(rad);
	res.m[1][0] = -std::sin(rad);
	res.m[1][1] = std::cos(rad);
	return res;
}
Matrix4x4 Math::MakeAffineMatrix(const Vector3& s, const Vector3& r, const Vector3& t) {
	Matrix4x4 S = MakeScaleMatrix(s);
	Matrix4x4 R = Multiply(Multiply(MakeRotateXMatrix(r.x), MakeRotateYMatrix(r.y)), MakeRotateZMatrix(r.z));
	Matrix4x4 T = MakeTranslateMatrix(t);
	return Multiply(Multiply(S, R), T);
}

// --- 投影・ビューポート行列生成 (画像の結果に適合) ---
Matrix4x4 Math::MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
	Matrix4x4 result = {0};
	float cot = 1.0f / std::tan(fovY / 2.0f);
	result.m[0][0] = cot / aspectRatio;
	result.m[1][1] = cot;
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;
	result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);
	return result;
}

Matrix4x4 Math::MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result = {0};
	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 Math::MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	Matrix4x4 result = {0};
	result.m[0][0] = width / 2.0f;
	result.m[1][1] = -height / 2.0f;
	result.m[2][2] = maxDepth - minDepth;
	result.m[3][0] = left + (width / 2.0f);
	result.m[3][1] = top + (height / 2.0f);
	result.m[3][2] = minDepth;
	result.m[3][3] = 1.0f;
	return result;
}

Vector3 Math::Transform(const Vector3& v, const Matrix4x4& m) {
	Vector3 res;
	// 行列とベクトルの乗算 (w成分も計算)
	res.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
	res.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
	res.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
	float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];

	// 透視除算 (wで割ることで、遠くのものが小さくなる)
	if (w != 0.0f) {
		res.x /= w;
		res.y /= w;
		res.z /= w;
	}
	return res;
}

Vector3 Math::Cross(const Vector3& v1, const Vector3& v2) { return {v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x}; }