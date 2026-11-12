#include "Simulator.h"

#include "../model/Circuit.h"
#include "../model/Component.h"
#include "../model/Pin.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace
{
    /// 并查集：路径压缩 + 按秩合并。
    /// 仿真层自带一份而不复用 NetlistExporter 里的那个：
    /// 后者在匿名命名空间内、仅服务网表导出，跨文件复用需要为其开接口，
    /// 而这里的实现只有几十行，自带一份换来的是 sim 层不依赖 io 层。
    class DisjointSet
    {
    public:
        /// @param count 元素个数，下标为 [0, count)
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
        std::vector<int> m_rank;   ///< 秩（树高上界）
    };

    /// 把 (元件 id, 引脚序号) 编码成单一 64 位键，便于作为 map 键使用。
    /// 引脚序号为 int 且实际很小，取低 20 位足够；元件 id 放在高位。
    /// @param componentId 元件 id
    /// @param pinIndex 引脚序号
    /// @return 组合键
    long long MakePinKey(int componentId, int pinIndex)
    {
        return (static_cast<long long>(componentId) << 20) | static_cast<long long>(pinIndex);
    }

    /// @param key MakePinKey 生成的组合键
    /// @return 还原出的元件 id
    int KeyToComponentId(long long key)
    {
        return static_cast<int>(key >> 20);
    }
} // namespace

