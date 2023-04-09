#pragma once

#include "framework.h"
#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <d3dcompiler.h>
#include <algorithm>
#include "SceneManager.h"
#include "LoadDDS.h"
#include "GeometryData.h"

class Renderer {
public:
    SceneManager pSceneManager;

    static Renderer& GetInstance() {
        static Renderer instance;
        return instance;
    }
    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    bool Init(HWND hWnd);
    HRESULT InitShaders();
    void Clean();
    bool Render();
    bool Resize(UINT width, UINT height);
    bool IsRunning() { return m_isRunning; }
    ~Renderer();
private:
    Renderer() {};
    HRESULT InitTextures();
    void InitSceneResources();
    HRESULT CompileShader(const std::wstring& path, ID3D11DeviceChild** ppShader, const std::string& ext,
        ID3DBlob** ppCode = nullptr,  D3D_SHADER_MACRO* macros = nullptr);
    HRESULT SetupBackBuffer();
    HRESULT SetupDepthBlend();
    HRESULT SetupColorBuffer();
    bool Update();

    unsigned int m_width = 1280;
    unsigned int m_height = 720;
    IDXGISwapChain* m_pSwapChain = NULL;
    ID3D11Device* m_pDevice = NULL;
    ID3D11DeviceContext* m_pDeviceContext = NULL;
    ID3D11RenderTargetView* m_pBackBufferRTV = NULL;

    ID3D11Buffer * m_pSphereVertexBuffer = NULL;
    ID3D11Buffer * m_pSphereIndexBuffer = NULL;
    ID3D11Buffer * m_pCubeVertexBuffer = NULL;
    ID3D11Buffer * m_pCubeIndexBuffer = NULL;

    ID3D11Buffer* m_pSceneBuffer = NULL;
    ID3D11Buffer* m_pViewBuffer = NULL;

    ID3D11PixelShader* m_pSkyboxPS = NULL;
    ID3D11VertexShader* m_pSkyboxVS = NULL;
    ID3D11InputLayout* m_pSkyboxInputLayout = NULL;

    ID3D11SamplerState* m_pTextureSampler = NULL;

    ID3D11Texture2D* m_pCubemapTexture = NULL;
    ID3D11ShaderResourceView* m_pCubemapTextureView = NULL;
    //
    ID3D11Texture2D* m_pDepthBuffer = NULL;
    ID3D11DepthStencilView* m_pDepthBufferDSV = NULL;
    ID3D11DepthStencilState* m_pDepthStateReadWrite = NULL;
    ID3D11DepthStencilState* m_pDepthStateRead = NULL;

    ID3D11BlendState* m_pTransBlendState = NULL;
    ID3D11Buffer* m_pPlaneVertexBuffer = NULL;
    ID3D11Buffer* m_pPlaneIndexBuffer = NULL;
    ID3D11PixelShader* m_pColorTexturePS = NULL;
    ID3D11VertexShader* m_pColorTextureVS = NULL;
    ID3D11InputLayout* m_pColorTextureInputLayout = NULL;

    //
    ID3D11PixelShader* m_pPostprocPixelShader = NULL;
    ID3D11VertexShader* m_pPostprocVertexShader = NULL;

    ID3D11VertexShader* m_pBaseVertexShader = NULL;
    ID3D11InputLayout* m_pBaseInputLayout = NULL;

    ID3D11PixelShader* m_pBasePixelShader = NULL;
    ID3D11PixelShader* m_pTransPixelShader = NULL;

    ID3D11Buffer* m_pDefaultModelBuffer = NULL;
    ID3D11Buffer* m_pVisibleBuffer = NULL;

    ID3D11Texture2D* m_pColorTextureArray = NULL;
    ID3D11ShaderResourceView* m_pColorTextureArrayView = NULL;

    ID3D11Texture2D* m_pNormalMapArray = NULL;
    ID3D11ShaderResourceView* m_pNormalMapArrayView = NULL;

    ID3D11PixelShader* m_pGrayPostprocPixelShader = NULL;

    ID3D11Buffer* m_pCubesModelBuffer = NULL;
    ID3D11Buffer* m_pSpheresModelBuffer = NULL;

    ID3D11Texture2D* m_pColorBuffer = NULL;
    ID3D11RenderTargetView* m_pColorBufferRTV = NULL;
    ID3D11ShaderResourceView* m_pColorBufferSRV = NULL;

    GeometryData SphereGeometry;
    GeometryData CubeGeometry;
    GeometryData PlaneGeometry;
    vector<ObjectBuffer> objBuffers;
    ObjectBuffer planeBuffers;

    HRESULT SetupDepthBuffer();

    bool m_isRunning = false;
};
