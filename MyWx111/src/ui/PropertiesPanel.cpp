#include "PropertiesPanel.h"

#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "../model/Circuit.h"

PropertiesPanel::PropertiesPanel(wxWindow* parent, Circuit& circuit)
    : wxPanel(parent, wxID_ANY)
    , m_circuit(circuit)
{
    // 与 wxAuiPaneInfo 的 BestSize 保持一致
    SetMinSize(wxSize(220, -1));

    BuildControls();
    RefreshFromCircuit();
}

void PropertiesPanel::BuildControls()
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    // 面板标题由 AUI 的 caption 提供，这里只放空态提示与属性表格
    m_emptyLabel = new wxStaticText(this, wxID_ANY, L"未选中元件");
    root->Add(m_emptyLabel, 0, wxALL, 10);

    auto* grid = new wxFlexGridSizer(2, 8, 8);
    m_grid = grid; // 保存为成员：RefreshFromCircuit 需按类型 Show/Hide 各项
    // 只让第二列（编辑控件）随面板宽度伸缩，属性名保持固定宽度
    grid->AddGrowableCol(1, 1);

    grid->Add(new wxStaticText(this, wxID_ANY, L"类型"), 0, wxALIGN_CENTER_VERTICAL);
    m_typeLabel = new wxStaticText(this, wxID_ANY, wxEmptyString);
    grid->Add(m_typeLabel, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, L"名称"), 0, wxALIGN_CENTER_VERTICAL);
    m_nameCtrl = new wxTextCtrl(this, wxID_ANY);
    grid->Add(m_nameCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, L"X 坐标"), 0, wxALIGN_CENTER_VERTICAL);
    m_xCtrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                             wxDefaultSize, wxSP_ARROW_KEYS, -100000, 100000, 0);
    grid->Add(m_xCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, L"Y 坐标"), 0, wxALIGN_CENTER_VERTICAL);
    m_yCtrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                             wxDefaultSize, wxSP_ARROW_KEYS, -100000, 100000, 0);
    grid->Add(m_yCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, L"朝向"), 0, wxALIGN_CENTER_VERTICAL);
    m_orientationCtrl = new wxChoice(this, wxID_ANY);
    m_orientationCtrl->Append(L"0°");
    m_orientationCtrl->Append(L"90°");
    m_orientationCtrl->Append(L"180°");
    m_orientationCtrl->Append(L"270°");
    grid->Add(m_orientationCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, L"位宽"), 0, wxALIGN_CENTER_VERTICAL);
    m_bitWidthCtrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                    wxDefaultSize, wxSP_ARROW_KEYS, 1, 32, 1);
    grid->Add(m_bitWidthCtrl, 1, wxEXPAND);

    // 物理芯片属性：gEDA 网表与 pcb 布局导出用。
    // 同一片多门 IC 的多个门必须填相同参考编号、用门序号区分，
    // 否则 PCB 会把它当成多片独立芯片。
    grid->Add(new wxStaticText(this, wxID_ANY, L"参考编号"), 0, wxALIGN_CENTER_VERTICAL);
    m_refDesCtrl = new wxTextCtrl(this, wxID_ANY);
    m_refDesCtrl->SetHint(L"留空 = 自动分配");
    grid->Add(m_refDesCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, L"封装"), 0, wxALIGN_CENTER_VERTICAL);
    m_footprintCtrl = new wxTextCtrl(this, wxID_ANY);
    m_footprintCtrl->SetHint(L"留空 = 按类型映射（DIP14）");
    grid->Add(m_footprintCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, L"器件型号"), 0, wxALIGN_CENTER_VERTICAL);
    m_valueCtrl = new wxTextCtrl(this, wxID_ANY);
    m_valueCtrl->SetHint(L"留空 = 按类型默认（如 7486）");
    grid->Add(m_valueCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, L"门序号"), 0, wxALIGN_CENTER_VERTICAL);
    m_slotCtrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, 1, 4, 1);
    grid->Add(m_slotCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, L"引脚数"), 0, wxALIGN_CENTER_VERTICAL);
    m_pinCountLabel = new wxStaticText(this, wxID_ANY, wxEmptyString);
    grid->Add(m_pinCountLabel, 1, wxEXPAND);

    // 以下三组仅在特定元件类型下显示（见 RefreshFromCircuit 中的 Show/Hide）
    m_inputLevelLabel = new wxStaticText(this, wxID_ANY, L"电平");
    grid->Add(m_inputLevelLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_inputLevelCtrl = new wxChoice(this, wxID_ANY);
    // 选项顺序必须与 LogicLevel 枚举一致（0=Low 1=High 2=Unknown），
    // 回填与提交都直接做序号↔枚举的静态转换
    m_inputLevelCtrl->Append(L"低");
    m_inputLevelCtrl->Append(L"高");
    m_inputLevelCtrl->Append(L"未知");
    grid->Add(m_inputLevelCtrl, 1, wxEXPAND);

    m_clockFreqLabel = new wxStaticText(this, wxID_ANY, L"频率(Hz)");
    grid->Add(m_clockFreqLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_clockFreqCtrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                    wxDefaultSize, wxSP_ARROW_KEYS, 1, 1000000, 1);
    grid->Add(m_clockFreqCtrl, 1, wxEXPAND);

    m_clockDutyLabel = new wxStaticText(this, wxID_ANY, L"占空比(%)");
    grid->Add(m_clockDutyLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_clockDutyCtrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                    wxDefaultSize, wxSP_ARROW_KEYS, 1, 99, 50);
    grid->Add(m_clockDutyCtrl, 1, wxEXPAND);

    root->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    root->AddStretchSpacer();

    SetSizer(root);

    m_nameCtrl->Bind(wxEVT_KILL_FOCUS, &PropertiesPanel::OnNameCommitted, this);
    m_xCtrl->Bind(wxEVT_SPINCTRL, &PropertiesPanel::OnPositionChanged, this);
    m_yCtrl->Bind(wxEVT_SPINCTRL, &PropertiesPanel::OnPositionChanged, this);
    m_orientationCtrl->Bind(wxEVT_CHOICE, &PropertiesPanel::OnOrientationChanged, this);
    m_bitWidthCtrl->Bind(wxEVT_SPINCTRL, &PropertiesPanel::OnBitWidthChanged, this);

    // 文本类字段统一在失焦时提交：wxEVT_TEXT 每敲一个字符都会改一次模型，
    // 会把撤销栈瞬间填满
    m_refDesCtrl->Bind(wxEVT_KILL_FOCUS, &PropertiesPanel::OnRefDesCommitted, this);
    m_footprintCtrl->Bind(wxEVT_KILL_FOCUS, &PropertiesPanel::OnFootprintCommitted, this);
    m_valueCtrl->Bind(wxEVT_KILL_FOCUS, &PropertiesPanel::OnValueCommitted, this);
    m_slotCtrl->Bind(wxEVT_SPINCTRL, &PropertiesPanel::OnSlotChanged, this);
    m_inputLevelCtrl->Bind(wxEVT_CHOICE, &PropertiesPanel::OnInputLevelChanged, this);
    m_clockFreqCtrl->Bind(wxEVT_SPINCTRL, &PropertiesPanel::OnClockChanged, this);
    m_clockDutyCtrl->Bind(wxEVT_SPINCTRL, &PropertiesPanel::OnClockChanged, this);
}

