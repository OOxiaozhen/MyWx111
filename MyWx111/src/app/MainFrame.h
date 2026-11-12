#pragma once

#include <map>
#include <memory>
#include <vector>

#include "../model/ComponentType.h"

#include <wx/aui/aui.h>
#include <wx/wx.h>

#include "../model/UndoStack.h"

#include "../ui/IconFactory.h"

class Circuit;
class CircuitCanvas;
class ToolPalette;
class PropertiesPanel;

/// 应用主窗口。
/// 采用 wxAuiManager 承载可停靠面板：左侧元件调色板、中央绘图画布、右侧属性面板，
/// 顶部为自绘按钮工具条，另配菜单栏与三段式状态栏。
/// 各组件之间通过回调单向联动，主窗口不直接参与绘制。
class MainFrame : public wxFrame
{
public:
    /// 构造主窗口：创建电路模型、构建五区界面并绑定联动回调。
    MainFrame();

    /// 析构：必须先解除 wxAuiManager 与窗口的绑定，否则窗口销毁时会触发断言。
    ~MainFrame() override;

private:
    /// 本窗口自定义的命令 id 区段。
    /// 从 wxID_HIGHEST + 100 起，避免与 wxWidgets 标准 id 以及 AUI 内部 id 冲突。
    enum MenuIds
    {
        ID_FileNew = wxID_HIGHEST + 100, ///< 文件 - 新建
        ID_FileOpen,                     ///< 文件 - 打开
        ID_FileSave,                     ///< 文件 - 保存
        ID_FileSaveAs,                   ///< 文件 - 另存为
        ID_FileExportNetlist,            ///< 文件 - 导出网表（KiCad .net）
        ID_FileExportGedaNet,            ///< 文件 - 导出 - gEDA 网表(.net)
        ID_FileExportPcb,                ///< 文件 - 导出 - pcb 布局(.pcb)
        ID_EditUndo,                     ///< 编辑 - 撤销
        ID_EditRedo,                     ///< 编辑 - 重做
        ID_EditCut,                      ///< 编辑 - 剪切
        ID_EditCopy,                     ///< 编辑 - 复制
        ID_EditPaste,                    ///< 编辑 - 粘贴
        ID_EditRotate,                   ///< 编辑 - 旋转选中元件
        ID_ViewZoomIn,                   ///< 视图 - 放大
        ID_ViewZoomOut,                  ///< 视图 - 缩小
        ID_ViewZoomReset,                ///< 视图 - 实际大小
        ID_ViewZoomFit,                  ///< 视图 - 适合窗口
        ID_ViewToggleGrid,               ///< 视图 - 显示网格
        ID_ViewFullScreen,               ///< 视图 - 全屏切换
        ID_ViewSimulation,               ///< 仿真 - 运行仿真（保留原名称，避免牵动既有绑定）
        ID_SimulationEvaluateOnce,       ///< 仿真 - 重新求值
        ID_ToolSelect,                   ///< 工具 - 选择
        ID_ToolWire,                     ///< 工具 - 连线
        ID_ProjectOptions,               ///< 项目 - 项目选项
        ID_ProjectAnalyze,               ///< 项目 - 分析电路
        ID_HelpShortcuts                 ///< 帮助 - 快捷键参考
    };

    /// 构建五个顶级菜单并绑定命令处理。
    void CreateMenuBar();

    /// 构建顶部按钮工具条（独立 wxPanel，由 AUI 的 ToolbarPane 承载）。
    void SetupToolBar();

    /// 创建三段式状态栏。
    void SetupStatusBar();

    /// 创建 AUI 管理的三栏布局。
    void CreateLayout();

    /// 绑定画布 → 状态栏 / 属性面板的回调，以及模型变更回调。
    void BindCanvasCallbacks();

    /// 绑定调色板 → 画布 / 状态栏的回调。
    void BindPaletteCallbacks();

    /// 按当前画布工具模式刷新状态栏第三段文本。
    void UpdateToolStatus();

    /// 按当前工具模式与网格显示状态刷新工具条按钮的激活态配色。
    /// 所有工具模式切换最终都会经过 UpdateToolStatus，在此统一同步按钮外观，
    /// 避免在每个命令处理函数里重复刷新。
    void UpdateToolButtons();

