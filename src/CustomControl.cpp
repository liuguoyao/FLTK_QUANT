///////////////////////////////////////////////////////////////////////////////
// src/CustomControl.cpp
//
//   自定义控件与画布的实现。详见 CustomControl.h。
///////////////////////////////////////////////////////////////////////////////
#include "CustomControl.h"

//=============================================================================
// 全局颜色常量定义
//=============================================================================
const Fl_Color CLR_BG         = fl_rgb_color(0x1E, 0x1E, 0x1E);
const Fl_Color CLR_GRID       = fl_rgb_color(0x2A, 0x2A, 0x2A);
const Fl_Color CLR_CONN       = fl_rgb_color(0x88, 0xC0, 0xA0);
const Fl_Color CLR_GLOW       = fl_rgb_color(0x33, 0xCC, 0x66);
const Fl_Color CLR_PIN_GREEN  = fl_rgb_color(0x33, 0xCC, 0x66);
const Fl_Color CLR_PIN_GREY   = fl_rgb_color(0xC0, 0xC0, 0xC0);
const Fl_Color CLR_TITLE_DARK = fl_rgb_color(0x2A, 0x2A, 0x2A);
const Fl_Color CLR_TEXT_LT    = fl_rgb_color(0xE0, 0xE0, 0xE0);
const Fl_Color CLR_TEXT_DIM   = fl_rgb_color(0x88, 0x88, 0x88);
const Fl_Color CLR_INPUT_BG   = fl_rgb_color(0x14, 0x14, 0x14);
const Fl_Color CLR_GREEN_THEME= fl_rgb_color(0x2E, 0x4A, 0x35);
const Fl_Color CLR_BROWN_THEME= fl_rgb_color(0x4A, 0x32, 0x32);

const int TITLE_H = 24;

//=============================================================================
// NodeType 辅助
//=============================================================================
const char *kNodeTypeNames[] = {
    "FORMULA_INPUT",
    "FEATURE_ENGINEERING",
    "XGBOOST_MODEL"
};

const char *NodeDisplayName(NodeType t) {
    switch (t) {
        case NODE_FORMULA_INPUT:       return "公式输入";
        case NODE_FEATURE_ENGINEERING: return "特征工程";
        case NODE_XGBOOST_MODEL:       return "XGBoost模型";
        default: return "未知";
    }
}

int NodeTypeFromName(const char *name) {
    for (int i = 0; i < NODE_TYPE_COUNT; ++i)
        if (strcmp(name, kNodeTypeNames[i]) == 0) return i;
    return -1;
}

//=============================================================================
// MyButton —— 实现
//=============================================================================
MyButton::MyButton(const char *L, Fl_OpButtonType io)
    : Fl_OpButton(L, io), pin_color_(CLR_PIN_GREEN) { }

void MyButton::SetPinColor(Fl_Color c) { pin_color_ = c; }

void MyButton::draw() {
    // 不调用 Fl_Button::draw(),完全自绘圆形端口
    int cx = x() + w() / 2;
    int cy = y() + h() / 2;
    int r = 6;

    // 外圈深色描边
    fl_color(fl_rgb_color(0x11, 0x11, 0x11));
    fl_pie(cx - r - 1, cy - r - 1, (r + 1) * 2, (r + 1) * 2, 0, 360);

    // 主体圆
    fl_color(pin_color_);
    fl_pie(cx - r, cy - r, r * 2, r * 2, 0, 360);

    // 端口标签
    fl_color(CLR_TEXT_LT);
    fl_font(FL_HELVETICA, 10);
    if (GetButtonType() == FL_OP_INPUT_BUTTON) {
        fl_draw(label(), x() + w() + 4, y(), 0, h(), FL_ALIGN_LEFT);
    } else {
        fl_draw(label(), x() - 4, y(), 0, h(), FL_ALIGN_RIGHT);
    }
}

