#include "DebugCamera.h"
#include "Math.h"

#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定（dinput.hより上に書くこと）
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")

namespace {
	// 移動速度
	constexpr float kMoveSpeed = 0.5f;
	// 回転速度（ラジアン/フレーム）
	constexpr float kRotSpeed = 0.02f;

	// 射影行列用パラメータ
	constexpr float kFovY         = 0.45f;
	constexpr float kAspectRatio  = 1280.0f / 720.0f;
	constexpr float kNearClip     = 0.1f;
	constexpr float kFarClip      = 1000.0f;
}

void DebugCamera::Initialize() {
	// 各種メンバ変数の初期化（必要に応じて）

	// 累積回転行列を単位行列で初期化
	matRot_ = Math::MakeIdentity4x4();

	// 射影行列の初期化
	projectionMatrix_ = Math::MakePerspectiveFovMatrix(kFovY, kAspectRatio, kNearClip, kFarClip);

	// 現在の座標・姿勢からビュー行列を計算しておく
	Matrix4x4 translateMatrix = Math::MakeTranslateMatrix(translation_);
	Matrix4x4 worldMatrix     = Math::Multiply(matRot_, translateMatrix);
	viewMatrix_ = Math::Inverse(worldMatrix);
}

void DebugCamera::Update(const unsigned char* key) {
	// =====================================================
	// 1. 入力によるカメラの移動や回転
	// =====================================================

	// --- 前後移動 ---
	// 自キャラが向いている方向に弾を発射する処理と同じ考え方。
	// 角度は毎フレーム変わる可能性があるので、前進ベクトル（速度）は毎回計算する。
	if (key[DIK_W]) {
		const float speed = kMoveSpeed; // 前進の速さ
		// カメラ移動ベクトル
		Vector3 move = {0.0f, 0.0f, speed};
		// 移動ベクトルを姿勢（累積回転行列）分だけ回転させる
		move = Math::Transform(move, matRot_);
		// 移動ベクトル分だけ座標を加算する
		translation_ = Math::Add(translation_, move);
	}
	if (key[DIK_S]) {
		const float speed = -kMoveSpeed; // 後退の速さ
		Vector3 move = {0.0f, 0.0f, speed};
		move = Math::Transform(move, matRot_);
		translation_ = Math::Add(translation_, move);
	}

	// --- 左右移動 ---
	// 前進／後退と同じ仕組みで実装できる。
	if (key[DIK_D]) {
		const float speed = kMoveSpeed; // 右移動の速さ
		Vector3 move = {speed, 0.0f, 0.0f};
		move = Math::Transform(move, matRot_);
		translation_ = Math::Add(translation_, move);
	}
	if (key[DIK_A]) {
		const float speed = -kMoveSpeed; // 左移動の速さ
		Vector3 move = {speed, 0.0f, 0.0f};
		move = Math::Transform(move, matRot_);
		translation_ = Math::Add(translation_, move);
	}

	// --- 上下移動 ---
	if (key[DIK_SPACE]) {
		const float speed = kMoveSpeed; // 上移動の速さ
		Vector3 move = {0.0f, speed, 0.0f};
		move = Math::Transform(move, matRot_);
		translation_ = Math::Add(translation_, move);
	}
	if (key[DIK_LCONTROL]) {
		const float speed = -kMoveSpeed; // 下移動の速さ
		Vector3 move = {0.0f, speed, 0.0f};
		move = Math::Transform(move, matRot_);
		translation_ = Math::Add(translation_, move);
	}

	// --- 回転処理 ---
	// 角度を増減させるのではなく、今回フレーム分の追加回転行列を作り、
	// 累積回転行列に合成することで姿勢を管理する。
	Matrix4x4 matRotDelta = Math::MakeIdentity4x4();

	// X軸周り回転の入力があったら、X軸周りの回転を加算する
	if (key[DIK_UP]) {
		matRotDelta = Math::Multiply(matRotDelta, Math::MakeRotateXMatrix(-kRotSpeed));
	}
	if (key[DIK_DOWN]) {
		matRotDelta = Math::Multiply(matRotDelta, Math::MakeRotateXMatrix(kRotSpeed));
	}
	// Y軸周り回転の入力があったら、Y軸周りの回転を加算する
	if (key[DIK_LEFT]) {
		matRotDelta = Math::Multiply(matRotDelta, Math::MakeRotateYMatrix(-kRotSpeed));
	}
	if (key[DIK_RIGHT]) {
		matRotDelta = Math::Multiply(matRotDelta, Math::MakeRotateYMatrix(kRotSpeed));
	}
	// Z軸周り回転の入力があったら、Z軸周りの回転を加算する
	if (key[DIK_Q]) {
		matRotDelta = Math::Multiply(matRotDelta, Math::MakeRotateZMatrix(-kRotSpeed));
	}
	if (key[DIK_E]) {
		matRotDelta = Math::Multiply(matRotDelta, Math::MakeRotateZMatrix(kRotSpeed));
	}

	// 累積の回転行列を合成する
	matRot_ = Math::Multiply(matRotDelta, matRot_);

	// =====================================================
	// 2. ビュー行列の更新
	// =====================================================
	// 座標から平行移動行列を計算する
	Matrix4x4 translateMatrix = Math::MakeTranslateMatrix(translation_);
	// 累積回転行列と平行移動行列からワールド行列を計算する
	Matrix4x4 worldMatrix = Math::Multiply(matRot_, translateMatrix);
	// ワールド行列の逆行列をビュー行列に代入する
	viewMatrix_ = Math::Inverse(worldMatrix);
}
