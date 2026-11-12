#include "App.h"

#include "MainFrame.h"

bool MyEDAApp::OnInit()
{
    // 提前注册全部图像处理器：后续工具栏/菜单若使用 PNG 图标，
    // 缺少这一步会导致位图静默加载失败（控件显示为空），排查成本很高
    wxInitAllImageHandlers();

    SetAppName("MyWx111 EDA");
    SetVendorName("MyWx111");

    auto* frame = new MainFrame();
    frame->Show(true);
    SetTopWindow(frame);
    return true;
}

// 全工程只允许出现一次 wxIMPLEMENT_APP。
// 因为 vcxproj 已移除 _CONSOLE 且子系统为 Windows，
// 由它生成的 WinMain 才是进程的真实入口；若同时定义 _CONSOLE，CRT 会去找 main，导致入口不匹配
wxIMPLEMENT_APP(MyEDAApp);
