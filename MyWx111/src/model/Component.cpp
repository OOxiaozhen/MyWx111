#include "Component.h"

#include <algorithm>
#include <cmath>

namespace
{
/// 圆周率常量。不使用 M_PI，避免依赖 _USE_MATH_DEFINES 这类平台相关的宏。
constexpr double kPi = 3.14159265358979323846;

/// 把本地坐标点绕指定中心顺时针旋转（仅支持 90 度的整数倍）。
/// @param pt 待旋转的本地坐标点
/// @param cx 旋转中心 X 坐标
/// @param cy 旋转中心 Y 坐标
/// @param degrees 顺时针旋转角度，取值 0 / 90 / 180 / 270
/// @return 旋转后的本地坐标点
wxPoint RotatePoint(const wxPoint& pt, double cx, double cy, int degrees)
{
    const double dx = pt.x - cx;
    const double dy = pt.y - cy;

    switch (degrees)
    {
    case 90:
        // 屏幕坐标系的 Y 轴向下，因此视觉上的顺时针 90 度等价于 (dx, dy) -> (-dy, dx)
        return wxPoint(static_cast<int>(std::lround(cx - dy)),
                       static_cast<int>(std::lround(cy + dx)));
    case 180:
        return wxPoint(static_cast<int>(std::lround(cx - dx)),
                       static_cast<int>(std::lround(cy - dy)));
    case 270:
        return wxPoint(static_cast<int>(std::lround(cx + dy)),
                       static_cast<int>(std::lround(cy - dx)));
    default:
        return pt;
    }
}
} // namespace

Component::Component(ComponentType type, int id)
    : m_id(id)
    , m_type(type)
    , m_bitWidth(GetComponentDefaultBitWidth(type))
{
}

void Component::SetOrientation(int degrees)
{
    // 先归一化到 [0, 360)，再向下吸附到 90 度的整数倍，
    // 保证 m_orientation 永远只有 4 种取值，简化后续的旋转换算
    int normalized = degrees % 360;
    if (normalized < 0)
    {
        normalized += 360;
    }
    m_orientation = (normalized / 90) * 90;
}

void Component::Rotate()
{
    SetOrientation(m_orientation + 90);
}

void Component::SetBitWidth(int width)
{
    m_bitWidth = std::clamp(width, 1, 32);
}

std::vector<Pin> Component::GetTransformedPins() const
{
    std::vector<Pin> pins = BuildPins();
    if (m_orientation == 0 || pins.empty())
    {
        return pins;
    }

    // 各元件的本地包围盒宽高相等（见 GateComponents.cpp 中的 kComponentBodySize），
    // 因此绕包围盒中心旋转 90/180/270 度后包围盒不变，无需重新计算尺寸
    const wxRect local = GetLocalBounds();
    const double cx = local.x + local.width / 2.0;
    const double cy = local.y + local.height / 2.0;

    for (Pin& pin : pins)
    {
        pin.offset = RotatePoint(pin.offset, cx, cy, m_orientation);
    }
    return pins;
}

wxPoint Component::GetPinWorldPos(int pinIndex) const
{
    const std::vector<Pin> pins = GetTransformedPins();
    if (pinIndex < 0 || pinIndex >= static_cast<int>(pins.size()))
    {
        return m_position;
    }
    return m_position + pins[static_cast<size_t>(pinIndex)].offset;
}

PinSide Component::GetPinSide(int pinIndex) const
{
    const std::vector<Pin> pins = GetTransformedPins();
    if (pinIndex < 0 || pinIndex >= static_cast<int>(pins.size()))
    {
        return PinSide::Right;
    }

    const wxPoint& offset = pins[static_cast<size_t>(pinIndex)].offset;
    // 取绝对值较大的一轴判定：斜向偏移的引脚按主要方向归类，两轴相等时归入水平侧
    const int absX = offset.x >= 0 ? offset.x : -offset.x;
    const int absY = offset.y >= 0 ? offset.y : -offset.y;
    if (absX >= absY)
    {
        return offset.x >= 0 ? PinSide::Right : PinSide::Left;
    }
    return offset.y >= 0 ? PinSide::Bottom : PinSide::Top;
}

bool Component::HitTest(const wxPoint& worldPt) const
{
    wxRect bounds = GetWorldBounds();
    // 外扩 4 像素：让用户更容易点中元件边缘与细小的引脚
    bounds.Inflate(4);
    return bounds.Contains(worldPt);
}

wxRect Component::GetWorldBounds() const
{
    const wxRect local = GetLocalBounds();
    // 本地包围盒宽高相等，旋转不会改变外框尺寸，因此只需平移
    return wxRect(m_position + local.GetPosition(), local.GetSize());
}

std::vector<wxPoint> Component::GetPinWorldPositions() const
{
    std::vector<wxPoint> result;
    const std::vector<Pin> pins = GetTransformedPins();
    result.reserve(pins.size());
    for (const Pin& pin : pins)
    {
        result.push_back(m_position + pin.offset);
    }
    return result;
}

