#include "MainFrame.h"

#include <algorithm>

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include "../io/NetlistExporter.h"
#include "../io/GedaNetlistExporter.h"
#include "../io/PcbFileExporter.h"
#include "../io/ProjectSerializer.h"
#include "../model/Circuit.h"
#include "../ui/IconFactory.h"
#include "../ui/PropertiesPanel.h"
#include "../ui/ToolMode.h"
#include "../ui/ToolPalette.h"
#include "../view/CircuitCanvas.h"

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, wxString(L"MyWx111 EDA"),
              wxDefaultPosition, wxSize(1400, 900))
{
    // 最小尺寸防止三栏布局被压扁到控件不可用
    SetMinSize(wxSize(1024, 700));

    m_circuit = std::make_unique<Circuit>();

    CreateMenuBar();
    SetupToolBar();
    SetupStatusBar();
    CreateLayout();

    BindCanvasCallbacks();
    BindPaletteCallbacks();

    // 命令绑定：菜单项与工具条按钮共用同一批处理函数
    // 文件菜单的四个命令各自处理，不再共用一个占位函数
    Bind(wxEVT_MENU, &MainFrame::OnFileNew, this, ID_FileNew);
    Bind(wxEVT_MENU, &MainFrame::OnFileOpen, this, ID_FileOpen);
    Bind(wxEVT_MENU, &MainFrame::OnFileSave, this, ID_FileSave);
    Bind(wxEVT_MENU, &MainFrame::OnFileSaveAs, this, ID_FileSaveAs);
    // 导出网表必须单独绑定：若并入上面的区间绑定会落到「尚未实现」提示
    Bind(wxEVT_MENU, &MainFrame::OnFileExportNetlist, this, ID_FileExportNetlist);
    // 导出子菜单的两个新命令也需单独绑定，不能落入占位函数的区间绑定
    Bind(wxEVT_MENU, &MainFrame::OnExportGedaNetlist, this, ID_FileExportGedaNet);
    Bind(wxEVT_MENU, &MainFrame::OnExportPcb, this, ID_FileExportPcb);
    // 撤销/重做已独立实现，不能并入占位函数的区间绑定
    Bind(wxEVT_MENU, &MainFrame::OnUndo, this, ID_EditUndo);
    Bind(wxEVT_MENU, &MainFrame::OnRedo, this, ID_EditRedo);
    // 剪切/复制/粘贴已有独立实现，不再走占位函数
    Bind(wxEVT_MENU, &MainFrame::OnCut, this, ID_EditCut);
    Bind(wxEVT_MENU, &MainFrame::OnCopy, this, ID_EditCopy);
    Bind(wxEVT_MENU, &MainFrame::OnPaste, this, ID_EditPaste);
    Bind(wxEVT_MENU, &MainFrame::OnRotate, this, ID_EditRotate);
    Bind(wxEVT_MENU, &MainFrame::OnDelete, this, wxID_DELETE);
    Bind(wxEVT_MENU, &MainFrame::OnSelectAll, this, wxID_SELECTALL);
    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &MainFrame::OnZoomIn, this, ID_ViewZoomIn);
    Bind(wxEVT_MENU, &MainFrame::OnZoomOut, this, ID_ViewZoomOut);
    Bind(wxEVT_MENU, &MainFrame::OnZoomReset, this, ID_ViewZoomReset);
    Bind(wxEVT_MENU, &MainFrame::OnZoomFit, this, ID_ViewZoomFit);
    Bind(wxEVT_MENU, &MainFrame::OnToggleFullScreen, this, ID_ViewFullScreen);
    Bind(wxEVT_MENU, &MainFrame::OnShowShortcuts, this, ID_HelpShortcuts);
    Bind(wxEVT_MENU, &MainFrame::OnToggleSimulation, this, ID_ViewSimulation);
    Bind(wxEVT_MENU, &MainFrame::OnSimulationEvaluateOnce, this, ID_SimulationEvaluateOnce);
    Bind(wxEVT_MENU, &MainFrame::OnToggleGrid, this, ID_ViewToggleGrid);
    Bind(wxEVT_MENU, &MainFrame::OnSelectTool, this, ID_ToolSelect);
    Bind(wxEVT_MENU, &MainFrame::OnWireTool, this, ID_ToolWire);
    Bind(wxEVT_MENU, &MainFrame::OnProjectPlaceholder, this, ID_ProjectOptions, ID_ProjectAnalyze);

    // 工具条按钮触发的是 wxEVT_BUTTON；command 事件会向父窗口传播，
    // 因此只需在此按 id 集中绑定一次，不必为每个按钮单独写回调
    Bind(wxEVT_BUTTON, &MainFrame::OnSelectTool, this, ID_ToolSelect);
    Bind(wxEVT_BUTTON, &MainFrame::OnWireTool, this, ID_ToolWire);
    Bind(wxEVT_BUTTON, &MainFrame::OnPlaceNoteTool, this, ID_ToolPlaceNote);
    Bind(wxEVT_BUTTON, &MainFrame::OnZoomIn, this, ID_ViewZoomIn);
    Bind(wxEVT_BUTTON, &MainFrame::OnZoomOut, this, ID_ViewZoomOut);
    Bind(wxEVT_BUTTON, &MainFrame::OnToggleGrid, this, ID_ViewToggleGrid);
    Bind(wxEVT_BUTTON, &MainFrame::OnUndo, this, ID_EditUndo);
    Bind(wxEVT_BUTTON, &MainFrame::OnRedo, this, ID_EditRedo);
    Bind(wxEVT_BUTTON, &MainFrame::OnPaste, this, ID_EditPaste);
    Bind(wxEVT_BUTTON, &MainFrame::OnRotate, this, ID_EditRotate);

    m_auiManager.Update();
    UpdateToolStatus();

    // 以启动时的空电路作为撤销栈基线：此后每次结构性变更都会在它之上压栈
    wxString baseline;
    if (m_circuit && project::SerializeCircuitToJson(*m_circuit, baseline, nullptr))
    {
        m_undoStack.Reset(baseline);
    }
    UpdateUndoRedoState();
    UpdateEditActionsState();

    Centre();
}

MainFrame::~MainFrame()
{
    // AUI 管理器需要在宿主窗口销毁前解除绑定，否则会残留布局事件导致断言甚至崩溃
    m_auiManager.UnInit();
}

