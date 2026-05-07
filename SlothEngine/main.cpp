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
#include <strsafe.h>

#include<dbghelp.h>


#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "dbghelp.lib")

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

	void Log(const std::wstring& message) {
		OutputDebugStringW(message.c_str());
	}

	static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
		// 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
		SYSTEMTIME time;
		GetLocalTime(&time);

		wchar_t filePath[MAX_PATH] = { 0 };
		CreateDirectory(L"./Dumps", nullptr);

		StringCchPrintfW(filePath,MAX_PATH,L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);

		HANDLE dumpFileHandle = CreateFile(filePath,GENERIC_WRITE,FILE_SHARE_WRITE | FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);

		// processId（このexeのId）とクラッシュ（例外）の発生したthreadIdを取得
		DWORD processId = GetCurrentProcessId();
		DWORD threadId = GetCurrentThreadId();

		// 設定情報を入力
		MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
		minidumpInformation.ThreadId = threadId;
		minidumpInformation.ExceptionPointers = exception;
		minidumpInformation.ClientPointers = TRUE;

		// Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
		MiniDumpWriteDump(
			GetCurrentProcess(),processId,dumpFileHandle,MiniDumpNormal,&minidumpInformation,nullptr,nullptr
		);

		// 他に関連づけられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
		return EXCEPTION_EXECUTE_HANDLER;
	}


	//Windowsアプリでのエントリーポイント(main関数)
	int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

		// 誰も捕捉しなかった場合に(Unhandled)、捕捉する関数を登録
		SetUnhandledExceptionFilter(ExportDump);

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

		WNDCLASS wc{};

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

		//ログのディレクトリを用意
		std::filesystem::create_directory("logs");
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
		std::string logFilePath = std::string("logs/log_") + dateString + ".log";
		//ファイルを作って書き込み準備
		std::ofstream logFile(logFilePath);


		// CommandQueueを生成する
		ID3D12CommandQueue* commandQueue = nullptr;
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
		hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
		// コマンドキューの生成がうまくいかなかったので起動できない
		assert(SUCCEEDED(hr));

		// CommandAllocatorを生成する
		ID3D12CommandAllocator* commandAllocator = nullptr;
		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
		// コマンドアロケータの生成がうまくいかなかったので起動できない
		assert(SUCCEEDED(hr));

		// CommandListを生成する
		ID3D12GraphicsCommandList* commandList = nullptr;
		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
		// コマンドリストの生成がうまくいかなかったので起動できない
		assert(SUCCEEDED(hr));

		// SwapChainを生成する
		IDXGISwapChain4* swapChain = nullptr;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChainDesc.Width = kClientWidth;     // 画面の幅
		swapChainDesc.Height = kClientHeight;   // 画面の高さ
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
		swapChainDesc.SampleDesc.Count = 1;     // マルチサンプルしない
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画ターゲットとして利用する
		swapChainDesc.BufferCount = 2;          // ダブルバッファ
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタにうつしたら、中身を破棄
		// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
		hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
		assert(SUCCEEDED(hr));

		// DescriptorHeapを生成する
		ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
		D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
		rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー用
		rtvDescriptorHeapDesc.NumDescriptors = 2; // ダブルバッファ用に2つ。多くても別に構わない
		hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
		// ディスクリプタヒープが作れなかったので起動できない
		assert(SUCCEEDED(hr));

		Log("Complete create DirectX12 Objects!!!\n");

		// SwapChainからResourceを引っ張ってくる
		ID3D12Resource* swapChainResources[2] = { nullptr };
		hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
		assert(SUCCEEDED(hr));
		hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
		assert(SUCCEEDED(hr));

		// RTVを作る
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2dテクスチャとして書き込む

		// ディスクリプタの先頭を取得する
		D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

		// RTVを2つ作るのでディスクリプタを2つ用意
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
		// 1つ目を作る
		rtvHandles[0] = rtvStartHandle;
		device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
		// 2つ目のディスクリプタハンドルを得る（自力で進める）
		rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		// 2つ目を作る
		device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);
		MSG msg{};

		//ログ出力
		Log(logFile, std::format("enemyHp:{}, texturePath:{}\n", enemyHp, texturePath));
		//ウィンドウの×ボタンが押されるまでループする
		while (msg.message != WM_QUIT) {
			//Window にメッセージが来ていたら最優先で処理する
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else {
				// これから書き込むバックバッファのインデックスを取得
				UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

				// 描画先のRTVを設定する
				commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

				// 指定した色で画面全体をクリアする
				float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f }; // 青っぽい色
				commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

				// コマンドリストの内容を確定させる
				hr = commandList->Close();
				assert(SUCCEEDED(hr));

				// GPUにコマンドリストの実行を行わせる
				ID3D12CommandList* commandLists[] = { commandList };
				commandQueue->ExecuteCommandLists(1, commandLists);

				// 画面の交換を行う（Present）
				swapChain->Present(1, 0);

				// 次のフレーム用のコマンドリストを準備
				hr = commandAllocator->Reset();
				assert(SUCCEEDED(hr));
				hr = commandList->Reset(commandAllocator, nullptr);
				assert(SUCCEEDED(hr));

				//ゲームの処理
				Log(logFile, std::format("Loop running...\n"));

			}
		}

		return 0;
	}