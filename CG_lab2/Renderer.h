#pragma once

#include "framework.h"
#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <d3dcompiler.h>

class Renderer {
public:
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
    HRESULT CompileShader(const std::wstring& path, ID3D11DeviceChild** ppShader, const std::string& ext, ID3DBlob** ppCode = nullptr);
    HRESULT SetupBackBuffer();

    unsigned int m_width = 1280;
    unsigned int m_height = 720;
    IDXGISwapChain* m_pSwapChain = NULL;
    ID3D11Device* m_pDevice = NULL;
    ID3D11DeviceContext* m_pDeviceContext = NULL;
    ID3D11RenderTargetView* m_pBackBufferRTV = NULL;

    ID3D11Buffer* m_pVertexBuffer = NULL;
    ID3D11Buffer* m_pIndexBuffer = NULL;

    ID3D11PixelShader* m_pPixelShader = NULL;
    ID3D11VertexShader* m_pVertexShader = NULL;
    ID3D11InputLayout* m_pInputLayout = NULL;

    bool m_isRunning = false;
};
