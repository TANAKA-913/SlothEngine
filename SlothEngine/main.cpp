#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>

//ウィンドウプロージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	//メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		//ウィンドウが破棄された
	case WM_DESTROY:
		//OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}
	//標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//ログ出力関数
void Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	WNDCLASS wc{};
	//ウィンドウプロージャ
	wc.lpfnWndProc = WindowProc;
	//ウィンドウクラス名(なんでも良い)
	wc.lpszClassName = L"CG2WindowClass";
	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスを登録する
	if (!RegisterClass(&wc)) {
		return -1;
	}

	//クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	// ウィンドウサイズのサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth,kClientHeight };

	// クライアント領域をもとに実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,     //利用するクラス名
		L"CG2",               //タイトルバーの文字
		WS_OVERLAPPEDWINDOW,  //ウィンドウスタイル
		CW_USEDEFAULT,        //表示X座標（OSに任せる）
		CW_USEDEFAULT,        //表示Y座標（OSに任せる）
		wrc.right - wrc.left, //ウィンドウ横幅
		wrc.bottom - wrc.top, //ウィンドウ縦幅
		nullptr,              //親ウィンドウハンドル
		nullptr,              //メニューハンドル
		wc.hInstance,         //インスタンスハンドル
		nullptr);             //オプション

	//ウィンドウ生成失敗チェック
	if (hwnd == nullptr) {
		return -1;
	}

	//ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

	//出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");

	//文字列を格納する
	std::string str0{ "STRING!!!" };

	//整数を文字列にする
	std::string str1{ std::to_string(10) };

	//テスト用変数
	int enemyHp = 100;
	std::string texturePath = "player.png";

	MSG msg{};
	//ウィンドウの×ボタンが押されるまでループする
	while (msg.message != WM_QUIT) {
		//Window にメッセージが来ていたら最優先で処理する
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			//ゲームの処理

			//ログ出力
			Log(std::format("enemyHp:{}, texturePath:{}\n", enemyHp, texturePath));
		}
	}

	return 0;
}