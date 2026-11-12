#include "CircuitCanvas.h"

#include <algorithm>
#include <memory>

CircuitCanvas::CircuitCanvas(wxWindow* parent, Circuit& circuit)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxBORDER_NONE | wxWANTS_CHARS)
    , m_circuit(circuit)
{
    // 不使用原生滚动条：平移由 GridTransform 的 pan 偏移实现，
    // 这样滚轮事件才能完整交给缩放逻辑，不会被 wx 的滚动处理吞掉
    SetScrollRate(0, 0);

    // 仿真器只持引用，构造时绑定一次即可
    m_simulator.SetCircuit(&m_circuit);

    // 双缓冲必需：否则拖动元件时会出现严重闪烁
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxColour(250, 250, 252));

    Bind(wxEVT_PAINT, &CircuitCanvas::OnPaint, this);
    Bind(wxEVT_ERASE_BACKGROUND, &CircuitCanvas::OnEraseBackground, this);
    Bind(wxEVT_SIZE, &CircuitCanvas::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &CircuitCanvas::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &CircuitCanvas::OnLeftUp, this);
    Bind(wxEVT_MOTION, &CircuitCanvas::OnMotion, this);
    Bind(wxEVT_MIDDLE_DOWN, &CircuitCanvas::OnMiddleDown, this);
    Bind(wxEVT_MIDDLE_UP, &CircuitCanvas::OnMiddleUp, this);
    Bind(wxEVT_MOUSEWHEEL, &CircuitCanvas::OnMouseWheel, this);
    Bind(wxEVT_KEY_DOWN, &CircuitCanvas::OnKeyDown, this);
    Bind(wxEVT_KEY_UP, &CircuitCanvas::OnKeyUp, this);
    Bind(wxEVT_LEAVE_WINDOW, &CircuitCanvas::OnLeaveWindow, this);

    SetFocus();
}

// ───────────────────────── 绘制 ─────────────────────────

void CircuitCanvas::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(wxColour(250, 250, 252)));
    dc.Clear();

    const wxSize clientSize = GetClientSize();

    // 1) 网格用普通 DC 绘制：1 像素细线不需要抗锯齿，比 GraphicsContext 快一个数量级
    if (m_gridVisible)
    {
        m_transform.DrawGrid(dc, clientSize);
    }

    // 2) 元件与连线用 GraphicsContext 抗锯齿绘制。
    //    必须在 DC 直接绘制之后创建、并在函数结束前析构，
    //    否则内部缓冲未 flush，DC 上会出现画面残缺
    const std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc)
    {
        return;
    }
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

    // 连线先画、元件后画：让引脚端点压在连线之上，视觉层次更清晰
    for (const auto& wire : m_circuit.GetWires())
    {
        // 传入选中态：选中连线会变蓝加粗；仿真开启时再补一个电平，
        // 由渲染器决定是否改用电平配色（选中态优先级更高）
        m_renderer.DrawWire(*gc, *wire, m_circuit, m_transform,
                            m_circuit.IsWireSelected(wire->GetId()),
                            m_simEnabled ? m_simulator.GetWireLevel(wire->GetId())
                                         : LogicLevel::Unknown,
                            m_simEnabled);
    }
    for (const auto& comp : m_circuit.GetComponents())
    {
        // 仿真开启时取该元件首个引脚所在网络电平，供 LED / 逻辑探针着色与显示文字；
        // 其它元件类型忽略这两个参数
        const LogicLevel pinLevel =
            m_simEnabled ? m_simulator.GetPinLevel(comp->GetId(), 0) : LogicLevel::Unknown;
        m_renderer.DrawComponent(*gc, *comp, m_transform, m_circuit.IsSelected(comp->GetId()),
                                 pinLevel, m_simEnabled);
    }

    // 3) 临时态（放置预览、拉线、框选）画在最上层
    if (m_tool.mode == ToolMode::Place && m_hasCursorWorld)
    {
        m_renderer.DrawPlacementPreview(*gc, m_tool.pendingType,
                                        m_transform.SnapToGrid(m_cursorWorld), m_transform);
    }

    if (m_wiring)
    {
        if (const Component* fromComp = m_circuit.FindComponent(m_wireFrom.componentId))
        {
            const wxPoint fromWorld = fromComp->GetPinWorldPos(m_wireFrom.pinIndex);
            // 预览线需要起点引脚朝向，才能与落线后的正交走线保持一致
            const PinSide fromSide = fromComp->GetPinSide(m_wireFrom.pinIndex);
            m_renderer.DrawPendingWire(*gc, fromWorld, fromSide, m_wireWaypoints,
                                       m_wireCursorWorld, m_transform);
        }
    }

    if (m_rubberBanding)
    {
        m_renderer.DrawSelectionRect(*gc, wxRect(m_rubberStartWorld, m_rubberEndWorld), m_transform);
    }
}

