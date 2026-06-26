#include <Windows.h>
#include <cstdint>
#include <format>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>

#include "Math.h"
#include "Matrix4x4.h"

#include "externals/DirectXTex/DirectXTex.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#include <cassert>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <strsafe.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

const int32_t kClientWidth  = 1280;
const int32_t kClientHeight = 720;

struct Vector2 { float x, y; };
struct Vector4 { float x, y, z, w; };

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

// 【変更】enableLightingを追加
struct Material {
	Vector4  color;
	int32_t  enableLighting;
};

// 【変更】法線フィールドを追加
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

// 【変更】WVPとWorldの両方を持つ構造体に変更
struct TransformationData {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

// 【追加】平行光源の構造体
struct DirectionalLight {
	Vector4 color;     //!< ライトの色
	Vector3 direction; //!< ライトの向き（正規化済み）
	float   intensity; //!< 輝度
};

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) { return true; }
#endif
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Log(const std::string& message)  { OutputDebugStringA(message.c_str()); }
void Log(const std::wstring& message) { OutputDebugStringW(message.c_str()); }
void Log(std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) return L"";
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring strTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &strTo[0], size_needed);
	return strTo;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) return "";
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = {0};
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
	                 time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ,
	                                   0, CREATE_ALWAYS, 0, 0);
	DWORD processId = GetCurrentProcessId();
	DWORD threadId  = GetCurrentThreadId();
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{0};
	minidumpInformation.ThreadId       = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal,
	                  &minidumpInformation, nullptr, nullptr);
	return EXCEPTION_EXECUTE_HANDLER;
}

IDxcBlob* CompileShader(
    const std::wstring& filePath, const wchar_t* profile,
    IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler, IDxcIncludeHandler* includeHandler)
{
	Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));

	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr      = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size     = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;
	LPCWSTR arguments[] = {
	    filePath.c_str(),
	    L"-E", L"main",
	    L"-T", profile,
	    L"-Zi", L"-Qembed_debug",
	    L"-Od",
	    L"-Zpr",
	};
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(&shaderSourceBuffer, arguments, _countof(arguments),
	                           includeHandler, IID_PPV_ARGS(&shaderResult));
	assert(SUCCEEDED(hr));

	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer());
		assert(false);
	}
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
	shaderSource->Release();
	shaderResult->Release();
	return shaderBlob;
}

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width              = sizeInBytes;
	resourceDesc.Height             = 1;
	resourceDesc.DepthOrArraySize   = 1;
	resourceDesc.MipLevels          = 1;
	resourceDesc.SampleDesc.Count   = 1;
	resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
	    &uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
	    &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
	    nullptr, IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

DirectX::ScratchImage LoadTexture(const std::string& filePath) {
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
	assert(SUCCEEDED(hr));
	DirectX::ScratchImage mipImage{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
	                               image.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, mipImage);
	assert(SUCCEEDED(hr));
	return mipImage;
}

ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width            = UINT(metadata.width);
	resourceDesc.Height           = UINT(metadata.height);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.MipLevels        = UINT16(metadata.mipLevels);
	resourceDesc.Format           = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension        = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type                 = D3D12_HEAP_TYPE_CUSTOM;
	heapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
	    &heapProperties, D3D12_HEAP_FLAG_NONE,
	    &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
	    nullptr, IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipimage) {
	const DirectX::TexMetadata& metadata = mipimage.GetMetadata();
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
		const DirectX::Image* ima = mipimage.GetImage(mipLevel, 0, 0);
		HRESULT hr = texture->WriteToSubresource(
		    UINT(mipLevel), nullptr, ima->pixels, UINT(ima->rowPitch), UINT(ima->slicePitch));
		assert(SUCCEEDED(hr));
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
    ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
    ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width              = width;
	resourceDesc.Height             = height;
	resourceDesc.MipLevels          = 1;
	resourceDesc.DepthOrArraySize   = 1;
	resourceDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count   = 1;
	resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
	    &heapProperties, D3D12_HEAP_FLAG_NONE,
	    &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
	    &depthClearValue, IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

// =============================================================================
// WinMain
// =============================================================================
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	CoInitializeEx(0, COINIT_MULTITHREADED);
	SetUnhandledExceptionFilter(ExportDump);

#ifdef USE_IMGUI
	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	IDXGIFactory7* dxgiFactory = nullptr;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	IDXGIAdapter1* useAdapter = nullptr;
	for (UINT i = 0;
	     dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
	                                              IID_PPV_ARGS(&useAdapter)) == S_OK; i++)
	{
		DXGI_ADAPTER_DESC1 adapterDesc{};
		hr = useAdapter->GetDesc1(&adapterDesc);
		assert(SUCCEEDED(hr));
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
			Log(std::format(L"Use Adapater : {}\n", adapterDesc.Description));
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr);

	ID3D12Device* device = nullptr;
	D3D_FEATURE_LEVEL featureLevels[]    = {D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
	const char*        featureLevelString[] = {"12.2", "12.1", "12.0"};
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr)) { Log(std::format("FeatureLevel : {}\n", featureLevelString[i])); break; }
	}
	assert(device != nullptr);
	Log("Complete create D3D12Device!!!\n");

