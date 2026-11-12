#pragma once

#include <functional>
#include <utility>

#include <wx/panel.h>
#include <wx/string.h>

class wxChoice;
class wxFlexGridSizer;
class wxFocusEvent;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;
class Circuit;

/// 右侧属性面板：显示并编辑当前选中元件的属性。
/// 控件修改后立即写回 Circuit 并触发画布重绘；刷新时刻意采用「值不同才写」的策略，
/// 避免 SetValue 重置文本插入点导致输入时光标跳动。
class PropertiesPanel : public wxPanel
{
public:
    /// @param parent 父窗口
    /// @param circuit 电路模型引用（所有权在 MainFrame）
    PropertiesPanel(wxWindow* parent, Circuit& circuit);

    /// @return 无额外资源需要释放
    ~PropertiesPanel() override = default;

    /// 按 Circuit 当前选中状态刷新全部控件；无选中时显示「未选中元件」并禁用编辑。
    void RefreshFromCircuit();

    /// 属性被用户修改后的回调（MainFrame 用它触发画布重绘）。
    /// @param handler 变更回调
    void SetOnChanged(std::function<void()> handler) { m_onChanged = std::move(handler); }

    /// 可编辑的元件物理芯片属性字段。
    enum class Field
    {
        RefDes,    ///< 参考编号（如 U1）
        Footprint, ///< 封装名（如 DIP14）
        Value,     ///< 器件型号（如 7486）
        Slot       ///< 门序号（1~4）
    };

    /// 属性编辑提交回调：由 MainFrame 转发到 Circuit 的对应 setter，
    /// 从而自动触发撤销快照与画布重绘。
    /// @param componentId 被编辑的元件 id
    /// @param field 字段
    /// @param value 新值文本（Slot 字段为十进制数字串）
    std::function<void(int componentId, Field field, const wxString& value)> OnFieldChanged;

private:
    /// 创建全部控件并绑定事件。
    void BuildControls();

    /// 启用或禁用全部可编辑控件。
    /// @param enable 是否启用
    void EnableControls(bool enable);

    /// 名称输入框失焦时提交。
    /// 不在 wxEVT_TEXT 里提交：每敲一个字符都会改一次模型并占用一步撤销，
    /// 收敛到失焦提交后一次编辑只产生一步。
    /// @param event 焦点事件（未使用）
    void OnNameCommitted(wxFocusEvent& event);

    /// X / Y 坐标数值框变化。
    void OnPositionChanged(wxSpinEvent& event);

    /// 朝向选择变化。
    void OnOrientationChanged(wxCommandEvent& event);

    /// 位宽数值框变化。
    void OnBitWidthChanged(wxSpinEvent& event);

    /// 参考编号输入框失焦提交。
    /// @param event 焦点事件（未使用）
    void OnRefDesCommitted(wxFocusEvent& event);

    /// 封装名输入框失焦提交。
    /// @param event 焦点事件（未使用）
    void OnFootprintCommitted(wxFocusEvent& event);

    /// 器件型号输入框失焦提交。
    /// @param event 焦点事件（未使用）
    void OnValueCommitted(wxFocusEvent& event);

    /// 门序号数值框变化提交。
    /// @param event 数值框事件（未使用）
    void OnSlotChanged(wxSpinEvent& event);

    /// 把某字段的新值交给 MainFrame（再转 Circuit setter）。
    /// @param field 被编辑的字段
    /// @param value 新值文本
    void NotifyFieldChanged(Field field, const wxString& value);

    /// 输入引脚电平下拉变化。
    /// @param event 选择事件（未使用）
    void OnInputLevelChanged(wxCommandEvent& event);

    /// 时钟频率或占空比变化。
    /// @param event 数值框事件（未使用）
    void OnClockChanged(wxSpinEvent& event);

    Circuit& m_circuit;                ///< 电路模型引用
    std::function<void()> m_onChanged; ///< 属性变更回调
    bool m_updating = false;           ///< 防回写递归哨兵

    wxStaticText* m_typeLabel = nullptr;     ///< 元件类型（只读）
    wxTextCtrl* m_nameCtrl = nullptr;        ///< 名称输入框
    wxSpinCtrl* m_xCtrl = nullptr;           ///< X 坐标
    wxSpinCtrl* m_yCtrl = nullptr;           ///< Y 坐标
    wxChoice* m_orientationCtrl = nullptr;   ///< 朝向选择（0/90/180/270）
    wxSpinCtrl* m_bitWidthCtrl = nullptr;    ///< 位宽（1~32）
    wxStaticText* m_pinCountLabel = nullptr; ///< 引脚数（只读）
    wxStaticText* m_emptyLabel = nullptr;    ///< 「未选中元件」提示

    wxTextCtrl* m_refDesCtrl = nullptr;    ///< 参考编号（如 U1）；空表示自动
    wxTextCtrl* m_footprintCtrl = nullptr; ///< 封装名（如 DIP14）；空表示自动
    wxTextCtrl* m_valueCtrl = nullptr;     ///< 器件型号（如 7486）；空表示自动
    wxSpinCtrl* m_slotCtrl = nullptr;      ///< 门序号（1~4）

    wxFlexGridSizer* m_grid = nullptr;         ///< 属性表格，用于按类型显隐字段
    wxStaticText* m_inputLevelLabel = nullptr; ///< 「电平」标签
    wxChoice* m_inputLevelCtrl = nullptr;      ///< 输入引脚电平
    wxStaticText* m_clockFreqLabel = nullptr;  ///< 「频率」标签
    wxSpinCtrl* m_clockFreqCtrl = nullptr;     ///< 时钟频率（Hz）
    wxStaticText* m_clockDutyLabel = nullptr;  ///< 「占空比」标签
    wxSpinCtrl* m_clockDutyCtrl = nullptr;     ///< 时钟占空比（百分比）
};
