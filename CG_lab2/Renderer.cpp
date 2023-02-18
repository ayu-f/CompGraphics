#include "Renderer.h"

template <class DirectXClass>
void SafeRelease(DirectXClass* pointer)
{
	if (pointer != NULL)
		pointer->Release();
}

inline HRESULT SetResourceName(ID3D11DeviceChild* pDevice, const std::string& name)
{
	return pDevice->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.length(), name.c_str());
}

bool Renderer::Init(HWND hWnd)
{
	// Create a DirectX graphics interface factory.
	IDXGIFactory* pFactory = nullptr;
	HRESULT result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);

	// Select hardware adapter
	IDXGIAdapter* pSelectedAdapter = NULL;
	if (SUCCEEDED(result))
	{
		IDXGIAdapter* pAdapter = NULL;
		for (UINT adapterIdx = 0; SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter)); adapterIdx++)
		{
			DXGI_ADAPTER_DESC desc;
			pAdapter->GetDesc(&desc);

			if (wcscmp(desc.Description, L"Microsoft Baisic Render Driver") != 0)
			{
				pSelectedAdapter = pAdapter;
				break;
			}

			pAdapter->Release();
		}
	}
	assert(pSelectedAdapter != NULL);

	// Create DirectX 11 device
	D3D_FEATURE_LEVEL level;
	D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
	if (SUCCEEDED(result))
	{
		UINT flags = 0;
#ifdef _DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG
		result = D3D11CreateDevice(pSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
			flags, levels, 1, D3D11_SDK_VERSION, &m_pDevice, &level, &m_pDeviceContext);
		if (D3D_FEATURE_LEVEL_11_0 != level || !SUCCEEDED(result))
			return false;
	}

	// Create swapchain

	DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc.Width = m_width;
	swapChainDesc.BufferDesc.Height = m_height;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Windowed = true;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = 0;

	result = pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
	if (!SUCCEEDED(result))
		return false;
	result = SetupBackBuffer();

	if (!SUCCEEDED(result))
		return false;
	result = InitShaders();

	SafeRelease(pSelectedAdapter);
	SafeRelease(pFactory);

	if (SUCCEEDED(result))
	{
		m_isRunning = true;
	}

	return SUCCEEDED(result);
}

Renderer::~Renderer() {
	Clean();
}

struct Vertex {
    float x, y, z;
    COLORREF color;
};

HRESULT Renderer::InitShaders() {
	static const Vertex Vertices[] = {
		{-0.5f, -0.5f, 0.0f, RGB(255, 0, 0)},
		{ 0.5f, -0.5f, 0.0f, RGB(0, 255, 0)},
		{ 0.0f,  0.5f, 0.0f, RGB(0, 0, 255)}
	};
	static const USHORT Indices[] = { 0, 2, 1 };
	HRESULT result;
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(Vertices);
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = &Vertices;
		data.SysMemPitch = sizeof(Vertices);
		data.SysMemSlicePitch = 0;

		result = m_pDevice->CreateBuffer(&desc, &data, &m_pVertexBuffer);
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pVertexBuffer, "VertexBuffer");
		}
	}
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(Indices);
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = &Indices;
		data.SysMemPitch = sizeof(Indices);
		data.SysMemSlicePitch = 0;

		result = m_pDevice->CreateBuffer(&desc, &data, &m_pIndexBuffer);
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pIndexBuffer, "IndexBuffer");
		}
	}
	ID3DBlob* pVertexShaderCode = NULL;
	if (SUCCEEDED(result)){
		result = CompileShader(L"VertexColor_VS.hlsl", (ID3D11DeviceChild**)&m_pVertexShader, "vs", &pVertexShaderCode);
	}
	if (SUCCEEDED(result)){
		result = CompileShader(L"VertexColor_PS.hlsl", (ID3D11DeviceChild**)&m_pPixelShader, "ps");
	}

	static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	result = m_pDevice->CreateInputLayout(InputDesc, 2, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pInputLayout);
	if (SUCCEEDED(result))
	{
		result = SetResourceName(m_pInputLayout, "InputLayout");
	}

	SafeRelease(pVertexShaderCode);

	return result;
}

