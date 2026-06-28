///////////////////////////////////////////////////////////////////////////////
// src/CustomControl.h
//
//   低代码 ML 工作流 DAG 编辑器的自定义控件与画布。
//
//   本文件声明:
//     - 全局颜色常量(深色主题)
//     - NodeType 枚举与辅助函数
//     - MyButton     : 圆形端口按钮 + DAG 连接约束
//     - StyledBox    : 圆角深色节点(标题栏 + 编辑图标)
//     - MyDesk       : 深色画布(网格背景 + 发光连线 + 拖放创建)
//     - NodeButton   : 左侧面板的可拖拽节点按钮
//
//   节点工厂方法(CreateFormulaInputNode 等)也在 MyDesk 中声明,
//   实现在 CustomControl.cpp。
///////////////////////////////////////////////////////////////////////////////
#ifndef CUSTOM_CONTROL_H
#define CUSTOM_CONTROL_H

#include <FL/Fl.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Counter.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>

#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

#include "Fl_OpDesk.H"
#include "Fl_OpBox.H"
#include "Fl_OpButton.H"
#include "Fl_OpConnect.H"

//=============================================================================
// 全局颜色常量(深色主题)
//=============================================================================
extern const Fl_Color CLR_BG;          // 画布灰背景  #FEFEDE
extern const Fl_Color CLR_GRID;        // 网格线        #2A2A2A
extern const Fl_Color CLR_CONN;        // 浅灰绿连线    #88C0A0
extern const Fl_Color CLR_GLOW;        // 绿色发光点    #33CC66
extern const Fl_Color CLR_PIN_GREEN;   // 绿色端口      #33CC66
extern const Fl_Color CLR_PIN_GREY;    // 灰白端口      #C0C0C0
extern const Fl_Color CLR_TITLE_DARK;  // 标题栏深色    #2A2A2A
extern const Fl_Color CLR_TEXT_LT;     // 浅色文字      #E0E0E0
extern const Fl_Color CLR_TEXT_DIM;    // 暗色文字      #888888
extern const Fl_Color CLR_INPUT_BG;    // 输入框深黑底  #141414
extern const Fl_Color CLR_GREEN_THEME; // 暗绿主题      #2E4A35
extern const Fl_Color CLR_BROWN_THEME; // 暗红褐主题    #4A3232

// 标题栏高度
extern const int TITLE_H;

//=============================================================================
// 节点类型定义
//=============================================================================
enum NodeType {
    NODE_FORMULA_INPUT = 0,
    NODE_FEATURE_ENGINEERING,
    NODE_XGBOOST_MODEL,
    NODE_DATA_SOURCE,
    NODE_TYPE_COUNT
};

extern const char *kNodeTypeNames[];

const char *NodeDisplayName(NodeType t);
int NodeTypeFromName(const char *name);

//=============================================================================
// MyButton —— 圆形端口按钮 + DAG 连接约束
//=============================================================================
class StyledBox;  // 前置声明(MyButton::handle 需要)

class MyButton : public Fl_OpButton {
public:
    MyButton(const char *L, Fl_OpButtonType io);

    void SetPinColor(Fl_Color c);

    void draw() override;
    int handle(int e) override;
    int Connecting(Fl_OpButton *to, std::string &errmsg) override;

private:
    Fl_Color pin_color_;

    static bool WouldCreateCycle(Fl_OpBox *start, Fl_OpBox *target);
};

//=============================================================================
// StyledBox —— 圆角深色节点,带标题栏 + 编辑图标
//=============================================================================
class StyledBox : public Fl_OpBox {
public:
    StyledBox(int X, int Y, int W, int H, const char *L,
              Fl_Color themeColor, Fl_Color pinColor = CLR_PIN_GREEN);

    void SetPinColorForAllButtons();
    bool IsInTitleBar(int mx, int my) const;

    // 设置端口垂直对齐偏移(相对于盒子顶部的 Y 坐标,<=0 表示用默认居中)
    // 用于让端口与某一行内容水平对齐(如 XGBoost 节点的副标题行)
    void SetPinAlignY(int inputY, int outputY) {
        pin_align_in_y_ = inputY;
        pin_align_out_y_ = outputY;
    }

