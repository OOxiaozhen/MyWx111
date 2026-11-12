#include "UndoStack.h"

void UndoStack::Reset(const wxString& initialSnapshot)
{
    m_snapshots.clear();
    m_snapshots.push_back(initialSnapshot);
    m_index = 0;
    m_applying = false;
}

void UndoStack::Clear()
{
    m_snapshots.clear();
    m_index = 0;
    m_applying = false;
}

bool UndoStack::Push(const wxString& snapshot)
{
    // 回放快照期间产生的变更不得入栈，否则会形成「撤销 → 通知 → 压栈」的循环
    if (m_applying)
    {
        return false;
    }

    if (m_snapshots.empty())
    {
        m_snapshots.push_back(snapshot);
        m_index = 0;
        return true;
    }

    // 去重兜底：内容未实际变化时不占用撤销步，
    // 可吸收「属性 setter 被调用但值其实没变」这类噪声
    if (m_snapshots[m_index] == snapshot)
    {
        return false;
    }

    // 截断重做分支：撤销之后一旦执行新操作，原有的重做链即失效
    m_snapshots.erase(m_snapshots.begin() + static_cast<std::ptrdiff_t>(m_index) + 1,
                      m_snapshots.end());
    m_snapshots.push_back(snapshot);
    ++m_index;

    // 超出容量时丢弃最旧快照，索引同步前移以继续指向同一份快照
    while (m_snapshots.size() > kMaxDepth)
    {
        m_snapshots.erase(m_snapshots.begin());
        if (m_index > 0)
        {
            --m_index;
        }
    }
    return true;
}

bool UndoStack::CanUndo() const
{
    return !m_snapshots.empty() && m_index > 0;
}

bool UndoStack::CanRedo() const
{
    return !m_snapshots.empty() && (m_index + 1) < m_snapshots.size();
}

const wxString* UndoStack::Undo()
{
    if (!CanUndo())
    {
        return nullptr;
    }

    --m_index;
    return &m_snapshots[m_index];
}

const wxString* UndoStack::Redo()
{
    if (!CanRedo())
    {
        return nullptr;
    }

    ++m_index;
    return &m_snapshots[m_index];
}
