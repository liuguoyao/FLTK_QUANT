///////////////////////////////////////////////////////////////////////////////
// src/main.cpp
//
//   基于 FLTK Fl_OpDesk 的可交互 DAG(有向无环图)编辑器。
//
//   功能:
//     - 左侧"节点库"面板:因子节点、数据源节点、ML模型节点
//     - 从面板按住拖拽到画布释放,即在该位置创建节点
//     - 拖拽端口建立连线(Fl_OpDesk 原生)
//     - 选中节点按 Delete 删除(Fl_OpDesk 原生)
//     - 拖拽已存在连线可断开(Fl_OpDesk 原生)
//     - 右键点击连线删除该连线
//
//   DAG 规则(由自定义 MyButton::Connecting 强制):
//     - 只允许 输出→输入(禁止 输入-输入 / 输出-输出)
//     - 禁止建立会形成环的连线(DFS 检测)
//
//   编译:见根目录 CMakeLists.txt 与 README.md
///////////////////////////////////////////////////////////////////////////////

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>

#include <string>
#include <vector>
#include <cstdio>
#include <cmath>

#include "Fl_OpDesk.H"
#include "Fl_OpBox.H"
#include "Fl_OpButton.H"

//=============================================================================
// 节点类型定义
//=============================================================================
enum NodeType {
    NODE_FACTOR = 0,      // 因子节点
    NODE_DATA_SOURCE,     // 数据源节点
    NODE_ML_MODEL,        // ML 模型节点
    NODE_TYPE_COUNT
};

// 每种节点类型的拖拽数据标识(通过 clipboard 传递)
static const char *kNodeTypeNames[] = {
    "FACTOR",
    "DATA_SOURCE",
    "ML_MODEL"
};

// 获取节点类型的中文显示名
static const char *NodeDisplayName(NodeType t) {
    switch (t) {
        case NODE_FACTOR:      return "因子";
        case NODE_DATA_SOURCE: return "数据源";
        case NODE_ML_MODEL:    return "ML模型";
        default:               return "未知";
    }
}

// 获取节点类型的端口配置
static void NodePortConfig(NodeType t,
                           std::vector<std::string> &inputs,
                           std::vector<std::string> &outputs) {
    inputs.clear();
    outputs.clear();
    switch (t) {
        case NODE_FACTOR:
            // 因子节点:无输入,输出因子数据
            outputs.push_back("Factors");
            break;
        case NODE_DATA_SOURCE:
            // 数据源节点:无输入,输出原始数据
            outputs.push_back("Data");
            break;
        case NODE_ML_MODEL:
            // ML模型节点:输入特征和模型,输出预测结果
            inputs.push_back("Features");
            inputs.push_back("Model");
            outputs.push_back("Prediction");
            break;
        default:
            break;
    }
}

// 从拖拽数据名解析出 NodeType,失败返回 -1
static int NodeTypeFromName(const char *name) {
    for (int i = 0; i < NODE_TYPE_COUNT; ++i) {
        if (strcmp(name, kNodeTypeNames[i]) == 0)
            return i;
    }
    return -1;
}

//=============================================================================
// MyButton —— 自定义按钮,重写 Connecting() 以强制 DAG 约束
//=============================================================================
class MyButton : public Fl_OpButton {
public:
    MyButton(const char *L, Fl_OpButtonType io) : Fl_OpButton(L, io) { }

