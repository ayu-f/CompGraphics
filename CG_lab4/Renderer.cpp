#include "Renderer.h"

template <class DirectXClass>
void SafeRelease(DirectXClass* pointer)
{
	if (pointer != NULL)
		pointer->Release();
}

struct Vertex {
	float x, y, z;
};

struct TextureVertex {
	float x, y, z;
	float u, v;
};


struct SceneBuffer {
	DirectX::XMMATRIX model;
};

struct ViewBuffer {
	DirectX::XMMATRIX vp;
	DirectX::XMVECTOR cameraPosition;
};

UINT32 Up(UINT32 a, UINT32 b)
{
	return (a + b - 1) / b;
}

std::string ws2s(const std::wstring& wstr) {
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
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

HRESULT Renderer::InitTextures() {
	TextureDesc textureDesc;
	HRESULT result;
	{
		const std::wstring TextureName = L"src/kit.dds";
		bool ddsRes = LoadDDS(TextureName.c_str(), textureDesc);
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Format = textureDesc.fmt;
		desc.ArraySize = 1;
		desc.MipLevels = textureDesc.mipmapsCount;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Height = textureDesc.height;
		desc.Width = textureDesc.width;

		UINT32 blockWidth = Up(desc.Width, 4u);
		UINT32 blockHeight = Up(desc.Height, 4u);
		UINT32 pitch = blockWidth * UINT32(GetBytesPerBlock(desc.Format));
		const char* pSrcData = reinterpret_cast<const char*>(textureDesc.pData);
		std::vector<D3D11_SUBRESOURCE_DATA> data;
		data.resize(desc.MipLevels);
		for (UINT32 i = 0; i < desc.MipLevels; i++)
		{
			data[i].pSysMem = pSrcData;
			data[i].SysMemPitch = pitch;
			data[i].SysMemSlicePitch = 0;
			pSrcData += pitch * size_t(blockHeight);
			blockHeight = max(1u, blockHeight / 2);
			blockWidth = max(1u, blockWidth / 2);
			pitch = blockWidth * UINT32(GetBytesPerBlock(desc.Format));
		}
		result = m_pDevice->CreateTexture2D(&desc, data.data(), &m_pKitTexture);

		if (SUCCEEDED(result))
			result = SetResourceName(m_pKitTexture, ws2s(TextureName));
	}
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = textureDesc.fmt;
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipLevels = textureDesc.mipmapsCount;
		desc.Texture2D.MostDetailedMip = 0;
		result = m_pDevice->CreateShaderResourceView(m_pKitTexture, &desc, &m_pKitTextureView);
		assert(SUCCEEDED(result));
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pKitTextureView, "KitTexture");
		}
	}
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_ANISOTROPIC;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MinLOD = -FLT_MAX;
		desc.MaxLOD = FLT_MAX;
		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = 16;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
		result = m_pDevice->CreateSamplerState(&desc, &m_pTextureSampler);
		assert(SUCCEEDED(result));
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pTextureSampler, "TextureSampler");
		}
	}

	DXGI_FORMAT textureFmt;
	{
		const std::wstring TextureNames[6] = {
			L"src/px.dds", L"src/nx.dds",
			L"src/py.dds", L"src/ny.dds",
			L"src/pz.dds", L"src/nz.dds"
		};
		TextureDesc texDescs[6];
		bool ddsRes = true;
		for (int i = 0; i < 6 && ddsRes; i++)
		{
			ddsRes = LoadDDS(TextureNames[i].c_str(), texDescs[i]);
		}
		textureFmt = texDescs[0].fmt; // Assume all are the same
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Format = textureFmt;
		desc.ArraySize = 6;
		desc.MipLevels = 1;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Height = texDescs[0].height;
		desc.Width = texDescs[0].width;
		UINT32 blockWidth = Up(desc.Width, 4u);
		UINT32 blockHeight = Up(desc.Height, 4u);
		UINT32 pitch = blockWidth * UINT32(GetBytesPerBlock(desc.Format));
		D3D11_SUBRESOURCE_DATA data[6];
		for (int i = 0; i < 6; i++) {
			data[i].pSysMem = texDescs[i].pData;
			data[i].SysMemPitch = pitch;
			data[i].SysMemSlicePitch = 0;
		}
		result = m_pDevice->CreateTexture2D(&desc, data, &m_pCubemapTexture);
		assert(SUCCEEDED(result));
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pCubemapTexture, "CubemapTexture");
		}
	}
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Format = textureFmt;
		desc.ViewDimension = D3D_SRV_DIMENSION_TEXTURECUBE;
		desc.TextureCube.MipLevels = 1;
		desc.TextureCube.MostDetailedMip = 0;

		result = m_pDevice->CreateShaderResourceView(m_pCubemapTexture, &desc, &m_pCubemapTextureView);
		assert(SUCCEEDED(result));
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pCubemapTextureView, "CubemapTextureView");
		}
	}
	return result;
}