void PropertiesPanel::RefreshFromCircuit()
{
    // 阻断「写回属性 → NotifyChanged → RefreshFromCircuit → SetValue → 再次触发事件」的递归
    m_updating = true;

    const int selectedId = m_circuit.GetPrimarySelection();
    const Component* comp = (selectedId >= 0) ? m_circuit.FindComponent(selectedId) : nullptr;

    if (comp == nullptr)
    {
        m_emptyLabel->Show();
        m_typeLabel->SetLabel(wxEmptyString);
        m_pinCountLabel->SetLabel(wxEmptyString);
        EnableControls(false);
        m_updating = false;
        Layout();
        return;
    }

    m_emptyLabel->Hide();
    m_typeLabel->SetLabel(GetComponentDisplayName(comp->GetType()));

    // 只在值真的不同时才写控件：wxTextCtrl::SetValue 即使内容相同也会重置插入点，
    // 用户连续输入名称时会出现光标跳到行尾的现象
    if (m_nameCtrl->GetValue() != comp->GetName())
    {
        m_nameCtrl->SetValue(comp->GetName());
    }
    if (m_xCtrl->GetValue() != comp->GetPosition().x)
    {
        m_xCtrl->SetValue(comp->GetPosition().x);
    }
    if (m_yCtrl->GetValue() != comp->GetPosition().y)
    {
        m_yCtrl->SetValue(comp->GetPosition().y);
    }

    const int orientationIndex = comp->GetOrientation() / 90;
    if (m_orientationCtrl->GetSelection() != orientationIndex)
    {
        m_orientationCtrl->SetSelection(orientationIndex);
    }
    if (m_bitWidthCtrl->GetValue() != comp->GetBitWidth())
    {
        m_bitWidthCtrl->SetValue(comp->GetBitWidth());
    }

    if (m_refDesCtrl->GetValue() != comp->GetRefDes())
    {
        m_refDesCtrl->SetValue(comp->GetRefDes());
    }
    if (m_footprintCtrl->GetValue() != comp->GetFootprint())
    {
        m_footprintCtrl->SetValue(comp->GetFootprint());
    }
    if (m_valueCtrl->GetValue() != comp->GetValue())
    {
        m_valueCtrl->SetValue(comp->GetValue());
    }
    if (m_slotCtrl->GetValue() != comp->GetSlot())
    {
        m_slotCtrl->SetValue(comp->GetSlot());
    }

    m_pinCountLabel->SetLabel(
        wxString::Format(L"%d", static_cast<int>(comp->GetTransformedPins().size())));

    // 按类型显隐专属字段，避免无关字段挤满面板
    const bool isInputPin = (comp->GetType() == ComponentType::InputPin);
    const bool isClock = (comp->GetType() == ComponentType::ClockSource);

    m_grid->Show(m_inputLevelLabel, isInputPin);
    m_grid->Show(m_inputLevelCtrl, isInputPin);
    m_grid->Show(m_clockFreqLabel, isClock);
    m_grid->Show(m_clockFreqCtrl, isClock);
    m_grid->Show(m_clockDutyLabel, isClock);
    m_grid->Show(m_clockDutyCtrl, isClock);

    if (isInputPin)
    {
        const int levelIndex = static_cast<int>(m_circuit.GetComponentInputLevel(comp->GetId()));
        if (m_inputLevelCtrl->GetSelection() != levelIndex)
        {
            m_inputLevelCtrl->SetSelection(levelIndex);
        }
    }
    else if (isClock)
    {
        const int frequency = m_circuit.GetComponentClockFrequency(comp->GetId());
        if (m_clockFreqCtrl->GetValue() != frequency)
        {
            m_clockFreqCtrl->SetValue(frequency);
        }

        // 占空比以百分比整数存储，回填时四舍五入避免出现 49% 这类抖动
        const int dutyPercent =
            static_cast<int>(m_circuit.GetComponentClockDuty(comp->GetId()) * 100.0 + 0.5);
        if (m_clockDutyCtrl->GetValue() != dutyPercent)
        {
            m_clockDutyCtrl->SetValue(dutyPercent);
        }
    }

    EnableControls(true);
    m_updating = false;
    Layout();
}

