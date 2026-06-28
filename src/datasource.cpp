///////////////////////////////////////////////////////////////////////////////
// src/datasource.cpp
//
//   IDataSource 的 MySQL 实现 —— 基于 MariaDB Connector/C (libmariadb)。
//
//   读取 marketdata.stock_a_spot 表的精简字段:
//     trade_date, code, name, latest_price, prev_close, total_mcap
//
//   安全性:所有外部字符串(trade_date / code)在拼接 SQL 前都经过
//          mysql_real_escape_string 转义,避免 SQL 注入。
//   编码:连接建立后调用 mysql_set_character_set("utf8mb4"),
//        以正确处理 A 股名称中的生僻字(表本身也是 utf8mb4)。
///////////////////////////////////////////////////////////////////////////////
#include "datasource.h"

#include <mysql.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

//=============================================================================
// MysqlDataSource
//=============================================================================
class MysqlDataSource : public IDataSource {
public:
    MysqlDataSource();
    ~MysqlDataSource() override;

    // 禁止拷贝(持有 MYSQL* 句柄)
    MysqlDataSource(const MysqlDataSource &) /*= delete*/;
    MysqlDataSource &operator=(const MysqlDataSource &) /*= delete*/;

    bool Connect(const DataSourceConfig &cfg) override;
    bool IsConnected() const override;
    void Disconnect() override;
    std::string LastError() const override;
    std::string TypeName() const override;

    bool GetSpotByDate(const std::string &trade_date,
                       std::vector<SpotRecord> &out) override;
    bool GetSpotByCode(const std::string &code, SpotRecord &out) override;

private:
    // 把一个外部字符串转义后写入 query_(追加),并追加尾部字面量 tail。
    // 返回 false 表示转义失败(连接异常)。
    bool AppendEscaped(const char *tail, const std::string &value);

    // 执行已构造好的 SQL(query_),成功返回 true。
    bool RunQuery();

    // 从当前结果集中读取所有行,填充 out。
    void ReadAllRows(std::vector<SpotRecord> &out);

    MYSQL       *mysql_ = nullptr;
    std::string  last_error_;
    std::string  query_;        // 复用的 SQL 构造缓冲
};

//=============================================================================
// 辅助:数值解析(数据库 NULL 或非法值 → 0.0)
//=============================================================================
static inline double ParseDouble(const char *s) {
    return (s && *s) ? std::strtod(s, nullptr) : 0.0;
}

//=============================================================================
// 构造 / 析构
//=============================================================================
MysqlDataSource::MysqlDataSource() {}

MysqlDataSource::~MysqlDataSource() {
    Disconnect();
}

//=============================================================================
// 连接管理
//=============================================================================
bool MysqlDataSource::Connect(const DataSourceConfig &cfg) {
    Disconnect();  // 先清理旧连接

    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        last_error_ = "mysql_init() 失败(内存不足)";
        return false;
    }

    // 连接超时
    unsigned timeout = cfg.connect_timeout > 0 ? cfg.connect_timeout : 10;
    mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    // 自动重连
    bool reconnect = cfg.auto_reconnect;
    mysql_options(mysql_, MYSQL_OPT_RECONNECT, &reconnect);
    // 字符集(连接级,影响后续所有字符串)
    if (!cfg.charset.empty())
        mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, cfg.charset.c_str());

    const char *db = cfg.database.empty() ? nullptr : cfg.database.c_str();
    if (!mysql_real_connect(mysql_,
                            cfg.host.c_str(),
                            cfg.user.c_str(),
                            cfg.password.c_str(),
                            db,
                            cfg.port,
                            nullptr,    // unix socket(Windows 不用)
                            0)) {       // client flag
        last_error_ = std::string("mysql_real_connect 失败: ")
                      + mysql_error(mysql_);
        mysql_close(mysql_);
        mysql_ = nullptr;
        return false;
    }

    // 再次显式设置字符集(确保服务端也按 utf8mb4 处理)
    if (!cfg.charset.empty())
        mysql_set_character_set(mysql_, cfg.charset.c_str());

    last_error_.clear();
    return true;
}