// ───────────────────────── 界面构建 ─────────────────────────

void MainFrame::CreateMenuBar()
{
    auto* fileMenu = new wxMenu();
    fileMenu->Append(ID_FileNew, wxString(L"新建(&N)\tCtrl+N"));
    fileMenu->Append(ID_FileOpen, wxString(L"打开(&O)…\tCtrl+O"));
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_FileSave, wxString(L"保存(&S)\tCtrl+S"));
    fileMenu->Append(ID_FileSaveAs, wxString(L"另存为(&A)…"));
    // 导出统一收进子菜单：gEDA 网表与 pcb 布局是任务书要求的交付物，
    // 原先的 KiCad 网表保留为第三种格式
    auto* exportMenu = new wxMenu();
    exportMenu->Append(ID_FileExportGedaNet, wxString(L"gEDA 网表(.net)…"));
    exportMenu->Append(ID_FileExportPcb, wxString(L"pcb 布局(.pcb)…"));
    exportMenu->AppendSeparator();
    exportMenu->Append(ID_FileExportNetlist, wxString(L"KiCad 网表(.net)…"));
    fileMenu->AppendSubMenu(exportMenu, wxString(L"导出(&E)…"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, wxString(L"退出(&X)\tAlt+F4"));

    m_editMenu = new wxMenu();
    m_editMenu->Append(ID_EditUndo, wxString(L"撤销(&U)\tCtrl+Z"));
    m_editMenu->Append(ID_EditRedo, wxString(L"重做(&R)\tCtrl+Y"));
    m_editMenu->AppendSeparator();
    m_editMenu->Append(ID_EditCut, wxString(L"剪切(&T)\tCtrl+X"));
    m_editMenu->Append(ID_EditCopy, wxString(L"复制(&C)\tCtrl+C"));
    m_editMenu->Append(ID_EditPaste, wxString(L"粘贴(&P)\tCtrl+V"));
    m_editMenu->Append(ID_EditRotate, wxString(L"旋转(&R)\tCtrl+R"));
    m_editMenu->AppendSeparator();
    m_editMenu->Append(wxID_DELETE, wxString(L"删除(&D)\tDel"));
    m_editMenu->Append(wxID_SELECTALL, wxString(L"全选(&A)\tCtrl+A"));

    m_viewMenu = new wxMenu();
    m_viewMenu->Append(ID_ViewZoomIn, wxString(L"放大(&I)\tCtrl++"));
    m_viewMenu->Append(ID_ViewZoomOut, wxString(L"缩小(&O)\tCtrl+-"));
    m_viewMenu->Append(ID_ViewZoomReset, wxString(L"实际大小(&R)\tCtrl+0"));
    m_viewMenu->Append(ID_ViewZoomFit, wxString(L"适合窗口(&F)\tF1"));
    m_viewMenu->AppendSeparator();
    m_viewMenu->AppendCheckItem(m_gridMenuItemId, wxString(L"显示网格(&G)"));
    m_viewMenu->AppendSeparator();
    m_viewMenu->AppendCheckItem(ID_ViewFullScreen, wxString(L"全屏(&L)\tF11"));
    // 默认与画布初始状态一致，避免菜单勾选态与实际显示不符
    m_viewMenu->Check(m_gridMenuItemId, true);

    // 仿真单独成一级菜单：「运行仿真」是状态开关，与纯视图操作语义不同
    m_simMenu = new wxMenu();
    m_simMenu->AppendCheckItem(ID_ViewSimulation, wxString(L"运行仿真(&S)\tF5"));
    // 画布默认处于仿真状态，菜单勾选态必须与之保持一致，
    // 否则首次取消勾选时会因状态错位而显得「点了没反应」
    m_simMenu->Check(ID_ViewSimulation, true);
    m_simMenu->AppendSeparator();
    m_simMenu->Append(ID_SimulationEvaluateOnce, wxString(L"重新求值(&R)\tF6"));

    auto* projectMenu = new wxMenu();
    projectMenu->Append(ID_ProjectOptions, wxString(L"项目选项(&O)…"));
    projectMenu->Append(ID_ProjectAnalyze, wxString(L"分析电路(&A)"));

    auto* helpMenu = new wxMenu();
    helpMenu->Append(ID_HelpShortcuts, wxString(L"快捷键参考(&K)…"));
    helpMenu->AppendSeparator();
    helpMenu->Append(wxID_ABOUT, wxString(L"关于(&A)…"));

    auto* menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, wxString(L"文件(&F)"));
    menuBar->Append(m_editMenu, wxString(L"编辑(&E)"));
    menuBar->Append(m_viewMenu, wxString(L"视图(&V)"));
    menuBar->Append(m_simMenu, wxString(L"仿真(&M)"));
    menuBar->Append(projectMenu, wxString(L"项目(&P)"));
    menuBar->Append(helpMenu, wxString(L"帮助(&H)"));
    SetMenuBar(menuBar);
}

