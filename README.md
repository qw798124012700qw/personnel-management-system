<div align="center">

# 人事管理系统 · Personnel Management System

一个用 **C++17** 编写的人事管理系统,提供**控制台版**与 **Qt 图形界面版**两种界面,功能一致、数据互通。
面向对象程序设计练习项目 —— 演示类设计、SQLite 数据库持久化、输入校验与图形界面开发。

**📚 适合正在做同类课程设计的同学参考**:双界面 + SQLite + 单元测试 + CI 全套工程化,非教科书式的"单文件 + txt"。

![CI](https://github.com/qw798124012700qw/personnel-management-system/actions/workflows/ci.yml/badge.svg)
![Release](https://img.shields.io/github/v/release/qw798124012700qw/personnel-management-system?style=flat-square&color=success)
![Downloads](https://img.shields.io/github/downloads/qw798124012700qw/personnel-management-system/total?style=flat-square)
![Stars](https://img.shields.io/github/stars/qw798124012700qw/personnel-management-system?style=flat-square)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Qt](https://img.shields.io/badge/Qt-5-41CD52?style=flat-square&logo=qt&logoColor=white)
![SQLite](https://img.shields.io/badge/SQLite-3-003B57?style=flat-square&logo=sqlite&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

**[⬇️ 下载 Windows 免安装版](https://github.com/qw798124012700qw/personnel-management-system/releases/latest)** · 解压双击即用,无需装 Qt   |   觉得有用请点个 ⭐ Star 支持一下

</div>

## 📚 目录

- [简介](#-简介) · [界面预览](#️-界面预览) · [功能特性](#-功能特性) · [设计与实现](#-设计与实现)
- [技术栈](#️-技术栈) · [项目结构](#-项目结构) · [快速开始](#-快速开始) · [数据存储](#-数据存储)
- [使用说明](#-使用说明) · [可改进方向](#️-可改进方向-roadmap) · [许可证](#-许可证)

## 📖 简介

当企事业单位的员工信息依靠纸质表格或零散电子表格维护时,容易出现查找慢、修改不及时、数据重复等问题。本系统将员工资料集中管理,支持**录入、查询、修改、删除、排序、统计与文件读写**,并提供命令行与图形界面两种使用方式。

选用 C++ 的原因:面向对象特性适合把"员工、日期、员工列表"抽象为类;标准库的 `vector`、`string`、文件流便于实现动态存储、字符串处理与持久化。

## 🖥️ 界面预览

![图形界面](screenshot-gui.png)

> 上图为图形界面版主表格(含示例数据)。下方还有"员工信息"编辑表单与"添加 / 修改 / 删除 / 保存 / 统计 / 排序"等操作按钮(见[功能特性](#-功能特性))。

## ✨ 功能特性

**数据管理**
- 📝 员工的**增加、删除、修改**(身份证、电话、日期、薪水格式校验;工作证号唯一)
- 📋 **显示**全部员工摘要;**清空**全部记录(需确认)

**查询与排序**
- 🔍 **查找**:按姓名(模糊)或工作证号(精确)
- 🎯 **高级查询**:按部门(精确)、薪水区间(自动校正上下限)、职务关键字(模糊)
- ↕️ **排序**:按工作证号、生日、薪水

**统计与持久化**
- 📊 **统计分析**:部门人数;薪水总额 / 平均 / 最高 / 最低
- 💾 **SQLite 数据库持久化**:控制台版用 SQLite C API、图形界面版用 Qt SQL 模块,**两种界面共用同一个数据库** `data/employees.db`;**首次运行自动从旧文本文件迁移**;读取时自动跳过异常记录

**图形界面专属**
- 🖱️ 表格点击行即载入表单编辑、点列头排序;关键字即时筛选;统计结果弹窗
- ↩️ **撤销**:可逐步回退最近的增 / 删 / 改操作
- 📤 **导出 CSV**:一键导出当前数据(UTF-8 带 BOM,可直接用 Excel / WPS 打开)
- 🪟 **关闭窗口时提示保存**,避免误关丢数据
- 🖥️ **高 DPI 适配** + 窗口尺寸**自适应屏幕**

## 🧩 设计与实现

控制台版采用三个核心类,职责单一、相互解耦:

| 类 | 职责 |
| --- | --- |
| `Date` | 日期的输入、格式化、合法性校验(含闰年判断) |
| `Employee` | 单个员工的字段封装、录入 / 输出 / 序列化 / 修改 |
| `EmployeeList` | 员工集合的增删改查、排序、统计与文件读写(内部用 `vector` 顺序表) |

技术点:运算符重载(`<<` / `>>`)、函数重载、`std::sort` + 静态比较函数、异常处理(文件容错读取)。

图形界面版使用 `QMainWindow` + `QTableView` / `QStandardItemModel` / `QSortFilterProxyModel`,通过信号槽连接按钮事件,与控制台版**共用同一个 SQLite 数据库**(图形界面通过 Qt SQL 模块的 `QSQLITE` 驱动访问)。

## 🛠️ 技术栈

| 部分 | 技术 |
| --- | --- |
| 语言 | C++17(`vector` / `string` / 文件流 / 异常) |
| 图形界面 | Qt 5 Widgets(`qmake` 构建) |
| 数据存储 | SQLite 3(控制台用 C API,图形界面用 Qt 5 Sql 的 `QSQLITE` 驱动;表结构见 `common/db_schema.h`) |
| 测试 / CI | 自带轻量断言框架(`make test`)+ GitHub Actions 自动构建 |
| 设计 | 面向对象、MVC(模型 / 视图 / 代理) |

## 📁 项目结构

```
.
├── src/                  控制台版源码
│   ├── main.cpp
│   ├── personnel_system.h
│   └── personnel_system.cpp
├── qt_gui/               图形界面版源码
│   ├── main.cpp
│   ├── personnel_gui.h
│   ├── personnel_gui.cpp
│   └── personnel_gui.pro
├── common/db_schema.h    两端共用的数据库表结构(单一事实来源)
├── tests/test_core.cpp   核心逻辑单元测试(make test)
├── .github/workflows/    GitHub Actions CI 配置
├── data/employees.txt    种子数据(120 条；首次运行自动导入 SQLite 数据库)
├── Makefile              构建脚本
├── 使用手册.md / 运行说明.md
├── architecture.png      系统框架图
├── screenshot-gui.png    界面截图
└── README.md
```

## 🚀 快速开始

> 💡 **不想编译?** 直接到 [Releases](https://github.com/qw798124012700qw/personnel-management-system/releases/latest) 下载 Windows 免安装包,解压双击 `人事管理系统.exe` 即可运行(无需安装 Qt)。下面是从源码构建的方式。

### 控制台版

```sh
# 需安装 SQLite 开发库(头文件 sqlite3.h + 链接 -lsqlite3)
g++ -std=c++17 -Wall -Wextra src/main.cpp src/personnel_system.cpp -o personnel_system -lsqlite3
./personnel_system
```

或使用 Makefile:`make`(编译) / `make run`(编译并运行) / `make test`(运行单元测试)。

### 图形界面版(需安装 Qt 5)

```sh
cd qt_gui
qmake personnel_gui.pro    # .pro 已含 QT += sql
make                       # Windows(MinGW)上用 mingw32-make
```

> 图形界面通过 Qt SQL 模块访问数据库;`QSQLITE` 驱动随 Qt 提供,运行时需位于 `sqldrivers/`(`make gui` 会自动部署)。

## 📂 数据存储

运行数据保存在 SQLite 数据库 `data/employees.db`(两端共用,表结构见 `common/db_schema.h`)。
首次运行时若数据库为空,会自动从同目录的**种子文本文件** `data/employees.txt` 迁移并写回数据库。

种子文本文件为 UTF-8,每个员工占一行,10 个字段以 `|` 分隔:

```
姓名|性别|身份证号|生日(YYYY-MM-DD)|电话|工作证号|家庭地址|薪水|职务|部门
```

示例:

```
张三|男|110101199001011234|1990-01-01|13800138000|1001|北京市海淀区中关村|8500|软件工程师|研发部
```

字段中的 `|` 与 `\` 会被转义;迁移 / 读取时遇到字段数错误、日期非法、薪水非数字或工作证号重复的行会自动跳过。

## 🏗️ 系统框架

![系统总框架图](architecture.png)

## 📖 使用说明

- 详细操作:见 [使用手册.md](使用手册.md)
- 构建 / 运行环境:见 [运行说明.md](运行说明.md)

## 🗺️ 可改进方向 (Roadmap)

- [x] 图形界面**高 DPI 适配 + 窗口尺寸自适应屏幕**
- [x] 用哈希表(`unordered_map`)为工作证号建索引,把查找 / 查重从 `O(n²)` 优化到 `O(1)` / `O(n)`,支撑更大数据量
- [x] 数据存储改用 **SQLite**(控制台 C API + 图形界面 `QSQLITE` 驱动,共用 `data/employees.db`,首次自动迁移)
- [x] 抽出共享数据库表结构 `common/db_schema.h`,消除两端 schema 重复
- [x] 图形界面增加 **导出 CSV** 与**撤销**操作
- [x] 引入**单元测试**(自带轻量断言框架)+ **GitHub Actions CI** 持续构建
- [ ] 进一步统一两端的员工数据模型(目前 schema 已统一,领域类仍各自实现)
- [ ] 把按部门 / 薪水区间等查询下推到 SQL(当前在内存中完成,数据量大时受益)
- [ ] 图形界面记忆列宽与排序状态;撤销 / 重做支持多级与快捷键

## 📄 许可证

本项目采用 [MIT License](LICENSE) 开源,仅供学习与交流。
