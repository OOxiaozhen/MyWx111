#pragma once

#include <wx/bitmap.h>
#include <wx/colour.h>

/// 应用内置矢量图标的标识。
/// 所有图标统一在 24x24 逻辑画布上、按 KiCad 图标设计规范（2px 主干线、纯色扁平、
/// 无渐变与阴影）用几何路径绘制。新增图标时在此追加枚举值，
/// 并在 IconFactory.cpp 的绘制分派中补充对应绘制函数即可，
/// 界面各处只需调用 IconFactory::GetIcon(IconId, ...) 取图。
enum class IconId
{
    Select,  ///< 选择工具：鼠标箭头指针
    Wire,    ///< 连线工具：正交走线，两端带引脚结点
    ZoomIn,  ///< 放大视图：放大镜加「+」
    ZoomOut, ///< 缩小视图：放大镜加「−」
    Grid,    ///< 网格显示：2x2 方格
    Undo,    ///< 撤销：向左折返的箭头
    Redo,    ///< 重做：撤销图标的水平镜像
    Rotate,  ///< 旋转：顺时针折线箭头
    Delete,  ///< 删除：叉号
    Paste,   ///< 粘贴：剪贴板与纸页
    TextLabel,   ///< 文本标注：T 字
    LevelSource, ///< 电平源：方框 + 内部 1/0
    Clock,       ///< 时钟：方波
    Xnor,        ///< 同或门：或门形 + 反相圆
    Buffer,      ///< 缓冲器：三角形
    TriState,    ///< 三态门：三角 + 使能短线
    Led,         ///< LED：圆 + 射线
    Probe,       ///< 逻辑探针：小三角
    Note         ///< 文本注释：方框 + 三条横线
};

/// 图标工厂：集中提供全部界面图标的矢量绘制与缓存。
/// 图标全部由 wxGraphicsContext 按几何路径实时绘制，不依赖任何图片资源文件，
/// 因此在静态链接等没有资源加载链路的场景下也不会出现图标缺失或空白按钮。
class IconFactory
{
public:
    /// 获取指定图标的位图（带缓存）。
    /// @param id 图标标识
    /// @param size 输出位图边长（像素）；绘制时按 24x24 逻辑坐标等比缩放，
    ///             传 FromDIP(18) 之类的 DPI 感知尺寸可获得清晰结果；小于等于 0 时按 24 兜底
    /// @param color 图标线条与填充颜色，默认深灰以适配浅色界面，
    ///              传入主题色即可复用于深色界面
    /// @param highlighted 是否绘制激活态：额外铺一层淡蓝圆角底以示「该工具正在使用中」，
    ///                    线条色仍由 color 决定；激活态与非激活态分别缓存，互不覆盖
    /// @return 32 位带 alpha 通道的位图，背景完全透明；不抛异常
    static wxBitmap GetIcon(IconId id, int size = 24, const wxColour& color = wxColour(60, 60, 60),
                            bool highlighted = false);

private:
    /// 按图标标识执行实际绘制（不走缓存）。
    /// @param id 图标标识
    /// @param size 输出位图边长（像素），需已确保大于 0
    /// @param color 绘制颜色
    /// @param highlighted 是否先铺激活态底色再绘制图标
    /// @return 新绘制出的图标位图；若系统不支持 wxGraphicsContext 则返回空白透明位图
    static wxBitmap RenderIcon(IconId id, int size, const wxColour& color, bool highlighted);
};
