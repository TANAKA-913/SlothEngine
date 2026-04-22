#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
//ファイルやディレクトリに関する操作をするライブラリ
#include <filesystem>
//ファイルに書いたり読んだりするライブラリ
#include <fstream>
//時間を扱うライブラリ
#include<chrono>

#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")


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

	void Log(std::ostream& os, const std::string& message) {
		os << message << std::endl;
		OutputDebugStringA(message.c_str());
	}



	//Windowsアプリでのエントリーポイント(main関数)
	int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {



		// DXGIファクトリー
		IDXGIFactory7* dxgiFactory = nullptr;
		// HRESULTはWindow系のエラーコードであり
		// 関数が成功したかどうかをSUCCEEDEDマクロで判定できる
		HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
		// 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているかどうにもできない場合が多いのでassertにしておく
		assert(SUCCEEDED(hr));

		// 使用するアダプタの変数。最初にnullptrを入れておく
		IDXGIAdapter1* useAdapter = nullptr;

		// よい順にアダプタを頼む
		for (UINT i = 0;
			dxgiFactory->EnumAdapterByGpuPreference(
				i,
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(&useAdapter)) == S_OK;
			i++) {

			// アダプタの情報を取得する
			DXGI_ADAPTER_DESC1 adapterDesc{};
			hr = useAdapter->GetDesc1(&adapterDesc);
			assert(SUCCEEDED(hr)); // 取得できないのは一大事

			// ソフトウェアアダプタでなければ採用
			if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
				// 採用したアダプタの情報をログに出力
				Log(std::format(L"Use Adapater : {}\n", adapterDesc.Description));
				Log(ConverString(std::format(L"Use Adapater : {}\n", adapterDesc.Description)));
				break;
			}

			// ソフトウェアアダプタだった場合はリセット
			useAdapter = nullptr;
		}

		// 適切なアダプタが見つからなかったのでは起動できない
		assert(useAdapter != nullptr);


		ID3D12Device* device = nullptr;
		//機能レベルとログ出力用の文字列
		D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
		};
		const char* featureLevelString[] = { "12.2","12.1","12.0" };
		//高い順に生成できるか試していく
		for (size_t i = 0; i < _countof(featureLevels); ++i) {
			hr = D3D12CreateDevice(
				useAdapter, //アダプタ
				featureLevels[i], //機能レベル
				IID_PPV_ARGS(&device)); //生成するデバイスへのポインタのアドレス
			if (SUCCEEDED(hr)) {
				Log(std::format("FeatureLevel : {}\n", featureLevelString[i]));
				break;
			}
		}
		//デバイスが生成できなかったので起動できない
		assert(device != nullptr);
		Log("Complete create D3D12Device!!!\n");

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

		//ログのディレクトリを用意
		std::filesystem::create_directory("Logs");
		//現在時刻を取得
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		//ログファイルの名前にコンマ何秒はいらないので秒単位に変換
		std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
			nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
		//日本時間に変換
		std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
		//formatを使って年月日_時分秒の形式に変換
		std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
		//時刻を使ってファイル名を決定
		std::string logFilePath = std::string("Logs/log_") + dateString + ".log";
		//ファイルを作って書き込み準備
		std::ofstream logFile(logFilePath);


		MSG msg{};

		//ログ出力
		Log(std::format("enemyHp:{}, texturePath:{}\n", enemyHp, texturePath));

		//ウィンドウの×ボタンが押されるまでループする
		while (msg.message != WM_QUIT) {
			//Window にメッセージが来ていたら最優先で処理する
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else {
				//ゲームの処理


			}
		}

		return 0;
	}