    int Connecting(Fl_OpButton *to, std::string &errmsg) override {
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
        MyButton *src = (myType == FL_OP_OUTPUT_BUTTON) ? this : (MyButton*)to;
        MyButton *dst = (myType == FL_OP_OUTPUT_BUTTON) ? (MyButton*)to : this;

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
        return 0;
    }

private:
    static bool WouldCreateCycle(Fl_OpBox *start, Fl_OpBox *target) {
        if (!start || !target) return false;
        std::vector<Fl_OpBox*> stack;
        std::vector<Fl_OpBox*> visited;
        stack.push_back(start);
        while (!stack.empty()) {
            Fl_OpBox *cur = stack.back();
            stack.pop_back();
            if (cur == target) return true;
            bool seen = false;
            for (size_t i = 0; i < visited.size(); ++i)
                if (visited[i] == cur) { seen = true; break; }
            if (seen) continue;
            visited.push_back(cur);
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
// MyBox —— 自定义盒子
//=============================================================================
class MyBox : public Fl_OpBox {
public:
    MyBox(int X, int Y, int W, int H, const char *L)
        : Fl_OpBox(X, Y, W, H, L) { }
};

//=============================================================================
// MyDesk —— 自定义画布:支持拖放创建节点 + 连线错误弹窗
//=============================================================================
class MyDesk : public Fl_OpDesk {
public:
    MyDesk(int X, int Y, int W, int H, const char *L = 0)
        : Fl_OpDesk(X, Y, W, H, L), factor_count_(0), datasource_count_(0),
          mlmodel_count_(0), scroll_(nullptr), last_rmb_x_(0), last_rmb_y_(0) { }

    // 记录包裹本画布的 Fl_Scroll(用于坐标换算)
    void SetScroll(Fl_Scroll *s) { scroll_ = s; }

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

    // 处理拖放事件:接收来自左侧面板的节点拖拽
    int handle(int e) override {
        switch (e) {
            case FL_DND_ENTER:
            case FL_DND_DRAG:
                return 1;  // 接受拖拽
            case FL_DND_RELEASE:
                return 1;  // 接受释放,随后会收到 FL_PASTE
            case FL_PASTE: {
                std::string text = Fl::event_text();
                int typeIdx = NodeTypeFromName(text.c_str());
                if (typeIdx < 0) return 0;  // 不是我们的拖拽数据

                // 计算释放位置在画布内的坐标
                // event_x/y 是相对于当前 handle 的 widget(画布)的窗口坐标
                // Fl_Scroll 的滚动偏移需补偿
                int dropX = Fl::event_x() - x();
                int dropY = Fl::event_y() - y();
                // 补偿 Fl_Scroll 滚动偏移
                Fl_Scroll *scr = scroll_parent();
                if (scr) {
                    dropX += scr->xposition();
                    dropY += scr->yposition();
                }
                CreateNodeAt((NodeType)typeIdx, dropX, dropY);
                return 1;
            }
            case FL_PUSH: {
                // 右键:弹出"删除最近连线"菜单
                if (Fl::event_button() == FL_RIGHT_MOUSE) {
                    // 记录右键位置(画布内坐标)供回调使用
                    last_rmb_x_ = Fl::event_x() - x();
                    last_rmb_y_ = Fl::event_y() - y();
                    Fl_Scroll *scr = scroll_parent();
                    if (scr) {
                        last_rmb_x_ += scr->xposition();
                        last_rmb_y_ += scr->yposition();
                    }
                    Fl_Menu_Item menu[] = {
                        {"删除最近的连线", 0, Menu_DeleteConnCb, this, 0, 0, 0, 0, 0},
                        {nullptr, 0, nullptr, nullptr, 0, 0, 0, 0, 0}
                    };
                    const Fl_Menu_Item *m = menu->popup(
                        Fl::event_x(), Fl::event_y());
                    if (m) m->do_callback(this, user_data());
                    return 1;
                }
                break;
            }
        }
        return Fl_OpDesk::handle(e);
    }

    // 在指定坐标创建节点
    void CreateNodeAt(NodeType type, int x, int y) {
        std::vector<std::string> inputs, outputs;
        NodePortConfig(type, inputs, outputs);

        // 生成带编号的标题
        char title[64];
        int num = 0;
        switch (type) {
            case NODE_FACTOR:      num = ++factor_count_;      break;
            case NODE_DATA_SOURCE: num = ++datasource_count_;  break;
            case NODE_ML_MODEL:    num = ++mlmodel_count_;     break;
            default: break;
        }
        snprintf(title, sizeof(title), "%s_%d", NodeDisplayName(type), num);

        // 确保节点不会创建在负坐标
        if (x < 0) x = 10;
        if (y < 0) y = 10;

        // 预估盒子宽高
        int bw = 180, bh = 40 + (int)((inputs.size() + outputs.size()) * 22);
        if (bh < 80) bh = 80;

        // 需要在画布 begin/end 上下文中创建
        this->begin();
        MyBox *box = new MyBox(x, y, bw, bh, strdup(title));
        box->begin();
        for (size_t i = 0; i < inputs.size(); ++i)
            new MyButton(inputs[i].c_str(), FL_OP_INPUT_BUTTON);
        for (size_t i = 0; i < outputs.size(); ++i)
            new MyButton(outputs[i].c_str(), FL_OP_OUTPUT_BUTTON);
        box->end();
        this->end();

        // 设置选中状态并重绘
        redraw();
    }

    // 删除点击位置最近的连线(返回是否删除成功)
    bool DeleteNearestConnection(int mx, int my) {
        int nConn = GetConnectionsTotal();
        if (nConn == 0) {
            fl_message("当前没有连线可删除。");
            return false;
        }
        Fl_OpConnect *best = nullptr;
        double bestDist = 1e18;
        for (int i = 0; i < nConn; ++i) {
            Fl_OpConnect *c = GetConnection(i);
            if (!c) continue;
            Fl_OpButton *a = c->GetSrcButton();
            Fl_OpButton *b = c->GetDstButton();
            if (!a || !b) continue;
            // 用连线两端按钮中点到鼠标的距离作为近似
            double midX = (a->x() + a->w()/2 + b->x() + b->w()/2) / 2.0;
            double midY = (a->y() + a->h()/2 + b->y() + b->h()/2) / 2.0;
            double dx = midX - mx, dy = midY - my;
            double d = dx*dx + dy*dy;
            if (d < bestDist) { bestDist = d; best = c; }
        }
        if (best) {
            Fl_OpButton *a = best->GetSrcButton();
            Fl_OpButton *b = best->GetDstButton();
            Disconnect(a, b);
            redraw();
            return true;
        }
        return false;
    }

private:
    int factor_count_;
    int datasource_count_;
    int mlmodel_count_;
    Fl_Scroll *scroll_;
    int last_rmb_x_;  // 最近一次右键点击的画布内坐标
    int last_rmb_y_;

    // 找到包裹本画布的 Fl_Scroll 父容器
    Fl_Scroll *scroll_parent() {
        return scroll_;
    }

    // 右键菜单回调(静态,通过 userdata 访问 this)
    static void Menu_DeleteConnCb(Fl_Widget*, void *data) {
        MyDesk *self = (MyDesk*)data;
        self->DeleteNearestConnection(self->last_rmb_x_, self->last_rmb_y_);
    }
};

//=============================================================================
// NodeButton —— 左侧面板的可拖拽节点按钮(拖拽源)
//=============================================================================
class NodeButton : public Fl_Button {
public:
    NodeButton(int X, int Y, int W, int H, const char *L, NodeType type)
        : Fl_Button(X, Y, W, H, L), type_(type), drag_started_(false) { }

    int handle(int e) override {
        switch (e) {
            case FL_PUSH:
                drag_started_ = false;
                push_x_ = Fl::event_x();
                push_y_ = Fl::event_y();
                break;
            case FL_DRAG: {
                // 检测是否拖动超过阈值,触发 DnD
                int dx = Fl::event_x() - push_x_;
                int dy = Fl::event_y() - push_y_;
                if (!drag_started_ && (dx*dx + dy*dy) > 16) {
                    drag_started_ = true;
                    // 把节点类型名放入 selection clipboard,启动拖拽
                    const char *name = kNodeTypeNames[type_];
                    Fl::copy(name, (int)strlen(name), 0);
                    Fl::dnd();
                }
                return 1;
            }
            case FL_RELEASE:
                drag_started_ = false;
                break;
        }
        return Fl_Button::handle(e);
    }

private:
    NodeType type_;
    bool drag_started_;
    int push_x_, push_y_;
};

//=============================================================================
// 全局指针与辅助函数
//=============================================================================
static MyDesk *g_desk = nullptr;

static void Menu_Quit(Fl_Widget*, void*) {
    if (g_desk) g_desk->window()->hide();
}

static void Menu_About(Fl_Widget*, void*) {
    fl_message("Fl_OpDesk DAG 编辑器\n\n"
               "基于 FLTK %d.%d 与 Fl_OpDesk (v0.82)。\n\n"
               "操作说明:\n"
               "  • 从左侧面板拖拽节点到画布创建\n"
               "  • 拖拽端口建立连线\n"
               "  • 选中节点按 Delete 删除\n"
               "  • 右键点击连线附近删除连线\n"
               "  • 拖拽已存在连线可断开",
               FL_MAJOR_VERSION, FL_MINOR_VERSION);
}

// 右键菜单的弹出已在 MyDesk::handle(FL_PUSH) 中实现(类内静态回调 Menu_DeleteConnCb)

//=============================================================================
// main
//=============================================================================
int main() {
    const int WIN_W = 1000, WIN_H = 660;
    const int PANEL_W = 170;
    const int MENUBAR_H = 25;

    Fl_Double_Window *win = new Fl_Double_Window(WIN_W, WIN_H, "Fl_OpDesk DAG 编辑器");

    // 顶部菜单栏
    Fl_Menu_Bar *menubar = new Fl_Menu_Bar(0, 0, WIN_W, MENUBAR_H);
    menubar->add("文件/退出",  FL_COMMAND + 'q', Menu_Quit);
    menubar->add("帮助/关于",                   0, Menu_About);

    // ---- 左侧节点库面板 ----
    Fl_Group *panel = new Fl_Group(0, MENUBAR_H, PANEL_W, WIN_H - MENUBAR_H);
    panel->box(FL_THIN_DOWN_BOX);
    {
        Fl_Box *title = new Fl_Box(0, MENUBAR_H + 8, PANEL_W, 24, "节点库");
        title->labelsize(14);
        title->labelfont(FL_HELVETICA_BOLD);

        int btnY = MENUBAR_H + 42;
        const int btnH = 56;
        const int btnGap = 12;
        const int btnW = PANEL_W - 24;
        const int btnX = 12;

        new NodeButton(btnX, btnY + 0*(btnH+btnGap), btnW, btnH,
                       "因子\n(Factor)", NODE_FACTOR);
        new NodeButton(btnX, btnY + 1*(btnH+btnGap), btnW, btnH,
                       "数据源\n(Data Source)", NODE_DATA_SOURCE);
        new NodeButton(btnX, btnY + 2*(btnH+btnGap), btnW, btnH,
                       "ML模型\n(ML Model)", NODE_ML_MODEL);

        // 操作提示
        Fl_Box *hint = new Fl_Box(8, WIN_H - 130, PANEL_W - 16, 120);
        hint->labelsize(10);
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

    // ---- 右侧画布滚动区 ----
    Fl_Scroll *scroll = new Fl_Scroll(PANEL_W, MENUBAR_H,
                                      WIN_W - PANEL_W, WIN_H - MENUBAR_H);
    scroll->box(FL_DOWN_BOX);

    const int DESK_W = 4000, DESK_H = 3000;
    MyDesk *desk = new MyDesk(0, 0, DESK_W, DESK_H);
    desk->SetConnectStyle(FL_OPCONNECT_STYLE_CURVE);
    g_desk = desk;

    scroll->end();
    desk->SetScroll(scroll);  // 记录 scroll 以便拖放时换算坐标

    // 布局:窗口可缩放时只有画布区跟随缩放
    win->resizable(scroll);
    win->size_range(720, 460);
    win->show();
    return Fl::run();
}
