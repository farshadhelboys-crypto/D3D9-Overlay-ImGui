#include "Drawing.h"
#include "esp.h"
#include "license.h"
#include "antidebug.h"

LPCSTR Drawing::lpWindowName = "D3D9 Overlay ImGui";
ImVec2 Drawing::vWindowSize = { 350, 75 };
ImGuiWindowFlags Drawing::WindowFlags = 0;
bool Drawing::bDraw = true;
UI::WindowItem Drawing::lpSelectedWindow = {nullptr, "", ""};
LPDIRECT3DDEVICE9 Drawing::pD3DDevice = nullptr;

static bool g_keyTried = false;

bool Drawing::isActive()
{
    return bDraw == true;
}

void Drawing::Draw()
{
    AntiDebug::Guard();

    if (!License::IsLicensed()) {
        if (!g_keyTried) {
            auto k = License::LoadSavedKey();
            if (!k.empty())
                License::Heartbeat();
            g_keyTried = true;
        }
        License::DrawGateAndTick();
        return;
    }

    License::DrawGateAndTick();

    if (GetAsyncKeyState(VK_INSERT) & 1)
        bDraw = !bDraw;

    if (isActive())
    {
        if (!UI::IsWindowTargeted())
        {
            std::vector<UI::WindowItem> WindowList;
            UI::GetAllWindow(&WindowList);

            if (!WindowList.empty())
            {
                ImGui::SetNextWindowSize({400, 100}, ImGuiCond_Once);
                ImGui::SetNextWindowBgAlpha(1.0f);

                ImGui::Begin("Overlay Target Chooser", &bDraw, WindowFlags);
                {
                    if(ImGui::BeginCombo("##combo", lpSelectedWindow.CurrentWindowTitle))
                    {
                        for (const auto& item : WindowList)
                        {
                            const bool is_selected = (strcmp(lpSelectedWindow.CurrentWindowTitle, item.CurrentWindowTitle) == 0);
                            if (ImGui::Selectable(item.CurrentWindowTitle, is_selected))
                                lpSelectedWindow = item;
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::NewLine();
                    if(ImGui::Button("Start Overlay"))
                    {
                        UI::SetTargetWindow(lpSelectedWindow.CurrentWindow);
                    }
                }
                ImGui::End();
            }
        }
        else
        {
            RenderESPMenu();
        }
    }

    if (UI::IsWindowTargeted())
        RenderESPBoxes();
}

void Drawing::DXDraw(const LPDIRECT3DDEVICE9 pCurrentD3DDevice)
{
    pD3DDevice = pCurrentD3DDevice;
}
