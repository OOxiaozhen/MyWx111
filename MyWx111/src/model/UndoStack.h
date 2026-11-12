#pragma once

#include <wx/string.h>

#include <cstddef>
#include <vector>

/// 基于 JSON 全量快照的撤销/重做栈。
/// 只负责索引推进与分支截断，不负责「取快照」与「应用快照」——
/// 后者需要序列化与画布，由 MainFrame 编排，避免 model 层反向依赖 io 与 view。
class UndoStack
{
public:
    /// 快照栈最大深度；超出后丢弃最旧快照。
    static constexpr std::size_t kMaxDepth = 50;

    /// 重置为基线状态：清空历史，只保留一个基线快照，索引归零。
    /// @param initialSnapshot 基线快照（通常是程序启动时的空电路 JSON）
    void Reset(const wxString& initialSnapshot);

    /// 清空所有快照并令索引归零（用于加载工程文件后丢弃历史）。
    void Clear();

    /// 压入一份新快照。
    /// 与当前快照完全相同则丢弃（去重兜底）；压栈前截断当前索引之后的重做分支。
    /// @param snapshot 新的快照文本
    /// @return 真正入栈返回 true；被去重丢弃或处于应用态返回 false
    bool Push(const wxString& snapshot);

    /// @return 是否可撤销（索引不在栈底）
    bool CanUndo() const;

    /// @return 是否可重做（索引不在栈顶）
    bool CanRedo() const;

    /// 索引前移一格并返回目标快照。
    /// @return 目标快照指针；不可撤销时返回 nullptr
    /// @warning 返回指针指向栈内元素，仅在下一次 Push/Undo/Redo/Reset/Clear 前有效
    const wxString* Undo();

    /// 索引后移一格并返回目标快照。
    /// @return 目标快照指针；不可重做时返回 nullptr
    /// @warning 返回指针的有效性约束同 Undo()
    const wxString* Redo();

    /// @return 是否处于「正在应用快照」状态；此期间产生的结构变更不得入栈
    bool IsApplying() const { return m_applying; }

    /// 设置「正在应用快照」标记。
    /// @param applying true 表示正在回放快照，此时 Push 会被直接忽略
    void SetApplying(bool applying) { m_applying = applying; }

    /// @return 当前快照数量（含基线）
    std::size_t GetDepth() const { return m_snapshots.size(); }

    /// @return 当前快照索引（0 为基线）
    std::size_t GetIndex() const { return m_index; }

private:
    std::vector<wxString> m_snapshots; ///< 快照序列，[0] 为基线
    std::size_t m_index = 0;           ///< 当前快照下标
    bool m_applying = false;           ///< 应用快照期间的重入标记
};