int MyButton::Connecting(Fl_OpButton *to, std::string &errmsg) {
    Fl_OpButtonType myType = this->GetButtonType();
    Fl_OpButtonType toType = to->GetButtonType();
    if (myType == FL_OP_INPUT_BUTTON && toType == FL_OP_INPUT_BUTTON) {
        errmsg = "不能连接两个输入按钮"; return -1;
    }
    if (myType == FL_OP_OUTPUT_BUTTON && toType == FL_OP_OUTPUT_BUTTON) {
        errmsg = "不能连接两个输出按钮"; return -1;
    }
    MyButton *src = (myType == FL_OP_OUTPUT_BUTTON) ? this : (MyButton*)to;
    MyButton *dst = (myType == FL_OP_OUTPUT_BUTTON) ? (MyButton*)to : this;
    Fl_OpBox *srcBox = src->GetOpBox();
    Fl_OpBox *dstBox = dst->GetOpBox();
    if (srcBox == dstBox) { errmsg = "不能连接同一个节点内的按钮"; return -1; }
    if (WouldCreateCycle(dstBox, srcBox)) {
        errmsg = "这条连线会形成环(DAG 不允许)"; return -1;
    }
    return 0;
}

bool MyButton::WouldCreateCycle(Fl_OpBox *start, Fl_OpBox *target) {
    if (!start || !target) return false;
    std::vector<Fl_OpBox*> stack, visited;
    stack.push_back(start);
    while (!stack.empty()) {
        Fl_OpBox *cur = stack.back(); stack.pop_back();
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
                Fl_OpBox *ds = cb ? cb->GetOpBox() : nullptr;
                if (ds) stack.push_back(ds);
            }
        }
    }
    return false;
}

//=============================================================================
// StyledBox —— 实现
//=============================================================================
StyledBox::StyledBox(int X, int Y, int W, int H, const char *L,
                     Fl_Color themeColor, Fl_Color pinColor)
    : Fl_OpBox(X, Y, W, H, L), theme_color_(themeColor), pin_color_(pinColor) {
    color(themeColor);
    box(FL_FLAT_BOX);
}

void StyledBox::SetPinColorForAllButtons() {
    for (int i = 0; i < GetTotalOutputButtons(); ++i) {
        MyButton *b = (MyButton*)GetOutputButton(i);
        b->SetPinColor(pin_color_);
    }
}

bool StyledBox::IsInTitleBar(int mx, int my) const {
    return mx >= x() && mx <= x() + w() && my >= y() && my <= y() + TITLE_H;
}

void StyledBox::draw() {
    // 1. 圆角背景
    fl_color(theme_color_);
    fl_rounded_rectf(x(), y(), w(), h(), 6);

    // 2. 标题栏(顶部深色条)
    fl_color(CLR_TITLE_DARK);
    fl_push_clip(x(), y(), w(), TITLE_H);
    fl_rectf(x(), y(), w(), TITLE_H + 6);
    fl_pop_clip();

    // 3. 标题栏文字
    fl_color(CLR_TEXT_LT);
    fl_font(FL_HELVETICA_BOLD, 11);
    fl_draw(label(), x() + 8, y(), w() - 30, TITLE_H, FL_ALIGN_LEFT);

    // 4. 右侧编辑图标(小方块)
    int iconX = x() + w() - 18;
    int iconY = y() + 5;
    int iconS = 14;
    fl_color(fl_rgb_color(0x3A, 0x3A, 0x3A));
    fl_rounded_rectf(iconX, iconY, iconS, iconS, 2);
    fl_color(CLR_TEXT_DIM);
    fl_font(FL_HELVETICA, 10);
    fl_draw("...", iconX, iconY, iconS, iconS, FL_ALIGN_CENTER);

    // 5. 画子控件(端口按钮 + 内部 Fl_Input/Fl_Counter 等)
    for (int i = 0; i < children(); ++i) {
        Fl_Widget *c = child(i);
        draw_child(*c);
    }

    // 6. 圆角边框(最后画)
    fl_color(fl_rgb_color(0x0A, 0x0A, 0x0A));
    fl_rounded_rect(x(), y(), w(), h(), 6);
}

