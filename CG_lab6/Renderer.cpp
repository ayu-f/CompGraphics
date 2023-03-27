#include "Renderer.h"

#define SafeRelease(A) if ((A) != NULL) { (A)->Release(); (A) = NULL; }

class D3DInclude : public ID3DInclude
{
	STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName,
		LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
	{
		FILE* pFile = nullptr;
		fopen_s(&pFile, pFileName, "rb");
		assert(pFile != nullptr);
		if (pFile == nullptr)
		{
			return E_FAIL;
		}

		_fseeki64(pFile, 0, SEEK_END);
		long long size = _ftelli64(pFile);
		_fseeki64(pFile, 0, SEEK_SET);

		char* pData = new char[size + 1];

		size_t rd = fread(pData, 1, size, pFile);
		assert(rd == (size_t)size);
		fclose(pFile);

		*ppData = pData;
		*pBytes = (UINT)size;
		return S_OK;
	}
	STDMETHOD(Close)(THIS_ LPCVOID pData)
	{
		free(const_cast<void*>(pData));
		return S_OK;
	}
};

struct Light
{
	DirectX::XMVECTOR pos = { 0, 0, 0, 0 };
	DirectX::XMVECTOR color = { 1, 1, 1, 0 };
};

struct SceneBuffer
{
	DirectX::XMMATRIX model;
	DirectX::XMMATRIX normTransform;
	DirectX::XMFLOAT4 lightParams;
	DirectX::XMFLOAT4 baseColor;
};

struct ViewBuffer
{
	DirectX::XMMATRIX vp;
	DirectX::XMVECTOR cameraPosition;
	DirectX::XMINT4 lightCount = { 0, 0, 0, 0 };
	Light lights[10];
	DirectX::XMFLOAT4 ambientColor;
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

	result = SetupDepthBlend();
	result = SetupBackBuffer();
	if (!SUCCEEDED(result))
		return false;

