#include "NetlistExporter.h"

#include "../model/Circuit.h"
#include "../model/Component.h"
#include "../model/ComponentType.h"

#include <wx/datetime.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace
{
    /// 单一元件类型的导出元数据。
    struct FootprintInfo
    {
        const wchar_t* footprint; ///< KiCad 封装名，形如 "Package_DIP:DIP-14_W7.62mm"
        const wchar_t* refPrefix; ///< 位号前缀，如 "U" / "J"
        const wchar_t* valueBase; ///< value 字段的 ASCII 基名，如 "AND_GATE"
        const wchar_t* libName;   ///< libsource / libpart 使用的库昵称，统一为 "MyWx111"
    };

    /// 全部元件类型的封装映射表。
    /// 逻辑门统一按 14 脚 DIP 处理：各类门电路的输入引脚数并不相同，统一到同一种封装
    /// 可以让后续 PCB 环节不会因「同类器件封装不一致」反复调整；
    /// 输入/输出引脚是单点接线端子，1x01 排针封装最贴合其物理形态。
    /// Splitter 目前视为逻辑辅助元件，沿用 DIP-14；若将来定义为真实器件，改这一行即可。
    const FootprintInfo kFootprints[] = {
        { L"Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical", L"J", L"IN", L"MyWx111" },
        { L"Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical", L"J", L"OUT", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"AND_GATE", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"OR_GATE", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"NOT_GATE", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"NAND_GATE", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"NOR_GATE", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"XOR_GATE", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"SPLITTER", L"MyWx111" },
        // 以下为仿真扩展新增类型。footprint 为空表示不对应物理器件，导出时自动跳过
        { L"", L"T", L"TEXT", L"MyWx111" },
        { L"", L"PWR", L"VCC", L"MyWx111" },
        { L"", L"PWR", L"GND", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"CLOCK", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"XNOR_GATE", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"BUFFER", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"TRISTATE", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"DFF", L"MyWx111" },
        { L"Package_DIP:DIP-14_W7.62mm", L"U", L"RS_LATCH", L"MyWx111" },
        { L"LED_D:LED_D3.0mm", L"LED", L"LED", L"MyWx111" },
        { L"TestPoint:TestPoint_Pad", L"TP", L"PROBE", L"MyWx111" },
        { L"Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical", L"J", L"LEVEL",
          L"MyWx111" }
    };

    /// 取元件类型在映射表中的下标。
    /// 用 switch 穷举而不是直接把枚举值当下标：将来新增元件类型时编译器能提示这里需要补分支，
    /// 避免静默取到越界数据或把类型映射到错误封装上。
    /// @param type 元件类型
    /// @return 映射表下标；未映射返回 -1
    int FootprintIndex(ComponentType type)
    {
        switch (type)
        {
        case ComponentType::InputPin:
            return 0;
        case ComponentType::OutputPin:
            return 1;
        case ComponentType::AndGate:
            return 2;
        case ComponentType::OrGate:
            return 3;
        case ComponentType::NotGate:
            return 4;
        case ComponentType::NandGate:
            return 5;
        case ComponentType::NorGate:
            return 6;
        case ComponentType::XorGate:
            return 7;
        case ComponentType::Splitter:
            return 8;
        case ComponentType::TextLabel:
            return 9;
        case ComponentType::VccSource:
            return 10;
        case ComponentType::GndSource:
            return 11;
        case ComponentType::ClockSource:
            return 12;
        case ComponentType::XnorGate:
            return 13;
        case ComponentType::BufferGate:
            return 14;
        case ComponentType::TriStateGate:
            return 15;
        case ComponentType::DFlipFlop:
            return 16;
        case ComponentType::RsLatch:
            return 17;
        case ComponentType::LedIndicator:
            return 18;
        case ComponentType::LogicProbe:
            return 19;
        case ComponentType::LevelSource:
            return 20;
        }
        return -1;
    }

    /// 查元件类型对应的导出元数据。
    /// 返回静态引用而非值，避免导出循环里反复拷贝四个指针。
    /// @param type 元件类型
    /// @return 元数据引用；未映射时返回 footprint 为空串的哨兵
    const FootprintInfo& LookupFootprint(ComponentType type)
    {
        const int index = FootprintIndex(type);
        if (index < 0)
        {
            static const FootprintInfo kUnknown{ L"", L"U", L"UNKNOWN", L"MyWx111" };
            return kUnknown;
        }
        return kFootprints[index];
    }

    /// 判断元件类型是否在映射表中有有效封装。
    /// 导出前用它逐元件校验，未映射的类型会被跳过并累积告警，
    /// 避免新增枚举值时静默输出缺封装的脏网表。
    /// @param type 元件类型
    /// @return 有映射返回 true
    bool HasFootprintMapping(ComponentType type)
    {
        return LookupFootprint(type).footprint[0] != L'\0';
    }

    /// 引脚唯一键：元件 id + 引脚索引。
    /// 不直接用 PinRef 做 map 键的原因：模型层的 PinRef 未提供 operator<，
    /// 且这里只需要可用于排序的两个整数。
    struct PinKey
    {
        int componentId = 0; ///< 引脚所属元件 id
        int pinIndex = 0;    ///< 引脚序号

        /// @param rhs 另一个引脚键
        /// @return 按元件 id、再按引脚索引的字典序比较结果
        bool operator<(const PinKey& rhs) const
        {
            if (componentId != rhs.componentId)
            {
                return componentId < rhs.componentId;
            }
            return pinIndex < rhs.pinIndex;
        }
    };

    /// 一个参与导出的元件（封装未映射的元件不会出现在此表中）。
    struct ExportedComponent
    {
        int id = 0;             ///< 原模型 componentId，用于回查引脚
        wxString ref;           ///< 生成的位号，如 "U1"
        wxString value;         ///< ASCII value，如 "AND_GATE_1"
        wxString footprint;     ///< KiCad 封装串
        wxString description;   ///< 中文元件名，写入 libsource.description
        wxString tstamp;        ///< 8 位大写十六进制时间戳，由元件 id 派生
        wxString libPartName;   ///< libpart 名（含引脚数），保证与 pins 声明一致
        int pinCount = 0;       ///< 该元件的引脚数（最大引脚索引 + 1）
    };

    /// 一个导出后的网络。
    struct ExportedNet
    {
        int code = 0;                                ///< 网络编号，从 1 递增
        wxString name;                               ///< 网络名，如 "Net-1"
        std::vector<std::pair<wxString, int>> nodes; ///< (位号, 引脚号)，已按位号与引脚号排序
    };

    /// 并查集：路径压缩 + 按秩合并。
    /// 连线是两两端点对，链式连接（A-B、B-C）必须归入同一网络，
    /// 并查集是处理这类等价类合并最直接且代价近似线性的结构。
    class DisjointSet
    {
    public:
        /// @param count 元素个数，元素下标为 [0, count)
        explicit DisjointSet(size_t count)
            : m_parent(count)
            , m_rank(count, 0)
        {
            for (size_t i = 0; i < count; ++i)
            {
                m_parent[i] = static_cast<int>(i);
            }
        }

        /// @param x 元素下标
        /// @return 所在集合的根下标（带路径压缩）
        int Find(int x)
        {
            int root = x;
            while (m_parent[static_cast<size_t>(root)] != root)
            {
                root = m_parent[static_cast<size_t>(root)];
            }

            // 路径压缩：把沿途节点直接挂到根上，后续查询接近常数时间
            while (m_parent[static_cast<size_t>(x)] != root)
            {
                const int next = m_parent[static_cast<size_t>(x)];
                m_parent[static_cast<size_t>(x)] = root;
                x = next;
            }
            return root;
        }

        /// @param a 第一个元素下标
        /// @param b 第二个元素下标
        void Union(int a, int b)
        {
            int rootA = Find(a);
            int rootB = Find(b);
            if (rootA == rootB)
            {
                return;
            }

            // 按秩合并：把浅树挂到深树下，避免极端情况下退化成链表
            if (m_rank[static_cast<size_t>(rootA)] < m_rank[static_cast<size_t>(rootB)])
            {
                std::swap(rootA, rootB);
            }
            m_parent[static_cast<size_t>(rootB)] = rootA;
            if (m_rank[static_cast<size_t>(rootA)] == m_rank[static_cast<size_t>(rootB)])
            {
                ++m_rank[static_cast<size_t>(rootA)];
            }
        }

    private:
        std::vector<int> m_parent; ///< 父节点下标，根节点自指
        std::vector<int> m_rank;   ///< 秩（树高上界），用于按秩合并
    };

    /// 为所有已映射封装的元件生成位号与导出元数据。
    /// @param circuit 只读电路
    /// @param[out] out 参与导出的元件表，按模型遍历顺序排列（保证跨次导出稳定）
    /// @param[in,out] warnings 累积「类型无封装映射被跳过」类告警
    void AssignRefs(const Circuit& circuit, std::vector<ExportedComponent>& out, wxString& warnings)
    {
        std::map<wxString, int> prefixCounter; // 每种位号前缀已用到的序号
        std::map<wxString, int> valueCounter;  // 每种 value 基名已用到的序号

        for (const auto& compPtr : circuit.GetComponents())
        {
            if (!compPtr)
            {
                continue;
            }

            const Component& comp = *compPtr;
            const ComponentType type = comp.GetType();

            // 文本标注是纯注释、电源/地是网络属性，都不对应物理器件，
            // 直接跳过而不报「未配置封装」告警
            if (type == ComponentType::TextLabel || type == ComponentType::VccSource ||
                type == ComponentType::GndSource)
            {
                continue;
            }

            if (!HasFootprintMapping(type))
            {
                warnings += wxString::Format(L"元件「%s」的类型未配置封装，已跳过\r\n", comp.GetName());
                continue;
            }

            const FootprintInfo& info = LookupFootprint(type);
            const wxString prefix(info.refPrefix);
            const wxString valueBase(info.valueBase);

            ExportedComponent item;
            item.id = comp.GetId();
            item.ref = wxString::Format(L"%s%d", prefix, ++prefixCounter[prefix]);
            // value 用 ASCII 基名 + 独立序号：KiCad 对 .net 中的非 ASCII 支持不稳定，
            // 中文名只写入 description，避免影响导入
            item.value = wxString::Format(L"%s_%d", valueBase, ++valueCounter[valueBase]);
            item.footprint = wxString(info.footprint);
            item.description = comp.GetName();
            // tstamp 由元件 id 派生：id 由模型保证唯一，因此 8 位大写十六进制同样唯一
            item.tstamp = wxString::Format(L"%08X", static_cast<unsigned>(0x10000000u) +
                                                        static_cast<unsigned>(comp.GetId()));

            int maxIndex = 0;
            for (const Pin& pin : comp.GetTransformedPins())
            {
                maxIndex = std::max(maxIndex, pin.index);
            }
            item.pinCount = maxIndex + 1;

            // 引脚数参与 libpart 名：同类但引脚数不同时不会共用同一份 libpart 定义，
            // 保证 libparts 里的 pin 声明永远覆盖网表中真实出现的引脚序号
            item.libPartName = wxString::Format(L"%s_%d", valueBase, item.pinCount);

            out.push_back(item);
        }
    }

    /// 把引脚按连线连通性划分成网络，并生成稳定的编号与节点顺序。
    /// @param circuit 只读电路
    /// @param comps 参与导出的元件表，用于把 componentId 映射回位号
    /// @param[out] outNets 生成的网络列表，已排序并编号
    /// @param[in,out] warnings 累积「连线端点不在已导出元件上」类告警
    void BuildNets(const Circuit& circuit, const std::vector<ExportedComponent>& comps,
                   std::vector<ExportedNet>& outNets, wxString& warnings)
    {
        std::map<int, const ExportedComponent*> compById;
        for (const ExportedComponent& comp : comps)
        {
            compById[comp.id] = &comp;
        }

        // 用 std::map 而非 unordered_map 分配引脚索引：std::map 的遍历顺序确定，
        // 索引分配顺序稳定，才能保证同一电路多次导出得到相同的网络编号
        std::map<PinKey, int> keyToIndex;
        std::vector<PinKey> indexToKey;

        for (const auto& compPtr : circuit.GetComponents())
        {
            if (!compPtr)
            {
                continue;
            }
            const Component& comp = *compPtr;
            if (compById.find(comp.GetId()) == compById.end())
            {
                continue; // 未参与导出的元件，其引脚不进入网表
            }

            for (const Pin& pin : comp.GetTransformedPins())
            {
                const PinKey key{ comp.GetId(), pin.index };
                if (keyToIndex.find(key) != keyToIndex.end())
                {
                    continue;
                }
                keyToIndex[key] = static_cast<int>(indexToKey.size());
                indexToKey.push_back(key);
            }
        }

        DisjointSet sets(indexToKey.size());

        // 以 Wire 列表作为连通性的唯一真相源：不读取 Pin::connected，
        // 避免该标志未及时刷新时导致漏连接
        for (const auto& wirePtr : circuit.GetWires())
        {
            if (!wirePtr)
            {
                continue;
            }

            const PinRef& from = wirePtr->GetFrom();
            const PinRef& to = wirePtr->GetTo();
            const auto fromIt = keyToIndex.find(PinKey{ from.componentId, from.pinIndex });
            const auto toIt = keyToIndex.find(PinKey{ to.componentId, to.pinIndex });
            if (fromIt == keyToIndex.end() || toIt == keyToIndex.end())
            {
                warnings += wxString::Format(L"连线 %d 的端点不在已导出元件上，已忽略\r\n",
                                             wirePtr->GetId());
                continue;
            }
            sets.Union(fromIt->second, toIt->second);
        }

        // 按根聚合引脚下标
        std::map<int, std::vector<int>> rootToPins;
        for (size_t i = 0; i < indexToKey.size(); ++i)
        {
            rootToPins[sets.Find(static_cast<int>(i))].push_back(static_cast<int>(i));
        }

        /// 编号前的中间网络：保留排序键，编号在整体排序之后统一分配。
        struct PendingNet
        {
            PinKey sortKey;                              ///< 组内最小引脚键，用于稳定排序
            std::vector<std::pair<wxString, int>> nodes; ///< (位号, 引脚号)
        };

        std::vector<PendingNet> pending;
        pending.reserve(rootToPins.size());

        for (auto& group : rootToPins)
        {
            std::vector<int>& indices = group.second;
            std::sort(indices.begin(), indices.end(),
                [&indexToKey](int lhs, int rhs) { return indexToKey[static_cast<size_t>(lhs)] <
                                                        indexToKey[static_cast<size_t>(rhs)]; });

            PendingNet net;
            net.sortKey = indexToKey[static_cast<size_t>(indices.front())];
            for (int index : indices)
            {
                const PinKey& key = indexToKey[static_cast<size_t>(index)];
                const auto compIt = compById.find(key.componentId);
                if (compIt == compById.end())
                {
                    continue; // 注册引脚时已过滤，这里仅作防御
                }
                net.nodes.emplace_back(compIt->second->ref, key.pinIndex);
            }

            // 节点按位号字符串升序、再按引脚号升序：输出可读且与遍历顺序无关
            std::sort(net.nodes.begin(), net.nodes.end(),
                [](const std::pair<wxString, int>& lhs, const std::pair<wxString, int>& rhs)
                {
                    if (lhs.first != rhs.first)
                    {
                        return lhs.first < rhs.first;
                    }
                    return lhs.second < rhs.second;
                });

            pending.push_back(std::move(net));
        }

        // 网络整体按组内最小引脚键排序后编号，使 Net-1..n 的分配稳定可复现
        std::sort(pending.begin(), pending.end(),
            [](const PendingNet& lhs, const PendingNet& rhs) { return lhs.sortKey < rhs.sortKey; });

        outNets.clear();
        outNets.reserve(pending.size());

        int code = 1;
        for (PendingNet& item : pending)
        {
            ExportedNet net;
            net.code = code;
            net.name = wxString::Format(L"Net-%d", code);
            net.nodes = std::move(item.nodes);
            outNets.push_back(std::move(net));
            ++code;
        }
    }

    /// 把文本转义为可安全放进 S-expression 双引号字段的内容。
    /// 只需处理反斜杠与双引号：括号出现在双引号内部时不会被解析器当作括号，
    /// 因此无需转义（而对括号做转义反而会让 KiCad 读到多余的字符）。
    /// @param text 原始文本
    /// @return 转义后的文本
    wxString EscapeSexpr(const wxString& text)
    {
        wxString result;
        result.reserve(text.length() + 8);

        for (const wxUniChar ch : text)
        {
            if (ch == L'"' || ch == L'\\')
            {
                result += L'\\';
            }
            result += ch;
        }
        return result;
    }

    /// 按固定两空格缩进追加一行文本。
    /// @param out 文本输出流
    /// @param text 行内容（不含缩进与换行）
    /// @param indentLevel 缩进层数，每层两个空格
    void WriteLine(wxTextOutputStream& out, const wxString& text, int indentLevel)
    {
        wxString indent;
        indent.reserve(static_cast<size_t>(indentLevel) * 2);
        for (int i = 0; i < indentLevel; ++i)
        {
            indent += L"  ";
        }

        out << indent << text << L'\n';
    }

    /// 写出 components 区块。
    /// @param out 文本输出流
    /// @param comps 参与导出的元件表
    void WriteComponents(wxTextOutputStream& out, const std::vector<ExportedComponent>& comps)
    {
        WriteLine(out, L"(components", 1);
        for (const ExportedComponent& comp : comps)
        {
            WriteLine(out, wxString::Format(L"(comp (ref %s) (value \"%s\") (footprint \"%s\")",
                                            comp.ref, EscapeSexpr(comp.value),
                                            EscapeSexpr(comp.footprint)),
                     2);
            WriteLine(out, wxString::Format(L"(libsource (lib \"MyWx111\") (part \"%s\") (description \"%s\"))",
                                            EscapeSexpr(comp.libPartName),
                                            EscapeSexpr(comp.description)),
                     3);
            WriteLine(out, wxString::Format(L"(tstamp %s)", comp.tstamp), 3);
            WriteLine(out, L"(sheetpath (names \"/\") (tstamps \"/\"))", 3);
            WriteLine(out, L")", 2);
        }
        WriteLine(out, L")", 1);
    }

    /// 写出 libparts 区块。
    /// 每个 (lib, part) 组合只输出一次，其 pins 必须覆盖网表中出现的全部引脚序号，
    /// 否则 KiCad 会报「libpart 中不存在该 pin」并拒绝解析对应元件。
    /// @param out 文本输出流
    /// @param libParts 已去重的 (lib, part) → 引脚数映射
    void WriteLibParts(wxTextOutputStream& out,
                       const std::map<std::pair<wxString, wxString>, int>& libParts)
    {
        WriteLine(out, L"(libparts", 1);
        for (const auto& entry : libParts)
        {
            const wxString& libName = entry.first.first;
            const wxString& partName = entry.first.second;
            const int pinCount = entry.second;

            WriteLine(out, wxString::Format(L"(libpart (lib \"%s\") (part \"%s\")",
                                            EscapeSexpr(libName), EscapeSexpr(partName)),
                     2);
            WriteLine(out, L"(pins", 3);
            for (int pin = 1; pin <= pinCount; ++pin)
            {
                WriteLine(out, wxString::Format(L"(pin (num %d) (name \"~\") (type passive))", pin), 4);
            }
            WriteLine(out, L")", 3);
            WriteLine(out, L")", 2);
        }
        WriteLine(out, L")", 1);
    }

    /// 写出 libraries 区块。
    /// uri 允许为空串：该逻辑库只用于让 KiCad 建立 libpart 的归属关系，
    /// 实际元件信息已完整写在 libparts 中，不依赖外部库文件。
    /// @param out 文本输出流
    void WriteLibraries(wxTextOutputStream& out)
    {
        WriteLine(out, L"(libraries", 1);
        WriteLine(out, L"(library (logical \"MyWx111\")", 2);
        WriteLine(out, L"(uri \"\")", 3);
        WriteLine(out, L")", 2);
        WriteLine(out, L")", 1);
    }

    /// 写出 nets 区块。
    /// 空网络不写出：KiCad 解析器要求每个 net 至少包含一个 node。
    /// @param out 文本输出流
    /// @param nets 已编号的网络列表
    void WriteNets(wxTextOutputStream& out, const std::vector<ExportedNet>& nets)
    {
        WriteLine(out, L"(nets", 1);
        for (const ExportedNet& net : nets)
        {
            if (net.nodes.empty())
            {
                continue;
            }

            WriteLine(out, wxString::Format(L"(net (code %d) (name \"%s\")", net.code,
                                            EscapeSexpr(net.name)),
                     2);
            for (const auto& node : net.nodes)
            {
                WriteLine(out, wxString::Format(L"(node (ref %s) (pin %d))", node.first, node.second), 3);
            }
            WriteLine(out, L")", 2);
        }
        WriteLine(out, L")", 1);
    }
}