wxString Component::GetPropertyLabel(int i) const
{
    switch (i)
    {
    case 0:
        return wxString(L"名称");
    case 1:
        return wxString(L"朝向");
    case 2:
        return wxString(L"位宽");
    default:
        return wxString();
    }
}

// ───────────────────────── 元件类型辅助函数 ─────────────────────────

wxString GetComponentDisplayName(ComponentType type)
{
    switch (type)
    {
    case ComponentType::InputPin:
        return wxString(L"输入引脚");
    case ComponentType::OutputPin:
        return wxString(L"输出引脚");
    case ComponentType::AndGate:
        return wxString(L"与门");
    case ComponentType::OrGate:
        return wxString(L"或门");
    case ComponentType::NotGate:
        return wxString(L"非门");
    case ComponentType::NandGate:
        return wxString(L"与非门");
    case ComponentType::NorGate:
        return wxString(L"或非门");
    case ComponentType::XorGate:
        return wxString(L"异或门");
    case ComponentType::Splitter:
        return wxString(L"分线器");
    case ComponentType::TextLabel:
        return wxString(L"文本标注");
    case ComponentType::VccSource:
        return wxString(L"高电平 VCC");
    case ComponentType::GndSource:
        return wxString(L"低电平 GND");
    case ComponentType::ClockSource:
        return wxString(L"时钟源");
    case ComponentType::XnorGate:
        return wxString(L"同或门");
    case ComponentType::BufferGate:
        return wxString(L"缓冲器");
    case ComponentType::TriStateGate:
        return wxString(L"三态门");
    case ComponentType::DFlipFlop:
        return wxString(L"D 触发器");
    case ComponentType::RsLatch:
        return wxString(L"RS 锁存器");
    case ComponentType::LedIndicator:
        return wxString(L"LED 指示灯");
    case ComponentType::LogicProbe:
        return wxString(L"逻辑探针");
    default:
        return wxString(L"未知元件");
    }
}

int GetComponentDefaultBitWidth(ComponentType type)
{
    // 当前阶段所有元件统一按 1 位处理；分线器虽是多输出，但其位宽语义留待仿真阶段定义
    wxUnusedVar(type);
    return 1;
}

wxString GetComponentGroupName(ComponentType type)
{
    switch (type)
    {
    case ComponentType::InputPin:
    case ComponentType::OutputPin:
        return wxString(L"输入/输出");

    case ComponentType::Splitter:
        return wxString(L"接线");

    case ComponentType::VccSource:
    case ComponentType::GndSource:
    case ComponentType::ClockSource:
        return wxString(L"信号源");

    case ComponentType::DFlipFlop:
    case ComponentType::RsLatch:
        return wxString(L"时序");

    case ComponentType::LedIndicator:
    case ComponentType::LogicProbe:
        return wxString(L"显示");

    case ComponentType::TextLabel:
        return wxString(L"标注");

    case ComponentType::AndGate:
    case ComponentType::OrGate:
    case ComponentType::NotGate:
    case ComponentType::NandGate:
    case ComponentType::NorGate:
    case ComponentType::XorGate:
    case ComponentType::XnorGate:
    case ComponentType::BufferGate:
    case ComponentType::TriStateGate:
        return wxString(L"逻辑门");
    }
    return wxString(L"逻辑门");
}

std::vector<wxString> GetAllComponentGroups()
{
    return { wxString(L"输入/输出"), wxString(L"逻辑门"), wxString(L"信号源"),
             wxString(L"时序"), wxString(L"显示"), wxString(L"标注"), wxString(L"接线") };
}

std::vector<ComponentType> GetComponentTypesInGroup(const wxString& groupName)
{
    if (groupName == wxString(L"输入/输出"))
    {
        return { ComponentType::InputPin, ComponentType::OutputPin };
    }
    if (groupName == wxString(L"逻辑门"))
    {
        return { ComponentType::AndGate,
                 ComponentType::OrGate,
                 ComponentType::NotGate,
                 ComponentType::NandGate,
                 ComponentType::NorGate,
                 ComponentType::XorGate,
                 ComponentType::XnorGate,
                 ComponentType::BufferGate,
                 ComponentType::TriStateGate };
    }
    if (groupName == wxString(L"信号源"))
    {
        return { ComponentType::VccSource, ComponentType::GndSource, ComponentType::ClockSource };
    }
    if (groupName == wxString(L"时序"))
    {
        return { ComponentType::DFlipFlop, ComponentType::RsLatch };
    }
    if (groupName == wxString(L"显示"))
    {
        return { ComponentType::LedIndicator, ComponentType::LogicProbe };
    }
    if (groupName == wxString(L"标注"))
    {
        return { ComponentType::TextLabel };
    }
    if (groupName == wxString(L"接线"))
    {
        return { ComponentType::Splitter };
    }
    return {};
}