void PropertiesPanel::EnableControls(bool enable)
{
    m_nameCtrl->Enable(enable);
    m_xCtrl->Enable(enable);
    m_yCtrl->Enable(enable);
    m_orientationCtrl->Enable(enable);
    m_bitWidthCtrl->Enable(enable);
    m_refDesCtrl->Enable(enable);
    m_footprintCtrl->Enable(enable);
    m_valueCtrl->Enable(enable);
    m_slotCtrl->Enable(enable);
}

void PropertiesPanel::OnNameCommitted(wxFocusEvent&)
{
    if (m_updating)
    {
        return;
    }

    const int selectedId = m_circuit.GetPrimarySelection();
    if (selectedId < 0)
    {
        return;
    }

    m_circuit.SetComponentName(selectedId, m_nameCtrl->GetValue());
    if (m_onChanged)
    {
        m_onChanged();
    }
}

void PropertiesPanel::OnPositionChanged(wxSpinEvent&)
{
    if (m_updating)
    {
        return;
    }

    const int selectedId = m_circuit.GetPrimarySelection();
    if (selectedId < 0)
    {
        return;
    }

    // 属性面板的手动输入不做网格吸附：用户可能确实需要非网格坐标，
    // 只有画布上的拖动才强制吸附到网格
    m_circuit.SetComponentPosition(selectedId,
                                   wxPoint(m_xCtrl->GetValue(), m_yCtrl->GetValue()));
    if (m_onChanged)
    {
        m_onChanged();
    }
}

