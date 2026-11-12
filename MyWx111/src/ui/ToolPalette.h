#pragma once

#include <functional>
#include <map>
#include <vector>

#include <wx/panel.h>
#include <wx/string.h>

#include "../model/ComponentType.h"

class wxButton;
class wxSizer;

/// 左侧元件调色板：按 Logisim 风格分组陈列可用元件。
/// 点击某个元件按钮后进入「待放置」高亮态，并通过 OnComponentTypeSelected 通知 MainFrame；
/// 面板标题由外层 wxAuiPaneInfo 的 caption 提供，内部不再重复绘制大标题。
class ToolPalette : public wxPanel
{
public:
    /// @param parent 父窗口
    explicit ToolPalette(wxWindow* parent);

    /// @return 无额外资源需要释放
    ~ToolPalette() override = default;

    /// 设置当前高亮的元件类型（全局互斥，同时只有一个高亮按钮）。
    /// @param type 要高亮的元件类型
    /// @param active false 表示取消全部高亮
    void SetActiveType(ComponentType type, bool active);

    /// 取当前高亮的元件类型。
    /// @param[out] outType 高亮类型
    /// @return 存在高亮项时返回 true
    bool GetActiveType(ComponentType& outType) const;

    /// 元件按钮被点击时回调，参数为被点击的元件类型。
    std::function<void(ComponentType)> OnComponentTypeSelected;

    /// 鼠标悬停在元件按钮上时回调，参数为说明文本。
    std::function<void(const wxString&)> OnHint;

private:
    /// 构建一个元件分组（带标题边框）。
    /// @param parentSizer 父布局器
    /// @param groupName 分组名
    /// @param types 该组包含的元件类型
    void BuildGroup(wxSizer* parentSizer, const wxString& groupName,
                    const std::vector<ComponentType>& types);

    /// 处理元件按钮点击。
    /// @param type 被点击的元件类型
    void OnTypeButtonClicked(ComponentType type);

    /// 按当前激活状态刷新全部按钮背景色。
    void HighlightActive();

    std::map<ComponentType, wxButton*> m_buttons;      ///< 元件类型 → 按钮映射
    bool m_hasActive = false;                           ///< 当前是否存在高亮项
    ComponentType m_activeType = ComponentType::AndGate; ///< 当前高亮的元件类型
    bool m_updating = false;                            ///< 防止高亮刷新触发按钮事件造成递归
};
