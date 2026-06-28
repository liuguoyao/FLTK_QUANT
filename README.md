# Fl_OpDesk DAG 编辑器

基于 [FLTK](https://www.fltk.org/) 与 [Fl_OpDesk](https://github.com/etorth/Fl_OpDesk) 实现的**可交互 DAG(有向无环图)编辑器**。

Fl_OpDesk 是 Greg Ercolano 编写的 FLTK 节点图(node graph)控件(v0.82),本程序在其基础上增加 DAG 约束,使画布始终保持无环结构。

## 界面

程序界面分为左侧「节点库」面板和右侧画布:

```
┌──────────────────────────────────────────────────────┐
│ 文件  帮助                                            │
├──────────┬───────────────────────────────────────────┤
│ 节点库   │                                           │
│          │                                           │
│ ┌──────┐ │     ┌─────────┐         ┌──────────┐      │
│ │ 因子 │ │     │ 因子_1  │         │ 数据源_1 │      │
│ ├──────┤ │     │ Factors │         │   Data   │      │
│ │数据源│ │     └────┬────┘         └────┬─────┘      │
│ ├──────┤ │          │                   │            │
│ │ML模型│ │          │     ┌─────────────▼──────┐     │
│ └──────┘ │          └────►│     ML模型_1       │     │
│          │                │ Features  Predic.. │     │
│ 操作:    │                │ Model              │     │
│ 拖拽节点 │                └────────────────────┘     │
│ 建连线... │                                           │
└──────────┴───────────────────────────────────────────┘
```

### 节点类型

| 节点 | 输入端口 | 输出端口 | 用途 |
|------|----------|----------|------|
| **因子** | (无) | Factors | 因子数据源,作为 DAG 的起点 |
| **数据源** | (无) | Data | 原始数据源,作为 DAG 的起点 |
| **ML模型** | Features, Model | Prediction | 接收特征和模型,输出预测结果 |

## 交互方式

| 操作 | 效果 |
|------|------|
| 从左侧面板拖拽节点到画布 | 在释放位置创建该类型节点 |
| 鼠标拖动盒子标题栏 | 移动盒子 |
| 从「输出」按钮拖拽到另一盒子的「输入」按钮 | 建立连线 |
| 选中盒子后按 `Delete` | 删除盒子(同时断开其所有连线) |
| **右键点击**画布 → 选择「删除最近的连线」 | 删除距点击位置最近的连线 |
| 拖拽已存在的连线 | 断开该连线 |
| 滚动条 / 鼠标滚轮 | 浏览大画布 |

## DAG 约束(自动强制)

连接校验由 `MyButton::Connecting()` 实现,以下连接会被拒绝并弹出提示:

1. **两个输入按钮互连** 或 **两个输出按钮互连** —— 只允许「输出 → 输入」
2. **同一盒子内的按钮互连**
3. **会形成环的连线** —— 通过 DFS 检测:若新边的目标盒子已能沿下游路径到达源盒子,则拒绝

例如,已存在 `因子_1 → ML模型_1` 的连线时,若尝试反向连接 `ML模型_1.Prediction → 因子_1`,会被拒绝,因为这会形成环。

## 依赖

- **C++11** 编译器(MSVC 或 GCC 均可)
- **CMake** ≥ 3.15
- **FLTK** —— 用 MSVC 构建时**无需预装**(从本地 `third_party/fltk` 源码编译);用 MinGW 构建时需预装

### 安装 FLTK(仅 MinGW/Linux/macOS 需要)

| 环境 | 命令 |
|------|------|
| MSYS2 / MinGW-w64 (ucrt64) | `pacman -S mingw-w64-ucrt-x86_64-fltk` |
| Debian / Ubuntu | `sudo apt install libfltk1.3-dev` |
| macOS (Homebrew) | `brew install fltk` |
| Windows (vcpkg) | `vcpkg install fltk` |

## 第三方依赖(third_party/)

`third_party/` 下放四个第三方库,**都不入 git**(`.gitignore` 的 `/third_party` 已忽略),需手动 clone(见下文「获取源码」)。其中 FLTK 与 mariadb-connector-c 由 CMake 用 `FetchContent(SOURCE_DIR ...)` **纯本地**引入 —— 配置时不联网拉取;spdlog 用 `add_subdirectory` 编译为静态库。

| 目录 | 来源 | 版本 / 分支 | 引入方式 |
|------|------|------------|---------|
| `third_party/Fl_OpDesk/` | https://github.com/etorth/Fl_OpDesk.git | **v0.82**(2012-03-23) | vendored:直接编译其 `.C/.H` 为静态库 `fl_opdesk` |
| `third_party/fltk/` | https://github.com/fltk/fltk.git | **master**(= FLTK **1.5.0**) | `FetchContent(SOURCE_DIR ...)` 本地 `add_subdirectory`,产物 `fltk` / `fltk_images` |
| `third_party/mariadb-connector-c/` | https://github.com/mariadb-corporation/mariadb-connector-c.git | **tag `v3.3.2`**(detached HEAD,commit `1bd8c8b`) | `FetchContent(SOURCE_DIR ...)` 本地 `add_subdirectory`,产物 `libmariadb`(DLL) |
| `third_party/spdlog/` | https://github.com/gabime/spdlog.git | **v1.17.0** | `add_subdirectory` 编译为静态库 `spdlog::spdlog`,供 `datasource` 等模块记日志 |

目录结构:

```
third_party/
├── Fl_OpDesk/                 # FLTK 节点图控件(vendored,直接编译)
│   ├── Fl_OpDesk.H/.C
│   ├── Fl_OpBox.H/.C
│   ├── Fl_OpButton.H/.C
│   └── Fl_OpConnect.H
├── fltk/                      # FLTK 本地 clone(master = 1.5.0)
├── mariadb-connector-c/       # MariaDB Connector/C 本地 clone(tag v3.3.2)
└── spdlog/                    # spdlog 本地 clone(v1.17.0)
```

> ⚠️ **mariadb-connector-c 必须钉在 `v3.3.2`**。若切到 `3.4` 分支:`WITH_SSL=OFF` 会被拒绝(3.4 强制 SSL),默认的 SCHANNEL 又会拖入 `winsock2`,与已包含的 `winsock.h` 冲突(`sockaddr` 重定义等)。本项目用明文连接本地 MySQL(`WITH_SSL OFF`),依赖 3.3.2 的这一行为。

> 运行时依赖:`libmariadb` 构建为 **SHARED DLL**。`dag_editor.exe` / `test_datasource.exe` 运行时需要 `libmariadb.dll` 在同目录。`CMakeLists.txt` 用 `add_custom_command(POST_BUILD copy_if_different ...)` 已自动把 DLL 拷到两个 exe 旁。

> **日志(spdlog)**:`datasource` 模块用 spdlog 记日志,输出到**控制台(彩色)+ 滚动文件 `logs/dag_editor.log`**(单文件 5MB、保留 3 个,首次记录时自动建 `logs/` 目录;级别 = info)。`logs/` 已在 `.gitignore` 中忽略。

## 获取源码

克隆本项目后,需把四个第三方库放进 `third_party/`:

```bash
# 1) Fl_OpDesk(vendored,直接编译源码)
git clone https://github.com/etorth/Fl_OpDesk.git third_party/Fl_OpDesk

# 2) FLTK(本地 clone,master 分支 = 1.5.0)
git clone https://github.com/fltk/fltk.git third_party/fltk

# 3) MariaDB Connector/C(本地 clone,务必 checkout 到 v3.3.2)
git clone https://github.com/mariadb-corporation/mariadb-connector-c.git third_party/mariadb-connector-c
cd third_party/mariadb-connector-c
git checkout v3.3.2          # 重要:钉在 v3.3.2,不要停留在 3.4 分支
cd ../..

# 4) spdlog(日志库,v1.x)
git clone https://github.com/gabime/spdlog.git third_party/spdlog
```

> 四个目录都在 `.gitignore` 的 `/third_party` 规则下,不会被提交。

## 构建与运行

本项目的 `CMakeLists.txt` 支持两套编译器,自动按当前编译器选择 FLTK 来源:

| 编译器 | FLTK 来源 | 说明 |
|--------|-----------|------|
| **MSVC (cl.exe)** | 本地 `third_party/fltk`(1.5.0) | `FetchContent(SOURCE_DIR)` 纯本地,不联网;无需预装 FLTK |
| **MinGW (g++)** | 系统已安装的 FLTK(pacman) | GCC ABI,直接 `find_package` |

> ⚠️ **切勿混用**:MSVC 和 MinGW 的 ABI 不兼容。用 VS 打开项目时必须用 MSVC 版 FLTK,
> 否则会遇到 `corecrt.h` / `__asm__` 等编译错误(MSVC 误读 MinGW 头文件)。

### 方式一:Visual Studio(MSVC,推荐)

**用 IDE"打开文件夹":** 直接用 VS 打开本项目文件夹,VS 会自动识别 CMakeLists.txt。
首次配置**不再联网**(三个依赖都是本地源码),但 FLTK 1.5.0 的特性检查 + mariadb 编译仍需约 **70~90 秒**,请耐心等它打印 "Generating done",不要中途取消。

**命令行等效**(需在「Developer Command Prompt for VS」或先 `call vcvars64.bat`):

```bash
cmake -B out/build/x64-Debug -S .
cmake --build out/build/x64-Debug
out/build/x64-Debug/Debug/dag_editor.exe
```

**运行数据源测试**(连真实 MySQL,验证取数):

```bash
out/build/x64-Debug/Debug/test_datasource.exe
# 退出码 0 = 通过(会打印查询到的行情条数 + 样例记录)
```

> 如切换编译器,**务必先删除 `out/build/` 目录**再重新配置,否则生成器不匹配会报错。

### 方式二:MSYS2 / MinGW(g++)

需先通过 pacman 安装 FLTK,并将 ucrt64/bin 加入 PATH(以找到运行时 DLL):

```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"   # 按实际安装路径调整
cmake -B build -G "MinGW Makefiles"
cmake --build build
./build/dag_editor.exe
```

> 注:`datasource` / `test_datasource` 目标当前依赖 MSVC 路径下的 libmariadb;MinGW 下若要用,需另行配置 MariaDB 客户端库。

### 方式三:Linux / macOS

```bash
cmake -B build
cmake --build build
./build/dag_editor
```

## 项目结构

```
.
├── CMakeLists.txt              # 构建脚本:FetchContent(fltk/mariadb,纯本地) + fl_opdesk + spdlog + datasource + dag_editor + test_datasource
├── src/
│   ├── main.cpp                # 程序入口:窗口、菜单、左侧面板、预置拓扑
│   ├── CustomControl.h/.cpp    # MyButton(端口)/ StyledBox(节点)/ MyDesk(画布)/ NodeButton(拖拽源)
│   ├── tool.h/.cpp             # 保存/加载 DAG(.dag)、拓扑排序执行、历史记录
│   └── datasource.h/.cpp       # 数据源后端:IDataSource 抽象 + MysqlDataSource(libmariadb)+ spdlog 日志
├── test/
│   └── test_datasource.cpp     # 控制台测试:连 MySQL 取 stock_a_spot,验证 datasource
├── third_party/                # 第三方库(不入 git,手动 clone;见「第三方依赖」)
│   ├── Fl_OpDesk/              # v0.82
│   ├── fltk/                   # master = 1.5.0
│   ├── mariadb-connector-c/    # tag v3.3.2
│   └── spdlog/                 # v1.17.0
├── AGENTS.md
└── README.md
```

## 实现要点

- **`MyDesk : public Fl_OpDesk`** —— 深色画布(网格背景 + 发光连线),节点工厂(`CreateFormulaInputNode` 等),拖放创建,右键删连线;重写 `handle()`/`draw()`。
- **`StyledBox : public Fl_OpBox`** —— 圆角深色节点(标题栏 + 副标题),端口垂直对齐可校正。
- **`MyButton : public Fl_OpButton`** —— 圆形端口;重写 `Connecting()`,实现 DAG 三大约束(见上)。环检测采用迭代式 DFS,沿输出按钮的连接遍历下游盒子。
- **`datasource`** —— `IDataSource` 抽象接口 + `MysqlDataSource`(libmariadb)实现,`mysql_real_escape_string` 转义防注入,utf8mb4 字符集;用 spdlog 记日志(控制台 + `logs/dag_editor.log`)。
- **`tool`** —— 保存/加载 DAG(`.dag`)、Kahn 拓扑排序执行、执行历史。