namespace netlist
{
bool PinKeyLess(const PinKey& a, const PinKey& b)
{
    if (a.componentId != b.componentId)
    {
        return a.componentId < b.componentId;
    }
    return a.pinIndex < b.pinIndex;
}

std::vector<std::vector<PinKey>> BuildNets(const Circuit& circuit)
{
    // 与 KiCad 导出共用同一套划分规则（并查集 + Wire 为唯一真相源），
    // 但两条导出路径的引脚集合各自独立维护：这里不做封装映射过滤，
    // 由调用方按自己的封装规则决定哪些引脚最终写进文件。
    std::map<std::pair<int, int>, int> keyToIndex;
    std::vector<PinKey> indexToKey;

    for (const auto& compPtr : circuit.GetComponents())
    {
        if (!compPtr)
        {
            continue;
        }
        const Component& comp = *compPtr;
        for (const Pin& pin : comp.GetTransformedPins())
        {
            const std::pair<int, int> key(comp.GetId(), pin.index);
            if (keyToIndex.find(key) != keyToIndex.end())
            {
                continue;
            }
            keyToIndex[key] = static_cast<int>(indexToKey.size());
            indexToKey.push_back(PinKey{ comp.GetId(), pin.index });
        }
    }

    DisjointSet sets(indexToKey.size());

    // 分线器的所有引脚先合并成一个节点：它只表达「一根线分成多路」，
    // 若当作普通两端元件处理，会把一条本该连续的网络人为切断
    for (const auto& compPtr : circuit.GetComponents())
    {
        if (!compPtr || compPtr->GetType() != ComponentType::Splitter)
        {
            continue;
        }
        const Component& comp = *compPtr;
        int firstIndex = -1;
        for (const Pin& pin : comp.GetTransformedPins())
        {
            const auto it = keyToIndex.find(std::make_pair(comp.GetId(), pin.index));
            if (it == keyToIndex.end())
            {
                continue;
            }
            if (firstIndex < 0)
            {
                firstIndex = it->second;
            }
            else
            {
                sets.Union(firstIndex, it->second);
            }
        }
    }

    for (const auto& wirePtr : circuit.GetWires())
    {
        if (!wirePtr)
        {
            continue;
        }
        const PinRef& from = wirePtr->GetFrom();
        const PinRef& to = wirePtr->GetTo();
        const auto fromIt = keyToIndex.find(std::make_pair(from.componentId, from.pinIndex));
        const auto toIt = keyToIndex.find(std::make_pair(to.componentId, to.pinIndex));
        if (fromIt == keyToIndex.end() || toIt == keyToIndex.end())
        {
            continue;
        }
        sets.Union(fromIt->second, toIt->second);
    }

    std::map<int, std::vector<int>> rootToPins;
    for (size_t i = 0; i < indexToKey.size(); ++i)
    {
        rootToPins[sets.Find(static_cast<int>(i))].push_back(static_cast<int>(i));
    }

    std::vector<std::vector<PinKey>> result;
    result.reserve(rootToPins.size());
    for (auto& group : rootToPins)
    {
        std::vector<int>& indices = group.second;
        std::sort(indices.begin(), indices.end(),
            [&indexToKey](int lhs, int rhs)
            {
                return PinKeyLess(indexToKey[static_cast<size_t>(lhs)],
                                  indexToKey[static_cast<size_t>(rhs)]);
            });

        std::vector<PinKey> net;
        net.reserve(indices.size());
        for (int index : indices)
        {
            net.push_back(indexToKey[static_cast<size_t>(index)]);
        }
        result.push_back(std::move(net));
    }

    // 按每个网络的最小引脚键排序，保证同一电路多次导出得到相同顺序
    std::sort(result.begin(), result.end(),
        [](const std::vector<PinKey>& lhs, const std::vector<PinKey>& rhs)
        {
            return PinKeyLess(lhs.front(), rhs.front());
        });
    return result;
}

bool ExportKiCadNetlist(const Circuit& circuit, const wxString& path, wxString* error)
{
    // 先完成全部纯计算：一旦有元件缺少封装映射或连线端点异常，
    // 这些情况只累积为告警，不影响文件写出
    std::vector<ExportedComponent> comps;
    wxString warnings;
    AssignRefs(circuit, comps, warnings);

    std::vector<ExportedNet> nets;
    BuildNets(circuit, comps, nets, warnings);

    wxFileOutputStream stream(path);
    if (!stream.IsOk())
    {
        if (error != nullptr)
        {
            *error = wxString(L"无法创建或写入文件：") + path;
        }
        return false;
    }

    // 固定使用 Unix 换行：同一电路在不同平台上导出的文件逐字节一致，便于版本比对
    wxTextOutputStream out(stream, wxEOL_UNIX);

    WriteLine(out, L"(export (version D)", 0);

    WriteLine(out, L"(design", 1);
    WriteLine(out, wxString::Format(L"(source \"%s\")", EscapeSexpr(L"MyWx111")), 2);
    // 日期用 ISO 格式：本地化格式（如 2026/1/5）会被 KiCad 的解析器拒绝
    WriteLine(out, wxString::Format(L"(date \"%s\")", wxDateTime::Now().FormatISODate()), 2);
    WriteLine(out, wxString::Format(L"(tool \"%s\")", EscapeSexpr(L"MyWx111 EDA")), 2);
    WriteLine(out, L")", 1);

    WriteComponents(out, comps);

    // libparts 必须按 (lib, part) 去重：同一类型的多个实例共用同一份 libpart 定义，
    // 重复输出会让 KiCad 报告库元件重名
    std::map<std::pair<wxString, wxString>, int> libParts;
    for (const ExportedComponent& comp : comps)
    {
        const std::pair<wxString, wxString> key(L"MyWx111", comp.libPartName);
        const auto it = libParts.find(key);
        if (it == libParts.end())
        {
            libParts.emplace(key, comp.pinCount);
        }
        else
        {
            // 同名 libpart 理论上引脚数一致；取较大值作为防御，保证 pins 声明始终够用
            it->second = std::max(it->second, comp.pinCount);
        }
    }
    WriteLibParts(out, libParts);

    WriteLibraries(out);
    WriteNets(out, nets);

    WriteLine(out, L")", 0);

    out.Flush();
    if (!stream.IsOk())
    {
        if (error != nullptr)
        {
            *error = wxString(L"写入文件失败：") + path;
        }
        return false;
    }

    stream.Sync();
    if (!stream.IsOk())
    {
        if (error != nullptr)
        {
            *error = wxString(L"写入文件后同步失败，文件可能不完整：") + path;
        }
        return false;
    }

    // 走到这里文件已成功写出：把告警交给调用方决定是否提示，返回值仍为 true，
    // 避免「文件已生成却报导出失败」的误导
    if (error != nullptr)
    {
        if (comps.empty())
        {
            *error = (warnings.empty() ? wxString() : warnings + wxString(L"\r\n"))
                     + wxString(L"电路为空或全部元件已被跳过，已导出空网表");
        }
        else
        {
            *error = warnings;
        }
    }
    return true;
}
} // namespace netlist
