#pragma once

#include <map>
#include <vector>

#include "../model/LogicLevel.h"

class Circuit;

/// 组合逻辑仿真层。
/// 只持有 const Circuit* 做只读消费，不修改模型、不依赖视图与界面，
/// 因此不会形成 model ← sim 的反向依赖。
namespace sim
{
/// 组合逻辑仿真器。
/// 使用方式：SetCircuit() → Evaluate() → 用 GetNetLevel / GetPinLevel / GetWireLevel 读数。
///
/// 求值采用「迭代松弛」而非拓扑排序：
/// - 电路规模只有数十元件，一轮全量迭代的代价可以忽略；
/// - 用户点击输入引脚后需要立即反馈，全量重算最简单直接；
/// - 用户可能连出反馈环（时序元件本次虽不参与传播，但仍可能被连成环），
///   迭代松弛用「轮数上限 + 整轮无变化即收敛」天然兜住环，不必预先检测。
class Simulator
{
public:
    /// 迭代轮数上限。超出仍不收敛即视为振荡（典型为门输出直连自己的输入）。
    static constexpr int kMaxIterations = 64;

    /// @param circuit 被仿真的电路；可为 nullptr，此时 Evaluate 为空操作
    explicit Simulator(const Circuit* circuit = nullptr);

    /// 更换被仿真电路并清空全部索引。
    /// @param circuit 新电路指针（调用方保证其生命周期长于本对象）
    void SetCircuit(const Circuit* circuit);

    /// 标记拓扑已变化（增删元件/连线后必须调用），下次 Evaluate 前重建索引。
    void InvalidateTopology();

    /// 执行一次完整求值；拓扑脏时先重建索引。结果写入内部网络电平表。
    void Evaluate();

    /// @param netId 网络 id
    /// @return 该网络当前电平；网络不存在时返回 Unknown
    LogicLevel GetNetLevel(int netId) const;

    /// @param componentId 元件 id
    /// @param pinIndex 引脚序号
    /// @return 该引脚所在网络的电平；引脚未连接或不存在时返回 Unknown
    LogicLevel GetPinLevel(int componentId, int pinIndex) const;

    /// @param wireId 连线 id
    /// @return 该连线所在网络的电平；未连接时返回 Unknown
    LogicLevel GetWireLevel(int wireId) const;

    /// @param componentId 元件 id
    /// @return 该元件全部输入引脚的电平（按引脚 index 升序，不含输出引脚）
    std::vector<LogicLevel> GetComponentInputs(int componentId) const;

    /// @return 上一次 Evaluate 是否因达到轮数上限而未收敛
    bool HasOscillation() const { return m_oscillation; }

    /// @return 上一次 Evaluate 中发生「多驱动冲突」的网络 id 列表
    const std::vector<int>& GetConflictedNets() const { return m_conflictedNets; }

private:
    /// 重建网络索引：用并查集按连线把引脚划分成网络。
    void RebuildTopology();

    /// 合并同一网络的多个驱动值。
    /// 全无有效值 → Unknown；存在不同有效值 → Unknown 并记入冲突列表；
    /// 否则取该有效值。这样「两个 VCC 接同一网」无害，而「门输出短接」会被提示。
    /// @param netId 网络 id
    /// @param drivers 该网络的候选驱动值
    /// @return 合并后的电平
    LogicLevel CombineDrivers(int netId, const std::vector<LogicLevel>& drivers);

    const Circuit* m_circuit = nullptr; ///< 被仿真电路（只读）
    bool m_topologyDirty = true;        ///< 拓扑是否需要重建
    bool m_oscillation = false;         ///< 上次求值是否未收敛

    std::map<long long, int> m_pinToNet;                        ///< (元件 id, 引脚序号) → 网络 id
    std::map<int, int> m_wireToNet;                             ///< 连线 id → 网络 id
    std::map<int, std::vector<int>> m_compInputNets;            ///< 元件 id → 输入引脚网络列表
    std::map<int, std::vector<int>> m_compOutputNets;           ///< 元件 id → 输出引脚网络列表
    std::map<int, LogicLevel> m_netValues;                      ///< 网络 id → 当前电平
    std::vector<int> m_conflictedNets;                          ///< 上次求值的冲突网络
};
} // namespace sim
