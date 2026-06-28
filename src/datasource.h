///////////////////////////////////////////////////////////////////////////////
// src/datasource.h
//
//   数据源功能 —— 抽象接口 + 精简版行情记录结构体。
//
//   背景:未来可能接入多种数据源(MySQL / 本地文件 / REST API 等),
//        因此抽象出统一的 IDataSource 接口。当前仅有 MysqlDataSource 实现,
//        用于读取 marketdata.stock_a_spot 表(取 latest_price / prev_close /
//        total_mcap 等关键字段)。
//
//   本模块是纯后端,不依赖 FLTK,不与 DAG 画布耦合。
//   真实 MySQL 实现在 datasource.cpp,通过工厂函数 CreateMysqlDataSource() 创建。
///////////////////////////////////////////////////////////////////////////////
#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <memory>
#include <string>
#include <vector>

//=============================================================================
// 数据源连接配置(数据库无关)
//
//   默认构造时自动从配置文件读取(顺序:config.ini → config.ini.example),
//   找不到的键回退到下面的内置默认值。也可用 LoadDataSourceConfig(path) 显式加载。
//   配置文件格式见仓库根的 config.ini.example,读取 [mysql] 段。
//=============================================================================
struct DataSourceConfig {
    // 默认构造:自动从 config.ini(回退 config.ini.example)加载 [mysql] 段。
    // 文件缺失或解析失败时,各项保留内置默认值。
    DataSourceConfig();

    std::string host      = "127.0.0.1";  // 主机
    unsigned    port      = 3306;          // 端口
    std::string user      = "root";        // 用户名
    std::string password;                  // 密码
    std::string database  = "marketdata";  // 默认库
    std::string charset   = "utf8mb4";     // 字符集(A股名称含生僻字)
    unsigned    connect_timeout = 10;      // 连接超时(秒)
    bool        auto_reconnect    = true;  // 断线自动重连
};

// 从指定 INI 文件加载 [mysql] 段到 DataSourceConfig。
// 文件缺失或键不存在时回退到 DataSourceConfig 内置默认。path 为空则跳过。
// 返回填充后的配置;同时用 spdlog 记录加载过程/错误。
DataSourceConfig LoadDataSourceConfig(const std::string &path);

//=============================================================================
// 行情快照记录 —— 精简版
//
//   仅覆盖 marketdata.stock_a_spot 中当前需要的字段。
//   数值字段使用 double 以保留精度;若数据库中为 NULL,取 0.0。
//=============================================================================
struct SpotRecord {
    std::string trade_date;   // 交易日 YYYYMMDD
    std::string code;         // 股票代码
    std::string name;         // 股票名称
    double      latest_price; // 最新价(收盘价)
    double      prev_close;   // 昨收价
    double      total_mcap;   // 总市值(元)
};

//=============================================================================
// IDataSource —— 数据源抽象接口
//
//   所有数据源实现该接口。调用方持有 IDataSource*,不关心底层是 MySQL 还是其它。
//=============================================================================
class IDataSource {
public:
    virtual ~IDataSource() {}

    // ---- 连接管理 ----
    // 建立连接。成功返回 true。失败时可通过 LastError() 取原因。
    virtual bool Connect(const DataSourceConfig &cfg) = 0;
    // 连接是否处于活跃状态。
    virtual bool IsConnected() const = 0;
    // 断开连接,释放资源。
    virtual void Disconnect() = 0;
    // 最近一次错误信息(连接/查询失败时设置)。无错误返回空串。
    virtual std::string LastError() const = 0;
    // 数据源类型标识,用于日志(如 "MySQL")。
    virtual std::string TypeName() const = 0;

    // ---- 行情查询(针对 stock_a_spot) ----
    // 按交易日查询全部股票快照。trade_date 格式 YYYYMMDD。
    // 成功返回 true 并填充 out;无数据或出错返回 false(查看 LastError)。
    virtual bool GetSpotByDate(const std::string &trade_date,
                               std::vector<SpotRecord> &out) = 0;
    // 按股票代码查询最近一条快照。code 为股票代码(如 "600519")。
    virtual bool GetSpotByCode(const std::string &code,
                               SpotRecord &out) = 0;
};

//=============================================================================
// 工厂函数 —— 创建 MySQL 数据源实现
//=============================================================================
std::unique_ptr<IDataSource> CreateMysqlDataSource();

//=============================================================================
// 命名数据源 —— 多数据源管理(config.ini 里可配置多个)
//
//   config.ini 中每个数据源占一个段,段名前缀 "source.",例如:
//     [source.主库]
//     name = 主库
//     type = mysql          ; 或 sqlite
//     host = 192.168.31.9   ; mysql 专用
//     port = 3306
//     user = root
//     password = xxx
//     database = marketdata
//     charset = utf8mb4
//     path = D:/data/a.db   ; sqlite 专用
//
//   节点(源节点)通过下拉框选择已配置的数据源名称,不自行保存连接信息。
//=============================================================================
struct NamedDataSource {
    std::string         name;   // 数据源名称(区分不同数据源)
    std::string         type;   // "mysql" 或 "sqlite"
    DataSourceConfig    cfg;    // mysql 连接配置
    std::string         path;   // sqlite 文件路径(type=sqlite 时用)
};

// 从 config.ini 读取所有 [source.*] 段为命名数据源列表(按段名顺序)。
// 文件缺失或无 source 段时返回空列表。
std::vector<NamedDataSource> LoadAllDataSources(const std::string &path);

// 仅取所有命名数据源的名称(供下拉框等使用)。
std::vector<std::string> ListDataSourceNames(const std::string &path);

// 追加一个命名数据源到 config.ini(写 [source.<name>] 段)。
// 不做去重;成功返回 true。文件不可写返回 false。
bool AppendDataSource(const std::string &path, const NamedDataSource &ds);

#endif // DATASOURCE_H