    /// 设置单个图标按钮的激活外观。
    /// @param id 按钮的命令 id
    /// @param active true 显示激活态（蓝色图标 + 淡蓝底），false 恢复常态深灰图标
    void SetToolButtonActive(int id, bool active);

    /// 弹出「尚未实现」提示，用于保存 / 加载 / 撤销等占位命令。
    /// @param feature 功能名称，显示在提示文本中
    void ShowNotImplemented(const wxString& feature);

    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    /// 新建空电路（清空前会二次确认）。
    /// @param event 菜单命令事件（未使用）
    void OnFileNew(wxCommandEvent& event);

    /// 打开工程文件并替换当前电路。
    /// @param event 菜单命令事件（未使用）
    void OnFileOpen(wxCommandEvent& event);

    /// 保存到当前工程文件；从未保存过时转交「另存为」。
    /// @param event 菜单命令事件（未使用）
    void OnFileSave(wxCommandEvent& event);

    /// 另存为新的工程文件并更新当前路径。
    /// @param event 菜单命令事件（未使用）
    void OnFileSaveAs(wxCommandEvent& event);

    /// 导出当前电路为 KiCad 6+ 网表文件（弹出保存对话框并调用 netlist::ExportKiCadNetlist）。
    /// @param event 菜单命令事件（未使用）
    void OnFileExportNetlist(wxCommandEvent& event);

    /// 导出 gEDA pcb 网表（.net）。导出前会先校验芯片 slot 是否冲突。
    /// @param event 菜单命令事件（未使用）
    void OnExportGedaNetlist(wxCommandEvent& event);

    /// 导出 pcb 初始布局文件（.pcb）。
    /// @param event 菜单命令事件（未使用）
    void OnExportPcb(wxCommandEvent& event);

    /// 导出前的公共校验：芯片 slot 冲突时弹窗提示并拒绝导出。
    /// @return true 表示可以继续导出
    bool ValidateBeforeExport();

    /// 弹导出成功提示，内含 pcb 侧的五步操作流程。
    /// @param netPath 已导出的网表路径；为空表示本次未导出网表
    /// @param pcbPath 已导出的布局文件路径；为空表示本次未导出布局
    void ShowExportSucceeded(const wxString& netPath, const wxString& pcbPath);
    /// 复制选中元件及其内部连线（两端都在选区内）到内部剪贴板。
    /// @param event 命令事件（未使用）
    void OnCopy(wxCommandEvent& event);

    /// 剪切：先复制，再删除选中元件。
    /// @param event 命令事件（未使用）
    void OnCut(wxCommandEvent& event);

    /// 粘贴剪贴板内容，位置相对复制时偏移一个网格步长。
    /// @param event 命令事件（未使用）
    void OnPaste(wxCommandEvent& event);

    /// 按「是否有选中」「剪贴板是否为空」刷新剪切/复制/粘贴的可用状态。
    void UpdateEditActionsState();

    /// 顺时针旋转全部选中元件（复用 Circuit::RotateSelected）。
    /// @param event 命令事件（未使用）
    void OnRotate(wxCommandEvent& event);

    /// 适合窗口：缩放并居中，使全部元件可见。
    /// @param event 命令事件（未使用）
    void OnZoomFit(wxCommandEvent& event);

    /// 切换全屏，并同步菜单勾选态。
    /// @param event 命令事件（未使用）
    void OnToggleFullScreen(wxCommandEvent& event);

    /// 弹出快捷键参考窗口。
    /// @param event 命令事件（未使用）
    void OnShowShortcuts(wxCommandEvent& event);

    /// 切换组合逻辑仿真。
    /// @param event 命令事件（未使用）
    void OnToggleSimulation(wxCommandEvent& event);

    /// 强制重新求值一次（仿真未开启时仅提示）。
    /// @param event 命令事件（未使用）
    void OnSimulationEvaluateOnce(wxCommandEvent& event);

    /// 撤销一步：从栈取上一快照并应用到当前电路。
    /// @param event 命令事件（菜单项或工具条按钮触发）
    void OnUndo(wxCommandEvent& event);

    /// 重做一步：从栈取下一快照并应用到当前电路。
    /// @param event 命令事件
    void OnRedo(wxCommandEvent& event);

    /// 把 JSON 快照应用为当前电路内容。
    /// 内部同时启用撤销栈的「应用态」标记与 Circuit 的通知抑制，
    /// 阻止回放过程反向压栈；结束后清空选中并重绘画布。
    /// @param json 目标快照（调用方应传值拷贝，避免栈内指针失效）
    void ApplySnapshot(const wxString& json);

