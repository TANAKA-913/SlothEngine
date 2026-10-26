#include "WinApp.h"
#include <cstdint>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

// ウィンドウプロシージャ
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) { return true; }
#endif
	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
	// ウィンドウが破棄された
	case WM_DESTROY:
		// OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}
	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void WinApp::Initialize()
{
	// クライアント領域のサイズ
	const int32_t kClientWidth  = 1280;
	const int32_t kClientHeight = 720;

	WNDCLASS wc{};
	wc.lpfnWndProc   = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance     = GetModuleHandle(nullptr);
	wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
	if (!RegisterClass(&wc)) { return; }

	RECT wrc = {0, 0, kClientWidth, kClientHeight};
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
	HWND hwnd = CreateWindow(wc.lpszClassName, L"CG2", WS_OVERLAPPEDWINDOW,
	                          CW_USEDEFAULT, CW_USEDEFAULT,
	                          wrc.right - wrc.left, wrc.bottom - wrc.top,
	                          nullptr, nullptr, wc.hInstance, nullptr);
	if (hwnd == nullptr) { return; }
	ShowWindow(hwnd, SW_SHOW);
}

void WinApp::Update()
{
}