std::string ws2s(const std::wstring& wstr) {
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}

HRESULT Renderer::CompileShader(const std::wstring& path, ID3D11DeviceChild** ppShader, const std::string& ext, ID3DBlob** ppCode) {
	FILE* pFile = nullptr;
	_wfopen_s(&pFile, path.c_str(), L"rb");
	assert(pFile != nullptr);

	_fseeki64(pFile, 0, SEEK_END);
	long long size = _ftelli64(pFile);
	_fseeki64(pFile, 0, SEEK_SET);

	std::vector<char> data;
	data.resize(size + 1);
	size_t rd = fread(data.data(), 1, size, pFile);
	fclose(pFile);

	std::string entryPoint = ext;
	std::string platform = ext + "_5_0";
	UINT flags1 = 0;
#ifdef _DEBUG
	flags1 |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // _DEBUG
	ID3DBlob* pCode = nullptr;
	ID3DBlob* pErrMsg = nullptr;
	std::string tmp = ws2s(path);
	HRESULT result = D3DCompile(data.data(), data.size(), tmp.c_str(), nullptr, nullptr, entryPoint.c_str(), platform.c_str(), flags1, 0, &pCode, &pErrMsg);
	if (!SUCCEEDED(result) && pErrMsg != nullptr) {
		OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());
	}
	assert(SUCCEEDED(result));
	SafeRelease(pErrMsg);

	if (ext == "vs") {
		result = m_pDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &m_pVertexShader);
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pVertexShader, ws2s(path).c_str());
		}
	}
	else if (ext == "ps") {
		result = m_pDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &m_pPixelShader);
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pPixelShader, ws2s(path).c_str());
		}
	}

	if (ppCode != nullptr) {
		*ppCode = pCode;
	}
	else {
		SafeRelease(pCode);
	}

	return result;
}

void Renderer::Clean() {
	SafeRelease(m_pInputLayout);
	SafeRelease(m_pPixelShader);
	SafeRelease(m_pVertexShader);
	SafeRelease(m_pIndexBuffer);
	SafeRelease(m_pVertexBuffer);
	SafeRelease(m_pBackBufferRTV);
	SafeRelease(m_pSwapChain);
	SafeRelease(m_pDevice);
	if (NULL != m_pDeviceContext)
		m_pDeviceContext->ClearState();
	SafeRelease(m_pDeviceContext);
	m_isRunning = false;
}

bool Renderer::Render()
{
	if (!m_isRunning)
	{
		return false;
	}

	m_pDeviceContext->ClearState();

	ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
	m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);

	static const FLOAT BackColor[4] = { 0.3f, 0.2f, 0.8f, 0.9f };
	m_pDeviceContext->ClearRenderTargetView(m_pBackBufferRTV, BackColor);

	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (FLOAT)m_width;
	viewport.Height = (FLOAT)m_height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_pDeviceContext->RSSetViewports(1, &viewport);

	D3D11_RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = m_width;
	rect.bottom = m_height;
	m_pDeviceContext->RSSetScissorRects(1, &rect);

	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	ID3D11Buffer* vertexBuffers[] = { m_pVertexBuffer };
	UINT strides[] = { 16 };
	UINT offsets[] = { 0 };
	m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
	m_pDeviceContext->DrawIndexed(3, 0, 0);

	HRESULT result = m_pSwapChain->Present(0, 0);

	return SUCCEEDED(result);
}

bool Renderer::Resize(UINT width, UINT height)
{
	if (width != m_width || height != m_height)
	{
		SafeRelease(m_pBackBufferRTV);

		HRESULT result = m_pSwapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		if (!SUCCEEDED(result))
			return false;
		m_width = width;
		m_height = height;

		result = SetupBackBuffer();
		return SUCCEEDED(result);
	}

	return true;
}

HRESULT Renderer::SetupBackBuffer()
{
	ID3D11Texture2D* pBackBuffer = NULL;
	HRESULT result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	assert(SUCCEEDED(result));
	if (SUCCEEDED(result))
	{
		result = m_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_pBackBufferRTV);
		assert(SUCCEEDED(result));
		SafeRelease(pBackBuffer);
	}
	return result;
}
