#include "GateComponents.h"

#include <algorithm>
#include <cmath>

namespace
{
/// 圆周率常量。不使用 M_PI，避免依赖平台相关的宏定义。
constexpr double kPi = 3.14159265358979323846;

/// 构造「2 输入 1 输出」门电路的引脚布局。
/// @return 引脚列表：0/1 为左侧输入，2 为右侧输出
std::vector<Pin> MakeTwoInputPins()
{
    std::vector<Pin> pins;

    Pin in0;
    in0.index = 0;
    in0.offset = wxPoint(-kPinLength, 12);
    in0.direction = PinDirection::Input;
    pins.push_back(in0);

    Pin in1;
    in1.index = 1;
    in1.offset = wxPoint(-kPinLength, 28);
    in1.direction = PinDirection::Input;
    pins.push_back(in1);

    Pin out;
    out.index = 2;
    out.offset = wxPoint(kComponentBodySize + kPinLength, 20);
    out.direction = PinDirection::Output;
    pins.push_back(out);

    return pins;
}

/// @return 标准本体包围盒（正方形，边长 kComponentBodySize）
wxRect BodyBounds()
{
    return wxRect(0, 0, kComponentBodySize, kComponentBodySize);
}

/// @return 名称文字锚点：本体正下方，留出引脚与文字的行距
wxPoint LabelAnchor()
{
    return wxPoint(kComponentBodySize / 2, kComponentBodySize + kPinLength + 8);
}

/// 填充「与门」主体：左侧直边 + 右侧半圆（Dee 形）。
/// @param[out] out 待填充的几何描述
void BuildAndBody(SymbolGeometry& out)
{
    out.body.push_back(wxPoint(0, 2));
    out.body.push_back(wxPoint(18, 2));

    // 右侧半圆：圆心 (18, 20)、半径 18。
    // 屏幕坐标 Y 轴向下，故 -90 度指向正上方、+90 度指向正下方
    for (int deg = -90; deg <= 90; deg += 18)
    {
        const double rad = deg * kPi / 180.0;
        out.body.push_back(wxPoint(
            static_cast<int>(std::lround(18 + 18 * std::cos(rad))),
            static_cast<int>(std::lround(20 + 18 * std::sin(rad)))));
    }

    out.body.push_back(wxPoint(0, 38));
    out.labelAnchor = LabelAnchor();
}

/// 填充「或门」主体：左侧内凹弧 + 上下外凸、右侧尖头。
/// 采用多边形顶点近似弧形，避免在模型层引入贝塞尔绘制依赖。
/// @param[out] out 待填充的几何描述
void BuildOrBody(SymbolGeometry& out)
{
    out.body = {
        wxPoint(0, 2),   // 左上
        wxPoint(12, 5),  // 上弧外凸
        wxPoint(24, 12),
        wxPoint(36, 20), // 右尖
        wxPoint(24, 28),
        wxPoint(12, 35),
        wxPoint(0, 38),  // 左下
        wxPoint(7, 20)   // 左侧内凹点，闭合回左上形成凹弧
    };
    out.labelAnchor = LabelAnchor();
}

/// 在主体右端添加反相小圆（与非门 / 或非门 / 非门共用）。
/// @param[out] out 待填充的几何描述
/// @param centerX 反相圆心 X 坐标（本体本地坐标）
void AddInversionBubble(SymbolGeometry& out, int centerX)
{
    out.inversionCenter = wxPoint(centerX, 20);
    out.inversionRadius = 3;
}
} // namespace

// ───────────────────────── 与门 ─────────────────────────

AndGate::AndGate(int id)
    : Component(ComponentType::AndGate, id)
{
}

std::vector<Pin> AndGate::BuildPins() const
{
    return MakeTwoInputPins();
}

wxRect AndGate::GetLocalBounds() const
{
    return BodyBounds();
}

void AndGate::DescribeSymbol(SymbolGeometry& out) const
{
    BuildAndBody(out);
}

// ───────────────────────── 或门 ─────────────────────────

OrGate::OrGate(int id)
    : Component(ComponentType::OrGate, id)
{
}

