#include "tool.h"
#include <cstdio>
#include <ctime>
#include <map>
#include <queue>
#include <FL/fl_ask.H>

static std::vector<ExecHistoryEntry> g_history;

bool SaveDAGToFile(MyDesk *desk, const char *filepath) {
    FILE *fp = fopen(filepath, "w");
    if (!fp) return false;
    fprintf(fp, "VERSION 1\n");
    int nBoxes = desk->GetOpBoxTotal();
    for (int i = 0; i < nBoxes; ++i) {
        Fl_OpBox *box = desk->GetOpBox(i);
        if (!box) continue;
        fprintf(fp, "NODE %s %d %d\n", box->label() ? box->label() : "", box->x(), box->y());
    }
    int nConns = desk->GetConnectionsTotal();
    for (int i = 0; i < nConns; ++i) {
        Fl_OpConnect *c = desk->GetConnection(i);
        if (!c) continue;
        Fl_OpButton *src = c->GetSrcButton(), *dst = c->GetDstButton();
        if (!src || !dst) continue;
        fprintf(fp, "CONN %s %s %s %s\n",
                src->GetOpBox()->label() ? src->GetOpBox()->label() : "",
                src->label() ? src->label() : "",
                dst->GetOpBox()->label() ? dst->GetOpBox()->label() : "",
                dst->label() ? dst->label() : "");
    }
    fclose(fp);
    return true;
}

bool LoadDAGFromFile(MyDesk *desk, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return false;
    char line[512];
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return false; }
    desk->begin();
    char label[256], srcLabel[256], srcBut[128], dstLabel[256], dstBut[128];
    int x, y;
    std::map<std::string, StyledBox*> created;
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "NODE %255s %d %d", label, &x, &y) == 3) {
            StyledBox *box = nullptr;
            if (strstr(label, "公式输入") || strstr(label, "FORMULA_INPUT"))
                box = desk->CreateFormulaInputNode(x, y, label);
            else if (strstr(label, "特征工程") || strstr(label, "FEATURE_ENGINEERING"))
                box = desk->CreateFeatureEngineeringNode(x, y, label);
            else if (strstr(label, "XGBoost") || strstr(label, "XGBOOST_MODEL"))
                box = desk->CreateXGBoostModelNode(x, y, label);
            if (box) { box->SetPinColorForAllButtons(); created[label] = box; }
        } else if (sscanf(line, "CONN %255s %127s %255s %127s", srcLabel, srcBut, dstLabel, dstBut) == 4) {
            auto si = created.find(srcLabel), di = created.find(dstLabel);
            if (si != created.end() && di != created.end()) {
                std::string err;
                desk->Connect(si->second, srcBut, di->second, dstBut, err);
            }
        }
    }
    fclose(fp);
    desk->end();
    desk->redraw();
    return true;
}

std::string ExecuteDAG(MyDesk *desk) {
    int n = desk->GetOpBoxTotal();
    if (n == 0) return "画布为空。\n";
    std::map<Fl_OpBox*, int> indegree;
    std::map<Fl_OpBox*, std::vector<Fl_OpBox*>> outEdges;
    for (int i = 0; i < n; ++i) indegree[desk->GetOpBox(i)] = 0;
    for (int i = 0; i < desk->GetConnectionsTotal(); ++i) {
        Fl_OpConnect *c = desk->GetConnection(i);
        if (!c) continue;
        Fl_OpButton *src = c->GetSrcButton(), *dst = c->GetDstButton();
        if (!src || !dst) continue;
        Fl_OpBox *sb = src->GetOpBox(), *db = dst->GetOpBox();
        if (sb && db) { outEdges[sb].push_back(db); indegree[db]++; }
    }
    std::queue<Fl_OpBox*> q;
    for (auto &kv : indegree)
        if (kv.second == 0) q.push(kv.first);
    std::string log;
    time_t now = time(0);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    log += "=== DAG 执行 " + std::string(ts) + " ===\n";
    int executed = 0;
    while (!q.empty()) {
        Fl_OpBox *box = q.front(); q.pop();
        log += std::string("  [") + std::to_string(++executed) + "] " + (box->label() ? box->label() : "") + "\n";
        for (Fl_OpBox *next : outEdges[box])
            if (--indegree[next] == 0) q.push(next);
    }
    if (executed < n)
        log += "⚠ 图中存在环，仅执行 " + std::to_string(executed) + "/" + std::to_string(n) + "\n";
    else
        log += "✓ 全部 " + std::to_string(n) + " 个节点执行完毕。\n";
    ExecHistoryEntry entry;
    entry.timestamp = ts; entry.log = log; entry.node_count = n; entry.conn_count = desk->GetConnectionsTotal();
    g_history.push_back(entry);
    fl_message("%s", log.c_str());
    return log;
}

const std::vector<ExecHistoryEntry>& GetExecutionHistory() { return g_history; }

void ShowExecutionHistory() {
    if (g_history.empty()) { fl_message("暂无执行历史。"); return; }
    std::string msg;
    char buf[1024];
    for (size_t i = 0; i < g_history.size(); ++i) {
        auto &e = g_history[i];
        snprintf(buf, sizeof(buf), "─── [%zu] %s ───\n节点: %d  连线: %d\n%s\n", i+1, e.timestamp.c_str(), e.node_count, e.conn_count, e.log.c_str());
        msg += buf;
    }
    fl_message("%s", msg.c_str());
}