void CircuitCanvas::OnEraseBackground(wxEraseEvent&)
{
    // 空实现且不调用 Skip()：背景已由双缓冲 OnPaint 完整覆盖，避免重绘时闪白
}

void CircuitCanvas::OnSize(wxSizeEvent& event)
{
    event.Skip();

    // 只在首次拿到有效尺寸时设置初始视图：后续用户平移过就不再重置，
    // 否则窗口一缩放视图就被拽回原点
    if (!m_initialized && GetClientSize().x > 0)
    {
        m_transform.SetPanOffset(wxPoint(120, 80));
        m_initialized = true;
    }
    Refresh();
}

// ───────────────────────── 鼠标事件 ─────────────────────────

void CircuitCanvas::OnLeftDown(wxMouseEvent& event)
{
    // 必须先把焦点拿回来，否则后续的键盘快捷键会被其它控件接收
    SetFocus();

    const wxPoint screenPt = event.GetPosition();
    const wxPoint world = m_transform.ScreenToWorld(screenPt);
    m_cursorWorld = world;
    m_hasCursorWorld = true;

    // 平移是「全局覆盖」操作，必须最先判断，
    // 否则在放置 / 连线模式下无法用空格 + 左键拖动平移画布
    if (m_spaceDown || m_tool.mode == ToolMode::Pan)
    {
        m_panning = true;
        m_panLastScreen = screenPt;
        SetCursor(wxCursor(wxCURSOR_SIZING));
        return;
    }

    switch (m_tool.mode)
    {
    case ToolMode::Select:
    {
        // 仿真模式下单击输入引脚直接切换高低电平；其余位置仍走原有选中/拖动逻辑
        if (m_simEnabled)
        {
            const PinRef pin = m_circuit.HitTestPin(world);
            if (pin.IsValid() && m_circuit.ToggleInputLevel(pin.componentId))
            {
                RefreshSimulation();
                return;
            }
        }

        Component* hit = m_circuit.HitTestComponent(world);
        if (hit != nullptr)
        {
            if (!m_circuit.IsSelected(hit->GetId()))
            {
                m_circuit.SelectOnly(hit->GetId());
            }
            else if (event.ShiftDown())
            {
                m_circuit.ToggleSelect(hit->GetId());
            }

            m_dragging = true;
            m_dragStartWorld = m_transform.SnapToGrid(world);
            m_dragOrigin.clear();
            for (int id : m_circuit.GetSelection())
            {
                if (const Component* comp = m_circuit.FindComponent(id))
                {
                    m_dragOrigin.emplace_back(id, comp->GetPosition());
                }
            }
            NotifySelectionChanged();
        }
        else if (const int wireId = m_circuit.HitTestWire(world); wireId >= 0)
        {
            // 命中连线：单选或 Shift 追加；连线不支持拖动，故不置 m_dragging
            if (event.ShiftDown())
            {
                m_circuit.ToggleWireSelect(wireId);
            }
            else
            {
                m_circuit.SelectOnlyWire(wireId);
            }
            NotifySelectionChanged();
        }
        else
        {
            m_circuit.ClearSelection();
            m_rubberBanding = true;
            m_rubberStartWorld = world;
            m_rubberEndWorld = world;
            NotifySelectionChanged();
        }
        break;
    }
    case ToolMode::Place:
    {
        PlaceComponentAt(screenPt);
        break;
    }
    case ToolMode::Wire:
    {
        if (!m_wiring)
        {
            const PinRef pin = m_circuit.HitTestPin(world);
            if (pin.IsValid())
            {
                m_wiring = true;
                m_wireFrom = pin;
                m_wireCursorWorld = world;
                m_wireWaypoints.clear();
            }
            else if (OnHint)
            {
                OnHint(wxString(L"请点击引脚端点开始连线"));
            }
        }
        else
        {
            // 已在走线中：命中有效终点引脚就完成连线，否则把当前点确认为一段后继续走
            const PinRef toPin = m_circuit.HitTestPin(world);
            if (toPin.IsValid())
            {
                EndWire(toPin);
            }
            else
            {
                const wxPoint snapped = m_transform.SnapToGrid(world);
                // 重复点击同一点不再累积折点，避免入库一串重复坐标
                if (m_wireWaypoints.empty() || !(m_wireWaypoints.back() == snapped))
                {
                    m_wireWaypoints.push_back(snapped);
                }
            }
        }
        break;
    }
    case ToolMode::Pan:
    {
        m_panning = true;
        m_panLastScreen = screenPt;
        break;
    }
    default:
        break;
    }

    Refresh();
}

