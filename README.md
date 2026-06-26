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
- **FLTK** —— 用 MSVC 构建时**无需预装**(CMake 会自动从源码编译);用 MinGW 构建时需预装

### 安装 FLTK(仅 MinGW/Linux/macOS 需要)

| 环境 | 命令 |
|------|------|
| MSYS2 / MinGW-w64 (ucrt64) | `pacman -S mingw-w64-ucrt-x86_64-fltk` |
| Debian / Ubuntu | `sudo apt install libfltk1.3-dev` |
| macOS (Homebrew) | `brew install fltk` |
| Windows (vcpkg) | `vcpkg install fltk` |

## 获取 Fl_OpDesk 源码

Fl_OpDesk 作为 vendored 依赖,需 clone 到 `third_party/`:

```bash
git clone https://github.com/etorth/Fl_OpDesk.git third_party/Fl_OpDesk
```

> `third_party/Fl_OpDesk/` 已在 `.gitignore` 中忽略。

## 构建与运行

本项目的 `CMakeLists.txt` 支持两套编译器,自动按当前编译器选择 FLTK 来源:

| 编译器 | FLTK 来源 | 说明 |
|--------|-----------|------|
| **MSVC (cl.exe)** | FetchContent 从源码编译 FLTK 1.4.0 | 无需预装 FLTK,CMake 自动拉取并用 cl.exe 编译 |
| **MinGW (g++)** | 系统已安装的 FLTK(pacman) | GCC ABI,直接 `find_package` |

> ⚠️ **切勿混用**:MSVC 和 MinGW 的 ABI 不兼容。用 VS 打开项目时必须用 MSVC 版 FLTK,
> 否则会遇到 `corecrt.h` / `__asm__` 等编译错误(MSVC 误读 MinGW 头文件)。

### 方式一:Visual Studio(MSVC,推荐用于 VS2026)

**用 IDE"打开文件夹":** 直接用 VS2026 打开本项目文件夹,VS 会自动识别 CMakeLists.txt,
选择 `Visual Studio 18 2026` 生成器。首次配置会从 GitHub 拉取并编译 FLTK 源码(约需 1~2 分钟)。

**命令行等效:**

```bash
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
./build/Debug/dag_editor.exe
```

> 如切换编译器,**务必先删除 `build/` 目录**再重新配置,否则生成器不匹配会报错。

### 方式二:MSYS2 / MinGW(g++)

需先通过 pacman 安装 FLTK,并将 ucrt64/bin 加入 PATH(以找到运行时 DLL):

```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"   # 按实际安装路径调整
cmake -B build -G "MinGW Makefiles"
cmake --build build
./build/dag_editor.exe
```

### 方式三:Linux / macOS

```bash
cmake -B build
cmake --build build
./build/dag_editor
```

## 项目结构

```
.
├── CMakeLists.txt              # 构建脚本:检测 FLTK、编译 fl_opdesk、链接主程序
├── src/
│   └── main.cpp                # DAG 编辑器主程序(含 MyButton/MyBox/MyDesk 派生类)
├── third_party/
│   └── Fl_OpDesk/              # vendored Fl_OpDesk v0.82(git clone 获取)
│       ├── Fl_OpDesk.H/.C
│       ├── Fl_OpBox.H/.C
│       ├── Fl_OpButton.H/.C
│       └── Fl_OpConnect.H
└── README.md
```

## 实现要点

- **`MyDesk : public Fl_OpDesk`** —— 重写 `ConnectionError()`,将默认输出到 stderr 的错误改为 `fl_alert()` 弹窗。
- **`MyBox : public Fl_OpBox`** —— 使用 `MyButton` 作为端口。
- **`MyButton : public Fl_OpButton`** —— 重写 `Connecting()`,实现 DAG 三大约束。环检测采用迭代式 DFS,沿输出按钮的连接遍历下游盒子。
- 连线样式设为贝塞尔曲线(`FL_OPCONNECT_STYLE_CURVE`)。
```
