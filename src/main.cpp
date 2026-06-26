///////////////////////////////////////////////////////////////////////////////
// src/main.cpp
//
//   基于 FLTK Fl_OpDesk 的可交互 DAG(有向无环图)编辑器。
//
//   预置一个算术运算 DAG 示例:
//
//        Source ──Out1──► Add ──Out──┐
//          │                  ▲      │
//          └──Out2──┐         │      ▼
//                   ▼         │   Multiply ──Out──► Sink
//                (未连)        │
//                             │
//        Source.Out1 ─► Add.A
//        Source.Out2 ─► Multiply.A
//        Add.Out     ─► Multiply.B
//        Multiply.Out─► Sink.In
//
//   交互(Fl_OpDesk 自带):
//     - 鼠标拖动盒子标题栏可移动盒子
//     - 从盒子右侧"输出"按钮拖拽到另一盒子左侧"输入"按钮可建立连线
//     - 选中盒子后按 Delete 可删除
//     - 拖拽已存在的连线可断开
//
//   DAG 规则(由自定义 MyButton::Connecting 强制):
//     - 只允许 输出→输入(禁止 输入-输入 / 输出-输出 / 输入-输出)
//     - 禁止建立会形成环的连线(DFS 检测)
//
//   编译:见根目录 CMakeLists.txt 与 README.md
///////////////////////////////////////////////////////////////////////////////

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Scroll.H>
#include <FL/fl_ask.H>

#include <string>
#include <vector>
#include <cstdio>

#include "Fl_OpDesk.H"
#include "Fl_OpBox.H"
#include "Fl_OpButton.H"

// 前置声明(彼此互相引用)
class MyDesk;
class MyBox;
class MyButton;

//=============================================================================
// MyButton —— 自定义按钮,重写 Connecting() 以强制 DAG 约束
//=============================================================================
class MyButton : public Fl_OpButton {
public:
    MyButton(const char *L, Fl_OpButtonType io) : Fl_OpButton(L, io) { }

    // 连接校验回调:返回 0 允许,-1 拒绝(填写 errmsg)
    int Connecting(Fl_OpButton *to, std::string &errmsg) override {
        // --- 规则 1:只允许「输出 → 输入」 ---
        //   to 是连接的对方按钮。若当前按钮是输入、对方也是输入 → 拒绝,以此类推。
        Fl_OpButtonType myType = this->GetButtonType();
        Fl_OpButtonType toType = to->GetButtonType();

        if (myType == FL_OP_INPUT_BUTTON && toType == FL_OP_INPUT_BUTTON) {
            errmsg = "不能连接两个输入按钮";
            return -1;
        }
        if (myType == FL_OP_OUTPUT_BUTTON && toType == FL_OP_OUTPUT_BUTTON) {
            errmsg = "不能连接两个输出按钮";
            return -1;
        }
        // 确定方向:src 必须是输出,dst 必须是输入
        MyButton *src = (myType == FL_OP_OUTPUT_BUTTON) ? this : (MyButton*)to;
        MyButton *dst = (myType == FL_OP_OUTPUT_BUTTON) ? (MyButton*)to : this;

        // --- 规则 2:禁止形成环 ---
        //   若从 dst 所在的盒子出发,沿着输出连线能到达 src 所在的盒子,
        //   那么再加上 src→dst 这条边就会形成环。
        Fl_OpBox *srcBox = src->GetOpBox();
        Fl_OpBox *dstBox = dst->GetOpBox();
        if (srcBox == dstBox) {
            errmsg = "不能连接同一个盒子内的按钮";
            return -1;
        }
        if (WouldCreateCycle(dstBox, srcBox)) {
            errmsg = "这条连线会形成环(DAG 不允许)";
            return -1;
        }
        return 0;  // 允许连接
    }

private:
    // DFS:检测从 start 出发沿「输出方向」能否到达 target。
    //   连线方向:输出按钮 → 输入按钮,因此从盒子的输出按钮的连接对象
    //   继续向其所在盒子的输出传播。
    static bool WouldCreateCycle(Fl_OpBox *start, Fl_OpBox *target) {
        if (!start || !target) return false;
        std::vector<Fl_OpBox*> stack;
        std::vector<Fl_OpBox*> visited;
        stack.push_back(start);
        while (!stack.empty()) {
            Fl_OpBox *cur = stack.back();
            stack.pop_back();
            if (cur == target) return true;
            // 去重
            bool seen = false;
            for (size_t i = 0; i < visited.size(); ++i)
                if (visited[i] == cur) { seen = true; break; }
            if (seen) continue;
            visited.push_back(cur);
            // 遍历当前盒子的每个输出按钮的连接,找下游盒子
            int nout = cur->GetTotalOutputButtons();
            for (int i = 0; i < nout; ++i) {
                Fl_OpButton *ob = cur->GetOutputButton(i);
                size_t nconn = ob->GetTotalConnectedButtons();
                for (size_t j = 0; j < nconn; ++j) {
                    Fl_OpButton *cb = ob->GetConnectedButton(j);
                    Fl_OpBox *downstream = cb ? cb->GetOpBox() : nullptr;
                    if (downstream) stack.push_back(downstream);
                }
            }
        }
        return false;
    }
};