void CircuitCanvas::OnMotion(wxMouseEvent& event)
{
    const wxPoint screenPt = event.GetPosition();
    m_cursorWorld = m_transform.ScreenToWorld(screenPt);
    m_hasCursorWorld = true;
    ReportStatus(screenPt);

    if (m_panning)
    {
        m_transform.PanBy(wxSize(screenPt.x - m_panLastScreen.x,
                                 screenPt.y - m_panLastScreen.y));
        m_panLastScreen = screenPt;
        Refresh();
        return;
    }

    if (m_dragging)
    {
        // 用「整体增量吸附」而不是「逐个元件吸附」：
        // 多选拖动时只有共享同一个位移增量，元件之间的相对位置才不会漂移
        const wxPoint delta = m_transform.SnapToGrid(m_cursorWorld) - m_dragStartWorld;
        for (const auto& item : m_dragOrigin)
        {
            m_circuit.SetComponentPositionSilent(item.first, item.second + delta);
        }
        Refresh();
        return;
    }

    if (m_rubberBanding)
    {
        m_rubberEndWorld = m_cursorWorld;
        Refresh();
        return;
    }

    if (m_wiring)
    {
        m_wireCursorWorld = m_cursorWorld;
        Refresh();
        return;
    }

    if (m_tool.mode == ToolMode::Place)
    {
        // 放置模式下预览符号需要跟随光标
        Refresh();
    }
}

void CircuitCanvas::OnLeftUp(wxMouseEvent& event)
{
    const wxPoint world = m_transform.ScreenToWorld(event.GetPosition());

    // 收尾顺序与按下时的优先级相反：先结束最外层（平移），再逐层向内
    if (m_panning)
    {
        m_panning = false;
        SetCursor(wxCursor(wxCURSOR_ARROW));
        return;
    }

    // 走线由左键点击分次确认，抬起事件不结束连线，因此这里不再处理 m_wiring
    if (m_rubberBanding)
    {
        m_rubberEndWorld = world;
        EndRubberBand();
        return;
    }

    if (m_dragging)
    {
        EndDrag();
    }
}

void CircuitCanvas::OnMiddleDown(wxMouseEvent& event)
{
    m_panning = true;
    m_panLastScreen = event.GetPosition();
    SetCursor(wxCursor(wxCURSOR_SIZING));
    SetFocus();
}

void CircuitCanvas::OnMiddleUp(wxMouseEvent&)
{
    m_panning = false;
    SetCursor(wxCursor(wxCURSOR_ARROW));
}

void CircuitCanvas::OnMouseWheel(wxMouseEvent& event)
{
    const double factor = (event.GetWheelRotation() > 0) ? 1.15 : (1.0 / 1.15);
    if (m_transform.ZoomAt(event.GetPosition(), factor))
    {
        RefreshAndReport();
    }
}

void CircuitCanvas::OnKeyDown(wxKeyEvent& event)
{
    const int code = event.GetKeyCode();

    if (event.ControlDown() && (code == 'A' || code == 'a'))
    {
        SelectAll();
        return;
    }

    // 其余带修饰键的组合交给菜单加速键处理，
    // 否则 Ctrl+X/C/V/R 会被画布吞掉而菜单永远收不到
    if (event.ControlDown() || event.AltDown())
    {
        event.Skip();
        return;
    }

    switch (code)
    {
    case WXK_DELETE:
        DeleteSelection();
        return;
    case 'R':
    case 'r':
        RotateSelection();
        return;
    case WXK_ESCAPE:
        SetToolMode(ToolMode::Select);
        if (OnHint)
        {
            OnHint(wxString(L"已切回选择模式"));
        }
        return;
    case WXK_SPACE:
        m_spaceDown = true;
        SetCursor(wxCursor(wxCURSOR_HAND));
        return;
    default:
        break;
    }

    // 刻意不调用 event.Skip()：否则 Delete 会被 wx 的默认处理再执行一次，
    // 方向键也会触发父窗口滚动，导致元件被意外删除或视图跳动
}