#ifdef USE_IMGUI
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		D3D12_MESSAGE_ID denyIds[]     = {D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE};
		D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs        = _countof(denyIds);
		filter.DenyList.pIDList       = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);
		infoQueue->Release();
	}
#endif

	WNDCLASS wc{};
	wc.lpfnWndProc   = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance     = GetModuleHandle(nullptr);
	wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
	if (!RegisterClass(&wc)) { return -1; }

	RECT wrc = {0, 0, kClientWidth, kClientHeight};
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
	HWND hwnd = CreateWindow(wc.lpszClassName, L"CG2", WS_OVERLAPPEDWINDOW,
	                          CW_USEDEFAULT, CW_USEDEFAULT,
	                          wrc.right - wrc.left, wrc.bottom - wrc.top,
	                          nullptr, nullptr, wc.hInstance, nullptr);
	if (hwnd == nullptr) { return -1; }
	ShowWindow(hwnd, SW_SHOW);

	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr));

	ID3D12CommandAllocator* commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	ID3D12GraphicsCommandList* commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr,
	                                IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width       = kClientWidth;
	swapChainDesc.Height      = kClientHeight;
	swapChainDesc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr,
	                                          reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert(SUCCEEDED(hr));

	ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	rtvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NumDescriptors = 2;
	hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
	assert(SUCCEEDED(hr));

	ID3D12Resource* swapChainResources[2] = {nullptr};
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0])); assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1])); assert(SUCCEEDED(hr));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

	ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
#ifdef USE_IMGUI
	D3D12_DESCRIPTOR_HEAP_DESC srvDescriptorHeapDesc{};
	srvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvDescriptorHeapDesc.NumDescriptors = 3;
	srvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hr = device->CreateDescriptorHeap(&srvDescriptorHeapDesc, IID_PPV_ARGS(&srvDescriptorHeap));
	assert(SUCCEEDED(hr));
#endif

	ID3D12Fence* fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);

	IDxcUtils*          dxcUtils    = nullptr;
	IDxcCompiler3*      dxcCompiler = nullptr;
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils,     IID_PPV_ARGS(&dxcUtils));    assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler,  IID_PPV_ARGS(&dxcCompiler)); assert(SUCCEEDED(hr));
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);              assert(SUCCEEDED(hr));

	Log("Complete create DirectX12 Objects!!!\n");

	const uint32_t desriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t desriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t desriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// テクスチャ読み込み
	DirectX::ScratchImage mipmapImage = LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipmapImage.GetMetadata();
	ID3D12Resource* textureresource = CreateTextureResource(device, metadata);
	UploadTextureData(textureresource, mipmapImage);

	DirectX::ScratchImage mipImages2 = LoadTexture("resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);
	UploadTextureData(textureResource2, mipImages2);

	ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);

	ID3D12DescriptorHeap* dsvDescriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc{};
	dsvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescriptorHeapDesc.NumDescriptors = 1;
	hr = device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&dsvDescriptorHeap));
	assert(SUCCEEDED(hr));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format        = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvHandle);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format                  = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels     = UINT(metadata.mipLevels);
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 1);
	device->CreateShaderResourceView(textureresource, &srvDesc, textureSrvHandleCPU);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format                  = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels     = UINT(metadata2.mipLevels);
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, desriptorSizeSRV, 2);
	device->CreateShaderResourceView(textureResource2, &srvDesc2, textureSrvHandleCPU2);

#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device, 2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
	                     srvDescriptorHeap,
	                     srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
	                     srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
