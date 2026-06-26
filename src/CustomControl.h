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
extern const Fl_Color CLR_BG;          // 画布深灰背景  #1E1E1E
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
    NODE_TYPE_COUNT
};

extern const char *kNodeTypeNames[];

const char *NodeDisplayName(NodeType t);
int NodeTypeFromName(const char *name);

//=============================================================================
// MyButton —— 圆形端口按钮 + DAG 连接约束
//=============================================================================
class MyButton : public Fl_OpButton {
public:
    MyButton(const char *L, Fl_OpButtonType io);

    void SetPinColor(Fl_Color c);

    void draw() override;
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

    void draw() override;

protected:
    Fl_Color theme_color_;
    Fl_Color pin_color_;
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

private:
    int factor_count_, feature_count_, xgb_count_;
    Fl_Scroll *scroll_;
    int last_rmb_x_, last_rmb_y_;

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