    // 设置副标题文字(纯绘制,不作为子控件,避免被 FindButtonByLabel 误认为端口)
    // 左右两段文字,绘制在标题栏下方指定行
    void SetSubTitles(const char *left, const char *right,
                      int rowY, int rowH) {
        subtitle_left_  = left  ? left  : "";
        subtitle_right_ = right ? right : "";
        subtitle_y_ = rowY;
        subtitle_h_ = rowH;
    }

    // 添加一行"行标签"(纯 fl_draw 绘制,不是子控件)。
    // 用于源节点等需要左对齐字段标签、且要避免 Fl_Box 居中基线不稳的场景。
    // x/y/w/h 都是相对盒子左上角的坐标;text 为标签文字。
    void AddRowLabel(int rx, int ry, int rw, int rh, const char *text) {
        RowLabel lbl;
        lbl.x = rx; lbl.y = ry; lbl.w = rw; lbl.h = rh;
        lbl.text = text ? text : "";
        row_labels_.push_back(lbl);
    }

    // 校正端口位置到 pin_align_y(覆盖 _RecalcButtonSizes 的默认居中)
    // public 以便 MyButton 在拖拽前调用
    void RepositionPins();

    void draw() override;

protected:
    Fl_Color theme_color_;
    Fl_Color pin_color_;
    int pin_align_in_y_;   // 输入端口中心 Y(相对盒顶),<=0 表默认
    int pin_align_out_y_;  // 输出端口中心 Y(相对盒顶),<=0 表默认
    std::string subtitle_left_;   // 左副标题(纯绘制)
    std::string subtitle_right_;  // 右副标题(纯绘制)
    int subtitle_y_;              // 副标题行 Y(相对盒顶)
    int subtitle_h_;              // 副标题行高

    // 行标签(纯 fl_draw 绘制,相对盒顶坐标)
    struct RowLabel { int x, y, w, h; std::string text; };
    std::vector<RowLabel> row_labels_;
};

//=============================================================================
// MyDesk —— 深色画布:网格背景 + 自定义发光连线 + 拖放创建
//=============================================================================
class MyDesk : public Fl_OpDesk {
public:
    MyDesk(int X, int Y, int W, int H, const char *L = 0);

    void SetScroll(Fl_Scroll *s);

    void draw() override;
    int handle(int e) override;

    // 在指定坐标创建节点(工厂入口)
    void CreateNodeAt(NodeType type, int x, int y);

    // 删除点击位置最近的连线
    bool DeleteNearestConnection(int mx, int my);

    // ---- 节点工厂方法(public,供 main 预置拓扑调用) ----
    StyledBox *CreateFormulaInputNode(int x, int y, const char *title);
    StyledBox *CreateFeatureEngineeringNode(int x, int y, const char *title);
    StyledBox *CreateXGBoostModelNode(int x, int y, const char *title);
    StyledBox *CreateDataSourceNode(int x, int y, const char *title);

    // 拖拽连线追踪(被 MyButton 调用)
    void SetDragButton(Fl_OpButton *btn, int mx, int my);
    void ClearDragButton();

private:
    int factor_count_, feature_count_, xgb_count_, source_count_;
    Fl_Scroll *scroll_;
    int last_rmb_x_, last_rmb_y_;

    // 拖拽状态追踪
    Fl_OpButton *drag_btn_{nullptr};
    int drag_mx_{0}, drag_my_{0};

    Fl_Scroll *scroll_parent();

    // 自定义连线绘制:覆盖原生,加发光点
    void DrawCustomConnections();

    static void Menu_DeleteConnCb(Fl_Widget*, void *data);
    static void Menu_ClearConnsCb(Fl_Widget*, void *data);
};

//=============================================================================
// NodeButton —— 左侧面板的可拖拽节点按钮(拖拽源)
//=============================================================================
class NodeButton : public Fl_Button {
public:
    NodeButton(int X, int Y, int W, int H, const char *L, NodeType type);

    int handle(int e) override;

private:
    NodeType type_;
    bool drag_started_;
    int push_x_, push_y_;
};

#endif // CUSTOM_CONTROL_H
