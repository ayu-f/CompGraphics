#include "SceneManager.h"

SceneManager::SceneManager() {
    m_animTime = 0.0;
    m_isPlay = true;
    m_isTrapped = false;
    m_zoomScene = 15.0f;
    m_cameraXRotation = DirectX::XMMatrixIdentity();
    m_cameraYRotation = DirectX::XMMatrixIdentity();
    Update(0.0);
}

void SceneManager::Update(double deltaTime)
{
    if (m_isPlay)
    {
        m_animTime += deltaTime * 3 * M_PI * 0.25;
    }
    m_lightPos = { 2.5f * sinf(static_cast<float>(m_animTime) / 1.5f), 3.5f, cosf(static_cast<float>(m_animTime) / 1.5f), 3 };
    m_modelTransform = DirectX::XMMatrixRotationAxis({ 0, 1, 0 }, -static_cast<float>(m_animTime));
    m_modelTransform *= DirectX::XMMatrixTranslation(0.0f, (1.0f + sinf(-static_cast<float>(m_animTime))) / 4, 0.0f);

    m_cameraTransform = DirectX::XMMatrixTranslation(0, 0, -m_zoomScene);
    m_cameraTransform *= m_cameraYRotation;
    if (m_isTrapped)
        m_cameraTransform *= DirectX::XMMatrixRotationAxis({ 1, 0, 0 }, (m_trapLast.y - m_trapStart.y) / m_angelDel);
    m_cameraTransform *= m_cameraXRotation;
    if (m_isTrapped)
        m_cameraTransform *= DirectX::XMMatrixRotationAxis({ 0, 1, 0 }, (m_trapLast.x - m_trapStart.x) / m_angelDel);

}

void SceneManager::OnMouseMove(WPARAM wParam, LPARAM lParam) {
    if (m_isTrapped) {
        m_trapLast.x = GET_X_LPARAM(lParam);
        m_trapLast.y = GET_Y_LPARAM(lParam);
    }
}

void SceneManager::OnLButtonDown(WPARAM wParam, LPARAM lParam) {
    m_isTrapped = true;
    m_trapLast.x = m_trapStart.x = GET_X_LPARAM(lParam);
    m_trapLast.y = m_trapStart.y = GET_Y_LPARAM(lParam);
}

void SceneManager::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    m_isTrapped = false;
    m_trapLast.x = GET_X_LPARAM(lParam);
    m_trapLast.y = GET_Y_LPARAM(lParam);
    m_cameraXRotation *= DirectX::XMMatrixRotationAxis({ 0, 1, 0 }, (m_trapLast.x - m_trapStart.x) / m_angelDel);
    m_cameraYRotation *= DirectX::XMMatrixRotationAxis({ 1, 0, 0 }, (m_trapLast.y - m_trapStart.y) / m_angelDel);
}


void SceneManager::OnKeyUp(WPARAM wParam, LPARAM lParam) {
    char key = MapVirtualKey(static_cast<UINT>(wParam), MAPVK_VK_TO_CHAR);
    switch (key)
    {
    case ' ':
        enablePostproc = !enablePostproc;
    }
}