std::vector<Pin> OrGate::BuildPins() const
{
    return MakeTwoInputPins();
}

wxRect OrGate::GetLocalBounds() const
{
    return BodyBounds();
}

void OrGate::DescribeSymbol(SymbolGeometry& out) const
{
    BuildOrBody(out);
}

// ───────────────────────── 非门 ─────────────────────────

NotGate::NotGate(int id)
    : Component(ComponentType::NotGate, id)
{
}

std::vector<Pin> NotGate::BuildPins() const
{
    std::vector<Pin> pins;

    Pin in;
    in.index = 0;
    in.offset = wxPoint(-kPinLength, 20);
    in.direction = PinDirection::Input;
    pins.push_back(in);

    Pin out;
    out.index = 1;
    out.offset = wxPoint(kComponentBodySize + kPinLength, 20);
    out.direction = PinDirection::Output;
    pins.push_back(out);

    return pins;
}

wxRect NotGate::GetLocalBounds() const
{
    return BodyBounds();
}

void NotGate::DescribeSymbol(SymbolGeometry& out) const
{
    out.body = { wxPoint(0, 2), wxPoint(32, 20), wxPoint(0, 38) };
    AddInversionBubble(out, 36);
    out.labelAnchor = LabelAnchor();
}

// ───────────────────────── 与非门 ─────────────────────────

NandGate::NandGate(int id)
    : Component(ComponentType::NandGate, id)
{
}

std::vector<Pin> NandGate::BuildPins() const
{
    return MakeTwoInputPins();
}

wxRect NandGate::GetLocalBounds() const
{
    return BodyBounds();
}

void NandGate::DescribeSymbol(SymbolGeometry& out) const
{
    BuildAndBody(out);
    AddInversionBubble(out, 39);
}

// ───────────────────────── 或非门 ─────────────────────────

NorGate::NorGate(int id)
    : Component(ComponentType::NorGate, id)
{
}

std::vector<Pin> NorGate::BuildPins() const
{
    return MakeTwoInputPins();
}

wxRect NorGate::GetLocalBounds() const
{
    return BodyBounds();
}

void NorGate::DescribeSymbol(SymbolGeometry& out) const
{
    BuildOrBody(out);
    AddInversionBubble(out, 39);
}

// ───────────────────────── 异或门 ─────────────────────────

XorGate::XorGate(int id)
    : Component(ComponentType::XorGate, id)
{
}

std::vector<Pin> XorGate::BuildPins() const
{
    return MakeTwoInputPins();
}

wxRect XorGate::GetLocalBounds() const
{
    return BodyBounds();
}

void XorGate::DescribeSymbol(SymbolGeometry& out) const
{
    BuildOrBody(out);

    // 异或门的标志：左侧再画一条背弧（与主体内凹弧平行），
    // 用折线近似，允许略微超出本体包围盒（绘制不受包围盒裁剪）
    SymbolPolyline backArc;
    backArc.points = {
        wxPoint(0, 2),
        wxPoint(-4, 10),
        wxPoint(-6, 20),
        wxPoint(-4, 30),
        wxPoint(0, 38)
    };
    backArc.closed = false;
    out.polylines.push_back(backArc);
}

// ───────────────────────── 输入引脚 ─────────────────────────

InputPin::InputPin(int id)
    : Component(ComponentType::InputPin, id)
{
}

std::vector<Pin> InputPin::BuildPins() const
{
    std::vector<Pin> pins;

    Pin out;
    out.index = 0;
    out.offset = wxPoint(kComponentBodySize + kPinLength, 20);
    out.direction = PinDirection::Output;
    pins.push_back(out);

    return pins;
}

wxRect InputPin::GetLocalBounds() const
{
    return BodyBounds();
}

void InputPin::DescribeSymbol(SymbolGeometry& out) const
{
    // 输入引脚画成方框，向外只有一个输出引脚
    out.body = {
        wxPoint(4, 10), wxPoint(36, 10), wxPoint(36, 30), wxPoint(4, 30)
    };
    out.labelAnchor = LabelAnchor();
}

