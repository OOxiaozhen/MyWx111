#pragma once

#include "Component.h"

/// 元件本地包围盒的边长（宽高相等）。
/// 统一采用正方形边界，使 90/180/270 度旋转不会改变包围盒尺寸，
/// 从而让命中测试、渲染偏移与引脚变换都保持简单。
constexpr int kComponentBodySize = 40;

/// 引脚端点相对本体边界的向外延伸距离（引脚短线的长度）。
constexpr int kPinLength = 8;

// ── 组合逻辑门（本次全部补上 Evaluate 实现）──

/// 与门：2 输入 1 输出，左侧直边 + 右侧半圆（Dee 形）。
class AndGate : public Component
{
public:
    /// @param id 元件唯一 id
    explicit AndGate(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

/// 或门：2 输入 1 输出，左侧内凹弧 + 右侧尖头。
class OrGate : public Component
{
public:
    /// @param id 元件唯一 id
    explicit OrGate(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

/// 非门：1 输入 1 输出，三角形 + 输出端反相小圆。
class NotGate : public Component
{
public:
    /// @param id 元件唯一 id
    explicit NotGate(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

/// 与非门：与门形状 + 输出端反相小圆。
class NandGate : public Component
{
public:
    /// @param id 元件唯一 id
    explicit NandGate(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

/// 或非门：或门形状 + 输出端反相小圆。
class NorGate : public Component
{
public:
    /// @param id 元件唯一 id
    explicit NorGate(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

/// 异或门：或门形状 + 左侧背弧。
class XorGate : public Component
{
public:
    /// @param id 元件唯一 id
    explicit XorGate(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

/// 同或门：异或门形状 + 输出端反相小圆。
class XnorGate : public Component
{
public:
    /// @param id 元件唯一 id
    explicit XnorGate(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

/// 缓冲器：三角形，无输出反相圆。
class BufferGate : public Component
{
public:
    /// @param id 元件唯一 id
    explicit BufferGate(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

/// 三态门：数据输入 + 使能输入 + 输出；使能为高时导通，否则输出高阻（Unknown）。
class TriStateGate : public Component
{
public:
    /// @param id 元件唯一 id
    explicit TriStateGate(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

// ── 输入/输出与接线 ──

/// 输入引脚：1 个输出脚，绘制为带标记的方框。
/// 电平由用户在画布上单击切换，作为仿真的驱动源。
class InputPin : public Component
{
public:
    /// @param id 元件唯一 id
    explicit InputPin(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;

    /// @return 当前设定的电平
    LogicLevel GetLevel() const { return m_level; }

    /// @param level 新的电平
    void SetLevel(LogicLevel level) { m_level = level; }

private:
    LogicLevel m_level = LogicLevel::Low; ///< 用户设定的输出电平
};

/// 输出引脚：1 个输入脚，绘制为带标记的方框。
class OutputPin : public Component
{
public:
    /// @param id 元件唯一 id
    explicit OutputPin(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
};

/// 分线器：1 输入 3 输出，绘制为梯形。
class Splitter : public Component
{
public:
    /// @param id 元件唯一 id
    explicit Splitter(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
};

// ── 信号源 ──

/// 高电平常量源：单输出脚，恒输出 High。
class VccSource : public Component
{
public:
    /// @param id 元件唯一 id
    explicit VccSource(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

/// 低电平常量源：单输出脚，恒输出 Low。
class GndSource : public Component
{
public:
    /// @param id 元件唯一 id
    explicit GndSource(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const override;
};

/// 时钟信号源：单输出脚。本次不实现翻转行为，输出保持 Unknown。
class ClockSource : public Component
{
public:
    /// @param id 元件唯一 id
    explicit ClockSource(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;

    /// @return 频率（Hz）
    int GetFrequencyHz() const { return m_frequencyHz; }

    /// @param hz 新频率；<= 0 时忽略（由 Circuit 的包装方法负责校验）
    void SetFrequencyHz(int hz)
    {
        if (hz > 0)
        {
            m_frequencyHz = hz;
        }
    }

    /// @return 占空比，取值 (0,1)
    double GetDutyCycle() const { return m_dutyCycle; }

    /// @param duty 新占空比；越界时忽略
    void SetDutyCycle(double duty)
    {
        if (duty > 0.0 && duty < 1.0)
        {
            m_dutyCycle = duty;
        }
    }

private:
    int m_frequencyHz = 1;    ///< 时钟频率（Hz），本次仅存储
    double m_dutyCycle = 0.5; ///< 占空比，本次仅存储
};

// ── 时序元件（本次不实现行为，仅作为可放置的元件存在）──

/// D 触发器：D/CLK 输入，Q/Q̄ 输出。本次不实现状态保持，输出保持 Unknown。
class DFlipFlop : public Component
{
public:
    /// @param id 元件唯一 id
    explicit DFlipFlop(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
};

/// RS 锁存器：S/R 输入，Q/Q̄ 输出。本次不实现状态保持，输出保持 Unknown。
class RsLatch : public Component
{
public:
    /// @param id 元件唯一 id
    explicit RsLatch(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
};

// ── 显示类 ──

/// LED 指示灯：单输入脚，渲染时按输入电平点亮或熄灭。
class LedIndicator : public Component
{
public:
    /// @param id 元件唯一 id
    explicit LedIndicator(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
};

/// 逻辑探针：单输入脚，渲染时在旁边显示 1/0/?。
class LogicProbe : public Component
{
public:
    /// @param id 元件唯一 id
    explicit LogicProbe(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
};

// ── 标注 ──

/// 文本标注：无引脚、不参与连线与仿真，仅用于在图上写说明文字。
/// 文本内容复用 Component::name 字段存储，避免为此单独增加序列化字段。
class TextLabel : public Component
{
public:
    /// @param id 元件唯一 id
    explicit TextLabel(int id);
    std::vector<Pin> BuildPins() const override;
    wxRect GetLocalBounds() const override;
    void DescribeSymbol(SymbolGeometry& out) const override;
    bool ParticipatesInSimulation() const override { return false; }

    /// @return 标注文本
    wxString GetText() const { return GetName(); }

    /// @param text 新文本（可含换行）
    void SetText(const wxString& text) { SetName(text); }
};
