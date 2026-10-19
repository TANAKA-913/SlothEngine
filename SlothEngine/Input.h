#pragma once
#include <windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>

// 入力
class Input {
public:
	// namespace省略
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public: // メンバ関数
	// 初期化
	void Initialize(HINSTANCE hInstance, HWND hwnd);
	// 更新
	void Update();

	/// <summary>
	/// キーの押下をチェック
	/// </summary>
	/// <param name="keyNumber">キー番号( DIK_0 等)</param>
	/// <returns>押されているか</returns>
	bool PushKey(BYTE keyNumber);

	/// <summary>
	/// キーのトリガーをチェック
	/// </summary>
	/// <param name="keyNumber">キー番号( DIK_0 等)</param>
	/// <returns>トリガーか</returns>
	bool TriggerKey(BYTE keyNumber);

private: // メンバ変数
	// DirectInputのインスタンス
	ComPtr<IDirectInput8> directInput;
	// キーボードのデバイス
	ComPtr<IDirectInputDevice8> keyboard;
	// 全キーの状態
	BYTE key[256] = {};
	// 前回の全キーの状態
	BYTE keyPre[256] = {};
};