// ───────────────────────── 输出引脚 ─────────────────────────

OutputPin::OutputPin(int id)
    : Component(ComponentType::OutputPin, id)
{
}

std::vector<Pin> OutputPin::BuildPins() const
{
    std::vector<Pin> pins;

    Pin in;
    in.index = 0;
    in.offset = wxPoint(-kPinLength, 20);
    in.direction = PinDirection::Input;
    pins.push_back(in);

    return pins;
}

wxRect OutputPin::GetLocalBounds() const
{
    return BodyBounds();
}

void OutputPin::DescribeSymbol(SymbolGeometry& out) const
{
    // 输出引脚画成方框，向内只有一个输入引脚
    out.body = {
        wxPoint(4, 10), wxPoint(36, 10), wxPoint(36, 30), wxPoint(4, 30)
    };
    out.labelAnchor = LabelAnchor();
}

// ───────────────────────── 分线器 ─────────────────────────

Splitter::Splitter(int id)
    : Component(ComponentType::Splitter, id)
{
}

std::vector<Pin> Splitter::BuildPins() const
{
    std::vector<Pin> pins;

    Pin in;
    in.index = 0;
    in.offset = wxPoint(-kPinLength, 20);
    in.direction = PinDirection::Input;
    pins.push_back(in);

    // 三个输出脚：位宽语义留待仿真阶段定义，此处固定 3 路输出
    const int outputY[] = { 8, 20, 32 };
    for (int i = 0; i < 3; ++i)
    {
        Pin out;
        out.index = 1 + i;
        out.offset = wxPoint(kComponentBodySize + kPinLength, outputY[i]);
        out.direction = PinDirection::Output;
        pins.push_back(out);
    }

    return pins;
}

wxRect Splitter::GetLocalBounds() const
{
    return BodyBounds();
}

void Splitter::DescribeSymbol(SymbolGeometry& out) const
{
    // 梯形：右侧高、左侧窄，呼应「1 输入分多路输出」的语义
    out.body = {
        wxPoint(8, 0), wxPoint(32, 0), wxPoint(40, 40), wxPoint(0, 40)
    };
    out.labelAnchor = LabelAnchor();
}

// ───────────────────────── 组合逻辑求值 ─────────────────────────

LogicLevel AndGate::Evaluate(const std::vector<LogicLevel>& inputs) const
{
    if (inputs.size() < 2)
    {
        return LogicLevel::Unknown;
    }
    return LogicAnd(inputs[0], inputs[1]);
}

LogicLevel OrGate::Evaluate(const std::vector<LogicLevel>& inputs) const
{
    if (inputs.size() < 2)
    {
        return LogicLevel::Unknown;
    }
    return LogicOr(inputs[0], inputs[1]);
}

LogicLevel NotGate::Evaluate(const std::vector<LogicLevel>& inputs) const
{
    if (inputs.empty())
    {
        return LogicLevel::Unknown;
    }
    return LogicNot(inputs[0]);
}

LogicLevel NandGate::Evaluate(const std::vector<LogicLevel>& inputs) const
{
    if (inputs.size() < 2)
    {
        return LogicLevel::Unknown;
    }
    return LogicNot(LogicAnd(inputs[0], inputs[1]));
}

LogicLevel NorGate::Evaluate(const std::vector<LogicLevel>& inputs) const
{
    if (inputs.size() < 2)
    {
        return LogicLevel::Unknown;
    }
    return LogicNot(LogicOr(inputs[0], inputs[1]));
}

LogicLevel XorGate::Evaluate(const std::vector<LogicLevel>& inputs) const
{
    if (inputs.size() < 2)
    {
        return LogicLevel::Unknown;
    }
    return LogicXor(inputs[0], inputs[1]);
}

LogicLevel XnorGate::Evaluate(const std::vector<LogicLevel>& inputs) const
{
    if (inputs.size() < 2)
    {
        return LogicLevel::Unknown;
    }
    return LogicNot(LogicXor(inputs[0], inputs[1]));
}