void CircuitCanvas::OnKeyUp(wxKeyEvent& event)
{
    if (event.GetKeyCode() == WXK_SPACE)
    {
        m_spaceDown = false;
        if (!m_panning)
        {
            SetCursor(wxCursor(wxCURSOR_ARROW));
        }
        return;
    }
    event.Skip();
}

void CircuitCanvas::OnLeaveWindow(wxMouseEvent& event)
{
    // 光标离开画布后不再显示放置预览，避免预览符号"粘"在最后一个位置
    m_hasCursorWorld = false;
    Refresh();
    event.Skip();
}

// ───────────────────────── 内部辅助 ─────────────────────────

void CircuitCanvas::ReportStatus(const wxPoint& screenPt)
{
    if (!OnStatusUpdate)
    {
        return;
    }
    const wxPoint world = m_transform.ScreenToWorld(screenPt);
    OnStatusUpdate(world.x, world.y, m_transform.GetZoom());
}

void CircuitCanvas::PlaceComponentAt(const wxPoint& screenPt)
{
    const wxPoint world = m_transform.SnapToGrid(m_transform.ScreenToWorld(screenPt));
    Component* created = m_circuit.AddComponent(m_tool.pendingType, world);
    if (created == nullptr)
    {
        return;
    }

    // 新增元件改变了网络划分，下次求值前必须重建索引
    m_simulator.InvalidateTopology();

    if (OnHint)
    {
        // 放置后保持 Place 模式，支持连续放置同类元件；按 Esc 退出
        OnHint(wxString::Format(L"已放置 %s，可继续点击放置，按 Esc 退出放置模式",
                                GetComponentDisplayName(m_tool.pendingType)));
    }
    NotifySelectionChanged();
    Refresh();
}

void CircuitCanvas::EndDrag()
{
    m_dragging = false;
    m_dragOrigin.clear();

    // 拖动过程中只改数据、不发通知（避免属性面板每帧刷新导致输入框抖动），
    // 松手时才统一提交一次变更：既要刷新面板显示最终坐标，也要让撤销栈记录一步
    m_circuit.NotifyChanged();
    m_circuit.NotifyStructureChanged();
    NotifySelectionChanged();
    Refresh();
}

void CircuitCanvas::EndRubberBand()
{
    m_rubberBanding = false;

    const wxRect rect(m_rubberStartWorld, m_rubberEndWorld);
    // 小于 2 像素的框视为误触，不做选中，避免单击空白处清空后又全选回来
    if (rect.width > 2 && rect.height > 2)
    {
        m_circuit.ClearSelection();
        for (const auto& comp : m_circuit.GetComponents())
        {
            if (rect.Intersects(comp->GetWorldBounds()))
            {
                m_circuit.ToggleSelect(comp->GetId());
            }
        }
    }

    m_circuit.NotifyChanged();
    NotifySelectionChanged();
    Refresh();
}

void CircuitCanvas::EndWire(const PinRef& toPin)
{
    // 连接规则（方向互补、自连、引脚占用）全部集中在 Circuit::CanConnect，
    // 画布只负责把中文错误文本展示到状态栏
    wxString error;
    if (!m_circuit.CanConnect(m_wireFrom, toPin, &error))
    {
        // 点到的引脚不合法（如把输入脚当起点）时只提示错误，保持走线状态：
        // 用户可能只是点偏了，不必因此打断整条走线
        if (OnHint)
        {
            OnHint(error);
        }
        Refresh();
        return;
    }

    m_circuit.AddWire(m_wireFrom, toPin, m_wireWaypoints);
    m_simulator.InvalidateTopology();
    if (OnHint)
    {
        OnHint(wxString(L"连线已创建"));
    }

    m_wiring = false;
    m_wireWaypoints.clear();
    Refresh();
}

void CircuitCanvas::RefreshAndReport()
{
    Refresh();
    ReportStatus(ScreenToClient(wxGetMousePosition()));
}

void CircuitCanvas::NotifySelectionChanged()
{
    if (OnSelectionChanged)
    {
        OnSelectionChanged(m_circuit.GetPrimarySelection());
    }
}