void PropertiesPanel::OnOrientationChanged(wxCommandEvent&)
{
    if (m_updating)
    {
        return;
    }

    const int selectedId = m_circuit.GetPrimarySelection();
    if (selectedId < 0)
    {
        return;
    }

    const int selection = m_orientationCtrl->GetSelection();
    if (selection < 0)
    {
        return;
    }

    m_circuit.SetComponentOrientation(selectedId, selection * 90);
    if (m_onChanged)
    {
        m_onChanged();
    }
}

void PropertiesPanel::OnBitWidthChanged(wxSpinEvent&)
{
    if (m_updating)
    {
        return;
    }

    const int selectedId = m_circuit.GetPrimarySelection();
    if (selectedId < 0)
    {
        return;
    }

    m_circuit.SetComponentBitWidth(selectedId, m_bitWidthCtrl->GetValue());
    if (m_onChanged)
    {
        m_onChanged();
    }
}

void PropertiesPanel::OnRefDesCommitted(wxFocusEvent&)
{
    NotifyFieldChanged(Field::RefDes, m_refDesCtrl->GetValue());
}

void PropertiesPanel::OnFootprintCommitted(wxFocusEvent&)
{
    NotifyFieldChanged(Field::Footprint, m_footprintCtrl->GetValue());
}

void PropertiesPanel::OnValueCommitted(wxFocusEvent&)
{
    NotifyFieldChanged(Field::Value, m_valueCtrl->GetValue());
}

void PropertiesPanel::OnSlotChanged(wxSpinEvent&)
{
    NotifyFieldChanged(Field::Slot, wxString::Format(L"%d", m_slotCtrl->GetValue()));
}

void PropertiesPanel::NotifyFieldChanged(Field field, const wxString& value)
{
    if (m_updating)
    {
        return;
    }

    const int selectedId = m_circuit.GetPrimarySelection();
    if (selectedId < 0)
    {
        return;
    }

    // 通过 MainFrame 转发而不是直接调 Circuit：MainFrame 侧负责把字段映射到
    // 对应的 SetComponentXxx，并保证结构变更通知与撤销快照正常工作
    if (OnFieldChanged)
    {
        OnFieldChanged(selectedId, field, value);
    }
}

void PropertiesPanel::OnInputLevelChanged(wxCommandEvent&)
{
    if (m_updating)
    {
        return;
    }

    const int selectedId = m_circuit.GetPrimarySelection();
    if (selectedId < 0)
    {
        return;
    }

    const int selection = m_inputLevelCtrl->GetSelection();
    if (selection < 0)
    {
        return;
    }

    // 下拉项顺序与 LogicLevel 枚举一致：0=低 1=高 2=未知
    m_circuit.SetComponentInputLevel(selectedId, static_cast<LogicLevel>(selection));
    if (m_onChanged)
    {
        m_onChanged();
    }
}

void PropertiesPanel::OnClockChanged(wxSpinEvent&)
{
    if (m_updating)
    {
        return;
    }

    const int selectedId = m_circuit.GetPrimarySelection();
    if (selectedId < 0)
    {
        return;
    }

    m_circuit.SetComponentClockFrequency(selectedId, m_clockFreqCtrl->GetValue());
    m_circuit.SetComponentClockDuty(selectedId, m_clockDutyCtrl->GetValue() / 100.0);
    if (m_onChanged)
    {
        m_onChanged();
    }
}