void MainFrame::SetupToolBar()
{
    m_toolPanel = new wxPanel(this, wxID_ANY);
    auto* sizer = new wxBoxSizer(wxHORIZONTAL);

    // 结构体把「命令 id / 图标 / 可读名称 / 悬停提示」放在一起，
    // 增删按钮时只需改这张表，不必重复四处代码
    struct ToolButtonSpec
    {
        int id;              ///< 命令 id，与菜单项共用同一处理函数
        IconId icon;         ///< 图标标识，由 IconFactory 统一矢量绘制
        const wchar_t* name; ///< 供无障碍工具读取的按钮名称（图标按钮没有可视文字）
        const wchar_t* tip;  ///< 鼠标悬停提示
    };

    const ToolButtonSpec buttons[] = {
        { ID_ToolSelect, IconId::Select, L"选择工具", L"选择工具：单击选中，拖动移动，空白处拖拽框选" },
        { ID_ToolWire, IconId::Wire, L"连线工具", L"连线工具：从输出引脚拖到输入引脚" },
        { ID_ToolPlaceNote, IconId::Note, L"文本注释", L"添加文本注释：点击画布放置并输入文字" },
        { ID_EditRotate, IconId::Rotate, L"旋转", L"顺时针旋转选中元件 (Ctrl+R)" },
        { ID_EditUndo, IconId::Undo, L"撤销", L"撤销上一步操作 (Ctrl+Z)" },
        { ID_EditRedo, IconId::Redo, L"重做", L"重做被撤销的操作 (Ctrl+Y)" },
        { ID_EditPaste, IconId::Paste, L"粘贴", L"粘贴剪贴板内容 (Ctrl+V)" },
        { ID_ViewZoomIn, IconId::ZoomIn, L"放大视图", L"放大视图（也可用滚轮缩放）" },
        { ID_ViewZoomOut, IconId::ZoomOut, L"缩小视图", L"缩小视图" },
        { ID_ViewToggleGrid, IconId::Grid, L"显示网格", L"显示或隐藏网格背景" }
    };

    // 工具条高度受 AUI ToolbarPane 的 MinSize(36) 约束，
    // 故按钮按 28x28 设计并留出上下各 2px 间距，总高 32px 仍在限制内
    const wxSize buttonSize = FromDIP(wxSize(28, 28));
    const int iconSize = FromDIP(18);   // 图标略小于按钮，保留四周留白避免贴边

    sizer->AddSpacer(FromDIP(4));       // 与面板左缘留白，避免按钮紧贴窗框

    for (const ToolButtonSpec& spec : buttons)
    {
        auto* button = new wxBitmapButton(m_toolPanel, spec.id,
                                          IconFactory::GetIcon(spec.icon, iconSize),
                                          wxDefaultPosition, buttonSize, wxBORDER_NONE);
        button->SetName(wxString(spec.name));
        button->SetToolTip(wxString(spec.tip));
        // 保存按钮指针与图标标识：UpdateToolButtons 据此切换激活态配色
        m_toolButtons[spec.id] = button;
        m_toolButtonIcons[spec.id] = spec.icon;
        sizer->Add(button, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    }

    m_toolPanel->SetSizer(sizer);
}

void MainFrame::SetupStatusBar()
{
    // 注意：这里用 SetupStatusBar 而不是 CreateStatusBar，
    // 否则会隐藏 wxFrame::CreateStatusBar 的重载并造成调用歧义
    CreateStatusBar(3);

    const int widths[] = { 180, 140, -1 };
    SetStatusWidths(3, widths);
    SetStatusText(wxString(L"坐标: (0, 0)"), 0);
    SetStatusText(wxString(L"缩放: 100%"), 1);
    SetStatusText(wxString(L"就绪"), 2);
}

void MainFrame::CreateLayout()
{
    // AUI 必须直接托管在 frame 上（而不是某个子面板），否则停靠区域无法铺满客户区
    m_auiManager.SetManagedWindow(this);

    m_palette = new ToolPalette(this);
    m_canvas = new CircuitCanvas(this, *m_circuit);
    m_props = new PropertiesPanel(this, *m_circuit);

    // 工具条用 AUI 的 ToolbarPane 承载自绘面板：这样无需依赖 wxToolBar 的位图资源，
    // 静态链接环境下也不会因为图标为空而显示异常
    m_auiManager.AddPane(m_toolPanel,
        wxAuiPaneInfo().Name("toolbar").ToolbarPane().Top()
            .CaptionVisible(false).Floatable(false).CloseButton(false)
            .Gripper(false).DockFixed(true).PaneBorder(false)                    // 与画布一致，消除 AUI 为 pane 预留的边框间距
                .MinSize(wxSize(-1, FromDIP(32)))     // 贴合按钮 28 + 上下各 2，不再固定 36
                .BestSize(wxSize(-1, FromDIP(32))));

    // 中央画布：占据剩余空间，固定不可浮动、不可关闭
    m_auiManager.AddPane(m_canvas,
        wxAuiPaneInfo().Name("canvas").CenterPane().PaneBorder(false));

    m_auiManager.AddPane(m_palette,
        wxAuiPaneInfo().Name("palette").Caption(wxString(L"元件")).Left().Layer(1)
            .BestSize(wxSize(190, 600)).MinSize(wxSize(160, -1))
            .CloseButton(false).MaximizeButton(false).Floatable(true));

    m_auiManager.AddPane(m_props,
        wxAuiPaneInfo().Name("props").Caption(wxString(L"属性")).Right().Layer(1)
            .BestSize(wxSize(260, 600)).MinSize(wxSize(220, -1))
            .CloseButton(false).MaximizeButton(false).Floatable(true));

    m_auiManager.Update();
}

void MainFrame::BindCanvasCallbacks()
{
    m_canvas->OnStatusUpdate = [this](int x, int y, double zoom)
    {
        SetStatusText(wxString::Format(L"坐标: (%d, %d)", x, y), 0);
        SetStatusText(wxString::Format(L"缩放: %.0f%%", zoom * 100.0), 1);
    };

    m_canvas->OnSelectionChanged = [this](int /*componentId*/)
    {
        if (m_props)
        {
            m_props->RefreshFromCircuit();
        }
        // 选中集合变化会影响剪切/复制是否可用
        UpdateEditActionsState();
    };

    m_canvas->OnHint = [this](const wxString& text)
    {
        SetStatusText(text, 2);
    };

    // 模型的单一变更入口：任何结构性修改最终都汇聚到这里，
    // 由主窗口统一驱动画布重绘与属性面板刷新
    m_circuit->SetChangeHandler([this]()
    {
        if (m_canvas)
        {
            m_canvas->Refresh();
        }
        if (m_props)
        {
            m_props->RefreshFromCircuit();
        }
    });

    // 结构变更单独汇聚到撤销栈：选中变化只走 SetChangeHandler，
    // 因此单击元件不会占用撤销步
    m_circuit->SetStructureChangeHandler([this]() { PushUndoSnapshot(); });

    if (m_props)
    {
        m_props->SetOnChanged([this]()
        {
            if (m_canvas)
            {
                m_canvas->Refresh();
            }
        });

        // 物理芯片属性编辑：字段 → Circuit 对应 setter。
        // 这些 setter 内部会发结构变更通知，因此自动进撤销栈并刷新画布与面板
        m_props->OnFieldChanged = [this](int componentId, PropertiesPanel::Field field,
                                         const wxString& value)
        {
            if (!m_circuit)
            {
                return;
            }

            switch (field)
            {
            case PropertiesPanel::Field::RefDes:
                m_circuit->SetComponentRefDes(componentId, value);
                break;
            case PropertiesPanel::Field::Footprint:
                m_circuit->SetComponentFootprint(componentId, value);
                break;
            case PropertiesPanel::Field::Value:
                m_circuit->SetComponentValue(componentId, value);
                break;
            case PropertiesPanel::Field::Slot:
                m_circuit->SetComponentSlot(componentId, wxAtoi(value));
                break;
            default:
                break;
            }
        };
    }
}

void MainFrame::BindPaletteCallbacks()
{
    m_palette->OnComponentTypeSelected = [this](ComponentType type)
    {
        if (m_canvas)
        {
            m_canvas->SetToolMode(ToolMode::Place, type);
        }
        UpdateToolStatus();
    };

    m_palette->OnHint = [this](const wxString& text)
    {
        SetStatusText(text, 2);
    };
}

// ───────────────────────── 状态与提示 ─────────────────────────

void MainFrame::UpdateToolStatus()
{
    if (!m_canvas)
    {
        return;
    }

    wxString text;
    switch (m_canvas->GetToolMode())
    {
    case ToolMode::Select:
        text = wxString(L"选择模式：单击选中，拖动移动，空白处拖拽框选");
        break;
    case ToolMode::Place:
        text = wxString::Format(L"放置模式：%s（点击画布放置，按 Esc 退出）",
                                GetComponentDisplayName(m_canvas->GetPendingType()));
        break;
    case ToolMode::Wire:
        text = wxString(L"连线模式：从输出引脚拖到输入引脚");
        break;
    case ToolMode::PlaceNote:
        text = wxString(L"文本注释模式：点击画布放置注释并输入文字，按 Esc 退出");
        break;
    case ToolMode::Pan:
        text = wxString(L"平移模式：拖拽移动视图");
        break;
    default:
        text = wxString(L"就绪");
        break;
    }

    SetStatusText(text, 2);

    // 所有工具模式切换最终都会汇聚到这里，顺带同步工具条按钮的激活态，
    // 避免在每个命令处理函数里重复刷新按钮
    UpdateToolButtons();
}

void MainFrame::UpdateToolButtons()
{
    if (!m_canvas)
    {
        return;
    }

    // 工具类按钮与当前工具模式一一对应；Place 模式由左侧调色板触发，
    // 工具条上没有对应按钮，此时两个工具按钮都不高亮
    const ToolMode mode = m_canvas->GetToolMode();
    const bool gridVisible = m_viewMenu ? m_viewMenu->IsChecked(m_gridMenuItemId) : true;

    SetToolButtonActive(ID_ToolSelect, mode == ToolMode::Select);
    SetToolButtonActive(ID_ToolWire, mode == ToolMode::Wire);
    // 放大/缩小是瞬时命令，不参与激活态
    SetToolButtonActive(ID_ViewToggleGrid, gridVisible);
}

void MainFrame::SetToolButtonActive(int id, bool active)
{
    const auto btnIt = m_toolButtons.find(id);
    const auto iconIt = m_toolButtonIcons.find(id);
    if (btnIt == m_toolButtons.end() || iconIt == m_toolButtonIcons.end() || !btnIt->second)
    {
        return;
    }

    // 激活用高亮蓝（与画布选中色 0,120,215 同一视觉语言），常态用深灰；
    // IconFactory 会把两态位图分别缓存，反复切换不会重复绘制
    const wxColour colour = active ? wxColour(0, 120, 215) : wxColour(60, 60, 60);
    btnIt->second->SetBitmapLabel(
        IconFactory::GetIcon(iconIt->second, FromDIP(18), colour, active));
}

void MainFrame::ShowNotImplemented(const wxString& feature)
{
    wxMessageBox(wxString::Format(L"%s 尚未实现（接口桩已预留：ISerializer / ISimulator）", feature),
                 wxString(L"提示"), wxOK | wxICON_INFORMATION, this);
}

// ───────────────────────── 命令处理 ─────────────────────────

void MainFrame::OnExit(wxCommandEvent&)
{
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent&)
{
    wxMessageBox(
        wxString::Format(
            L"MyWx111 EDA\nwxWidgets %d.%d.%d\n数字电路设计与仿真 —— 前端骨架阶段",
            wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER),
        wxString(L"关于"), wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnFileNew(wxCommandEvent&)
{
    if (!m_circuit)
    {
        return;
    }

    // 当前电路未必已保存，先确认再清空，避免误操作丢失内容
    const int answer = wxMessageBox(wxString(L"新建电路将清空当前内容，是否继续？"),
                                    wxString(L"新建电路"),
                                    wxYES_NO | wxICON_QUESTION, this);
    if (answer != wxYES)
    {
        return;
    }

    // Clear 内部会触发变更通知，画布与属性面板因此自动刷新
    m_circuit->Clear();
    m_projectPath.clear();
    if (m_canvas)
    {
        m_canvas->Refresh();
    }
    SetStatusText(wxString(L"已新建空电路"), 0);
}

void MainFrame::OnFileOpen(wxCommandEvent&)
{
    if (!m_circuit)
    {
        return;
    }

    wxFileDialog dialog(this, wxString(L"打开工程文件"), m_lastProjectDir, wxString(),
                        wxString(L"MyWx111 工程 (*.json)|*.json|所有文件 (*.*)|*.*"),
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() == wxID_CANCEL)
    {
        return;
    }

    const wxString path = dialog.GetPath();
    wxString error;
    // 加载会重建全部元件与连线，若不抑制通知会触发大量刷新，
    // 而且每建一个元件/连线都会压一次撤销快照
    bool loaded = false;
    {
        Circuit::SuppressScope suppress(*m_circuit);
        loaded = project::LoadProjectJson(*m_circuit, path, &error);
    }

    if (!loaded)
    {
        // 加载失败时模型未被改动，原电路保持不变
        wxMessageBox(error, wxString(L"打开失败"), wxOK | wxICON_ERROR, this);
        return;
    }

    m_projectPath = path;
    m_lastProjectDir = wxFileName(path).GetPath();

    // 加载后重置撤销基线：旧快照基于上一份文档，继续撤销会回到完全不同的电路
    wxString baseline;
    if (project::SerializeCircuitToJson(*m_circuit, baseline, nullptr))
    {
        m_undoStack.Reset(baseline);
        UpdateUndoRedoState();
    }

    if (m_canvas)
    {
        m_canvas->Refresh();
    }
    if (m_props)
    {
        m_props->RefreshFromCircuit();
    }
    SetStatusText(wxString::Format(L"已打开工程：%s", path), 0);

    // error 非空表示加载成功但存在告警（如个别连线被跳过）
    if (!error.empty())
    {
        wxMessageBox(error, wxString(L"打开完成但有告警"), wxOK | wxICON_WARNING, this);
    }
}

void MainFrame::OnFileSave(wxCommandEvent& event)
{
    if (!m_circuit)
    {
        return;
    }

    // 从未保存过时没有目标路径，转交「另存为」让用户选择位置
    if (m_projectPath.empty())
    {
        OnFileSaveAs(event);
        return;
    }

    wxString error;
    if (!project::SaveProjectJson(*m_circuit, m_projectPath, &error))
    {
        wxMessageBox(error, wxString(L"保存失败"), wxOK | wxICON_ERROR, this);
        return;
    }
    SetStatusText(wxString::Format(L"已保存：%s", m_projectPath), 0);
}

void MainFrame::OnFileSaveAs(wxCommandEvent&)
{
    if (!m_circuit)
    {
        return;
    }

    wxFileDialog dialog(this, wxString(L"另存工程文件"), m_lastProjectDir,
                        wxString(L"MyWx111.json"),
                        wxString(L"MyWx111 工程 (*.json)|*.json|所有文件 (*.*)|*.*"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() == wxID_CANCEL)
    {
        return;
    }

    wxFileName file(dialog.GetPath());
    // 统一补成 .json，否则用户输入的无扩展名文件在打开对话框中无法被识别
    if (file.GetExt().CmpNoCase(L"json") != 0)
    {
        file.SetExt(L"json");
    }

    const wxString path = file.GetFullPath();
    wxString error;
    if (!project::SaveProjectJson(*m_circuit, path, &error))
    {
        wxMessageBox(error, wxString(L"保存失败"), wxOK | wxICON_ERROR, this);
        return;
    }

    m_projectPath = path;
    m_lastProjectDir = file.GetPath();
    SetStatusText(wxString::Format(L"已保存：%s", path), 0);
}

void MainFrame::OnFileExportNetlist(wxCommandEvent&)
{
    if (!m_circuit)
    {
        return;
    }

    wxFileDialog dialog(this, wxString(L"导出 KiCad 网表"), m_lastExportDir,
                        wxString(L"MyWx111.net"),
                        wxString(L"KiCad 网表 (*.net)|*.net|所有文件 (*.*)|*.*"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    // 用户取消时静默返回，不改动状态栏
    if (dialog.ShowModal() == wxID_CANCEL)
    {
        return;
    }

    wxFileName file(dialog.GetPath());
    // 用户可能输入不带扩展名或其它扩展名的文件名，统一补成 .net，
    // 否则生成的文本文件在 KiCad 的打开对话框中无法被识别
    if (file.GetExt().CmpNoCase(L"net") != 0)
    {
        file.SetExt(L"net");
    }
    m_lastExportDir = file.GetPath();

    const wxString path = file.GetFullPath();
    wxString error;
    const bool ok = netlist::ExportKiCadNetlist(*m_circuit, path, &error);

    if (!ok)
    {
        wxMessageBox(error, wxString(L"导出失败"), wxOK | wxICON_ERROR, this);
        return;
    }

    // 成功路径只用状态栏提示，避免每次导出都弹窗打断操作；
    // error 非空表示文件已写出但存在告警（如部分元件被跳过），再补一个警告框
    SetStatusText(wxString::Format(L"网表已导出：%s", path), 0);
    if (!error.empty())
    {
        wxMessageBox(error, wxString(L"导出完成但有告警"), wxOK | wxICON_WARNING, this);
    }
}

void MainFrame::OnExportGedaNetlist(wxCommandEvent&)
{
    if (!m_circuit || !ValidateBeforeExport())
    {
        return;
    }

    // 默认文件名沿用当前工程的主干名，未保存过工程时用程序名
    wxString defaultName = wxString(L"MyWx111");
    if (!m_projectPath.empty())
    {
        defaultName = wxFileName(m_projectPath).GetName();
    }

    wxFileDialog dialog(this, wxString(L"导出 gEDA 网表"), m_lastGedaExportDir,
                        defaultName + wxString(L".net"),
                        wxString(L"gEDA 网表 (*.net)|*.net|所有文件 (*.*)|*.*"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() == wxID_CANCEL)
    {
        return;
    }

    wxFileName file(dialog.GetPath());
    // 统一补成 .net，否则 pcb 侧的 Load netlist 对话框不认
    if (file.GetExt().CmpNoCase(L"net") != 0)
    {
        file.SetExt(L"net");
    }
    m_lastGedaExportDir = file.GetPath();

    const wxString path = file.GetFullPath();
    wxString error;
    if (!geda::ExportGedaNetlist(*m_circuit, path, &error))
    {
        wxMessageBox(error, wxString(L"导出失败"), wxOK | wxICON_ERROR, this);
        return;
    }

    SetStatusText(wxString::Format(L"gEDA 网表已导出：%s", path), 0);
    if (!error.empty())
    {
        wxMessageBox(error, wxString(L"导出完成但有告警"), wxOK | wxICON_WARNING, this);
    }
}

void MainFrame::OnExportPcb(wxCommandEvent&)
{
    if (!m_circuit || !ValidateBeforeExport())
    {
        return;
    }

    wxString defaultName = wxString(L"MyWx111");
    if (!m_projectPath.empty())
    {
        defaultName = wxFileName(m_projectPath).GetName();
    }

    wxFileDialog dialog(this, wxString(L"导出 pcb 布局文件"), m_lastGedaExportDir,
                        defaultName + wxString(L".pcb"),
                        wxString(L"pcb 布局 (*.pcb)|*.pcb|所有文件 (*.*)|*.*"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() == wxID_CANCEL)
    {
        return;
    }

    wxFileName file(dialog.GetPath());
    if (file.GetExt().CmpNoCase(L"pcb") != 0)
    {
        file.SetExt(L"pcb");
    }
    m_lastGedaExportDir = file.GetPath();

    const wxString path = file.GetFullPath();
    wxString error;
    if (!geda::ExportPcbFile(*m_circuit, path, &error))
    {
        wxMessageBox(error, wxString(L"导出失败"), wxOK | wxICON_ERROR, this);
        return;
    }

    SetStatusText(wxString::Format(L"pcb 布局已导出：%s", path), 0);
    ShowExportSucceeded(wxString(), path);
}

bool MainFrame::ValidateBeforeExport()
{
    wxString message;
    if (m_circuit && m_circuit->HasSlotConflict(&message))
    {
        wxMessageBox(message + wxString(L"\n\n请先修正重复的 slot 后再导出。"),
                     wxString(L"导出被取消"), wxOK | wxICON_WARNING, this);
        return false;
    }
    return true;
}

void MainFrame::ShowExportSucceeded(const wxString& netPath, const wxString& pcbPath)
{
    wxString detail = wxString(L"已导出：\n");
    if (!netPath.empty())
    {
        detail += wxString(L"  ") + netPath + L"\n";
    }
    if (!pcbPath.empty())
    {
        detail += wxString(L"  ") + pcbPath + L"\n";
    }

    // 任务书要求交付 .net 与 .pcb 两个文件，这里把 pcb 侧的操作步骤一并给出，
    // 避免使用者不知道这两个文件该怎么用
    detail += wxString(L"\npcb 中操作步骤：\n"
                       L"  1. File → Load layout data to buffer  → 选择上述 .pcb\n"
                       L"  2. File → Load netlist                → 选择上述 .net\n"
                       L"  3. Select → Disperse all elements\n"
                       L"  4. 按 o 键（显示飞线 / rats nest）\n"
                       L"  5. Connects → Auto-route all rats");

    wxMessageBox(detail, wxString(L"导出完成"), wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnCopy(wxCommandEvent&)
{
    if (!m_circuit)
    {
        return;
    }

    const std::vector<int> selection = m_circuit->GetSelection();
    if (selection.empty())
    {
        SetStatusText(wxString(L"请先选中要复制的元件"), 2);
        return;
    }

    // 先求选区左上角作为基准：剪贴板存相对偏移，
    // 粘贴位置因此只取决于粘贴时的平移量，与原件绝对坐标无关
    bool hasOrigin = false;
    wxPoint origin;
    for (int id : selection)
    {
        const Component* comp = m_circuit->FindComponent(id);
        if (comp == nullptr)
        {
            continue;
        }
        const wxPoint topLeft = comp->GetWorldBounds().GetTopLeft();
        if (!hasOrigin)
        {
            origin = topLeft;
            hasOrigin = true;
        }
        else
        {
            origin.x = std::min(origin.x, topLeft.x);
            origin.y = std::min(origin.y, topLeft.y);
        }
    }
    if (!hasOrigin)
    {
        return;
    }

    m_clipComponents.clear();
    m_clipWires.clear();
    m_clipOrigin = origin;
    m_pasteCounter = 0;

    std::map<int, int> idToIndex;
    for (int id : selection)
    {
        const Component* comp = m_circuit->FindComponent(id);
        if (comp == nullptr)
        {
            continue;
        }

        ClipComponent item;
        item.type = comp->GetType();
        item.name = comp->GetName();
        item.offset = comp->GetPosition() - origin;
        item.orientation = comp->GetOrientation();
        item.bitWidth = comp->GetBitWidth();
        item.refDes = comp->GetRefDes();
        item.footprint = comp->GetFootprint();
        item.value = comp->GetValue();
        item.slot = comp->GetSlot();

        idToIndex[id] = static_cast<int>(m_clipComponents.size());
        m_clipComponents.push_back(item);
    }

    // 只复制两端都在选区内的连线：向外伸出的线若一起复制，
    // 粘贴后会变成一批悬空的半截导线
    for (const auto& wirePtr : m_circuit->GetWires())
    {
        if (!wirePtr)
        {
            continue;
        }
        const auto fromIt = idToIndex.find(wirePtr->GetFrom().componentId);
        const auto toIt = idToIndex.find(wirePtr->GetTo().componentId);
        if (fromIt == idToIndex.end() || toIt == idToIndex.end())
        {
            continue;
        }

        ClipWire wire;
        wire.fromIndex = fromIt->second;
        wire.fromPin = wirePtr->GetFrom().pinIndex;
        wire.toIndex = toIt->second;
        wire.toPin = wirePtr->GetTo().pinIndex;
        for (const wxPoint& bend : wirePtr->GetBendPoints())
        {
            wire.bends.push_back(bend - origin);
        }
        m_clipWires.push_back(wire);
    }

    UpdateEditActionsState();
    SetStatusText(wxString::Format(L"已复制 %d 个元件、%d 条连线",
                                   static_cast<int>(m_clipComponents.size()),
                                   static_cast<int>(m_clipWires.size())),
                  2);
}

void MainFrame::OnCut(wxCommandEvent& event)
{
    if (!m_circuit || m_circuit->GetSelection().empty())
    {
        SetStatusText(wxString(L"请先选中要剪切的元件"), 2);
        return;
    }

    // 先复制再删除：DeleteSelected 内部会发结构变更通知，剪切因此占 1 个撤销步
    OnCopy(event);
    if (m_clipComponents.empty())
    {
        return;
    }

    m_circuit->DeleteSelected();
    UpdateEditActionsState();
    SetStatusText(wxString(L"已剪切选中元件"), 2);
}

void MainFrame::OnPaste(wxCommandEvent&)
{
    if (!m_circuit || m_clipComponents.empty())
    {
        return;
    }

    // 每次粘贴多错开一个网格，连续粘贴不会完全重叠
    ++m_pasteCounter;
    const int step = m_canvas ? m_canvas->GetGridStep() : 16;
    const wxPoint shift(step * m_pasteCounter, step * m_pasteCounter);

    std::vector<int> newIds;
    newIds.reserve(m_clipComponents.size());

    {
        // 抑制通知：批量重建期间逐元件刷新画布与属性面板会产生大量无效重绘，
        // 也避免中途状态被撤销栈捕获
        Circuit::SuppressScope suppress(*m_circuit);

        std::vector<int> indexToNewId(m_clipComponents.size(), -1);
        for (size_t i = 0; i < m_clipComponents.size(); ++i)
        {
            const ClipComponent& item = m_clipComponents[i];
            Component* comp = m_circuit->AddComponent(item.type, item.offset + shift);
            if (comp == nullptr)
            {
                continue;
            }

            comp->SetName(item.name);
            comp->SetOrientation(item.orientation);
            comp->SetBitWidth(item.bitWidth);
            comp->SetRefDes(item.refDes);
            comp->SetFootprint(item.footprint);
            comp->SetValue(item.value);
            comp->SetSlot(item.slot);

            indexToNewId[i] = comp->GetId();
            newIds.push_back(comp->GetId());
        }

        for (const ClipWire& wire : m_clipWires)
        {
            if (wire.fromIndex < 0 || wire.toIndex < 0)
            {
                continue;
            }
            if (indexToNewId[static_cast<size_t>(wire.fromIndex)] < 0 ||
                indexToNewId[static_cast<size_t>(wire.toIndex)] < 0)
            {
                continue;
            }

            std::vector<wxPoint> bends;
            bends.reserve(wire.bends.size());
            for (const wxPoint& bend : wire.bends)
            {
                bends.push_back(bend + shift);
            }

            m_circuit->AddWire(
                PinRef{ indexToNewId[static_cast<size_t>(wire.fromIndex)], wire.fromPin },
                PinRef{ indexToNewId[static_cast<size_t>(wire.toIndex)], wire.toPin }, bends);
        }

        // 粘贴后自动选中新元件；抑制区内不会触发刷新，故在此之前统一设置
        m_circuit->ClearSelection();
        for (int id : newIds)
        {
            m_circuit->ToggleSelect(id);
        }
    }

    // 抑制已结束：一次刷新 + 一次结构通知（粘贴算 1 个撤销步）
    m_circuit->NotifyChanged();
    m_circuit->NotifyStructureChanged();

    UpdateEditActionsState();
    SetStatusText(wxString::Format(L"已粘贴 %d 个元件", static_cast<int>(newIds.size())), 2);
}

void MainFrame::OnRotate(wxCommandEvent&)
{
    if (m_canvas)
    {
        m_canvas->RotateSelection();
    }
}

void MainFrame::OnZoomFit(wxCommandEvent&)
{
    if (m_canvas)
    {
        m_canvas->ZoomFit();
    }
}

void MainFrame::OnToggleFullScreen(wxCommandEvent&)
{
    ShowFullScreen(!IsFullScreen());

    // 菜单勾选态必须与实际全屏状态一致，否则再次点击会看不出当前是进还是出
    if (m_viewMenu)
    {
        m_viewMenu->Check(ID_ViewFullScreen, IsFullScreen());
    }
}

void MainFrame::OnToggleSimulation(wxCommandEvent&)
{
    if (!m_canvas)
    {
        return;
    }

    // 菜单项的勾选态由 wxWidgets 自动翻转，这里读到的即为切换后的状态
    const bool enabled = m_simMenu ? m_simMenu->IsChecked(ID_ViewSimulation) : false;
    m_canvas->SetSimulationEnabled(enabled);

    SetStatusText(enabled ? wxString(L"仿真已开启：单击输入引脚可切换高/低电平")
                          : wxString(L"仿真已关闭"),
                  2);
}

void MainFrame::OnSimulationEvaluateOnce(wxCommandEvent&)
{
    if (!m_canvas)
    {
        return;
    }

    if (!m_canvas->IsSimulationEnabled())
    {
        SetStatusText(wxString(L"仿真未开启，请先勾选「运行仿真」"), 2);
        return;
    }

    m_canvas->RefreshSimulation();
    SetStatusText(wxString(L"已重新求值"), 2);
}

void MainFrame::OnShowShortcuts(wxCommandEvent&)
{
    const wxString content =
        wxString(L"【菜单快捷键】\n"
                 L"  Ctrl+N        新建电路\n"
                 L"  Ctrl+O        打开工程文件\n"
                 L"  Ctrl+S        保存工程文件\n"
                 L"  Ctrl+Z        撤销\n"
                 L"  Ctrl+Y        重做\n"
                 L"  Ctrl+X/C/V    剪切 / 复制 / 粘贴\n"
                 L"  Ctrl+R        顺时针旋转选中元件\n"
                 L"  Del           删除选中元件或连线\n"
                 L"  Ctrl+A        全选\n"
                 L"\n"
                 L"【视图快捷键】\n"
                 L"  Ctrl++        放大\n"
                 L"  Ctrl+-        缩小\n"
                 L"  Ctrl+0        实际大小\n"
                 L"  F1            适合窗口\n"
                 L"  F11           全屏切换\n"
                 L"\n"
                 L"【鼠标操作】\n"
                 L"  左键          选择元件 / 拖动移动\n"
                 L"  Shift+左键    追加或取消选中\n"
                 L"  空白处拖拽    框选\n"
                 L"  中键拖动      平移视图\n"
                 L"  空格+左键拖动 平移视图\n"
                 L"  滚轮          以光标为轴心缩放\n"
                 L"  连线模式      左键逐段确认折点，点到引脚完成\n"
                 L"  Esc           退出当前工具");

    wxDialog dialog(this, wxID_ANY, wxString(L"快捷键参考"), wxDefaultPosition,
                    FromDIP(wxSize(520, 460)));
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    auto* text = new wxTextCtrl(&dialog, wxID_ANY, content, wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    sizer->Add(text, 1, wxEXPAND | wxALL, FromDIP(10));

    auto* closeButton = new wxButton(&dialog, wxID_OK, wxString(L"关闭"));
    sizer->Add(closeButton, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, FromDIP(10));

    dialog.SetSizer(sizer);
    dialog.CentreOnParent();
    dialog.ShowModal();
}

void MainFrame::UpdateEditActionsState()
{
    const bool hasSelection = m_circuit && !m_circuit->GetSelection().empty();
    const bool hasClipboard = !m_clipComponents.empty();

    if (m_editMenu)
    {
        m_editMenu->Enable(ID_EditCut, hasSelection);
        m_editMenu->Enable(ID_EditCopy, hasSelection);
        m_editMenu->Enable(ID_EditPaste, hasClipboard);
    }

    if (wxBitmapButton* button = FindToolButton(ID_EditPaste))
    {
        button->Enable(hasClipboard);
    }
}

void MainFrame::OnUndo(wxCommandEvent&)
{
    const wxString* target = m_undoStack.Undo();
    if (target == nullptr)
    {
        return;
    }

    // 值拷贝：应用快照期间栈内容可能改变，持有栈内指针不安全
    const wxString snapshot = *target;
    ApplySnapshot(snapshot);
    SetStatusText(wxString(L"已撤销上一步操作"), 2);
}

void MainFrame::OnRedo(wxCommandEvent&)
{
    const wxString* target = m_undoStack.Redo();
    if (target == nullptr)
    {
        return;
    }

    const wxString snapshot = *target;
    ApplySnapshot(snapshot);
    SetStatusText(wxString(L"已重做一步操作"), 2);
}

void MainFrame::ApplySnapshot(const wxString& json)
{
    if (!m_circuit)
    {
        return;
    }

    // 逻辑层守卫：回放期间不得再压栈
    m_undoStack.SetApplying(true);
    {
        // 表现层守卫：回放会反复改动模型，抑制期内不派发刷新，避免逐元件回调风暴
        Circuit::SuppressScope suppress(*m_circuit);

        wxString error;
        if (!project::DeserializeCircuitFromJson(*m_circuit, json, &error))
        {
            m_undoStack.SetApplying(false);
            wxMessageBox(wxString::Format(L"撤销失败：%s", error), wxString(L"错误"),
                         wxOK | wxICON_ERROR, this);
            return;
        }
    } // 抑制区在此结束

    // 抑制已结束，此处的清空选中会正常通知属性面板刷新
    m_circuit->ClearSelection();

    // 选中集合本就为空时 ClearSelection 不会发通知，这里兜底重绘一次
    if (m_canvas)
    {
        m_canvas->Refresh();
    }

    m_undoStack.SetApplying(false);
    UpdateUndoRedoState();
}

void MainFrame::PushUndoSnapshot()
{
    if (!m_circuit || m_undoStack.IsApplying())
    {
        return;
    }

    wxString json;
    wxString error;
    if (!project::SerializeCircuitToJson(*m_circuit, json, &error))
    {
        // 序列化失败说明文档已处于非法状态，记录但不打断用户操作
        wxLogDebug(L"撤销快照序列化失败：%s", error);
        return;
    }

    // Push 内部会去重，内容未实际变化时不会占用撤销步
    if (m_undoStack.Push(json))
    {
        UpdateUndoRedoState();
    }
}

void MainFrame::UpdateUndoRedoState()
{
    const bool canUndo = m_undoStack.CanUndo();
    const bool canRedo = m_undoStack.CanRedo();

    if (m_editMenu)
    {
        m_editMenu->Enable(ID_EditUndo, canUndo);
        m_editMenu->Enable(ID_EditRedo, canRedo);
    }

    if (wxBitmapButton* button = FindToolButton(ID_EditUndo))
    {
        button->Enable(canUndo);
    }
    if (wxBitmapButton* button = FindToolButton(ID_EditRedo))
    {
        button->Enable(canRedo);
    }
}

wxBitmapButton* MainFrame::FindToolButton(int id) const
{
    const auto it = m_toolButtons.find(id);
    return (it != m_toolButtons.end()) ? it->second : nullptr;
}

void MainFrame::OnDelete(wxCommandEvent&)
{
    if (m_canvas)
    {
        m_canvas->DeleteSelection();
    }
}

void MainFrame::OnSelectAll(wxCommandEvent&)
{
    if (m_canvas)
    {
        m_canvas->SelectAll();
    }
}

void MainFrame::OnSelectTool(wxCommandEvent&)
{
    if (!m_canvas)
    {
        return;
    }
    m_canvas->SetToolMode(ToolMode::Select);
    // 同步取消调色板高亮：切到选择模式后已不再有待放置的元件类型
    if (m_palette)
    {
        m_palette->SetActiveType(ComponentType::AndGate, false);
    }
    UpdateToolStatus();
}

void MainFrame::OnWireTool(wxCommandEvent&)
{
    if (!m_canvas)
    {
        return;
    }
    m_canvas->SetToolMode(ToolMode::Wire);
    if (m_palette)
    {
        m_palette->SetActiveType(ComponentType::AndGate, false);
    }
    UpdateToolStatus();
}

void MainFrame::OnPlaceNoteTool(wxCommandEvent&)
{
    if (!m_canvas)
    {
        return;
    }

    m_canvas->SetToolMode(ToolMode::PlaceNote);
    // 注释不是元件，清掉调色板的待放置元件高亮以免误导
    if (m_palette)
    {
        m_palette->SetActiveType(ComponentType::AndGate, false);
    }
    UpdateToolStatus();
}

void MainFrame::OnZoomIn(wxCommandEvent&)
{
    if (m_canvas)
    {
        m_canvas->ZoomIn();
    }
}

void MainFrame::OnZoomOut(wxCommandEvent&)
{
    if (m_canvas)
    {
        m_canvas->ZoomOut();
    }
}

void MainFrame::OnZoomReset(wxCommandEvent&)
{
    if (m_canvas)
    {
        m_canvas->ZoomReset();
    }
}

void MainFrame::OnToggleGrid(wxCommandEvent&)
{
    if (!m_canvas)
    {
        return;
    }

    // 勾选态由菜单维护，工具条按钮与菜单走同一入口，两者因此始终一致
    const bool visible = m_viewMenu ? m_viewMenu->IsChecked(m_gridMenuItemId) : true;
    m_canvas->SetGridVisible(visible);
    SetStatusText(visible ? wxString(L"网格已显示") : wxString(L"网格已隐藏"), 2);

    // 本函数不经过 UpdateToolStatus，网格按钮的激活态需自行刷新
    UpdateToolButtons();
}

void MainFrame::OnProjectPlaceholder(wxCommandEvent& event)
{
    ShowNotImplemented(event.GetId() == ID_ProjectOptions ? wxString(L"项目选项")
                                                          : wxString(L"电路分析"));
}
