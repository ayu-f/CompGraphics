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
    HRESULT CompileShader(const std::wstring& path, ID3D11DeviceChild** ppShader, const std::string& ext,
        ID3DBlob** ppCode = nullptr,  D3D_SHADER_MACRO* macros = nullptr);
    HRESULT SetupBackBuffer();
    HRESULT SetupDepthBlend();
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

    ID3D11PixelShader* m_pTexturePS = NULL;
    ID3D11VertexShader* m_pTextureVS = NULL;
    ID3D11InputLayout* m_pTextureInputLayout = NULL;

    ID3D11PixelShader* m_pSkyboxPS = NULL;
    ID3D11VertexShader* m_pSkyboxVS = NULL;
    ID3D11InputLayout* m_pSkyboxInputLayout = NULL;

    ID3D11Texture2D* m_pKitTexture = NULL;
    ID3D11ShaderResourceView* m_pKitTextureView = NULL;
    ID3D11SamplerState* m_pTextureSampler = NULL;

    ID3D11Texture2D* m_pCubemapTexture = NULL;
    ID3D11ShaderResourceView* m_pCubemapTextureView = NULL;
    //
    ID3D11Texture2D* m_pDepthBuffer = NULL;
    ID3D11DepthStencilView* m_pDepthBufferDSV = NULL;
    ID3D11DepthStencilState* m_pDepthStateReadWrite = NULL;
    ID3D11DepthStencilState* m_pDepthStateRead = NULL;

    ID3D11PixelShader* m_pTransColorPS = NULL;
    ID3D11VertexShader* m_pTransColorVS= NULL;
    ID3D11InputLayout* m_pTransColorInputLayout = NULL;
    ID3D11BlendState* m_pTransBlendState = NULL;

    ID3D11Buffer* m_pPlaneVertexBuffer = NULL;
    ID3D11Buffer* m_pPlaneIndexBuffer = NULL;

    ID3D11PixelShader* m_pColorTexturePS = NULL;
    ID3D11VertexShader* m_pColorTextureVS = NULL;
    ID3D11InputLayout* m_pColorTextureInputLayout = NULL;

    ID3D11Texture2D* m_pWallBaseTexture = NULL;
    ID3D11ShaderResourceView* m_pWallBaseTextureView = NULL;

    ID3D11Texture2D* m_pWallNormalTexture = NULL;
    ID3D11ShaderResourceView* m_pWallNormalTextureView = NULL;

    GeometryData SphereGeometry;
    GeometryData CubeGeometry;
    GeometryData PlaneGeometry;

    HRESULT SetupDepthBuffer();

    bool m_isRunning = false;
};
