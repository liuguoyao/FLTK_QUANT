///////////////////////////////////////////////////////////////////////////////
// test/test_datasource.cpp
//
//   数据源模块的连通性 + 取数测试(非单元测试框架,纯控制台程序)。
//
//   验证三件事:
//     1) 能连上 MySQL(用 DataSourceConfig 里的默认参数)
//     2) GetSpotByDate('20260626') 能取回 stock_a_spot 的精简字段数据
//     3) GetSpotByCode 能取回单只股票最新一条快照
//
//   运行:需要 libmariadb.dll 在同目录(CMake 会自动拷贝)。
//   退出码:0=全部通过,1=连接或查询失败。
///////////////////////////////////////////////////////////////////////////////
#include "../src/datasource.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

// 打印一条 SpotRecord
static void PrintRecord(const SpotRecord &r, size_t idx) {
    printf("[%zu] date=%s code=%-8s name=%-10s "
           "latest=%.2f prevClose=%.2f totalMcap=%.0f\n",
           idx + 1,
           r.trade_date.c_str(),
           r.code.c_str(),
           r.name.c_str(),
           r.latest_price,
           r.prev_close,
           r.total_mcap);
}

int main() {
    std::cout << "=== 数据源测试(datasource) ===\n";

    // ---- 创建数据源(默认参数已由用户在 datasource.h 配好) ----
    auto ds = CreateMysqlDataSource();
    DataSourceConfig cfg;   // 直接用默认值

    std::cout << "连接 " << cfg.host << ":" << cfg.port
              << " 用户=" << cfg.user
              << " 库=" << cfg.database << " ...\n";

    if (!ds->Connect(cfg)) {
        std::cerr << "[FAIL] 连接失败: " << ds->LastError() << "\n";
        return 1;
    }
    std::cout << "[OK] 连接成功,类型=" << ds->TypeName()
              << " 已连接=" << (ds->IsConnected() ? "yes" : "no") << "\n\n";

    // ---- 测试 1:按交易日取全部股票 ----
    const std::string trade_date = "20260626";
    std::vector<SpotRecord> rows;
    std::cout << "---- GetSpotByDate('" << trade_date << "') ----\n";
    if (!ds->GetSpotByDate(trade_date, rows)) {
        std::cerr << "[FAIL] 按日期查询失败: " << ds->LastError() << "\n";
        return 1;
    }

    std::cout << "[OK] 返回 " << rows.size() << " 条记录\n";
    if (rows.empty()) {
        std::cerr << "[WARN] 没有取到数据(确认 trade_date=" << trade_date
                  << " 在表中存在)\n";
        return 1;
    }

    // 只打印前 5 条 + 最后 1 条(避免刷屏)
    const size_t show = 5;
    for (size_t i = 0; i < rows.size() && i < show; ++i)
        PrintRecord(rows[i], i);
    if (rows.size() > show) {
        std::cout << "  ... (" << (rows.size() - show) << " 条省略) ...\n";
        PrintRecord(rows.back(), rows.size() - 1);
    }

    // ---- 测试 2:按股票代码取最新一条 ----
    // 取返回结果里的第一个股票代码做样本
    const std::string sample_code = rows.front().code;
    std::cout << "\n---- GetSpotByCode('" << sample_code << "') ----\n";
    SpotRecord one;
    if (!ds->GetSpotByCode(sample_code, one)) {
        std::cerr << "[FAIL] 按代码查询失败: " << ds->LastError() << "\n";
        return 1;
    }
    std::cout << "[OK] 取到该股票最新一条:\n";
    PrintRecord(one, 0);

    // ---- 断开 ----
    ds->Disconnect();
    std::cout << "\n[OK] 断开连接。全部测试通过。\n";
    return 0;
}
