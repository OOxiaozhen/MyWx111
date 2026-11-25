#include "GedaFootprints.h"

namespace geda
{
GatePinMap Dip14GatePins(int slot)
{
    // 74xx 四门器件（7400 / 7402 / 7404 / 7409 / 7432 / 7486 等）的 DIP14 标准脚位：
    // 门1(1,2→3) 门2(4,5→6) 门3(9,10→8) 门4(12,13→11)，电源 GND=7、VCC=14。
    // 门 3 与门 4 的输出脚号小于各自的输入脚号，因此必须查表而不能用线性公式。
    static const GatePinMap kMaps[4] = {
        { 1, 2, 3 },   // slot 1
        { 4, 5, 6 },   // slot 2
        { 9, 10, 8 },  // slot 3
        { 12, 13, 11 } // slot 4
    };

    const int index = (slot - 1) % 4;
    return kMaps[(index < 0) ? 0 : index];
}

int MapLogicalPinToDip14(ComponentType type, int slot, int logicalPinIndex)
{
    switch (type)
    {
    case ComponentType::AndGate:
    case ComponentType::OrGate:
    case ComponentType::NandGate:
    case ComponentType::NorGate:
    case ComponentType::XorGate:
    {
        // MakeTwoInputPins 的引脚顺序：0/1 为左侧输入，2 为右侧输出
        const GatePinMap pins = Dip14GatePins(slot);
        if (logicalPinIndex == 0)
        {
            return pins.inputA;
        }
        if (logicalPinIndex == 1)
        {
            return pins.inputB;
        }
        if (logicalPinIndex == 2)
        {
            return pins.output;
        }
        return -1;
    }

    case ComponentType::NotGate:
    {
        // 非门只有 1 入 1 出：index 0 为输入、index 1 为输出
        const GatePinMap pins = Dip14GatePins(slot);
        if (logicalPinIndex == 0)
        {
            return pins.inputA;
        }
        if (logicalPinIndex == 1)
        {
            return pins.output;
        }
        return -1;
    }

    case ComponentType::XnorGate:
    case ComponentType::TriStateGate:
    {
        // 与两输入门共用同一套 DIP14 脚位（三态门的使能脚按第二个输入处理）
        const GatePinMap pins = Dip14GatePins(slot);
        if (logicalPinIndex == 0)
        {
            return pins.inputA;
        }
        if (logicalPinIndex == 1)
        {
            return pins.inputB;
        }
        if (logicalPinIndex == 2)
        {
            return pins.output;
        }
        return -1;
    }

    case ComponentType::BufferGate:
    {
        const GatePinMap pins = Dip14GatePins(slot);
        if (logicalPinIndex == 0)
        {
            return pins.inputA;
        }
        if (logicalPinIndex == 1)
        {
            return pins.output;
        }
        return -1;
    }

    case ComponentType::DFlipFlop:
    case ComponentType::RsLatch:
    {
        // 四脚时序元件：两输入 + 两输出按槽位顺序落到 DIP14 上，
        // 第二路输出取同一门的相邻脚号，保证同片内不撞脚
        const GatePinMap pins = Dip14GatePins(slot);
        if (logicalPinIndex == 0)
        {
            return pins.inputA;
        }
        if (logicalPinIndex == 1)
        {
            return pins.inputB;
        }
        if (logicalPinIndex == 2)
        {
            return pins.output;
        }
        if (logicalPinIndex == 3)
        {
            return pins.inputA + 1;
        }
        return -1;
    }

    // VccSource/GndSource 已废弃，分支仅为满足穷举（工厂已拒绝创建，运行期不可达）
    case ComponentType::VccSource:
    case ComponentType::GndSource:
    case ComponentType::LevelSource:
    case ComponentType::ClockSource:
    case ComponentType::LedIndicator:
    case ComponentType::LogicProbe:
    case ComponentType::InputPin:
    case ComponentType::OutputPin:
        // 单脚元件（连接器 / 电平源 / 显示类）：唯一引脚映射到第 1 脚
        return (logicalPinIndex == 0) ? 1 : -1;

    case ComponentType::Splitter:
    case ComponentType::TextLabel:
        // 纯逻辑 / 纯注释元件，导出时被穿透，不产生网表引脚
        return -1;
    }
    return -1;
}

wxString DefaultFootprint(ComponentType type)
{
    switch (type)
    {
    case ComponentType::InputPin:
    case ComponentType::OutputPin:
    case ComponentType::LevelSource:
        return wxString(kSinglePinFootprint);

    case ComponentType::ClockSource:
        return wxString(L"OSC-1");

    case ComponentType::LedIndicator:
        return wxString(L"LED-3mm");

    case ComponentType::LogicProbe:
        return wxString(L"TEST-POINT");

    case ComponentType::VccSource:
    case ComponentType::GndSource:
        // 电源与地是网络属性而不是物理器件，不占封装
        return wxString();

    case ComponentType::Splitter:
    case ComponentType::TextLabel:
        return wxString();

    default:
        return wxString(kDip14Footprint);
    }
}

wxString DefaultValue(ComponentType type)
{
    switch (type)
    {
    case ComponentType::AndGate:
        return wxString(L"7409");
    case ComponentType::OrGate:
        return wxString(L"7432");
    case ComponentType::NotGate:
        return wxString(L"7404");
    case ComponentType::NandGate:
        return wxString(L"7400");
    case ComponentType::NorGate:
        return wxString(L"7402");
    case ComponentType::XorGate:
        return wxString(L"7486");
    case ComponentType::XnorGate:
        return wxString(L"74266");
    case ComponentType::BufferGate:
        return wxString(L"74LS07");
    case ComponentType::TriStateGate:
        return wxString(L"74HC125");
    case ComponentType::DFlipFlop:
        return wxString(L"74HC74");
    case ComponentType::RsLatch:
        return wxString(L"74HC279");
    case ComponentType::ClockSource:
        return wxString(L"CLOCK");
    case ComponentType::LedIndicator:
        return wxString(L"LED");
    case ComponentType::LogicProbe:
        return wxString(L"PROBE");
    case ComponentType::VccSource:
        return wxString(L"VCC");
    case ComponentType::LevelSource:
        return wxString(L"LEVEL_SOURCE");
    case ComponentType::GndSource:
        return wxString(L"GND");
    case ComponentType::TextLabel:
        return wxString();
    case ComponentType::InputPin:
    case ComponentType::OutputPin:
        return wxString(L"CONN");
    case ComponentType::Splitter:
        return wxString();
    }
    return wxString();
}
} // namespace geda