LogicLevel BufferGate::Evaluate(const std::vector<LogicLevel>& inputs) const
{
    if (inputs.empty())
    {
        return LogicLevel::Unknown;
    }
    return inputs[0];
}

LogicLevel TriStateGate::Evaluate(const std::vector<LogicLevel>& inputs) const
{
    // 使能脚高电平时透传数据，否则输出高阻（用 Unknown 表示）
    if (inputs.size() < 2 || inputs[1] != LogicLevel::High)
    {
        return LogicLevel::Unknown;
    }
    return inputs[0];
}

LogicLevel InputPin::Evaluate(const std::vector<LogicLevel>&) const
{
    // 输入引脚是驱动源，输出值由用户设定，与输入侧无关
    return m_level;
}

LogicLevel VccSource::Evaluate(const std::vector<LogicLevel>&) const
{
    return LogicLevel::High;
}

LogicLevel GndSource::Evaluate(const std::vector<LogicLevel>&) const
{
    return LogicLevel::Low;
}

// ───────────────────────── 同或门 ─────────────────────────

XnorGate::XnorGate(int id)
    : Component(ComponentType::XnorGate, id)
{
}

std::vector<Pin> XnorGate::BuildPins() const
{
    return MakeTwoInputPins();
}

wxRect XnorGate::GetLocalBounds() const
{
    return BodyBounds();
}

void XnorGate::DescribeSymbol(SymbolGeometry& out) const
{
    BuildOrBody(out);

    // 与异或门一致的左侧背弧
    SymbolPolyline backArc;
    backArc.points = {
        wxPoint(0, 2), wxPoint(-4, 10), wxPoint(-6, 20), wxPoint(-4, 30), wxPoint(0, 38)
    };
    backArc.closed = false;
    out.polylines.push_back(backArc);

    // 输出端反相小圆，与异或门区分
    AddInversionBubble(out, 39);
}

// ───────────────────────── 缓冲器 ─────────────────────────

BufferGate::BufferGate(int id)
    : Component(ComponentType::BufferGate, id)
{
}

std::vector<Pin> BufferGate::BuildPins() const
{
    std::vector<Pin> pins;

    Pin in;
    in.index = 0;
    in.offset = wxPoint(-kPinLength, 20);
    in.direction = PinDirection::Input;
    pins.push_back(in);

    Pin out;
    out.index = 1;
    out.offset = wxPoint(kComponentBodySize + kPinLength, 20);
    out.direction = PinDirection::Output;
    pins.push_back(out);

    return pins;
}

wxRect BufferGate::GetLocalBounds() const
{
    return BodyBounds();
}

void BufferGate::DescribeSymbol(SymbolGeometry& out) const
{
    out.body = { wxPoint(4, 4), wxPoint(34, 20), wxPoint(4, 36) };
    out.labelAnchor = LabelAnchor();
}

// ───────────────────────── 三态门 ─────────────────────────

TriStateGate::TriStateGate(int id)
    : Component(ComponentType::TriStateGate, id)
{
}

std::vector<Pin> TriStateGate::BuildPins() const
{
    std::vector<Pin> pins;

    // 0 = 数据输入，1 = 使能输入，2 = 输出
    Pin data;
    data.index = 0;
    data.offset = wxPoint(-kPinLength, 26);
    data.direction = PinDirection::Input;
    pins.push_back(data);

    Pin enable;
    enable.index = 1;
    enable.offset = wxPoint(-kPinLength, 10);
    enable.direction = PinDirection::Input;
    pins.push_back(enable);

    Pin out;
    out.index = 2;
    out.offset = wxPoint(kComponentBodySize + kPinLength, 20);
    out.direction = PinDirection::Output;
    pins.push_back(out);

    return pins;
}

wxRect TriStateGate::GetLocalBounds() const
{
    return BodyBounds();
}

void TriStateGate::DescribeSymbol(SymbolGeometry& out) const
{
    out.body = { wxPoint(4, 4), wxPoint(34, 20), wxPoint(4, 36) };

    // 顶部使能短线：三态门的惯用标识
    SymbolPolyline enableStub;
    enableStub.points = { wxPoint(12, 12), wxPoint(12, 0) };
    out.polylines.push_back(enableStub);

    out.labelAnchor = LabelAnchor();
}

