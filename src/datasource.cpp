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
#include "DsLogger.h"

#include <mysql.h>
#include <INIReader.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

//=============================================================================
// DataSourceConfig —— 从 config.ini 加载(顺序:config.ini → config.ini.example)
//=============================================================================
// 把 [mysql] 段读入已有 cfg(在其当前值基础上覆盖)。文件缺失/键缺失时保留原值。
// 返回 true 表示成功从文件读取。
static bool LoadInto(DataSourceConfig &cfg, const std::string &path) {
    if (path.empty()) return false;
    INIReader reader(path);
    int err = reader.ParseError();
    if (err != 0) {
        // -1 = 文件打不开;>0 = 第 err 行解析错误
        if (err < 0)
            DS_LOG_INFO("配置文件 {} 不存在,使用内置默认配置", path);
        else
            DS_LOG_WARN("配置文件 {} 第 {} 行解析错误,使用内置默认配置", path, err);
        return false;
    }

    cfg.host           = reader.Get("mysql", "host", cfg.host);
    cfg.port           = static_cast<unsigned>(
                            reader.GetInteger("mysql", "port", cfg.port));
    cfg.user           = reader.Get("mysql", "user", cfg.user);
    cfg.password       = reader.Get("mysql", "password", cfg.password);
    cfg.database       = reader.Get("mysql", "database", cfg.database);
    cfg.charset        = reader.Get("mysql", "charset", cfg.charset);
    cfg.connect_timeout = static_cast<unsigned>(
                            reader.GetInteger("mysql", "connect_timeout",
                                              cfg.connect_timeout));
    cfg.auto_reconnect = reader.GetBoolean("mysql", "auto_reconnect",
                                           cfg.auto_reconnect);
    DS_LOG_INFO("已加载配置文件 {} (host={} port={} db={} user={})",
                path, cfg.host, cfg.port, cfg.database, cfg.user);
    return true;
}

DataSourceConfig LoadDataSourceConfig(const std::string &path) {
    DataSourceConfig cfg;            // 成员默认初始化(不触发文件加载)
    LoadInto(cfg, path);
    return cfg;
}

DataSourceConfig::DataSourceConfig() {
    // 成员已有默认值。尝试从文件覆盖:
    // 顺序:config.ini(用户编辑版) → config.ini.example(模板)
    static const char *kCandidates[] = { "config.ini", "config.ini.example" };
    for (const char *cand : kCandidates) {
        if (LoadInto(*this, cand)) return;
    }
    DS_LOG_INFO("未找到 config.ini / config.ini.example,使用内置默认配置");
}

//=============================================================================
// 命名数据源 —— 多数据源管理(config.ini 的 [source.*] 段)
//=============================================================================
namespace {
// 段名前缀,例如 [source.主库]
constexpr const char *kSourcePrefix = "source.";

// 判断段名是否为 source.* 并返回去掉前缀后的名称;否则返回空。
std::string SourceNameFromSection(const std::string &section) {
    if (section.compare(0, std::strlen(kSourcePrefix), kSourcePrefix) != 0)
        return std::string();
    return section.substr(std::strlen(kSourcePrefix));
}
}  // namespace

std::vector<NamedDataSource> LoadAllDataSources(const std::string &path) {
    std::vector<NamedDataSource> result;
    if (path.empty()) return result;

    INIReader reader(path);
    if (reader.ParseError() != 0) {
        DS_LOG_INFO("LoadAllDataSources: 无法解析 {}", path);
        return result;
    }

    auto sections = reader.Sections();
    for (const std::string &sec : sections) {
        std::string nm = SourceNameFromSection(sec);
        if (nm.empty()) continue;  // 非 source.* 段(如旧的 [mysql])

        NamedDataSource ds;
        ds.name = reader.Get(sec, "name", nm);
        ds.type = reader.Get(sec, "type", "mysql");
        if (ds.type == "sqlite") {
            ds.path = reader.Get(sec, "path", "");
        } else {
            // mysql:复用 DataSourceConfig 字段
            ds.cfg.host = reader.Get(sec, "host", ds.cfg.host);
            ds.cfg.port = static_cast<unsigned>(
                reader.GetInteger(sec, "port", ds.cfg.port));
            ds.cfg.user     = reader.Get(sec, "user", ds.cfg.user);
            ds.cfg.password = reader.Get(sec, "password", ds.cfg.password);
            ds.cfg.database = reader.Get(sec, "database", ds.cfg.database);
            ds.cfg.charset  = reader.Get(sec, "charset", ds.cfg.charset);
            ds.cfg.connect_timeout = static_cast<unsigned>(
                reader.GetInteger(sec, "connect_timeout", ds.cfg.connect_timeout));
            ds.cfg.auto_reconnect = reader.GetBoolean(sec, "auto_reconnect",
                                                      ds.cfg.auto_reconnect);
        }
        result.push_back(std::move(ds));
    }
    DS_LOG_INFO("LoadAllDataSources: 从 {} 读取 {} 个数据源", path, result.size());
    return result;
}

