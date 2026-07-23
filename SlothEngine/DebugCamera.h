#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"

/// <summary>
/// デバッグ用フリーカメラ
/// </summary>
class DebugCamera {
public:

	// ローカル座標
	Vector3 translation_ = {0.0f, 0.0f, -50.0f};

	// 累積回転行列
	// （オイラー角(Vector3)での管理はやめて、回転行列で姿勢を管理する）
	Matrix4x4 matRot_;

	// ビュー行列
	Matrix4x4 viewMatrix_;
	// 射影行列
	Matrix4x4 projectionMatrix_;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	/// <param name="key">DirectInputで取得したキーボードの入力状態（256バイト配列）</param>
	void Update(const unsigned char* key);
};