//=============================================================================
// MyDesk —— 实现
//=============================================================================
MyDesk::MyDesk(int X, int Y, int W, int H, const char *L)
    : Fl_OpDesk(X, Y, W, H, L), factor_count_(0), feature_count_(0),
      xgb_count_(0), scroll_(nullptr), last_rmb_x_(0), last_rmb_y_(0) {
    color(CLR_BG);
    box(FL_NO_BOX);
    SetConnectStyle(FL_OPCONNECT_STYLE_CURVE);
    SetOpConnectColor(CLR_CONN);
    SetOpConnectWidth(1);
}

void MyDesk::SetScroll(Fl_Scroll *s) { scroll_ = s; }

Fl_Scroll *MyDesk::scroll_parent() { return scroll_; }

void MyDesk::draw() {
    // 1. 深色背景 + 网格
    fl_color(CLR_BG);
    fl_rectf(x(), y(), w(), h());
    fl_color(CLR_GRID);
    fl_line_style(FL_SOLID, 0);
    const int grid = 20;
    for (int gx = x(); gx < x() + w(); gx += grid) {
        fl_begin_line();
        fl_vertex(gx, y());
        fl_vertex(gx, y() + h());
        fl_end_line();
    }
    for (int gy = y(); gy < y() + h(); gy += grid) {
        fl_begin_line();
        fl_vertex(x(), gy);
        fl_vertex(x() + w(), gy);
        fl_end_line();
    }
    fl_line_style(0);

    // 2. 父类 draw 画子控件(节点)+ 原生连线
    Fl_OpDesk::draw();

    // 3. 自定义连线(覆盖原生,加发光点)
    DrawCustomConnections();
}

int MyDesk::handle(int e) {
    switch (e) {
        case FL_DND_ENTER:
        case FL_DND_DRAG:
            return 1;
        case FL_DND_RELEASE:
            return 1;
        case FL_PASTE: {
            std::string text = Fl::event_text();
            int typeIdx = NodeTypeFromName(text.c_str());
            if (typeIdx < 0) return 0;
            int dropX = Fl::event_x() - x();
            int dropY = Fl::event_y() - y();
            Fl_Scroll *scr = scroll_parent();
            if (scr) {
                dropX += scr->xposition();
                dropY += scr->yposition();
            }
            CreateNodeAt((NodeType)typeIdx, dropX, dropY);
            return 1;
        }
        case FL_PUSH: {
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                last_rmb_x_ = Fl::event_x() - x();
                last_rmb_y_ = Fl::event_y() - y();
                Fl_Scroll *scr = scroll_parent();
                if (scr) {
                    last_rmb_x_ += scr->xposition();
                    last_rmb_y_ += scr->yposition();
                }
                Fl_Menu_Item menu[] = {
                    {"删除最近的连线", 0, Menu_DeleteConnCb, this, 0, FL_MENU_DIVIDER},
                    {"清除所有连线",   0, Menu_ClearConnsCb, this, 0, 0},
                    {nullptr, 0, nullptr, nullptr, 0, 0}
                };
                const Fl_Menu_Item *m = menu->popup(Fl::event_x(), Fl::event_y());
                if (m) m->do_callback(this, user_data());
                return 1;
            }
            break;
        }
    }
    return Fl_OpDesk::handle(e);
}

void MyDesk::CreateNodeAt(NodeType type, int x, int y) {
    if (x < 0) x = 10;
    if (y < 0) y = 10;
    begin();
    StyledBox *box = nullptr;
    switch (type) {
        case NODE_FORMULA_INPUT: {
            char title[64];
            snprintf(title, sizeof(title), "公式输入_%d", ++factor_count_);
            box = CreateFormulaInputNode(x, y, title);
            break;
        }
        case NODE_FEATURE_ENGINEERING: {
            char title[64];
            snprintf(title, sizeof(title), "特征工程_%d", ++feature_count_);
            box = CreateFeatureEngineeringNode(x, y, title);
            break;
        }
        case NODE_XGBOOST_MODEL: {
            char title[64];
            snprintf(title, sizeof(title), "XGBoost_%d", ++xgb_count_);
            box = CreateXGBoostModelNode(x, y, title);
            break;
        }
        default: break;
    }
    end();
    if (box) {
        box->SetPinColorForAllButtons();
        redraw();
    }
}