namespace sim
{
Simulator::Simulator(const Circuit* circuit)
    : m_circuit(circuit)
{
}

void Simulator::SetCircuit(const Circuit* circuit)
{
    m_circuit = circuit;
    m_topologyDirty = true;
    m_oscillation = false;
    m_pinToNet.clear();
    m_wireToNet.clear();
    m_compInputNets.clear();
    m_compOutputNets.clear();
    m_netValues.clear();
    m_conflictedNets.clear();
}

void Simulator::InvalidateTopology()
{
    m_topologyDirty = true;
}

void Simulator::RebuildTopology()
{
    m_pinToNet.clear();
    m_wireToNet.clear();
    m_compInputNets.clear();
    m_compOutputNets.clear();

    if (m_circuit == nullptr)
    {
        m_topologyDirty = false;
        return;
    }

    // 先登记全部引脚，得到稳定下标；顺带记住电气方向，供后面分类到输入/输出网络表
    std::map<long long, int> keyToIndex;
    std::vector<long long> indexToKey;
    std::map<long long, PinDirection> keyToDirection;

    for (const auto& compPtr : m_circuit->GetComponents())
    {
        if (!compPtr)
        {
            continue;
        }
        const Component& comp = *compPtr;
        for (const Pin& pin : comp.GetTransformedPins())
        {
            const long long key = MakePinKey(comp.GetId(), pin.index);
            if (keyToIndex.find(key) != keyToIndex.end())
            {
                continue;
            }
            keyToIndex[key] = static_cast<int>(indexToKey.size());
            indexToKey.push_back(key);
            keyToDirection[key] = pin.direction;
        }
    }

    // 连线两端归入同一网络
    DisjointSet sets(indexToKey.size());
    for (const auto& wirePtr : m_circuit->GetWires())
    {
        if (!wirePtr)
        {
            continue;
        }
        const PinRef& from = wirePtr->GetFrom();
        const PinRef& to = wirePtr->GetTo();
        const auto fromIt = keyToIndex.find(MakePinKey(from.componentId, from.pinIndex));
        const auto toIt = keyToIndex.find(MakePinKey(to.componentId, to.pinIndex));
        if (fromIt == keyToIndex.end() || toIt == keyToIndex.end())
        {
            continue;
        }
        sets.Union(fromIt->second, toIt->second);
    }

    // 分线器的所有引脚先合并成一个节点：它只表达「一根线分成多路」，
    // 若当作普通两端元件处理会把一条本该连续的网络人为切断；
    // 合并后分线器本身无需参与求值即可透明导通
    for (const auto& compPtr : m_circuit->GetComponents())
    {
        if (!compPtr || compPtr->GetType() != ComponentType::Splitter)
        {
            continue;
        }
        const Component& comp = *compPtr;
        int firstIndex = -1;
        for (const Pin& pin : comp.GetTransformedPins())
        {
            const auto it = keyToIndex.find(MakePinKey(comp.GetId(), pin.index));
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

    // 按根分配网络 id，并填充引脚→网络、元件输入/输出网络表
    std::map<int, int> rootToNet;
    int nextNetId = 0;

    for (size_t i = 0; i < indexToKey.size(); ++i)
    {
        const int root = sets.Find(static_cast<int>(i));
        auto rootIt = rootToNet.find(root);
        if (rootIt == rootToNet.end())
        {
            rootIt = rootToNet.emplace(root, nextNetId++).first;
        }

        const int netId = rootIt->second;
        const long long key = indexToKey[i];
        m_pinToNet[key] = netId;

        const int componentId = KeyToComponentId(key);
        const auto dirIt = keyToDirection.find(key);
        const PinDirection direction =
            (dirIt != keyToDirection.end()) ? dirIt->second : PinDirection::Input;

        // 列表顺序即引脚 index 升序（登记时按 BuildPins 顺序遍历），
        // 保证 Evaluate 拿到的 inputs[0]/inputs[1] 与门的 in0/in1 对应
        if (direction == PinDirection::Input || direction == PinDirection::Bidi)
        {
            m_compInputNets[componentId].push_back(netId);
        }
        if (direction == PinDirection::Output || direction == PinDirection::Bidi)
        {
            m_compOutputNets[componentId].push_back(netId);
        }
    }

    // 连线到网络的映射：取起点引脚所在网络即可（两端同网）
    for (const auto& wirePtr : m_circuit->GetWires())
    {
        if (!wirePtr)
        {
            continue;
        }
        const PinRef& from = wirePtr->GetFrom();
        const auto it = m_pinToNet.find(MakePinKey(from.componentId, from.pinIndex));
        if (it != m_pinToNet.end())
        {
            m_wireToNet[wirePtr->GetId()] = it->second;
        }
    }

    m_topologyDirty = false;
}

LogicLevel Simulator::CombineDrivers(int netId, const std::vector<LogicLevel>& drivers)
{
    bool hasHigh = false;
    bool hasLow = false;

    for (LogicLevel level : drivers)
    {
        if (level == LogicLevel::High)
        {
            hasHigh = true;
        }
        else if (level == LogicLevel::Low)
        {
            hasLow = true;
        }
    }

    if (hasHigh && hasLow)
    {
        // 门输出短接：电平无定义，记入冲突表供界面提示
        m_conflictedNets.push_back(netId);
        return LogicLevel::Unknown;
    }
    if (hasHigh)
    {
        return LogicLevel::High;
    }
    if (hasLow)
    {
        return LogicLevel::Low;
    }
    return LogicLevel::Unknown;
}

void Simulator::Evaluate()
{
    if (m_circuit == nullptr)
    {
        return;
    }
    if (m_topologyDirty)
    {
        RebuildTopology();
    }

    m_netValues.clear();
    m_conflictedNets.clear();
    m_oscillation = false;

    // 预先收集参与仿真的元件，避免每轮都重新遍历容器并重复判断类型
    std::vector<const Component*> comps;
    comps.reserve(m_circuit->GetComponents().size());
    for (const auto& compPtr : m_circuit->GetComponents())
    {
        if (compPtr && compPtr->ParticipatesInSimulation())
        {
            comps.push_back(compPtr.get());
        }
    }

    for (int iteration = 0; iteration < kMaxIterations; ++iteration)
    {
        // 冲突只反映「当前这一轮」的状态，故每轮重建
        m_conflictedNets.clear();

        std::map<int, std::vector<LogicLevel>> drivers;

        for (const Component* comp : comps)
        {
            std::vector<LogicLevel> inputs;
            const auto inputIt = m_compInputNets.find(comp->GetId());
            if (inputIt != m_compInputNets.end())
            {
                inputs.reserve(inputIt->second.size());
                for (int netId : inputIt->second)
                {
                    const auto valueIt = m_netValues.find(netId);
                    inputs.push_back(valueIt != m_netValues.end() ? valueIt->second
                                                                  : LogicLevel::Unknown);
                }
            }

            const LogicLevel out = comp->Evaluate(inputs);
            if (out == LogicLevel::Unknown)
            {
                // Unknown 表示「本次不参与传播」（时钟、触发器、未驱动等），
                // 必须跳过写入而不是写 Unknown，否则会把同网络其他有效驱动冲掉
                continue;
            }

            const auto outputIt = m_compOutputNets.find(comp->GetId());
            if (outputIt == m_compOutputNets.end())
            {
                continue;
            }
            for (int netId : outputIt->second)
            {
                drivers[netId].push_back(out);
            }
        }

        bool changed = false;

        for (const auto& entry : drivers)
        {
            const LogicLevel merged = CombineDrivers(entry.first, entry.second);
            const auto valueIt = m_netValues.find(entry.first);
            if (valueIt == m_netValues.end())
            {
                m_netValues[entry.first] = merged;
                changed = true;
            }
            else if (valueIt->second != merged)
            {
                valueIt->second = merged;
                changed = true;
            }
        }

        // 上一轮有值、本轮已无人驱动（例如输入引脚被切成未知、或驱动源被删），
        // 必须回落为 Unknown，否则会残留陈旧电平
        for (auto& entry : m_netValues)
        {
            if (drivers.find(entry.first) == drivers.end() && entry.second != LogicLevel::Unknown)
            {
                entry.second = LogicLevel::Unknown;
                changed = true;
            }
        }

        if (!changed)
        {
            // 整轮无变化：已收敛
            return;
        }
    }

    // 跑满轮数仍未收敛，典型如门输出经连线直接回到自身输入
    m_oscillation = true;
}

LogicLevel Simulator::GetNetLevel(int netId) const
{
    const auto it = m_netValues.find(netId);
    return (it != m_netValues.end()) ? it->second : LogicLevel::Unknown;
}

LogicLevel Simulator::GetPinLevel(int componentId, int pinIndex) const
{
    const auto it = m_pinToNet.find(MakePinKey(componentId, pinIndex));
    if (it == m_pinToNet.end())
    {
        return LogicLevel::Unknown;
    }
    return GetNetLevel(it->second);
}

LogicLevel Simulator::GetWireLevel(int wireId) const
{
    const auto it = m_wireToNet.find(wireId);
    if (it == m_wireToNet.end())
    {
        return LogicLevel::Unknown;
    }
    return GetNetLevel(it->second);
}

std::vector<LogicLevel> Simulator::GetComponentInputs(int componentId) const
{
    std::vector<LogicLevel> result;
    const auto it = m_compInputNets.find(componentId);
    if (it == m_compInputNets.end())
    {
        return result;
    }

    result.reserve(it->second.size());
    for (int netId : it->second)
    {
        result.push_back(GetNetLevel(netId));
    }
    return result;
}
} // namespace sim
