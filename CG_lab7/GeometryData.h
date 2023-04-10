#pragma once
#include "framework.h"
#include <vector>

using namespace std;

struct SceneBuffer
{
	DirectX::XMMATRIX model;
	DirectX::XMMATRIX normTransform;
	DirectX::XMFLOAT4 lightParams;
	DirectX::XMFLOAT4 baseColor;
	union{
		DirectX::XMINT4 colorTextureId_normalMapId_useLight = { -1, -1, 1, 0 };
		struct
		{
			int textureId;
			int normalMapId;
			int useLight;
		};
	};
	
	void setModel(const DirectX::XMMATRIX& model) {
		this->model = model;
		normTransform = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, model));
	}
	SceneBuffer() {
		textureId = 0;
		normalMapId = 0;
		useLight = 1;
		setModel(DirectX::XMMatrixIdentity());
	}
};

struct Vertex {
	float x, y, z;
};

struct TextureNormalVertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 tang;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 textureUV;
};

struct Instance {
	SceneBuffer sceneBuffer;
	DirectX::XMFLOAT3 minVec;
	DirectX::XMFLOAT3 maxVec;

	Instance(DirectX::XMMATRIX& model, DirectX::XMFLOAT4& lightParams, DirectX::XMFLOAT4 &baseColor, int textureId = 0) {
		sceneBuffer.setModel(model);
		sceneBuffer.lightParams = lightParams;
		sceneBuffer.baseColor = baseColor;
		sceneBuffer.textureId = textureId;
	}

	Instance() {
		sceneBuffer.lightParams = { 1.0f, 1.0f, 3.0f, 32.0f };
		sceneBuffer.baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		sceneBuffer.textureId = 0;
	}

	void UpdateAABB(const std::vector<DirectX::XMFLOAT3>& AABBSupportVectors, const DirectX::XMMATRIX& model)
	{
		if (AABBSupportVectors.size() == 0)
			return;
		DirectX::XMFLOAT3 vec = *reinterpret_cast<DirectX::XMFLOAT3*>((DirectX::XMMatrixTranslation(AABBSupportVectors[0].x, AABBSupportVectors[0].y, AABBSupportVectors[0].z) * model).r[3].m128_f32);
		minVec = vec;
		maxVec = vec;
		for (auto& supportVec : AABBSupportVectors)
		{
			vec = *reinterpret_cast<DirectX::XMFLOAT3*>((DirectX::XMMatrixTranslation(supportVec.x, supportVec.y, supportVec.z) * model).r[3].m128_f32);
			minVec.x = min(minVec.x, vec.x);
			minVec.y = min(minVec.y, vec.y);
			minVec.z = min(minVec.z, vec.z);
			maxVec.x = max(maxVec.x, vec.x);
			maxVec.y = max(maxVec.y, vec.y);
			maxVec.z = max(maxVec.z, vec.z);
		}
	}

	bool IsVisible(const std::vector<DirectX::XMVECTOR>& frustum)
	{
		for (auto& plane : frustum)
		{
			DirectX::XMVECTOR p =
			{
				plane.m128_f32[0] < 0 ? minVec.x : maxVec.x,
				plane.m128_f32[1] < 0 ? minVec.y : maxVec.y,
				plane.m128_f32[2] < 0 ? minVec.z : maxVec.z,
				1.0f
			};
			float s = DirectX::XMVector4Dot(p, plane).m128_f32[0];
			if (s < 0.0f)
				return false;
		}
		return true;
	}
};

struct ObjectBuffer {
	std::vector<Instance> instances;

	void set(int idx, const DirectX::XMMATRIX& model, vector<DirectX::XMFLOAT3>& vectorsAABB, int textureId = 0) {
		instances[idx].sceneBuffer.setModel(model);
		instances[idx].UpdateAABB(vectorsAABB, model);
		instances[idx].sceneBuffer.textureId = textureId;
	}

	void Update(vector<DirectX::XMFLOAT3>& vectorsAABB) {
		for (auto& inst : instances)
		{
			inst.UpdateAABB(vectorsAABB, inst.sceneBuffer.model);
		}
	}
};

struct GeometryData {
    ID3D11Buffer* pIndexBuffer;
    ID3D11Buffer* vertexBuffer[1];
    UINT strides[1];
    UINT offsets[1];
    UINT indexCount;
	vector<DirectX::XMFLOAT3> vectorsAABB;
    GeometryData()
    {
        pIndexBuffer = nullptr;
        vertexBuffer[0] = nullptr;
        strides[0] = 0;
        offsets[0] = 0;
        indexCount = 0;
    }
    GeometryData(ID3D11Buffer* pIndexBuffer, ID3D11Buffer* pVertexBuffer, UINT stride, UINT offset, UINT indexCount): pIndexBuffer(pIndexBuffer), indexCount(indexCount)
    {
        vertexBuffer[0] = pVertexBuffer;
        strides[0] = stride;
        offsets[0] = offset;
    }