// ───────────────────────── 信号源 ─────────────────────────

VccSource::VccSource(int id)
    : Component(ComponentType::VccSource, id)
{
}

std::vector<Pin> VccSource::BuildPins() const
{
    std::vector<Pin> pins;

    Pin out;
    out.index = 0;
    out.offset = wxPoint(20, kComponentBodySize + kPinLength);
    out.direction = PinDirection::Output;
    pins.push_back(out);

    return pins;
}

wxRect VccSource::GetLocalBounds() const
{
    return BodyBounds();
}

void VccSource::DescribeSymbol(SymbolGeometry& out) const
{
    // T 形：横杠 + 向下竖线引到引脚
    SymbolPolyline bar;
    bar.points = { wxPoint(8, 12), wxPoint(32, 12) };
    out.polylines.push_back(bar);

    SymbolPolyline stem;
    stem.points = { wxPoint(20, 12), wxPoint(20, 40) };
    out.polylines.push_back(stem);

    out.labelAnchor = wxPoint(20, 4);
}

GndSource::GndSource(int id)
    : Component(ComponentType::GndSource, id)
{
}

std::vector<Pin> GndSource::BuildPins() const
{
    std::vector<Pin> pins;

    Pin out;
    out.index = 0;
    out.offset = wxPoint(20, -kPinLength);
    out.direction = PinDirection::Output;
    pins.push_back(out);

    return pins;
}

wxRect GndSource::GetLocalBounds() const
{
    return BodyBounds();
}

void GndSource::DescribeSymbol(SymbolGeometry& out) const
{
    // 接地符号：三条递减横线 + 向上引线
    SymbolPolyline lead;
    lead.points = { wxPoint(20, 0), wxPoint(20, 16) };
    out.polylines.push_back(lead);

    SymbolPolyline bar1;
    bar1.points = { wxPoint(8, 16), wxPoint(32, 16) };
    out.polylines.push_back(bar1);

    SymbolPolyline bar2;
    bar2.points = { wxPoint(13, 24), wxPoint(27, 24) };
    out.polylines.push_back(bar2);

    SymbolPolyline bar3;
    bar3.points = { wxPoint(17, 32), wxPoint(23, 32) };
    out.polylines.push_back(bar3);

    out.labelAnchor = wxPoint(20, 38);
}

ClockSource::ClockSource(int id)
    : Component(ComponentType::ClockSource, id)
{
}

std::vector<Pin> ClockSource::BuildPins() const
{
    std::vector<Pin> pins;

    Pin out;
    out.index = 0;
    out.offset = wxPoint(kComponentBodySize + kPinLength, 20);
    out.direction = PinDirection::Output;
    pins.push_back(out);

    return pins;
}

wxRect ClockSource::GetLocalBounds() const
{
    return BodyBounds();
}

void ClockSource::DescribeSymbol(SymbolGeometry& out) const
{
    // 方波：直观表达「周期信号」
    SymbolPolyline wave;
    wave.points = { wxPoint(6, 28), wxPoint(14, 28), wxPoint(14, 12),
                    wxPoint(26, 12), wxPoint(26, 28), wxPoint(34, 28) };
    out.polylines.push_back(wave);

    out.labelAnchor = LabelAnchor();
}

// ───────────────────────── 时序元件 ─────────────────────────