bool MyDesk::DeleteNearestConnection(int mx, int my) {
    int nConn = GetConnectionsTotal();
    if (nConn == 0) { fl_message("当前没有连线可删除。"); return false; }
    Fl_OpConnect *best = nullptr;
    double bestDist = 1e18;
    for (int i = 0; i < nConn; ++i) {
        Fl_OpConnect *c = GetConnection(i);
        if (!c) continue;
        Fl_OpButton *a = c->GetSrcButton();
        Fl_OpButton *b = c->GetDstButton();
        if (!a || !b) continue;
        double midX = (a->x() + a->w()/2 + b->x() + b->w()/2) / 2.0;
        double midY = (a->y() + a->h()/2 + b->y() + b->h()/2) / 2.0;
        double dx = midX - mx, dy = midY - my;
        double d = dx*dx + dy*dy;
        if (d < bestDist) { bestDist = d; best = c; }
    }
    if (best) {
        Disconnect(best->GetSrcButton(), best->GetDstButton());
        redraw();
        return true;
    }
    return false;
}

void MyDesk::DrawCustomConnections() {
    int XC = x() + Fl::box_dx(box());
    int YC = y() + Fl::box_dy(box());
    int WC = (scrollbar.visible() ? w() - scrollbar.w() : w()) - Fl::box_dw(box());
    int HC = (hscrollbar.visible() ? h() - hscrollbar.h() : h()) - Fl::box_dh(box());
    fl_push_clip(XC, YC, WC, HC);
    fl_line_style(FL_SOLID, 1);
    for (int i = 0; i < GetConnectionsTotal(); ++i) {
        Fl_OpConnect *c = GetConnection(i);
        if (!c) continue;
        Fl_OpButton *a = c->GetSrcButton();
        Fl_OpButton *b = c->GetDstButton();
        if (!a || !b) continue;
        int ax = a->x() + a->w() / 2;
        int ay = a->y() + a->h() / 2;
        int bx = b->x() + b->w() / 2;
        int by = b->y() + b->h() / 2;
        fl_color(CLR_CONN);
        fl_begin_line();
        int midX = (ax + bx) / 2;
        fl_curve(ax, ay, midX, ay, midX, by, bx, by);
        fl_end_line();
        // 中点发光点
        int gx = (ax + bx) / 2;
        int gy = (ay + by) / 2;
        fl_color(fl_color_average(CLR_GLOW, CLR_BG, 0.3));
        fl_pie(gx - 6, gy - 6, 12, 12, 0, 360);
        fl_color(CLR_GLOW);
        fl_pie(gx - 4, gy - 4, 8, 8, 0, 360);
    }
    fl_line_style(0);
    fl_pop_clip();
}

void MyDesk::Menu_DeleteConnCb(Fl_Widget*, void *data) {
    MyDesk *self = (MyDesk*)data;
    self->DeleteNearestConnection(self->last_rmb_x_, self->last_rmb_y_);
}

void MyDesk::Menu_ClearConnsCb(Fl_Widget*, void *data) {
    MyDesk *self = (MyDesk*)data;
    self->DisconnectAll();
    self->redraw();
}

//=============================================================================
// 节点工厂实现
//=============================================================================

// 🔹 节点 A:公式输入节点
StyledBox *MyDesk::CreateFormulaInputNode(int x, int y, const char *title) {
    StyledBox *box = new StyledBox(x, y, 200, 60, strdup(title), CLR_GREEN_THEME);
    box->begin();
    {
        Fl_Input *inp = new Fl_Input(30, TITLE_H + 8, 140, 22);
        inp->box(FL_FLAT_BOX);
        inp->color(CLR_INPUT_BG);
        inp->textcolor(CLR_TEXT_DIM);
        inp->textfont(FL_HELVETICA);
        inp->textsize(11);
        inp->value("公式");
        new MyButton("公式", FL_OP_OUTPUT_BUTTON);
    }
    box->end();
    return box;
}