	result = SetupDepthBuffer();
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

HRESULT Renderer::SetupDepthBlend() {
	HRESULT result = S_OK;
	{
		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
		desc.StencilEnable = FALSE;

		result = m_pDevice->CreateDepthStencilState(&desc, &m_pDepthStateReadWrite);
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pDepthStateReadWrite, "DepthStateReadWrite");
		}

	}
	{
		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
		desc.StencilEnable = FALSE;

		result = m_pDevice->CreateDepthStencilState(&desc, &m_pDepthStateRead);
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pDepthStateRead, "DepthStateRead");
		}
	}
	{
		D3D11_BLEND_DESC desc = {};
		desc.AlphaToCoverageEnable = FALSE;
		desc.IndependentBlendEnable = FALSE;
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		result = m_pDevice->CreateBlendState(&desc, &m_pTransBlendState);

		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pTransBlendState, "TransBlendState");
		}
	}
	assert(SUCCEEDED(result));
	return result;
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
	//
	if (SUCCEEDED(result))
	{
		const std::wstring TextureName = L"src/w_base.dds";
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
		UINT32 pitch = UINT(blockWidth * GetBytesPerBlock(desc.Format));
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
			pitch = UINT(blockWidth * GetBytesPerBlock(desc.Format));
		}
		result = m_pDevice->CreateTexture2D(&desc, data.data(), &m_pWallBaseTexture);

		if (SUCCEEDED(result))
			result = SetResourceName(m_pWallBaseTexture, ws2s(TextureName));
	}
	if (SUCCEEDED(result))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = textureDesc.fmt;
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipLevels = textureDesc.mipmapsCount;
		desc.Texture2D.MostDetailedMip = 0;
		result = m_pDevice->CreateShaderResourceView(m_pWallBaseTexture, &desc, &m_pWallBaseTextureView);
		assert(SUCCEEDED(result));
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pWallBaseTextureView, "CracksBasecolorTextureView");
		}
	}
	if (SUCCEEDED(result))
	{
		const std::wstring TextureName = L"src/w_normal.dds";
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
		UINT32 pitch = UINT(blockWidth * GetBytesPerBlock(desc.Format));
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
			pitch = UINT(blockWidth * GetBytesPerBlock(desc.Format));
		}
		result = m_pDevice->CreateTexture2D(&desc, data.data(), &m_pWallNormalTexture);

		if (SUCCEEDED(result))
			result = SetResourceName(m_pWallNormalTexture, ws2s(TextureName));
	}
	if (SUCCEEDED(result))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = textureDesc.fmt;
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipLevels = textureDesc.mipmapsCount;
		desc.Texture2D.MostDetailedMip = 0;
		result = m_pDevice->CreateShaderResourceView(m_pWallNormalTexture, &desc, &m_pWallNormalTextureView);
		assert(SUCCEEDED(result));
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pWallNormalTextureView, "CracksNormalTextureView");
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

	GeometryData::getSphereGeometry(sphereVertices, sphereIndices, hRes, wRes, rad);

	std::vector<TextureNormalVertex> cubeVertices;
	std::vector<USHORT> cubeIndices;
	GeometryData::getCubeGeometry(cubeVertices, cubeIndices);

	std::vector<TextureNormalVertex> planeVertices;
	std::vector<USHORT> planeIndices;
	GeometryData::getPlaneGeometry(planeVertices, planeIndices);

	HRESULT result;

	// plane
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = UINT(sizeof(TextureNormalVertex) * planeVertices.size());
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = planeVertices.data();
		data.SysMemPitch = UINT(sizeof(TextureNormalVertex) * planeVertices.size());
		data.SysMemSlicePitch = 0;

		result = m_pDevice->CreateBuffer(&desc, &data, &m_pPlaneVertexBuffer);
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pPlaneVertexBuffer, "PlaneVertexBuffer");
		}
	}
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = UINT(sizeof(USHORT) * planeIndices.size());
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = planeIndices.data();
		data.SysMemPitch = UINT(sizeof(USHORT) * planeIndices.size());
		data.SysMemSlicePitch = 0;

		result = m_pDevice->CreateBuffer(&desc, &data, &m_pPlaneIndexBuffer);
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pPlaneIndexBuffer, "PlaneIndexBuffer");
		}
	}
	//sphere
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
	//cube
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = UINT(cubeVertices.size() * sizeof(TextureNormalVertex));
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = cubeVertices.data();
		data.SysMemPitch = UINT(cubeVertices.size() * sizeof(TextureNormalVertex));
		data.SysMemSlicePitch = 0;

		result = m_pDevice->CreateBuffer(&desc, &data, &m_pCubeVertexBuffer);
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pCubeVertexBuffer, "CubeVertexBuffer");
		}
	}
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = UINT(sizeof(USHORT) * cubeIndices.size());
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = cubeIndices.data();
		data.SysMemPitch = UINT(sizeof(USHORT) * cubeIndices.size());
		data.SysMemSlicePitch = 0;

		result = m_pDevice->CreateBuffer(&desc, &data, &m_pCubeIndexBuffer);
		if (SUCCEEDED(result)) {
			result = SetResourceName(m_pCubeIndexBuffer, "CubeIndexBuffer");
		}
	}
	// texture
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
	std::vector<D3D_SHADER_MACRO> shaderDefines;

	static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	if (SUCCEEDED(result)) {
		result = CompileShader(L"Texture_VS.hlsl", (ID3D11DeviceChild**)&m_pTextureVS, "vs", &pVertexShaderCode);
	}
	if (SUCCEEDED(result)) {
		shaderDefines.resize(3);
		shaderDefines[0] = D3D_SHADER_MACRO{ "USE_LIGHT", "" };
		shaderDefines[1] = D3D_SHADER_MACRO{ "USE_NORMAL_MAP", "" };
		shaderDefines[2] = D3D_SHADER_MACRO{ nullptr, nullptr };
		result = CompileShader(L"Texture_PS.hlsl", (ID3D11DeviceChild**)&m_pTexturePS, "ps", nullptr, shaderDefines.data());
	}

	result = m_pDevice->CreateInputLayout(InputDesc, 4, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pTextureInputLayout);
	if (SUCCEEDED(result))
	{
		result = SetResourceName(m_pTextureInputLayout, "TextureInputLayout");
	}
	SafeRelease(pVertexShaderCode);

	// skybox
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
		result = m_pDevice->CreateInputLayout(InputDesc, 4, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pSkyboxInputLayout);
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pSkyboxInputLayout, "SkyboxInputLayout");
		}
	}
	SafeRelease(pVertexShaderCode);
	//


	if (SUCCEEDED(result)) {
		result = CompileShader(L"ColorTexture_VS.hlsl", (ID3D11DeviceChild**)&m_pTransColorVS, "vs", &pVertexShaderCode);
	}
	if (SUCCEEDED(result)) {
		shaderDefines.resize(3);
		shaderDefines[0] = D3D_SHADER_MACRO{ "USE_LIGHT", "" };
		shaderDefines[1] = D3D_SHADER_MACRO{ "USE_TRANSPARENCY", "" };
		shaderDefines[2] = D3D_SHADER_MACRO{ nullptr, nullptr };
		result = CompileShader(L"ColorTexture_PS.hlsl", (ID3D11DeviceChild**)&m_pTransColorPS, "ps", nullptr, shaderDefines.data());
	}

	result = m_pDevice->CreateInputLayout(InputDesc, 4, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pTransColorInputLayout);
	if (SUCCEEDED(result))
	{
		result = SetResourceName(m_pTransColorInputLayout, "TransColorInputLayout");
	}
	SafeRelease(pVertexShaderCode);


	if (SUCCEEDED(result)) {
		result = CompileShader(L"ColorTexture_VS.hlsl", (ID3D11DeviceChild**)&m_pColorTextureVS, "vs", &pVertexShaderCode);
	}
	if (SUCCEEDED(result)) {
		result = CompileShader(L"ColorTexture_PS.hlsl", (ID3D11DeviceChild**)&m_pColorTexturePS, "ps");
	}

	result = m_pDevice->CreateInputLayout(InputDesc, 4, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pColorTextureInputLayout);
	if (SUCCEEDED(result))
	{
		result = SetResourceName(m_pColorTextureInputLayout, "ColorTextureInputLayout");
	}
	SafeRelease(pVertexShaderCode);

	SphereGeometry = { m_pSphereIndexBuffer, m_pSphereVertexBuffer, sizeof(Vertex), 0, 756 };
	CubeGeometry = { m_pCubeIndexBuffer, m_pCubeVertexBuffer, sizeof(TextureNormalVertex), 0, UINT(cubeIndices.size()) };
	PlaneGeometry = { m_pPlaneIndexBuffer, m_pPlaneVertexBuffer, sizeof(TextureNormalVertex), 0, UINT(planeIndices.size()) };

	return result;
}