HRESULT Renderer::InitShaders() {
	std::vector<Vertex> sphereVertices;
	std::vector<USHORT> sphereIndices;
	int hRes = 18;
	int wRes = 8;
	float rad = 1.1f;

	for (int w = 0; w <= wRes; w++)
	{
		for (int h = 0; h <= hRes; h++)
		{
			float alpha = float(M_PI) * 2.0f * h / hRes;
			float beta = float(M_PI) * 1.0f * w / wRes;
			float x = rad * sinf(beta) * cosf(alpha);
			float z = rad * sinf(beta) * sinf(alpha);
			float y = rad * cosf(beta);
			sphereVertices.push_back({ x, y, z });
		}
	}

	for (int w = 0; w < wRes; w++)
	{
		for (int h = 0; h < hRes; h++)
		{
			int i = w * (hRes + 1) + h;
			int iNext = i + (hRes + 1);
			if (w != 0)
			{
				sphereIndices.push_back(iNext + 1);
				sphereIndices.push_back(i + 1);
				sphereIndices.push_back(i);
			}
			if (w + 1 != wRes)
			{
				sphereIndices.push_back(i);
				sphereIndices.push_back(iNext);
				sphereIndices.push_back(iNext + 1);
			}

		}
	}


	static const TextureVertex Vertices[] = {
		{-1.0, -1.0, -1.0 , 0.0, 1.0},
		{-1.0,  1.0, -1.0, 0.0, 0.0},
		{ 1.0,  1.0, -1.0, 1.0, 0.0},
		{ 1.0, -1.0, -1.0, 1.0, 1.0},

		{ 1.0, -1.0,  1.0, 0.0, 1.0},
		{ 1.0,  1.0,  1.0, 0.0, 0.0},
		{-1.0,  1.0,  1.0, 1.0, 0.0},
		{-1.0, -1.0,  1.0, 1.0, 1.0},

		{-1.0, -1.0,  1.0, 0.0, 1.0},
		{-1.0,  1.0,  1.0, 0.0, 0.0},
		{-1.0,  1.0, -1.0, 1.0, 0.0},
		{-1.0, -1.0, -1.0, 1.0, 1.0},

		{ 1.0, -1.0, -1.0, 0.0, 1.0},
		{ 1.0,  1.0, -1.0, 0.0, 0.0},
		{ 1.0,  1.0,  1.0, 1.0, 0.0},
		{ 1.0, -1.0,  1.0, 1.0, 1.0},

		{-1.0,  1.0, -1.0, 0.0, 1.0},
		{-1.0,  1.0,  1.0, 0.0, 0.0},
		{ 1.0,  1.0,  1.0, 1.0, 0.0},
		{ 1.0,  1.0, -1.0, 1.0, 1.0},

		{-1.0, -1.0,  1.0, 0.0, 1.0},
		{-1.0, -1.0, -1.0, 0.0, 0.0},
		{ 1.0, -1.0, -1.0, 1.0, 0.0},
		{ 1.0, -1.0,  1.0, 1.0, 1.0},

	};

	static const USHORT Indices[] = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4,
		8, 9, 10, 10, 11, 8,
		12, 13, 14, 14, 15, 12,
		16, 17, 18, 18, 19, 16,
		20, 21, 22, 22, 23, 20,
	};
	HRESULT result;
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(Vertex) * UINT32(sphereVertices.size());
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = sphereVertices.data();
		data.SysMemPitch = sizeof(Vertex) * UINT32(sphereVertices.size());
		data.SysMemSlicePitch = 0;

		result = m_pDevice->CreateBuffer(&desc, &data, &m_pSphereVertexBuffer);
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pSphereVertexBuffer, "SphereVertexBuffer");
		}
	}
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(UINT32) * UINT32(sphereIndices.size());
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = sphereIndices.data();
		data.SysMemPitch = sizeof(UINT32) * UINT32(sphereIndices.size());
		data.SysMemSlicePitch = 0;

		result = m_pDevice->CreateBuffer(&desc, &data, &m_pSphereIndexBuffer);
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pSphereIndexBuffer, "SphereIndexBuffer");
		}
	}
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

		result = m_pDevice->CreateBuffer(&desc, &data, &m_pCubeVertexBuffer);
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pCubeVertexBuffer, "CubeVertexBuffer");
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

		result = m_pDevice->CreateBuffer(&desc, &data, &m_pCubeIndexBuffer);
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pCubeIndexBuffer, "CubeIndexBuffer");
		}
	}
	result = InitTextures();
	assert(SUCCEEDED(result));
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(SceneBuffer);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		result = m_pDevice->CreateBuffer(&desc, nullptr, &m_pSceneBuffer);
		assert(SUCCEEDED(result));
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pSceneBuffer, "SceneBuffer");
		}
	}
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(ViewBuffer);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		result = m_pDevice->CreateBuffer(&desc, nullptr, &m_pViewBuffer);
		assert(SUCCEEDED(result));
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pViewBuffer, "ViewBuffer");
		}
	}

	// texture shader
	ID3DBlob* pVertexShaderCode = nullptr;
	if (SUCCEEDED(result)) {
		result = CompileShader(L"Texture_VS.hlsl", (ID3D11DeviceChild**)&m_pTextureVS, "vs", &pVertexShaderCode);
	}
	if (SUCCEEDED(result)) {
		result = CompileShader(L"Texture_PS.hlsl", (ID3D11DeviceChild**)&m_pTexturePS, "ps");
	}

	static const D3D11_INPUT_ELEMENT_DESC TextureInputDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	result = m_pDevice->CreateInputLayout(TextureInputDesc, 2, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pTextureInputLayout);
	if (SUCCEEDED(result))
	{
		result = SetResourceName(m_pTextureInputLayout, "TextureInputLayout");
	}
	SafeRelease(pVertexShaderCode);

	// skybox
	static const D3D11_INPUT_ELEMENT_DESC SkyboxInputDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	if (SUCCEEDED(result))
	{
		result = CompileShader(L"Skybox_VS.hlsl", (ID3D11DeviceChild**)&m_pSkyboxVS, "vs", &pVertexShaderCode);
	}
	if (SUCCEEDED(result))
	{
		result = CompileShader(L"Skybox_PS.hlsl", (ID3D11DeviceChild**)&m_pSkyboxPS, "ps");
	}

	if (SUCCEEDED(result))
	{
		result = m_pDevice->CreateInputLayout(SkyboxInputDesc, 1, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pSkyboxInputLayout);
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pSkyboxInputLayout, "SimpleSkyboxInputLayout");
		}
	}
	SafeRelease(pVertexShaderCode);

	return result;
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
		result = m_pDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, (ID3D11VertexShader**)ppShader);
		if (SUCCEEDED(result)) {
			result = SetResourceName(*ppShader, ws2s(path).c_str());
		}
	}
	else if (ext == "ps") {
		result = m_pDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, (ID3D11PixelShader**)ppShader);
		if (SUCCEEDED(result)) {
			result = SetResourceName(*ppShader, ws2s(path).c_str());
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
	SafeRelease(m_pTextureSampler);
	SafeRelease(m_pKitTextureView);
	SafeRelease(m_pKitTexture);

	SafeRelease(m_pCubemapTextureView);
	SafeRelease(m_pCubemapTexture);

	SafeRelease(m_pSkyboxInputLayout);
	SafeRelease(m_pSkyboxPS);
	SafeRelease(m_pSkyboxVS);
	SafeRelease(m_pSphereVertexBuffer);
	SafeRelease(m_pSphereIndexBuffer);
	SafeRelease(m_pCubeVertexBuffer);
	SafeRelease(m_pCubeIndexBuffer);

	SafeRelease(m_pTextureInputLayout);
	SafeRelease(m_pTexturePS);
	SafeRelease(m_pTextureVS);
	//
	SafeRelease(m_pBackBufferRTV);
	SafeRelease(m_pSwapChain);
	SafeRelease(m_pDevice);
	SafeRelease(m_pViewBuffer);
	SafeRelease(m_pSceneBuffer);
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

	DirectX::XMMATRIX v = DirectX::XMMatrixInverse(nullptr, pSceneManager.m_cameraTransform);
	float f = 100.0f;
	float n = 0.1f;
	float fov = 3.14f / 3;
	float c = 1.0f / tanf(fov / 2);
	float aspectRatio = (float)m_height / m_width;

	float width = n * tanf(fov / 2) * 2;
	float height = aspectRatio * width;
	float skyboxRad = sqrtf(powf(n, 2) + powf(width / 2, 2) + powf(height / 2, 2)) * 1.1f;
	DirectX::XMMATRIX skyboxScale = DirectX::XMMatrixScaling(skyboxRad, skyboxRad, skyboxRad);

	DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveLH(tanf(fov / 2) * 2 * n, tanf(fov / 2) * 2 * n * aspectRatio, n, f);


	D3D11_MAPPED_SUBRESOURCE subresource;
	HRESULT result = m_pDeviceContext->Map(m_pViewBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
	assert(SUCCEEDED(result));
	if (SUCCEEDED(result)) {
		ViewBuffer& sceneBuffer = *reinterpret_cast<ViewBuffer*>(subresource.pData);

		sceneBuffer.vp = DirectX::XMMatrixMultiply(v, p);
		sceneBuffer.cameraPosition = pSceneManager.m_cameraTransform.r[3];
		m_pDeviceContext->Unmap(m_pViewBuffer, 0);
	}

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

	{
		SceneBuffer sceneTransformsBuffer = { skyboxScale };

		m_pDeviceContext->UpdateSubresource(m_pSceneBuffer, 0, nullptr, &sceneTransformsBuffer, 0, 0);

		m_pDeviceContext->IASetInputLayout(m_pSkyboxInputLayout);
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetShader(m_pSkyboxVS, nullptr, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pViewBuffer);
		m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pSceneBuffer);
		m_pDeviceContext->PSSetShader(m_pSkyboxPS, nullptr, 0);

		ID3D11SamplerState* samplers[] = { m_pTextureSampler };
		m_pDeviceContext->PSSetSamplers(0, 1, samplers);
		ID3D11ShaderResourceView* resources[] = { m_pCubemapTextureView };
		m_pDeviceContext->PSSetShaderResources(0, 1, resources);

		m_pDeviceContext->IASetIndexBuffer(m_pSphereIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
		ID3D11Buffer* vertexBuffers[] = { m_pSphereVertexBuffer };
		UINT strides[] = { sizeof(Vertex) };
		UINT offsets[] = { 0 };

		m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);

		m_pDeviceContext->DrawIndexed(756u, 0, 0);
	}
	{
		SceneBuffer sceneBuffer = { pSceneManager.m_modelTransform };

		m_pDeviceContext->UpdateSubresource(m_pSceneBuffer, 0, nullptr, &sceneBuffer, 0, 0);

		m_pDeviceContext->IASetInputLayout(m_pTextureInputLayout);
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetShader(m_pTextureVS, nullptr, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pViewBuffer);
		m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pSceneBuffer);
		m_pDeviceContext->PSSetShader(m_pTexturePS, nullptr, 0);

		ID3D11SamplerState* samplers[] = { m_pTextureSampler };
		m_pDeviceContext->PSSetSamplers(0, 1, samplers);
		ID3D11ShaderResourceView* resources[] = { m_pKitTextureView };
		m_pDeviceContext->PSSetShaderResources(0, 1, resources);


		m_pDeviceContext->IASetIndexBuffer(m_pCubeIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
		ID3D11Buffer* vertexBuffers[] = { m_pCubeVertexBuffer };
		UINT strides[] = { sizeof(TextureVertex) };
		UINT offsets[] = { 0 };
		m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);

		m_pDeviceContext->DrawIndexed(36, 0, 0);
	}

	result = m_pSwapChain->Present(0, 0);

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
