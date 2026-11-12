#pragma once

#include <wx/wx.h>

/// 应用程序对象：负责 wxWidgets 运行时初始化与主窗口创建。
class MyEDAApp : public wxApp
{
public:
    /// 应用初始化入口。
    /// @return 主窗口创建成功返回 true；返回 false 时 wxWidgets 会立即结束进程。
    bool OnInit() override;
};