HRESULT Renderer::CompileShader(const std::wstring& path, ID3D11DeviceChild** ppShader, const std::string& ext, ID3DBlob** ppCode, D3D_SHADER_MACRO* macros) {
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
	D3DInclude d3dInclude;
	std::string tmp = ws2s(path);
	HRESULT result = D3DCompile(data.data(), data.size(), tmp.c_str(), macros, &d3dInclude, entryPoint.c_str(), platform.c_str(), flags1, 0, &pCode, &pErrMsg);
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

	SafeRelease(m_pTransColorPS);
	SafeRelease(m_pTransColorVS);
	SafeRelease(m_pTransColorInputLayout);

	SafeRelease(m_pColorTexturePS);
	SafeRelease(m_pColorTextureVS);
	SafeRelease(m_pColorTextureInputLayout);
	SafeRelease(m_pWallBaseTexture);
	SafeRelease(m_pWallBaseTextureView);
	SafeRelease(m_pWallNormalTexture);
	SafeRelease(m_pWallNormalTextureView);

	SafeRelease(m_pTextureSampler);
	SafeRelease(m_pKitTextureView);
	SafeRelease(m_pKitTexture);
	SafeRelease(m_pCubemapTextureView);
	SafeRelease(m_pCubemapTexture);

	SafeRelease(m_pSkyboxInputLayout);
	SafeRelease(m_pSkyboxPS);
	SafeRelease(m_pSkyboxVS);

	SafeRelease(m_pDepthBuffer);
	SafeRelease(m_pDepthBufferDSV);
	SafeRelease(m_pViewBuffer);
	SafeRelease(m_pSceneBuffer);

	SafeRelease(m_pPlaneVertexBuffer);
	SafeRelease(m_pPlaneIndexBuffer);
	SafeRelease(m_pTransBlendState);

	SafeRelease(m_pSphereIndexBuffer);
	SafeRelease(m_pSphereVertexBuffer);
	SafeRelease(m_pCubeIndexBuffer);
	SafeRelease(m_pCubeVertexBuffer);

	SafeRelease(m_pDepthStateRead);
	SafeRelease(m_pDepthStateReadWrite);
	SafeRelease(m_pBackBufferRTV);
	SafeRelease(m_pSwapChain);
	SafeRelease(m_pDeviceContext);

	SafeRelease(m_pTextureInputLayout);
	SafeRelease(m_pTexturePS);
	SafeRelease(m_pTextureVS);
	if (NULL != m_pDeviceContext)
		m_pDeviceContext->ClearState();
	SafeRelease(m_pDeviceContext);

	SafeRelease(m_pDevice);
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

	DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveLH(tanf(fov / 2) * 2 * f, tanf(fov / 2) * 2 * f * aspectRatio, f, n);


	D3D11_MAPPED_SUBRESOURCE subresource;
	HRESULT result = m_pDeviceContext->Map(m_pViewBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
	assert(SUCCEEDED(result));
	if (SUCCEEDED(result)) {
		ViewBuffer& viewBuffer = *reinterpret_cast<ViewBuffer*>(subresource.pData);

		viewBuffer.vp = DirectX::XMMatrixMultiply(v, p);
		viewBuffer.cameraPosition = pSceneManager.m_cameraTransform.r[3];
		viewBuffer.ambientColor = { 0.1f, 0.1f, 0.1f, 1 };
		std::vector<Light> lights;
		lights.push_back({ pSceneManager.m_lightPos, { 20, 20, 20, 1 } });;
		int lightCount = int(min(lights.size(), 10));
		viewBuffer.lightCount = { lightCount, 0, 0, 0 };
		for (int i = 0; i < lightCount; i++)
		{
			viewBuffer.lights[i] = lights[i];
		}
		m_pDeviceContext->Unmap(m_pViewBuffer, 0);
	}

	ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
	m_pDeviceContext->OMSetRenderTargets(1, views, m_pDepthBufferDSV);

	static const FLOAT BackColor[4] = { 0.3f, 0.2f, 0.8f, 0.9f };
	m_pDeviceContext->ClearRenderTargetView(m_pBackBufferRTV, BackColor);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthBufferDSV, D3D11_CLEAR_DEPTH, 0.0f, 0);

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
	const int indexCountCubes = 36;
	{
		DirectX::XMFLOAT4 lightParams = { 1.0f, 1.0f, 3.0f, 32.0f };
		DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMMATRIX model = DirectX::XMMatrixScaling(0.2f, 0.2f, 0.2f) * DirectX::XMMatrixTranslationFromVector(pSceneManager.m_lightPos);
		SceneBuffer sceneBuffer = { model, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, model)), lightParams, baseColor };

		m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateReadWrite, 0);
		m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		m_pDeviceContext->IASetInputLayout(m_pColorTextureInputLayout);
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetShader(m_pColorTextureVS, nullptr, 0);
		m_pDeviceContext->PSSetShader(m_pColorTexturePS, nullptr, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pViewBuffer);
		m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pSceneBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pViewBuffer);
		m_pDeviceContext->PSSetConstantBuffers(1, 1, &m_pSceneBuffer);

		m_pDeviceContext->IASetIndexBuffer(SphereGeometry.pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
		m_pDeviceContext->IASetVertexBuffers(0, 1, SphereGeometry.vertexBuffer, SphereGeometry.strides, SphereGeometry.offsets);
		m_pDeviceContext->UpdateSubresource(m_pSceneBuffer, 0, nullptr, &sceneBuffer, 0, 0);
		m_pDeviceContext->DrawIndexed(SphereGeometry.indexCount, 0, 0);
	}
	{
		//Texture
		m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateReadWrite, 0);
		m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		m_pDeviceContext->IASetInputLayout(m_pTextureInputLayout);
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetShader(m_pTextureVS, nullptr, 0);
		m_pDeviceContext->PSSetShader(m_pTexturePS, nullptr, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pViewBuffer);
		m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pSceneBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pViewBuffer);
		m_pDeviceContext->PSSetConstantBuffers(1, 1, &m_pSceneBuffer);

		ID3D11SamplerState* samplers[] = { m_pTextureSampler };
		m_pDeviceContext->PSSetSamplers(0, 1, samplers);

		std::vector<SceneBuffer> sceneTransformsBuffer(2);
		DirectX::XMMATRIX model = pSceneManager.m_modelTransform;
		DirectX::XMFLOAT4 lightParams = { 1.0f, 1.0f, 3.0f, 32.0f };
		DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		sceneTransformsBuffer[0] = { model , DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, model)), lightParams, baseColor };
		auto tmpp = DirectX::XMMatrixTranslation(1.8f, 0.3f, -1.8f);
		sceneTransformsBuffer[1] = { tmpp,  DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, tmpp)), lightParams, baseColor };


		m_pDeviceContext->IASetIndexBuffer(CubeGeometry.pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
		m_pDeviceContext->IASetVertexBuffers(0, 1, CubeGeometry.vertexBuffer, CubeGeometry.strides, CubeGeometry.offsets);

		{
			ID3D11ShaderResourceView* resources[] = { m_pWallBaseTextureView , m_pWallNormalTextureView };
			m_pDeviceContext->PSSetShaderResources(0, 2, resources);
			m_pDeviceContext->UpdateSubresource(m_pSceneBuffer, 0, nullptr, &sceneTransformsBuffer[0], 0, 0);
			m_pDeviceContext->DrawIndexed(CubeGeometry.indexCount, 0, 0);
		}
		{
			ID3D11ShaderResourceView* resources[] = { m_pKitTextureView , m_pWallNormalTextureView };
			m_pDeviceContext->PSSetShaderResources(0, 2, resources);
			m_pDeviceContext->UpdateSubresource(m_pSceneBuffer, 0, nullptr, &sceneTransformsBuffer[1], 0, 0);
			m_pDeviceContext->DrawIndexed(CubeGeometry.indexCount, 0, 0);
		}
	}
	{
		//skybox
		m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateRead, 0);
		m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		m_pDeviceContext->IASetInputLayout(m_pSkyboxInputLayout);
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetShader(m_pSkyboxVS, nullptr, 0);
		m_pDeviceContext->PSSetShader(m_pSkyboxPS, nullptr, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pViewBuffer);
		m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pSceneBuffer);

		ID3D11SamplerState* samplers[] = { m_pTextureSampler };
		m_pDeviceContext->PSSetSamplers(0, 1, samplers);
		SceneBuffer sceneTransformsBuffer = { skyboxScale };

		ID3D11ShaderResourceView* resources[] = { m_pCubemapTextureView };
		m_pDeviceContext->PSSetShaderResources(0, 1, resources);

		m_pDeviceContext->IASetIndexBuffer(m_pSphereIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
		ID3D11Buffer* vertexBuffers[] = { m_pSphereVertexBuffer };
		UINT strides[] = { sizeof(Vertex) };
		UINT offsets[] = { 0 };
		m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);

		m_pDeviceContext->UpdateSubresource(m_pSceneBuffer, 0, nullptr, &sceneTransformsBuffer, 0, 0);
		m_pDeviceContext->DrawIndexed(756, 0, 0);
	}
	{
		m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateRead, 0);
		m_pDeviceContext->OMSetBlendState(m_pTransBlendState, nullptr, 0xFFFFFFFF);
		m_pDeviceContext->IASetInputLayout(m_pTransColorInputLayout);
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetShader(m_pTransColorVS, nullptr, 0);
		m_pDeviceContext->PSSetShader(m_pTransColorPS, nullptr, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pViewBuffer);
		m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pSceneBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pViewBuffer);
		m_pDeviceContext->PSSetConstantBuffers(1, 1, &m_pSceneBuffer);


		std::vector<SceneBuffer> sceneBuffers;
		DirectX::XMFLOAT4 lightParams = { 1.0f, 1.0f, 3.0f, 32.0f };
		DirectX::XMFLOAT4 baseColor = { 1.0f, 0.0f, 0.0f, 0.4f };
		DirectX::XMMATRIX model = DirectX::XMMatrixTranslation(-2.125f, 1.0f, -1.25f);
		SceneBuffer sceneBuffer = { model, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, model)), lightParams, baseColor };
		sceneBuffers.emplace_back(std::move(sceneBuffer));

		lightParams = { 1.0f, 1.0f, 3.0f, 32.0f };
		baseColor = { 0.0f, 1.0f, 0.0f, 0.4f };
		model = DirectX::XMMatrixScaling(2, 2, 2) * DirectX::XMMatrixTranslation(-1.125f, 1.0f, 3.25f);
		sceneBuffer = { model, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, model)), lightParams, baseColor };
		sceneBuffers.emplace_back(std::move(sceneBuffer));

		std::vector<std::pair<int, float>> cameraDist;
		for (int i = 0; i < sceneBuffers.size(); i++)
		{
			float minDist = 0.1;
			for (size_t j = 0; j < 4; j++)
			{
				float x = -1.0f + 2.0f * (j % 2);
				float y = -1.0f + 2.0f * (j / 2 % 2);
				float dist = DirectX::XMVectorGetZ((DirectX::XMMatrixTranslation(x, y, 0.0f) * sceneBuffers[i].model * v).r[3]);
				if (dist < minDist)
					minDist = dist;
			}
			cameraDist.push_back({ i, minDist });
		}

		std::stable_sort(cameraDist.begin(), cameraDist.end(), [](const std::pair<int, float>& a, const std::pair<int, float>& b)
			{
				return a.second > b.second;
			});

		

		for (int i = 0; i < cameraDist.size(); i++)
		{
			m_pDeviceContext->IASetIndexBuffer(PlaneGeometry.pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
			m_pDeviceContext->IASetVertexBuffers(0, 1, PlaneGeometry.vertexBuffer, PlaneGeometry.strides, PlaneGeometry.offsets);
			m_pDeviceContext->UpdateSubresource(m_pSceneBuffer, 0, nullptr, &sceneBuffers[cameraDist[i].first], 0, 0);
			m_pDeviceContext->DrawIndexed(PlaneGeometry.indexCount, 0, 0);
		}
	}

	result = m_pSwapChain->Present(0, 0);

	return SUCCEEDED(result);
}


