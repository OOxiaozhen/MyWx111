#include "ToolPalette.h"

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/statbox.h>

namespace
{
/// 取元件类型的一句话说明，用于状态栏提示。
/// @param type 元件类型
/// @return 中文说明文本
wxString GetTypeHint(ComponentType type)
{
    switch (type)
    {
    case ComponentType::InputPin:
        return wxString(L"输入引脚：电路输入端，对外提供一个输出脚");
    case ComponentType::OutputPin:
        return wxString(L"输出引脚：电路输出端，接收一个输入脚");
    case ComponentType::AndGate:
        return wxString(L"与门：两输入逻辑与，仅当两输入均为真时输出为真");
    case ComponentType::OrGate:
        return wxString(L"或门：两输入逻辑或，任一输入为真则输出为真");
    case ComponentType::NotGate:
        return wxString(L"非门：单输入取反");
    case ComponentType::NandGate:
        return wxString(L"与非门：与门结果取反");
    case ComponentType::NorGate:
        return wxString(L"或非门：或门结果取反");
    case ComponentType::XorGate:
        return wxString(L"异或门：两输入相异时输出为真");
    case ComponentType::Splitter:
        return wxString(L"分线器：把一路输入拆成多路输出");
    default:
        return wxString(L"未知元件");
    }
}
} // namespace

ToolPalette::ToolPalette(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    // 与 wxAuiPaneInfo 的 BestSize 保持一致，避免面板被压缩后按钮溢出
    SetMinSize(wxSize(180, -1));

    auto* root = new wxBoxSizer(wxVERTICAL);

    for (const wxString& groupName : GetAllComponentGroups())
    {
        BuildGroup(root, groupName, GetComponentTypesInGroup(groupName));
    }
    root->AddStretchSpacer();

    SetSizer(root);
}

void ToolPalette::BuildGroup(wxSizer* parentSizer, const wxString& groupName,
                             const std::vector<ComponentType>& types)
{
    if (types.empty())
    {
        return;
    }

    auto* box = new wxStaticBoxSizer(wxVERTICAL, this, groupName);
    wxWindow* boxParent = box->GetStaticBox();

    for (ComponentType type : types)
    {
        auto* button = new wxButton(boxParent, wxID_ANY, GetComponentDisplayName(type),
                                    wxDefaultPosition, wxSize(-1, 28));

        const wxString hint = GetTypeHint(type);
        button->SetToolTip(hint);

        // 用事件绑定携带 type 参数，避免为每个元件单独写一个处理函数
        button->Bind(wxEVT_BUTTON,
                     [this, type](wxCommandEvent&) { OnTypeButtonClicked(type); });

        // 悬停时把说明推到状态栏，方便用户在不记得逻辑门符号时快速确认
        button->Bind(wxEVT_ENTER_WINDOW,
                     [this, hint](wxMouseEvent& event)
                     {
                         if (OnHint)
                         {
                             OnHint(hint);
                         }
                         event.Skip();
                     });

        box->Add(button, 0, wxEXPAND | wxALL, 4);
        m_buttons[type] = button;
    }

    parentSizer->Add(box, 0, wxEXPAND | wxALL, 6);
}

void ToolPalette::OnTypeButtonClicked(ComponentType type)
{
    if (m_updating)
    {
        return;
    }

    m_activeType = type;
    m_hasActive = true;
    HighlightActive();

    if (OnComponentTypeSelected)
    {
        OnComponentTypeSelected(type);
    }
}

void ToolPalette::SetActiveType(ComponentType type, bool active)
{
    // 高亮刷新会触发按钮重绘，个别平台下可能回到按钮事件，
    // 用哨兵避免形成「点击 → 高亮 → 再点击」的递归
    m_updating = true;
    m_activeType = type;
    m_hasActive = active;
    HighlightActive();
    m_updating = false;
}

bool ToolPalette::GetActiveType(ComponentType& outType) const
{
    if (!m_hasActive)
    {
        return false;
    }
    outType = m_activeType;
    return true;
}

void ToolPalette::HighlightActive()
{
    const wxColour activeColour(204, 232, 255);

    for (auto& entry : m_buttons)
    {
        if (entry.second == nullptr)
        {
            continue;
        }

        const bool isActive = m_hasActive && (entry.first == m_activeType);
        // 传 wxNullColour 表示恢复系统默认背景，避免留下上次的高亮色
        entry.second->SetBackgroundColour(isActive ? activeColour : wxNullColour);
        entry.second->Refresh();
    }
}