#endif

	// =========================================================================
	// RootSignature
	// 【変更】RootParameterを4つに拡張（DirectionalLight用b1をPixelShaderに追加）
	// =========================================================================
	ID3D12RootSignature* rootSignature = nullptr;
	D3D12_ROOT_PARAMETER rootParameters[4]{};

	// b0: Material（PixelShader）
	rootParameters[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_PIXEL;

	// b1: TransformationMatrix（VertexShader）
	rootParameters[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].Descriptor.ShaderRegister = 1;
	rootParameters[1].ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

	// t0: Texture（PixelShader）
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].NumDescriptors                    = 1;
	descriptorRange[0].BaseShaderRegister                = 0;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[2].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
	rootParameters[2].DescriptorTable.pDescriptorRanges   = descriptorRange;
	rootParameters[2].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

	// 【追加】b1: DirectionalLight（PixelShader）
	rootParameters[3].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].Descriptor.ShaderRegister = 1; // register(b1)
	rootParameters[3].ShaderVisibility          = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC staticSampler[1] = {};
	staticSampler[0].Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler[0].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler[0].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler[0].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler[0].ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler[0].MaxLOD           = D3D12_FLOAT32_MAX;
	staticSampler[0].ShaderRegister   = 0;
	staticSampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.pParameters     = rootParameters;
	descriptionRootSignature.NumParameters   = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSampler;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSampler);
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob     = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1,
	                                  &signatureBlob, &errorBlob);
	if (FAILED(hr)) { Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer())); assert(false); }
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
	                                  signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	// =========================================================================
	// InputLayout
	// 【修正】配列サイズを [3] に修正（法線を追加）
	// =========================================================================
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName      = "POSITION";
	inputElementDescs[0].SemanticIndex     = 0;
	inputElementDescs[0].Format            = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName      = "TEXCOORD";
	inputElementDescs[1].SemanticIndex     = 0;
	inputElementDescs[1].Format            = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	// 【追加】法線のInputLayout
	inputElementDescs[2].SemanticName      = "NORMAL";
	inputElementDescs[2].SemanticIndex     = 0;
	inputElementDescs[2].Format            = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements        = _countof(inputElementDescs);

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);
	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl",  L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderBlob  != nullptr);

	ID3D12PipelineState* graphicsPipelineState = nullptr;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature        = rootSignature;
	graphicsPipelineStateDesc.InputLayout           = inputLayoutDesc;
	graphicsPipelineStateDesc.VS                    = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
	graphicsPipelineStateDesc.PS                    = {pixelShaderBlob->GetBufferPointer(),  pixelShaderBlob->GetBufferSize()};
	graphicsPipelineStateDesc.BlendState            = blendDesc;
	graphicsPipelineStateDesc.RasterizerState       = rasterizerDesc;
	graphicsPipelineStateDesc.NumRenderTargets      = 1;
	graphicsPipelineStateDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count      = 1;
	graphicsPipelineStateDesc.SampleMask            = D3D12_DEFAULT_SAMPLE_MASK;
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable    = TRUE;
	graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	graphicsPipelineStateDesc.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	// =========================================================================
	// 【変更】TransformationDataリソース（WVP + World の2行列分）
	// =========================================================================
	ID3D12Resource* transformationResource = CreateBufferResource(device, sizeof(TransformationData));
	TransformationData* transformationData = nullptr;
	transformationResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationData));
	transformationData->WVP   = Math::MakeIdentity4x4();
	transformationData->World = Math::MakeIdentity4x4();

	// =========================================================================
	// 球の頂点データ
	// =========================================================================
	const uint32_t kSubdivision  = 16;
	const uint32_t kVertexCount  = kSubdivision * kSubdivision * 6;

	ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * kVertexCount);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes    = sizeof(VertexData) * kVertexCount;
	vertexBufferView.StrideInBytes  = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	const float pi         = 3.1415926535f;
	const float kLonEvery  = 2.0f * pi / static_cast<float>(kSubdivision);
	const float kLatEvery  = pi          / static_cast<float>(kSubdivision);

	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		float lat     = -pi / 2.0f + kLatEvery * static_cast<float>(latIndex);
		float latNext = lat + kLatEvery;

		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			uint32_t startIndex = (latIndex * kSubdivision + lonIndex) * 6;

			Vector3 a{cosf(lat)     * cosf(lonIndex       * kLonEvery), sinf(lat),     cosf(lat)     * sinf(lonIndex       * kLonEvery)};
			Vector3 b{cosf(latNext) * cosf(lonIndex       * kLonEvery), sinf(latNext), cosf(latNext) * sinf(lonIndex       * kLonEvery)};
			Vector3 c{cosf(lat)     * cosf((lonIndex + 1) * kLonEvery), sinf(lat),     cosf(lat)     * sinf((lonIndex + 1) * kLonEvery)};
			Vector3 d{cosf(latNext) * cosf((lonIndex + 1) * kLonEvery), sinf(latNext), cosf(latNext) * sinf((lonIndex + 1) * kLonEvery)};

			float u0 = static_cast<float>(lonIndex)     / static_cast<float>(kSubdivision);
			float u1 = static_cast<float>(lonIndex + 1) / static_cast<float>(kSubdivision);
			float v0 = 1.0f - static_cast<float>(latIndex)     / static_cast<float>(kSubdivision);
			float v1 = 1.0f - static_cast<float>(latIndex + 1) / static_cast<float>(kSubdivision);

			vertexData[startIndex + 0].position = {a.x, a.y, a.z, 1.0f};
			vertexData[startIndex + 1].position = {b.x, b.y, b.z, 1.0f};
			vertexData[startIndex + 2].position = {c.x, c.y, c.z, 1.0f};
			vertexData[startIndex + 3].position = {c.x, c.y, c.z, 1.0f};
			vertexData[startIndex + 4].position = {b.x, b.y, b.z, 1.0f};
			vertexData[startIndex + 5].position = {d.x, d.y, d.z, 1.0f};

			vertexData[startIndex + 0].texcoord = {u0, v0};
			vertexData[startIndex + 1].texcoord = {u0, v1};
			vertexData[startIndex + 2].texcoord = {u1, v0};
			vertexData[startIndex + 3].texcoord = {u1, v0};
			vertexData[startIndex + 4].texcoord = {u0, v1};
			vertexData[startIndex + 5].texcoord = {u1, v1};

			// 【追加】単位球なので位置ベクトル = 法線ベクトル
			vertexData[startIndex + 0].normal = {a.x, a.y, a.z};
			vertexData[startIndex + 1].normal = {b.x, b.y, b.z};
			vertexData[startIndex + 2].normal = {c.x, c.y, c.z};
			vertexData[startIndex + 3].normal = {c.x, c.y, c.z};
			vertexData[startIndex + 4].normal = {b.x, b.y, b.z};
			vertexData[startIndex + 5].normal = {d.x, d.y, d.z};
		}
	}

	// =========================================================================
	// マテリアル（モンスターボール用、ライティングON）
	// =========================================================================
	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color          = {1.0f, 1.0f, 1.0f, 1.0f};
	materialData->enableLighting = true; // 【追加】ライティング有効

	// =========================================================================
	// 【追加】Sprite用マテリアル（ライティングOFF）
	// =========================================================================
	ID3D12Resource* materialResourceSprite = CreateBufferResource(device, sizeof(Material));
	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color          = {1.0f, 1.0f, 1.0f, 1.0f};
	materialDataSprite->enableLighting = false; // SpriteにはLightingしない

	// =========================================================================
	// Sprite頂点データ
	// =========================================================================
	ID3D12Resource* VertexResourcesprite = CreateBufferResource(device, sizeof(VertexData) * 6);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	vertexBufferViewSprite.BufferLocation = VertexResourcesprite->GetGPUVirtualAddress();
	vertexBufferViewSprite.SizeInBytes    = sizeof(VertexData) * 6;
	vertexBufferViewSprite.StrideInBytes  = sizeof(VertexData);

	VertexData* vertexDataSprite = nullptr;
	VertexResourcesprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	vertexDataSprite[0].position = {0.0f,   360.0f, 0.0f, 1.0f}; vertexDataSprite[0].texcoord = {0.0f, 1.0f};
	vertexDataSprite[1].position = {0.0f,   0.0f,   0.0f, 1.0f}; vertexDataSprite[1].texcoord = {0.0f, 0.0f};
	vertexDataSprite[2].position = {640.0f, 360.0f, 0.0f, 1.0f}; vertexDataSprite[2].texcoord = {1.0f, 1.0f};
	vertexDataSprite[3].position = {0.0f,   0.0f,   0.0f, 1.0f}; vertexDataSprite[3].texcoord = {0.0f, 0.0f};
	vertexDataSprite[4].position = {640.0f, 0.0f,   0.0f, 1.0f}; vertexDataSprite[4].texcoord = {1.0f, 0.0f};
	vertexDataSprite[5].position = {640.0f, 360.0f, 0.0f, 1.0f}; vertexDataSprite[5].texcoord = {1.0f, 1.0f};

	// 【追加】Spriteの法線は使わないが-z方向で設定
	for (int i = 0; i < 6; i++) {
		vertexDataSprite[i].normal = {0.0f, 0.0f, -1.0f};
	}

	// Sprite用TransformationDataリソース
	ID3D12Resource* transformationResourceSprite = CreateBufferResource(device, sizeof(TransformationData));
	TransformationData* transformationDataSprite = nullptr;
	transformationResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationDataSprite));
	transformationDataSprite->WVP   = Math::MakeIdentity4x4();
	transformationDataSprite->World = Math::MakeIdentity4x4();

	Transform transformSprite{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

	ID3D12Resource* indexresourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//リソースの先頭アドレスから使う
	indexBufferViewSprite.BufferLocation = indexresourceSprite->GetGPUVirtualAddress();
	// インデックスバッファのサイズは6個分
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	// インデックスバッファの形式は32bit符号なし整数
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	// インデックスバッファの内容を設定
	uint32_t* indexDataSprite = nullptr;
	indexResou
	// =========================================================================
	// 【追加】平行光源リソース
	// =========================================================================
	ID3D12Resource* directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// 初期値: 真上から白いライトで照らす
	directionalLightData->color     = {1.0f, 1.0f, 1.0f, 1.0f};
	directionalLightData->direction = {0.0f, -1.0f, 0.0f}; // 正規化済み・真上から下へ
	directionalLightData->intensity = 1.0f;

	// ビューポートとシザー矩形
	D3D12_VIEWPORT viewport{};
	viewport.Width    = kClientWidth;
	viewport.Height   = kClientHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissorRect{};
	scissorRect.right  = kClientWidth;
	scissorRect.bottom = kClientHeight;

	// ログ
	std::filesystem::create_directory("logs");
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	std::chrono::zoned_time localTime{std::chrono::current_zone(), nowSeconds};
	std::string logFilePath = std::string("logs/log_") + std::format("{:%Y%m%d_%H%M%S}", localTime) + ".log";
	std::ofstream logFile(logFilePath);

	// Transformの初期値
	Transform transform{
	    Vector3{1.0f, 1.0f, 1.0f},
	    Vector3{0.0f, 0.0f, 0.0f},
	    Vector3{0.0f, 0.0f, 0.0f}
	};
	Transform cameraTransform{
	    Vector3{1.0f, 1.0f, 1.0f},
	    Vector3{0.0f, 0.0f, 0.0f},
	    Vector3{0.0f, 0.0f, -5.0f}
	};

	bool useMonsterBall = true;

	MSG msg{};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
#ifdef USE_IMGUI
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			ImGui::ShowDemoWindow();
			ImGui::Checkbox("useMonsterBall", &useMonsterBall);
			// 【任意】ImGuiでライト設定を変更できるようにする
			ImGui::Begin("Directional Light");
			ImGui::ColorEdit4("color", &directionalLightData->color.x);
			ImGui::SliderFloat3("direction", &directionalLightData->direction.x, -1.0f, 1.0f);
			ImGui::SliderFloat("intensity", &directionalLightData->intensity, 0.0f, 1.0f);
			ImGui::End();
#endif

			// Update
			transform.rotate.y += 0.03f;

			// ワールド行列
			Matrix4x4 worldMatrix = Math::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

			// ビュー・プロジェクション行列
			Matrix4x4 cameraMatrix     = Math::MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix       = Math::Inverse(cameraMatrix);
			float aspectRatio          = static_cast<float>(kClientWidth) / static_cast<float>(kClientHeight);
			Matrix4x4 projectionMatrix = Math::MakePerspectiveFovMatrix(0.45f, aspectRatio, 0.1f, 100.0f);
			Matrix4x4 wvpMatrix        = Math::Multiply(worldMatrix, Math::Multiply(viewMatrix, projectionMatrix));

			// 【変更】WVPとWorldの両方を転送
			transformationData->WVP   = wvpMatrix;
			transformationData->World = worldMatrix;

			// Sprite用行列更新
			Matrix4x4 worldMatrixSprite      = Math::MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite       = Math::MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = Math::MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
			Matrix4x4 wvpMatrixSprite        = Math::Multiply(worldMatrixSprite, Math::Multiply(viewMatrixSprite, projectionMatrixSprite));
			transformationDataSprite->WVP    = wvpMatrixSprite;
			transformationDataSprite->World  = worldMatrixSprite;

#ifdef USE_IMGUI
			ImGui::Render();
#endif

			// --- 描画開始 ---
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource   = swapChainResources[backBufferIndex];
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
			commandList->ResourceBarrier(1, &barrier);

			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);
			commandList->SetGraphicsRootSignature(rootSignature);
			commandList->SetPipelineState(graphicsPipelineState);

			ID3D12DescriptorHeap* descriptorHeaps[] = {srvDescriptorHeap};
			commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			// 【追加】平行光源をRootParameter[3]にセット（毎フレーム共通）
			commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

			// -----------------------------------------------------------------
			// ① 3Dオブジェクト（モンスターボール）の描画
			// -----------------------------------------------------------------
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(1, transformationResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(kVertexCount, 1, 0, 0);

			// -----------------------------------------------------------------
			// ② Spriteの描画（ライティングなし）
			// -----------------------------------------------------------------
			commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress()); // 【変更】Sprite用マテリアル
			commandList->SetGraphicsRootConstantBufferView(1, transformationResourceSprite->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
			commandList->IASetIndexBuffer(&indexBufferViewSprite);
			commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

#ifdef USE_IMGUI
			commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif

			// --- 描画終了 ---
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
			commandList->ResourceBarrier(1, &barrier);

			hr = commandList->Close(); assert(SUCCEEDED(hr));
			ID3D12CommandList* commandLists[] = {commandList};
			commandQueue->ExecuteCommandLists(1, commandLists);

			swapChain->Present(1, 0);

			fenceValue++;
			commandQueue->Signal(fence, fenceValue);
			if (fence->GetCompletedValue() < fenceValue) {
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			hr = commandAllocator->Reset(); assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator, nullptr); assert(SUCCEEDED(hr));

			logFile << "Loop running..." << std::endl;
		}
	}

	CloseHandle(fenceEvent);
	fenceValue++;
	commandQueue->Signal(fence, fenceValue);
	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif

	// リソース解放
	if (directionalLightResource)   directionalLightResource->Release();
	if (materialResourceSprite)     materialResourceSprite->Release();
	if (materialResource)           materialResource->Release();
	if (transformationResource)     transformationResource->Release();
	if (vertexResource)             vertexResource->Release();
	if (VertexResourcesprite)       VertexResourcesprite->Release();
	if (transformationResourceSprite) transformationResourceSprite->Release();
	if (textureresource)            textureresource->Release();
	if (textureResource2)           textureResource2->Release();
	if (graphicsPipelineState)      graphicsPipelineState->Release();
	if (rootSignature)              rootSignature->Release();
	if (vertexShaderBlob)           vertexShaderBlob->Release();
	if (pixelShaderBlob)            pixelShaderBlob->Release();
	if (signatureBlob)              signatureBlob->Release();
	if (errorBlob)                  errorBlob->Release();
	if (includeHandler)             includeHandler->Release();
	if (dxcCompiler)                dxcCompiler->Release();
	if (dxcUtils)                   dxcUtils->Release();
	if (depthStencilResource)       depthStencilResource->Release();
	if (dsvDescriptorHeap)          dsvDescriptorHeap->Release();
#ifdef USE_IMGUI
	if (srvDescriptorHeap)          srvDescriptorHeap->Release();
#endif
	if (swapChainResources[0])      swapChainResources[0]->Release();
	if (swapChainResources[1])      swapChainResources[1]->Release();
	if (rtvDescriptorHeap)          rtvDescriptorHeap->Release();
	if (swapChain)                  swapChain->Release();
	if (commandList)                commandList->Release();
	if (commandAllocator)           commandAllocator->Release();
	if (commandQueue)               commandQueue->Release();
	if (fence)                      fence->Release();
	if (device)                     device->Release();
	if (useAdapter)                 useAdapter->Release();
	if (dxgiFactory)                dxgiFactory->Release();
#ifdef USE_IMGUI
	if (debugController)            debugController->Release();
#endif

	CloseWindow(hwnd);

	IDXGIDebug1* debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL,  DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP,  DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}

	CoUninitialize();
	return 0;
}
