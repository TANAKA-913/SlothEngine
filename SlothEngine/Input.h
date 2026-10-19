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

private: // メンバ変数
	// キーボードのデバイス
	ComPtr<IDirectInputDevice8> keyboard;
};
