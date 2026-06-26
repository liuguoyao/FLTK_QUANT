///////////////////////////////////////////////////////////////////////////////
// src/main.cpp
//
//   低代码机器学习工作流 DAG 编辑器(深色主题)——程序入口。
//
//   自定义控件与画布实现在 CustomControl.h / CustomControl.cpp。
//   本文件只负责窗口搭建、菜单、左侧面板与预置拓扑。
///////////////////////////////////////////////////////////////////////////////
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>

#include <string>

#include "CustomControl.h"

//=============================================================================
// 全局指针与菜单回调
//=============================================================================
static MyDesk *g_desk = nullptr;

static void Menu_Quit(Fl_Widget*, void*) {
    if (g_desk) g_desk->window()->hide();
}

static void Menu_About(Fl_Widget*, void*) {
    fl_message("低代码 ML 工作流 DAG 编辑器\n\n"
               "基于 FLTK %d.%d 与 Fl_OpDesk (v0.82)。\n\n"
               "操作:\n"
               "  • 从左侧面板拖拽节点到画布\n"
               "  • 拖端口建连线 / Del 删节点\n"
               "  • 右键删连线 / 拖连线断开",
               FL_MAJOR_VERSION, FL_MINOR_VERSION);
}

//=============================================================================
// main
//=============================================================================
int main() {
    const int WIN_W = 1100, WIN_H = 700;
    const int PANEL_W = 170;
    const int MENUBAR_H = 25;

    Fl_Double_Window *win = new Fl_Double_Window(WIN_W, WIN_H,
                                                  "低代码 ML 工作流 DAG");

    // 顶部菜单栏
    Fl_Menu_Bar *menubar = new Fl_Menu_Bar(0, 0, WIN_W, MENUBAR_H);
    menubar->add("文件/退出", FL_COMMAND + 'q', Menu_Quit);
    menubar->add("帮助/关于", 0, Menu_About);

    // ---- 左侧节点库面板 ----
    Fl_Group *panel = new Fl_Group(0, MENUBAR_H, PANEL_W, WIN_H - MENUBAR_H);
    panel->box(FL_THIN_DOWN_BOX);
    panel->color(fl_rgb_color(0x25, 0x25, 0x25));
    {
        Fl_Box *title = new Fl_Box(0, MENUBAR_H + 8, PANEL_W, 24, "节点库");
        title->labelsize(14);
        title->labelcolor(CLR_TEXT_LT);
        title->labelfont(FL_HELVETICA_BOLD);

        int btnY = MENUBAR_H + 42;
        const int btnH = 56;
        const int btnGap = 12;
        const int btnW = PANEL_W - 24;
        const int btnX = 12;

        new NodeButton(btnX, btnY + 0*(btnH+btnGap), btnW, btnH,
                       "公式输入\n(FormulaInput)", NODE_FORMULA_INPUT);
        new NodeButton(btnX, btnY + 1*(btnH+btnGap), btnW, btnH,
                       "特征工程\n(FeatureEng)", NODE_FEATURE_ENGINEERING);
        new NodeButton(btnX, btnY + 2*(btnH+btnGap), btnW, btnH,
                       "XGBoost模型\n(XGBoostModel)", NODE_XGBOOST_MODEL);

        Fl_Box *hint = new Fl_Box(8, WIN_H - 140, PANEL_W - 16, 130);
        hint->labelsize(10);
        hint->labelcolor(CLR_TEXT_DIM);
        hint->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
        hint->label("操作:\n"
                    "  • 拖拽节点→画布\n"
                    "  • 拖端口建连线\n"
                    "  • Del 删节点\n"
                    "  • 右键删连线\n"
                    "  • 拖连线可断开");
    }
    panel->end();
    panel->resizable(nullptr);

    // ---- 右侧画布 ----
    Fl_Scroll *scroll = new Fl_Scroll(PANEL_W, MENUBAR_H,
                                      WIN_W - PANEL_W, WIN_H - MENUBAR_H);
    scroll->box(FL_DOWN_BOX);
    scroll->color(CLR_BG);

    const int DESK_W = 4000, DESK_H = 3000;
    MyDesk *desk = new MyDesk(0, 0, DESK_W, DESK_H);
    g_desk = desk;

    scroll->end();
    desk->SetScroll(scroll);

    // ---- 预置三节点拓扑 ----
    desk->begin();
    {
        // 节点 A:公式输入(80, 200)
        StyledBox *nodeA = desk->CreateFormulaInputNode(80, 200, "公式输入");
        // 节点 B:特征工程(360, 200)— Y 与 A 中心对齐
        StyledBox *nodeB = desk->CreateFeatureEngineeringNode(360, 200, "特征工程");
        // 节点 C:XGBoost(660, 120)— C 顶部与 B 顶部对齐
        StyledBox *nodeC = desk->CreateXGBoostModelNode(660, 120, "XGBoost");

        nodeA->SetPinColorForAllButtons();
        nodeB->SetPinColorForAllButtons();
        nodeC->SetPinColorForAllButtons();

        // 连线 1: A.公式 → B.特征公式
        std::string err;
        desk->Connect(nodeA, "公式", nodeB, "特征公式", err);
        // 连线 2: B.特征工程 → C.特征工程
        desk->Connect(nodeB, "特征工程", nodeC, "特征工程", err);
    }
    desk->end();

    win->resizable(scroll);
    win->size_range(800, 500);
    win->show();
    return Fl::run();
}
