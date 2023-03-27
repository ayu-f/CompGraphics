#pragma once

#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <memory>

struct TextureDesc
{
    std::unique_ptr<uint8_t[]> ddsData;
    UINT32 pitch = 0;
    UINT32 mipmapsCount = 0;
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    UINT32 width = 0;
    UINT32 height = 0;
    const void* pData = nullptr;
};

size_t GetBytesPerBlock(DXGI_FORMAT fmt);

bool LoadDDS(const wchar_t* fileName, TextureDesc& outTextureDesc);