std::vector<std::string> ListDataSourceNames(const std::string &path) {
    std::vector<std::string> names;
    for (const auto &ds : LoadAllDataSources(path))
        names.push_back(ds.name);
    return names;
}

bool AppendDataSource(const std::string &path, const NamedDataSource &ds) {
    FILE *fp = std::fopen(path.c_str(), "a");
    if (!fp) {
        DS_LOG_ERROR("AppendDataSource: 无法写入 {}", path);
        return false;
    }
    std::fprintf(fp, "\n[source.%s]\n", ds.name.c_str());
    std::fprintf(fp, "name = %s\n", ds.name.c_str());
    std::fprintf(fp, "type = %s\n", ds.type.c_str());
    if (ds.type == "sqlite") {
        std::fprintf(fp, "path = %s\n", ds.path.c_str());
    } else {
        std::fprintf(fp, "host = %s\n",            ds.cfg.host.c_str());
        std::fprintf(fp, "port = %u\n",            ds.cfg.port);
        std::fprintf(fp, "user = %s\n",            ds.cfg.user.c_str());
        std::fprintf(fp, "password = %s\n",        ds.cfg.password.c_str());
        std::fprintf(fp, "database = %s\n",        ds.cfg.database.c_str());
        std::fprintf(fp, "charset = %s\n",         ds.cfg.charset.c_str());
        std::fprintf(fp, "connect_timeout = %u\n", ds.cfg.connect_timeout);
        std::fprintf(fp, "auto_reconnect = %s\n",  ds.cfg.auto_reconnect ? "true" : "false");
    }
    std::fclose(fp);
    DS_LOG_INFO("AppendDataSource: 已追加 [source.{}] 到 {}", ds.name, path);
    return true;
}





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
        DS_LOG_ERROR("Connect 失败: mysql_init() 返回 null(内存不足)");
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
        DS_LOG_ERROR("Connect 失败: {}@{}:{} 库={} 错误={}",
                     cfg.user, cfg.host, cfg.port, cfg.database, last_error_);
        mysql_close(mysql_);
        mysql_ = nullptr;
        return false;
    }

    // 再次显式设置字符集(确保服务端也按 utf8mb4 处理)
    if (!cfg.charset.empty())
        mysql_set_character_set(mysql_, cfg.charset.c_str());

    last_error_.clear();
    DS_LOG_INFO("Connect 成功: {}@{}:{} 库={} 字符集={}",
                cfg.user, cfg.host, cfg.port, cfg.database,
                cfg.charset.empty() ? "default" : cfg.charset);
    return true;
}

bool MysqlDataSource::IsConnected() const {
    return mysql_ != nullptr && mysql_ping(const_cast<MYSQL*>(mysql_)) == 0;
}

void MysqlDataSource::Disconnect() {
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
        DS_LOG_INFO("Disconnect: 已关闭 MySQL 连接");
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
        DS_LOG_ERROR("查询失败: {} SQL=[{}]", last_error_, query_);
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
        DS_LOG_ERROR("mysql_store_result 失败: {}", last_error_);
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
    if (last_error_.empty())
        DS_LOG_INFO("GetSpotByDate('{}') 返回 {} 条记录", trade_date, out.size());
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
        DS_LOG_INFO("GetSpotByCode('{}') 命中: {} {} 最新价={}",
                    code, out.code, out.name, out.latest_price);
        return true;
    }
    if (last_error_.empty()) {
        last_error_ = "未找到 code=" + code + " 的记录";
        DS_LOG_ERROR("GetSpotByCode('{}') 未找到记录", code);
    }
    return false;
}

//=============================================================================
// 工厂函数
//   只负责创建 MysqlDataSource 实例,不越权初始化日志(logger 在首次 DS_LOG_*
//   调用时自行懒初始化,见 DsLogger.cpp)。
//=============================================================================
std::unique_ptr<IDataSource> CreateMysqlDataSource() {
    return std::unique_ptr<IDataSource>(new MysqlDataSource());
}