//=============================================================================
// MyBox —— 自定义盒子,用 MyButton 作为端口
//=============================================================================
class MyBox : public Fl_OpBox {
public:
    MyBox(int X, int Y, int W, int H, const char *L)
        : Fl_OpBox(X, Y, W, H, L) { }
};

//=============================================================================
// MyDesk —— 自定义画布,重写 ConnectionError() 改用弹窗显示错误
//=============================================================================
class MyDesk : public Fl_OpDesk {
public:
    MyDesk(int X, int Y, int W, int H, const char *L = 0)
        : Fl_OpDesk(X, Y, W, H, L) { }

    void ConnectionError(Fl_OpButton *src, Fl_OpButton *dst,
                         std::string &errmsg) override {
        std::string msg;
        if (src && dst) {
            msg = std::string("连接失败:\n  ") +
                  src->GetFullName() + "\n  → " + dst->GetFullName() +
                  "\n\n原因:" + errmsg;
        } else {
            msg = std::string("连接失败:") + errmsg;
        }
        fl_alert("%s", msg.c_str());
    }
};

//=============================================================================
// 辅助:创建一个带输入/输出端口的盒子
//=============================================================================
static MyBox *CreateBox(MyDesk *desk, int x, int y, const char *title,
                        const std::vector<std::string> &inputs,
                        const std::vector<std::string> &outputs) {
    MyBox *box = new MyBox(x, y, 180, 110, strdup(title));
    box->begin();
    for (size_t i = 0; i < inputs.size(); ++i)
        new MyButton(inputs[i].c_str(), FL_OP_INPUT_BUTTON);
    for (size_t i = 0; i < outputs.size(); ++i)
        new MyButton(outputs[i].c_str(), FL_OP_OUTPUT_BUTTON);
    box->end();
    return box;
}

//=============================================================================
// 菜单回调
//=============================================================================
static MyDesk *g_desk = nullptr;

static void Menu_Quit(Fl_Widget*, void*) {
    if (g_desk) g_desk->window()->hide();
}

static void Menu_About(Fl_Widget*, void*) {
    fl_message("Fl_OpDesk DAG 编辑器\n\n"
               "基于 FLTK %d.%d 与 Fl_OpDesk (v0.82)。\n"
               "拖动端口建立连线;选中盒子按 Delete 删除。",
               FL_MAJOR_VERSION, FL_MINOR_VERSION);
}

//=============================================================================
// main
//=============================================================================
int main() {
    const int WIN_W = 960, WIN_H = 640;

    Fl_Double_Window *win = new Fl_Double_Window(WIN_W, WIN_H, "Fl_OpDesk DAG 编辑器");

    // 顶部菜单栏
    Fl_Menu_Bar *menubar = new Fl_Menu_Bar(0, 0, WIN_W, 25);
    menubar->add("文件/退出",  FL_COMMAND + 'q', Menu_Quit);
    menubar->add("帮助/关于",                   0, Menu_About);

    // 滚动容器包裹画布
    Fl_Scroll *scroll = new Fl_Scroll(0, 25, WIN_W, WIN_H - 25);
    scroll->box(FL_DOWN_BOX);

    // 画布(大尺寸以支持滚动)
    const int DESK_W = 4000, DESK_H = 3000;
    MyDesk *desk = new MyDesk(0, 0, DESK_W, DESK_H);
    desk->SetConnectStyle(FL_OPCONNECT_STYLE_CURVE);  // 贝塞尔曲线连线
    g_desk = desk;

    desk->begin();
    {
        // ---- 预置 DAG 示例 ----
        MyBox *src = CreateBox(desk, 60,  120, "Source",
                               {},            {"Out1", "Out2"});
        MyBox *add = CreateBox(desk, 400, 60,  "Add",
                               {"A", "B"},    {"Out"});
        MyBox *mul = CreateBox(desk, 720, 140, "Multiply",
                               {"A", "B"},    {"Out"});
        MyBox *snk = CreateBox(desk, 1040,180, "Sink",
                               {"In"},        {});

        // 建立连线(输出 → 输入)
        std::string err;
        desk->Connect(src, "Out1", add, "A", err);   // Source.Out1 → Add.A
        desk->Connect(src, "Out2", mul, "A", err);   // Source.Out2 → Multiply.A
        desk->Connect(add, "Out",  mul, "B", err);   // Add.Out     → Multiply.B
        desk->Connect(mul, "Out",  snk, "In", err);  // Multiply.Out→ Sink.In
    }
    desk->end();

    scroll->end();
    win->resizable(scroll);
    win->size_range(640, 400);
    win->show();
    return Fl::run();
}