namespace
{
/// 生成「两输入 + 两输出」的矩形时序元件引脚布局。
/// @return 引脚列表：0/1 为左侧输入，2/3 为右侧输出
std::vector<Pin> MakeTwoInputTwoOutputPins()
{
    std::vector<Pin> pins;

    Pin in0;
    in0.index = 0;
    in0.offset = wxPoint(-kPinLength, 12);
    in0.direction = PinDirection::Input;
    pins.push_back(in0);

    Pin in1;
    in1.index = 1;
    in1.offset = wxPoint(-kPinLength, 28);
    in1.direction = PinDirection::Input;
    pins.push_back(in1);

    Pin out0;
    out0.index = 2;
    out0.offset = wxPoint(kComponentBodySize + kPinLength, 12);
    out0.direction = PinDirection::Output;
    pins.push_back(out0);

    Pin out1;
    out1.index = 3;
    out1.offset = wxPoint(kComponentBodySize + kPinLength, 28);
    out1.direction = PinDirection::Output;
    pins.push_back(out1);

    return pins;
}

/// 绘制时序元件的矩形本体与内部分隔线。
/// @param[out] out 待填充的几何描述
void BuildFlipFlopBody(SymbolGeometry& out)
{
    out.body = { wxPoint(6, 4), wxPoint(34, 4), wxPoint(34, 36), wxPoint(6, 36) };
    out.labelAnchor = LabelAnchor();
}
} // namespace

DFlipFlop::DFlipFlop(int id)
    : Component(ComponentType::DFlipFlop, id)
{
}

std::vector<Pin> DFlipFlop::BuildPins() const
{
    return MakeTwoInputTwoOutputPins();
}

wxRect DFlipFlop::GetLocalBounds() const
{
    return BodyBounds();
}

void DFlipFlop::DescribeSymbol(SymbolGeometry& out) const
{
    BuildFlipFlopBody(out);
}

RsLatch::RsLatch(int id)
    : Component(ComponentType::RsLatch, id)
{
}

std::vector<Pin> RsLatch::BuildPins() const
{
    return MakeTwoInputTwoOutputPins();
}

wxRect RsLatch::GetLocalBounds() const
{
    return BodyBounds();
}

void RsLatch::DescribeSymbol(SymbolGeometry& out) const
{
    BuildFlipFlopBody(out);
}

// ───────────────────────── 显示类 ─────────────────────────

LedIndicator::LedIndicator(int id)
    : Component(ComponentType::LedIndicator, id)
{
}

std::vector<Pin> LedIndicator::BuildPins() const
{
    std::vector<Pin> pins;

    Pin in;
    in.index = 0;
    in.offset = wxPoint(-kPinLength, 20);
    in.direction = PinDirection::Input;
    pins.push_back(in);

    return pins;
}

wxRect LedIndicator::GetLocalBounds() const
{
    return BodyBounds();
}

void LedIndicator::DescribeSymbol(SymbolGeometry& out) const
{
    // 用 12 边形近似圆，避免在模型层引入圆弧绘制依赖
    out.body.clear();
    constexpr int kSides = 12;
    constexpr double kRadius = 14.0;
    for (int i = 0; i < kSides; ++i)
    {
        const double angle = 2.0 * kPi * i / kSides;
        out.body.push_back(wxPoint(static_cast<int>(std::lround(20 + kRadius * std::cos(angle))),
                                   static_cast<int>(std::lround(20 + kRadius * std::sin(angle)))));
    }

    // 两条射线，表达「发光」
    SymbolPolyline ray1;
    ray1.points = { wxPoint(32, 8), wxPoint(38, 2) };
    out.polylines.push_back(ray1);

    SymbolPolyline ray2;
    ray2.points = { wxPoint(34, 14), wxPoint(40, 12) };
    out.polylines.push_back(ray2);

    out.labelAnchor = LabelAnchor();
}

LogicProbe::LogicProbe(int id)
    : Component(ComponentType::LogicProbe, id)
{
}

std::vector<Pin> LogicProbe::BuildPins() const
{
    std::vector<Pin> pins;

    Pin in;
    in.index = 0;
    in.offset = wxPoint(-kPinLength, 20);
    in.direction = PinDirection::Input;
    pins.push_back(in);

    return pins;
}

wxRect LogicProbe::GetLocalBounds() const
{
    return BodyBounds();
}

void LogicProbe::DescribeSymbol(SymbolGeometry& out) const
{
    // 探针：小三角 + 尾部短线
    out.body = { wxPoint(8, 12), wxPoint(30, 20), wxPoint(8, 28) };

    SymbolPolyline lead;
    lead.points = { wxPoint(0, 20), wxPoint(8, 20) };
    out.polylines.push_back(lead);

    out.labelAnchor = LabelAnchor();
}