	static void getSphereGeometry(vector<Vertex>& sphereVertices, vector<USHORT>& sphereIndices, int hRes, int wRes, float rad) {
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
	}

	static void getCubeGeometry(vector<TextureNormalVertex>& outVertices, vector<USHORT>& outIndices) {
		static const TextureNormalVertex vertices[] = {
			{ {-1.0f, -1.0f, -1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f } },
			{ {-1.0f,  1.0f, -1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f } },
			{ { 1.0f,  1.0f, -1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f } },
			{ { 1.0f, -1.0f, -1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f } },

			{ { 1.0f, -1.0f,  1.5f }, {-1.0f,  0.0,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f } },
			{ { 1.0f,  1.0f,  1.5f }, {-1.0f,  0.0,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f } },
			{ {-1.0f,  1.0f,  1.5f }, {-1.0f,  0.0,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f } },
			{ {-1.0f, -1.0f,  1.5f }, {-1.0f,  0.0,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f } },

			{ {-1.0f, -1.0f,  1.5f }, { 0.0f,  0.0, -1.0f }, {-1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } },
			{ {-1.0f,  1.0f,  1.5f }, { 0.0f,  0.0, -1.0f }, {-1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } },
			{ {-1.0f,  1.0f, -1.5f }, { 0.0f,  0.0, -1.0f }, {-1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } },
			{ {-1.0f, -1.0f, -1.5f }, { 0.0f,  0.0, -1.0f }, {-1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } },

			{ { 1.0f, -1.0f, -1.5f }, { 0.0f,  0.0,  1.0f }, { 1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } },
			{ { 1.0f,  1.0f, -1.5f }, { 0.0f,  0.0,  1.0f }, { 1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } },
			{ { 1.0f,  1.0f,  1.5f }, { 0.0f,  0.0,  1.0f }, { 1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } },
			{ { 1.0f, -1.0f,  1.5f }, { 0.0f,  0.0,  1.0f }, { 1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } },

			{ {-1.0f,  1.0f, -1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f } },
			{ {-1.0f,  1.0f,  1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f } },
			{ { 1.0f,  1.0f,  1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f } },
			{ { 1.0f,  1.0f, -1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f } },

			{ {-1.0f, -1.0f,  1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  -1.0f,  0.0f }, { 0.0f, 1.0f } },
			{ {-1.0f, -1.0f, -1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  -1.0f,  0.0f }, { 0.0f, 0.0f } },
			{ { 1.0f, -1.0f, -1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  -1.0f,  0.0f }, { 1.0f, 0.0f } },
			{ { 1.0f, -1.0f,  1.5f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  -1.0f,  0.0f }, { 1.0f, 1.0f } },

		};

		static const USHORT indices[] = {
			0, 1, 2,
			2, 3, 0,

			4, 5, 6,
			6, 7, 4,

			8, 9, 10,
			10, 11, 8,

			12, 13, 14,
			14, 15, 12,

			16, 17, 18,
			18, 19, 16,

			20, 21, 22,
			22, 23, 20,
		};

		outVertices.resize(sizeof(vertices) / sizeof(TextureNormalVertex));
		outVertices.assign((TextureNormalVertex*)vertices, ((TextureNormalVertex*)vertices + sizeof(vertices) / sizeof(TextureNormalVertex)));
		outIndices.resize(sizeof(indices) / sizeof(USHORT));
		outIndices.assign((USHORT*)indices, ((USHORT*)indices + sizeof(indices) / sizeof(USHORT)));
	}

	static void getPlaneGeometry(vector<TextureNormalVertex>& outVertices, vector<USHORT>& outIndices)
	{
		static const TextureNormalVertex vertices[] = {
			{ {-1.5f, -1.5f,  1.0f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f } },
			{ {-1.5f,  1.5f,  1.0f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f } },
			{ { 1.5f,  1.5f,  1.0f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f } },
			{ { 1.5f, -1.5f,  1.0f }, { 1.0f,  0.0,  0.0f }, { 0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f } },

			{ { 1.5f, -1.5f,  1.0f }, {-1.0f,  0.0,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f } },
			{ { 1.5f,  1.5f,  1.0f }, {-1.0f,  0.0,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f } },
			{ {-1.5f,  1.5f,  1.0f }, {-1.0f,  0.0,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f } },
			{ {-1.5f, -1.5f,  1.0f }, {-1.0f,  0.0,  0.0f }, { 0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f } },
		};
		static const USHORT indices[] = {
			0, 1, 2,
			2, 3, 0,

			4, 5, 6,
			6, 7, 4,
		};
		outVertices.resize(8);
		outVertices.assign((TextureNormalVertex*)vertices, ((TextureNormalVertex*)vertices + 8));
		outIndices.resize(12);
		outIndices.assign((USHORT*)indices, ((USHORT*)indices + 12));
	}
};






