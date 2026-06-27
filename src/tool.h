#ifndef TOOL_H
#define TOOL_H

#include <string>
#include <vector>
#include "CustomControl.h"

bool SaveDAGToFile(MyDesk *desk, const char *filepath);
bool LoadDAGFromFile(MyDesk *desk, const char *filepath);
std::string ExecuteDAG(MyDesk *desk);

struct ExecHistoryEntry {
    std::string timestamp;
    std::string log;
    int node_count;
    int conn_count;
};
const std::vector<ExecHistoryEntry>& GetExecutionHistory();
void ShowExecutionHistory();

#endif