bool MysqlDataSource::IsConnected() const {
    return mysql_ != nullptr && mysql_ping(const_cast<MYSQL*>(mysql_)) == 0;
}

void MysqlDataSource::Disconnect() {
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
    last_error_.clear();
}

std::string MysqlDataSource::LastError() const {
    return last_error_;
}

std::string MysqlDataSource::TypeName() const {
    return "MySQL";
}

//=============================================================================
// SQL 构造与执行
//=============================================================================
bool MysqlDataSource::AppendEscaped(const char *tail, const std::string &value) {
    if (!mysql_) return false;
    // 最坏情况:每个字符展开为 2 个字符 + 安全余量
    std::vector<char> esc(value.size() * 2 + 1, '\0');
    unsigned long n = mysql_real_escape_string(mysql_, esc.data(),
                                               value.c_str(),
                                               static_cast<unsigned long>(value.size()));
    query_.append(esc.data(), n);
    query_.append(tail);
    return true;
}

bool MysqlDataSource::RunQuery() {
    if (!mysql_) { last_error_ = "未连接"; return false; }
    if (mysql_query(mysql_, query_.c_str()) != 0) {
        last_error_ = std::string("查询失败: ") + mysql_error(mysql_);
        return false;
    }
    return true;
}

void MysqlDataSource::ReadAllRows(std::vector<SpotRecord> &out) {
    MYSQL_RES *res = mysql_store_result(mysql_);
    if (!res) {
        // 没有结果集(理论上 SELECT 不会走到这里)
        last_error_ = std::string("mysql_store_result 失败: ")
                      + mysql_error(mysql_);
        return;
    }

    // 字段顺序固定为: trade_date, code, name, latest_price, prev_close, total_mcap
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        SpotRecord r;
        r.trade_date   = row[0] ? row[0] : "";
        r.code         = row[1] ? row[1] : "";
        r.name         = row[2] ? row[2] : "";
        r.latest_price = ParseDouble(row[3]);
        r.prev_close   = ParseDouble(row[4]);
        r.total_mcap   = ParseDouble(row[5]);
        out.push_back(std::move(r));
    }
    mysql_free_result(res);
}

//=============================================================================
// 行情查询
//=============================================================================
bool MysqlDataSource::GetSpotByDate(const std::string &trade_date,
                                    std::vector<SpotRecord> &out) {
    out.clear();
    if (!mysql_) { last_error_ = "未连接"; return false; }

    // SELECT trade_date, code, name, latest_price, prev_close, total_mcap
    // FROM stock_a_spot WHERE trade_date = '<esc>'
    query_.assign(
        "SELECT trade_date, code, name, latest_price, prev_close, total_mcap "
        "FROM stock_a_spot WHERE trade_date = '");
    if (!AppendEscaped("'", trade_date)) return false;

    if (!RunQuery()) return false;
    ReadAllRows(out);
    return last_error_.empty();
}

bool MysqlDataSource::GetSpotByCode(const std::string &code, SpotRecord &out) {
    if (!mysql_) { last_error_ = "未连接"; return false; }

    // 取该股票最新一条快照
    query_.assign(
        "SELECT trade_date, code, name, latest_price, prev_close, total_mcap "
        "FROM stock_a_spot WHERE code = '");
    if (!AppendEscaped("' ORDER BY trade_date DESC LIMIT 1", code)) return false;

    if (!RunQuery()) return false;

    std::vector<SpotRecord> rows;
    ReadAllRows(rows);
    if (last_error_.empty() && !rows.empty()) {
        out = rows.front();
        return true;
    }
    if (last_error_.empty())
        last_error_ = "未找到 code=" + code + " 的记录";
    return false;
}

//=============================================================================
// 工厂函数
//=============================================================================
std::unique_ptr<IDataSource> CreateMysqlDataSource() {
    return std::unique_ptr<IDataSource>(new MysqlDataSource());
}
