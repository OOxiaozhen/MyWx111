#pragma once

#include <functional>
#include <utility>
#include <vector>

#include <wx/dcbuffer.h>
#include <wx/scrolwin.h>

#include "../model/Circuit.h"
#include "../sim/Simulator.h"
#include "../ui/ToolMode.h"
#include "ComponentRenderer.h"
#include "GridTransform.h"

/// 电路绘图画布。
/// 继承 wxScrolledWindow 但**不启用原生滚动**（SetScrollRate(0, 0)），
/// 平移改由 GridTransform 的 pan 偏移实现；这样滚轮事件才能完整交给缩放逻辑，
/// 不会被 wx 滚动条吞掉。
class CircuitCanvas : public wxScrolledWindow
{
public:
    /// @param parent 父窗口
    /// @param circuit 电路文档模型（引用，所有权在 MainFrame）
    CircuitCanvas(wxWindow* parent, Circuit& circuit);

    /// @return 无额外资源需要释放
    ~CircuitCanvas() override = default;

    // ── 工具控制 ──
    /// 切换交互模式，并中止所有进行中的交互。
    /// @param mode 目标模式
    /// @param type Place 模式下要放置的元件类型（其它模式忽略）
    void SetToolMode(ToolMode mode, ComponentType type = ComponentType::AndGate);

    /// @return 当前交互模式
    ToolMode GetToolMode() const { return m_tool.mode; }

    /// @return Place 模式下待放置的元件类型
    ComponentType GetPendingType() const { return m_tool.pendingType; }

    // ── 视图控制（供菜单与工具栏调用）──
    /// 以客户区中心放大 1.25 倍。
    void ZoomIn();

    /// 以客户区中心缩小到 0.8 倍。
    void ZoomOut();

    /// 恢复 100% 缩放并把视图复位到初始位置。
    void ZoomReset();

    /// @param visible 是否显示网格背景
    void SetGridVisible(bool visible);

    /// @return 当前是否显示网格
    bool IsGridVisible() const { return m_gridVisible; }

    /// @return 当前缩放比例
    double GetZoom() const { return m_transform.GetZoom(); }

    /// @return 当前网格间距（世界单位），供粘贴偏移等需要对齐网格的操作使用
    int GetGridStep() const;

    /// 缩放并居中，使全部元件恰好落在客户区内（四周留边）。
    /// 空电路时保持当前视图不变。
    void ZoomFit();

    // ── 编辑操作（供菜单调用）──
    /// 删除全部选中元件及其连线。
    void DeleteSelection();

    /// 全选所有元件。
    void SelectAll();

    /// 把全部选中元件顺时针旋转 90 度。
    void RotateSelection();

    // ── 仿真控制 ──
    /// 开关组合逻辑仿真。
    /// 开启时：单击输入引脚切换高低电平、连线按电平着色；
    /// 关闭时：所有交互与配色回到编辑态，与未接入仿真前完全一致。
    /// @param enabled 是否开启
    void SetSimulationEnabled(bool enabled);

    /// @return 仿真是否开启
    bool IsSimulationEnabled() const { return m_simEnabled; }

    /// 重新求值并重绘（拓扑或输入电平变化后调用）。
    void RefreshSimulation();

    /// @return 仿真器只读引用（供状态栏等服务读取电平）
    const sim::Simulator& GetSimulator() const { return m_simulator; }

    // ── 外部回调 ──
    /// 鼠标移动或缩放变化时回调：(世界 X, 世界 Y, 缩放比例)
    std::function<void(int, int, double)> OnStatusUpdate;

    /// 选中集合变化时回调，参数为主选中元件 id（无选中传 -1）
    std::function<void(int)> OnSelectionChanged;

    /// 操作提示回调，文本推送至状态栏
    std::function<void(const wxString&)> OnHint;

private:
    // ── 事件处理 ──
    void OnPaint(wxPaintEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);
    void OnMiddleDown(wxMouseEvent& event);
    void OnMiddleUp(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);
    void OnLeaveWindow(wxMouseEvent& event);

    // ── 内部辅助 ──
    /// 把当前鼠标世界坐标与缩放比例推送到状态栏。
    /// @param screenPt 鼠标屏幕坐标
    void ReportStatus(const wxPoint& screenPt);

    /// 在吸附后的网格点放置一个待放置类型的元件。
    /// @param screenPt 鼠标屏幕坐标
    void PlaceComponentAt(const wxPoint& screenPt);

    /// 结束拖动并提交位置变更。
    void EndDrag();

    /// 结束框选并按矩形范围选中元件。
    void EndRubberBand();

    /// 结束连线并尝试建立连线。
    /// @param screenPt 松开左键时的屏幕坐标
    /// @param toPin 终点引脚；不合法时给出提示并保持走线状态
    void EndWire(const PinRef& toPin);

    /// 重绘并把状态同步到状态栏。
    void RefreshAndReport();

    /// 通知外部选中状态已变化。
    void NotifySelectionChanged();

    Circuit& m_circuit;           ///< 电路模型引用
    GridTransform m_transform;    ///< 坐标变换
    ComponentRenderer m_renderer;  ///< 绘制策略
    ToolContext m_tool;           ///< 当前工具上下文
    bool m_gridVisible = true;    ///< 是否显示网格背景
    sim::Simulator m_simulator;   ///< 组合逻辑仿真器
    bool m_simEnabled = false;    ///< 是否处于仿真模式

    // 拖动状态
    bool m_dragging = false;                           ///< 是否正在拖动选中元件
    wxPoint m_dragStartWorld;                          ///< 拖动起点（吸附后的世界坐标）
    std::vector<std::pair<int, wxPoint>> m_dragOrigin;  ///< 拖动开始时各选中元件的原始位置

    // 框选状态
    bool m_rubberBanding = false;  ///< 是否正在框选
    wxPoint m_rubberStartWorld;    ///< 框选起点（世界坐标）
    wxPoint m_rubberEndWorld;      ///< 框选终点（世界坐标）

    // 连线状态
    bool m_wiring = false;         ///< 是否正在拉线
    PinRef m_wireFrom;             ///< 拉线起点引脚
    wxPoint m_wireCursorWorld;     ///< 拉线时鼠标当前世界坐标
    std::vector<wxPoint> m_wireWaypoints; ///< 多击式走线中已由点击确认的折点（世界坐标）

    // 平移状态
    bool m_spaceDown = false;      ///< 空格键是否按下
    bool m_panning = false;        ///< 是否正在平移
    wxPoint m_panLastScreen;       ///< 上一次平移时的屏幕坐标

    // 光标与初始布局状态
    bool m_hasCursorWorld = false;  ///< 是否已记录过鼠标世界坐标（用于放置预览）
    wxPoint m_cursorWorld;          ///< 鼠标当前世界坐标
    bool m_initialized = false;     ///< 是否已做过首次视图初始化
};