bool Renderer::Resize(UINT width, UINT height)
{
	if (width != m_width || height != m_height)
	{
		SafeRelease(m_pBackBufferRTV);
		SafeRelease(m_pDepthBufferDSV);
		SafeRelease(m_pDepthBuffer);

		HRESULT result = m_pSwapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		if (!SUCCEEDED(result))
			return false;
		m_width = width;
		m_height = height;

		result = SetupBackBuffer();
		assert(SUCCEEDED(result));
		result = SetupDepthBuffer();
		return SUCCEEDED(result);
	}

	return true;
}

HRESULT Renderer::SetupDepthBuffer()
{
	SafeRelease(m_pDepthBufferDSV);
	SafeRelease(m_pDepthBuffer);
	HRESULT result = S_OK;
	if (SUCCEEDED(result))
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Format = DXGI_FORMAT_D32_FLOAT;
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Height = m_height;
		desc.Width = m_width;
		desc.MipLevels = 1;
		result = m_pDevice->CreateTexture2D(&desc, nullptr, &m_pDepthBuffer);
		assert(SUCCEEDED(result));
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pDepthBuffer, "DepthBuffer");
		}
	}
	if (SUCCEEDED(result))
	{
		result = m_pDevice->CreateDepthStencilView(m_pDepthBuffer, nullptr, &m_pDepthBufferDSV);
		assert(SUCCEEDED(result));
		if (SUCCEEDED(result))
		{
			result = SetResourceName(m_pDepthBufferDSV, "pDepthBufferDSV");
		}
	}
	return result;
}

HRESULT Renderer::SetupBackBuffer()
{
	SafeRelease(m_pBackBufferRTV);
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