// 🔹 节点 B:特征工程构建节点
StyledBox *MyDesk::CreateFeatureEngineeringNode(int x, int y, const char *title) {
    StyledBox *box = new StyledBox(x, y, 200, 110, strdup(title), CLR_BROWN_THEME);
    box->begin();
    {
        new MyButton("特征公式", FL_OP_INPUT_BUTTON);
        new MyButton("特征工程", FL_OP_OUTPUT_BUTTON);
        Fl_Input *inp1 = new Fl_Input(30, TITLE_H + 8, 140, 22);
        inp1->box(FL_FLAT_BOX);
        inp1->color(CLR_INPUT_BG);
        inp1->textcolor(CLR_TEXT_DIM);
        inp1->textsize(11);
        inp1->value("特征公式");
        Fl_Input *inp2 = new Fl_Input(30, TITLE_H + 8 + 28, 140, 22);
        inp2->box(FL_FLAT_BOX);
        inp2->color(CLR_INPUT_BG);
        inp2->textcolor(CLR_TEXT_DIM);
        inp2->textsize(11);
        inp2->value("标签公式");
    }
    box->end();
    return box;
}

// 🔹 节点 C:XGBoost 模型节点
StyledBox *MyDesk::CreateXGBoostModelNode(int x, int y, const char *title) {
    struct Param { const char *key; double val; };
    static const Param params[] = {
        {"训练开始时间", 20250101},
        {"训练结束时间", 20250301},
        {"决策树数量",   100},
        {"最大深度",     3},
        {"学习率",       0.100},
        {"最小子权值",   1},
        {"Gamma",        0.000},
        {"子样本比例",   1.000},
        {"列采样比例",   1.000},
        {"L1正则化",     0.000},
        {"L2正则化",     1.000},
    };
    const int N = 11;
    const int rowH = 26;
    const int subH = 24;
    int boxH = TITLE_H + subH + N * rowH + 8;
    int boxW = 260;

    StyledBox *box = new StyledBox(x, y, boxW, boxH, strdup(title), CLR_BROWN_THEME,
                                    CLR_PIN_GREY);
    box->begin();
    {
        new MyButton("特征工程", FL_OP_INPUT_BUTTON);
        new MyButton("模型", FL_OP_OUTPUT_BUTTON);

        Fl_Box *subL = new Fl_Box(30, TITLE_H, 90, subH, "特征工程");
        subL->labelsize(9);
        subL->labelcolor(CLR_TEXT_DIM);
        subL->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Box *subR = new Fl_Box(boxW - 90 - 30, TITLE_H, 90, subH, "模型");
        subR->labelsize(9);
        subR->labelcolor(CLR_TEXT_DIM);
        subR->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

        for (int i = 0; i < N; ++i) {
            int ry = TITLE_H + subH + i * rowH;
            Fl_Box *lab = new Fl_Box(30, ry, 95, rowH - 2, params[i].key);
            lab->labelsize(10);
            lab->labelcolor(CLR_TEXT_LT);
            lab->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            Fl_Counter *ctr = new Fl_Counter(130, ry, 110, rowH - 4);
            ctr->box(FL_FLAT_BOX);
            ctr->color(CLR_INPUT_BG);
            ctr->labelcolor(CLR_TEXT_LT);
            ctr->textcolor(CLR_TEXT_LT);
            ctr->textsize(10);
            ctr->value(params[i].val);
            ctr->step(1);
            ctr->lstep(10);
        }
    }
    box->end();
    return box;
}

//=============================================================================
// NodeButton —— 实现
//=============================================================================
NodeButton::NodeButton(int X, int Y, int W, int H, const char *L, NodeType type)
    : Fl_Button(X, Y, W, H, L), type_(type), drag_started_(false) {
    box(FL_RSHADOW_BOX);
    color(fl_rgb_color(0x3A, 0x3A, 0x42));
    labelcolor(CLR_TEXT_LT);
    labelsize(11);
    labelfont(FL_HELVETICA);
}

int NodeButton::handle(int e) {
    switch (e) {
        case FL_PUSH:
            drag_started_ = false;
            push_x_ = Fl::event_x();
            push_y_ = Fl::event_y();
            break;
        case FL_DRAG: {
            int dx = Fl::event_x() - push_x_;
            int dy = Fl::event_y() - push_y_;
            if (!drag_started_ && (dx*dx + dy*dy) > 16) {
                drag_started_ = true;
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