// ───────────────────────── 工具与视图控制 ─────────────────────────

void CircuitCanvas::SetToolMode(ToolMode mode, ComponentType type)
{
    m_tool.mode = mode;
    m_tool.pendingType = type;

    // 切换工具时中止所有进行中的交互，避免状态残留导致下一次点击出现意外行为
    m_dragging = false;
    m_rubberBanding = false;
    m_wiring = false;
    m_wireWaypoints.clear();
    m_panning = false;
    m_dragOrigin.clear();

    SetCursor(wxCursor(mode == ToolMode::Pan ? wxCURSOR_HAND : wxCURSOR_ARROW));
    Refresh();
}

void CircuitCanvas::ZoomIn()
{
    const wxSize size = GetClientSize();
    if (m_transform.ZoomAt(wxPoint(size.x / 2, size.y / 2), 1.25))
    {
        RefreshAndReport();
    }
}

void CircuitCanvas::ZoomOut()
{
    const wxSize size = GetClientSize();
    if (m_transform.ZoomAt(wxPoint(size.x / 2, size.y / 2), 1.0 / 1.25))
    {
        RefreshAndReport();
    }
}

void CircuitCanvas::ZoomReset()
{
    m_transform.SetZoom(1.0);
    m_transform.SetPanOffset(wxPoint(120, 80));
    RefreshAndReport();
}

int CircuitCanvas::GetGridStep() const
{
    return m_transform.GetGridSize();
}

void CircuitCanvas::ZoomFit()
{
    const auto& comps = m_circuit.GetComponents();
    if (comps.empty())
    {
        // 空电路不改变视图：没有可包围的内容，缩放到任何比例都没有依据
        return;
    }

    bool hasBounds = false;
    wxRect bounds;
    for (const auto& compPtr : comps)
    {
        if (!compPtr)
        {
            continue;
        }
        const wxRect world = compPtr->GetWorldBounds();
        if (!hasBounds)
        {
            bounds = world;
            hasBounds = true;
        }
        else
        {
            bounds.Union(world);
        }
    }

    if (!hasBounds || bounds.width <= 0 || bounds.height <= 0)
    {
        return;
    }

    const wxSize client = GetClientSize();
    if (client.x <= 0 || client.y <= 0)
    {
        return;
    }

    // 四周留出边距，避免元件正好贴在画布边缘而看不清轮廓
    const int margin = FromDIP(40) * 2;
    const int usableX = std::max(1, client.x - margin);
    const int usableY = std::max(1, client.y - margin);
    const double zoom = std::min(static_cast<double>(usableX) / bounds.width,
                                 static_cast<double>(usableY) / bounds.height);

    // SetZoom 内部已钳制到 [kMinZoom, kMaxZoom]，此处无需重复判断
    m_transform.SetZoom(zoom);
    m_transform.CenterOn(
        wxPoint(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2), client);
    RefreshAndReport();
}

void CircuitCanvas::SetGridVisible(bool visible)
{
    m_gridVisible = visible;
    Refresh();
}

void CircuitCanvas::DeleteSelection()
{
    m_circuit.DeleteSelected();

    // 删除会拆掉网络，索引必须失效重建
    m_simulator.InvalidateTopology();
    NotifySelectionChanged();
    RefreshSimulation();
}

void CircuitCanvas::SelectAll()
{
    m_circuit.SelectAll();
    NotifySelectionChanged();
    Refresh();
}

void CircuitCanvas::RotateSelection()
{
    m_circuit.RotateSelected();
    NotifySelectionChanged();
    Refresh();
}

void CircuitCanvas::SetSimulationEnabled(bool enabled)
{
    if (m_simEnabled == enabled)
    {
        return;
    }

    m_simEnabled = enabled;
    if (m_simEnabled)
    {
        // 开启时强制重建索引：编辑期间增删的元件与连线可能尚未登记
        m_simulator.InvalidateTopology();
        m_simulator.Evaluate();
    }
    else
    {
        // 关闭后丢弃电平结果，避免下次开启瞬间闪出上一次的陈旧配色
        m_simulator.InvalidateTopology();
    }

    Refresh();
}

void CircuitCanvas::RefreshSimulation()
{
    if (m_simEnabled)
    {
        m_simulator.Evaluate();
    }
    Refresh();
}