    /// 对当前电路取快照并压栈（结构变更回调的统一入口）。
    /// 处于应用态时直接返回；快照被去重时不会刷新按钮状态。
    void PushUndoSnapshot();

    /// 按 CanUndo()/CanRedo() 刷新菜单项与工具条按钮的可用状态。
    void UpdateUndoRedoState();

    /// 按命令 id 查找工具条按钮。
    /// @param id 命令 id
    /// @return 找到返回按钮指针；未找到返回 nullptr
    wxBitmapButton* FindToolButton(int id) const;
    void OnDelete(wxCommandEvent& event);
    void OnSelectAll(wxCommandEvent& event);
    void OnSelectTool(wxCommandEvent& event);
    void OnWireTool(wxCommandEvent& event);
    void OnZoomIn(wxCommandEvent& event);
    void OnZoomOut(wxCommandEvent& event);
    void OnZoomReset(wxCommandEvent& event);
    void OnToggleGrid(wxCommandEvent& event);
    void OnProjectPlaceholder(wxCommandEvent& event);

    std::unique_ptr<Circuit> m_circuit; ///< 电路文档模型（唯一所有权）
    UndoStack m_undoStack;              ///< 撤销/重做快照栈
    CircuitCanvas* m_canvas = nullptr;  ///< 中央绘图画布（窗口树持有）
    ToolPalette* m_palette = nullptr;   ///< 左侧元件调色板
    PropertiesPanel* m_props = nullptr; ///< 右侧属性面板
    wxPanel* m_toolPanel = nullptr;     ///< 顶部工具条宿主面板
    std::map<int, wxBitmapButton*> m_toolButtons; ///< 工具条按钮，按命令 id 索引
    std::map<int, IconId> m_toolButtonIcons;      ///< 各按钮对应的图标标识，刷新配色时用

    wxAuiManager m_auiManager;          ///< 可停靠面板管理器
    wxMenu* m_editMenu = nullptr;       ///< 编辑菜单（用于同步撤销/重做可用态）
    wxMenu* m_viewMenu = nullptr;       ///< 视图菜单（用于同步「显示网格」勾选态）
    wxMenu* m_simMenu = nullptr;        ///< 仿真菜单（用于同步「运行仿真」勾选态）
    int m_gridMenuItemId = ID_ViewToggleGrid; ///< 「显示网格」菜单项 id
    wxString m_lastExportDir;            ///< 上次导出所在目录，仅会话内记忆（不落盘）
    wxString m_lastGedaExportDir;        ///< 上次 gEDA 导出目录，仅会话内记忆

    /// 剪贴板中的一个元件。坐标为相对选区左上角的偏移，
    /// 这样粘贴位置只取决于粘贴时的平移量，与原件绝对位置无关。
    struct ClipComponent
    {
        ComponentType type = ComponentType::AndGate; ///< 元件类型
        wxString name;       ///< 显示名称
        wxPoint offset;      ///< 相对选区左上角的偏移
        int orientation = 0; ///< 朝向
        int bitWidth = 1;    ///< 位宽
        wxString refDes;     ///< 参考编号
        wxString footprint;  ///< 封装名
        wxString value;      ///< 器件型号
        int slot = 1;        ///< 门序号
    };

    /// 剪贴板中的一条连线：两端元件都在复制范围内。
    struct ClipWire
    {
        int fromIndex = -1;          ///< 起点元件在剪贴板元件表中的下标
        int fromPin = -1;            ///< 起点引脚索引
        int toIndex = -1;            ///< 终点元件下标
        int toPin = -1;              ///< 终点引脚索引
        std::vector<wxPoint> bends;  ///< 相对选区左上角的折点
    };

    std::vector<ClipComponent> m_clipComponents; ///< 剪贴板元件表
    std::vector<ClipWire> m_clipWires;           ///< 剪贴板连线表
    wxPoint m_clipOrigin;                        ///< 复制时选区左上角的绝对世界坐标
    int m_pasteCounter = 0;                      ///< 连续粘贴次数，用于逐次错开位置
    wxString m_projectPath;              ///< 当前工程文件完整路径；空表示尚未保存过
    wxString m_lastProjectDir;           ///< 上次工程文件所在目录，供对话框复用
};