// ───────────────────────── 文本标注 ─────────────────────────

TextLabel::TextLabel(int id)
    : Component(ComponentType::TextLabel, id)
{
}

std::vector<Pin> TextLabel::BuildPins() const
{
    // 纯注释元件：没有任何引脚，因此不会被连线命中
    return {};
}

wxRect TextLabel::GetLocalBounds() const
{
    // 按文本长度估算包围盒，保证可被选中与拖动
    const int length = static_cast<int>(GetName().length());
    const int width = std::max(40, length * 8);
    return wxRect(0, 0, width, 20);
}

void TextLabel::DescribeSymbol(SymbolGeometry& out) const
{
    // 文字由渲染层直接绘制，几何描述留空
    out.labelAnchor = wxPoint(0, 14);
}

// ───────────────────────── 统一电平源 ─────────────────────────

LevelSource::LevelSource(int id)
    : Component(ComponentType::LevelSource, id)
{
}

std::vector<Pin> LevelSource::BuildPins() const
{
    std::vector<Pin> pins;

    // 单输出脚，底部出线（与已废弃的 VccSource 保持一致）
    Pin out;
    out.index = 0;
    out.offset = wxPoint(20, kComponentBodySize + kPinLength);
    out.direction = PinDirection::Output;
    pins.push_back(out);

    return pins;
}

wxRect LevelSource::GetLocalBounds() const
{
    return BodyBounds();
}

void LevelSource::DescribeSymbol(SymbolGeometry& out) const
{
    // 方框主体；框内的 1/0 文字由渲染层按当前电平绘制，
    // 模型层只提供几何，不产出文字
    out.body = { wxPoint(4, 4), wxPoint(36, 4), wxPoint(36, 36), wxPoint(4, 36) };
    out.labelAnchor = wxPoint(0, -kComponentBodySize);
}

LogicLevel LevelSource::Evaluate(const std::vector<LogicLevel>&) const
{
    // 恒定的驱动源，与输入无关
    return m_level;
}

// ───────────────────────── 元件工厂 ─────────────────────────

std::unique_ptr<Component> ComponentFactory::Create(ComponentType type, int id)
{
    switch (type)
    {
    case ComponentType::InputPin:
        return std::make_unique<InputPin>(id);
    case ComponentType::OutputPin:
        return std::make_unique<OutputPin>(id);
    case ComponentType::AndGate:
        return std::make_unique<AndGate>(id);
    case ComponentType::OrGate:
        return std::make_unique<OrGate>(id);
    case ComponentType::NotGate:
        return std::make_unique<NotGate>(id);
    case ComponentType::NandGate:
        return std::make_unique<NandGate>(id);
    case ComponentType::NorGate:
        return std::make_unique<NorGate>(id);
    case ComponentType::XorGate:
        return std::make_unique<XorGate>(id);
    case ComponentType::Splitter:
        return std::make_unique<Splitter>(id);
    case ComponentType::TextLabel:
        return std::make_unique<TextLabel>(id);
    // 已废弃类型：旧工程文件可能仍含这些元件，返回 nullptr 让加载端
    // 走「类型无效，已跳过」分支，而不是在此处报错
    case ComponentType::VccSource:
    case ComponentType::GndSource:
    case ComponentType::DFlipFlop:
    case ComponentType::RsLatch:
        return nullptr;
    case ComponentType::ClockSource:
        return std::make_unique<ClockSource>(id);
    case ComponentType::XnorGate:
        return std::make_unique<XnorGate>(id);
    case ComponentType::BufferGate:
        return std::make_unique<BufferGate>(id);
    case ComponentType::TriStateGate:
        return std::make_unique<TriStateGate>(id);
    case ComponentType::LedIndicator:
        return std::make_unique<LedIndicator>(id);
    case ComponentType::LogicProbe:
        return std::make_unique<LogicProbe>(id);
    case ComponentType::LevelSource:
        return std::make_unique<LevelSource>(id);
    default:
        return nullptr;
    }
}
