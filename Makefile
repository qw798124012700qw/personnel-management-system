# 人事管理系统 —— Windows (NUAAProgEnv / clang64) 构建脚本
# 在项目根目录使用 MSYS2 的 make 运行：
#   make            编译控制台版 -> build/personnel_system.exe
#   make run        编译并运行控制台版
#   make gui        构建 Qt 图形界面版 -> qt_gui/release/personnel_gui.exe
#   make run-gui    构建并运行 Qt 图形界面版
#   make clean      清除控制台版可执行文件
#   make clean-gui  清除图形界面版构建产物

CXX      := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -g
SRC      := src/main.cpp src/personnel_system.cpp
HDR      := src/personnel_system.h
BIN      := build/personnel_system.exe
LIBS     := -lsqlite3   # 链接 SQLite 嵌入式数据库

# Qt 图形界面版（Qt5，qmake 名为 qmake-qt5）
GUI_DIR     := qt_gui
GUI_BIN     := $(GUI_DIR)/release/personnel_gui.exe
QT_PLUGINS  := C:/Users/Public/NUAAProgEnv/msys64/clang64/share/qt5/plugins

.PHONY: all run clean gui run-gui clean-gui

# ---------- 控制台版 ----------
all: $(BIN)

$(BIN): $(SRC) $(HDR)
	-mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN) $(LIBS)

run: all
	-mkdir -p data
	./$(BIN)

clean:
	rm -f $(BIN)

# ---------- Qt 图形界面版 ----------
# qmake-qt5 生成 Makefile，mingw32-make 编译，最后把平台插件放到 exe 旁边。
gui:
	cd $(GUI_DIR) && qmake-qt5 personnel_gui.pro && mingw32-make
	-mkdir -p $(GUI_DIR)/release/platforms
	cp "$(QT_PLUGINS)/platforms/qwindows.dll" $(GUI_DIR)/release/platforms/
	-mkdir -p $(GUI_DIR)/release/sqldrivers
	cp "$(QT_PLUGINS)/sqldrivers/qsqlite.dll" $(GUI_DIR)/release/sqldrivers/

# 以 qt_gui 为工作目录运行，使 ../data/employees.txt 指向项目根 data/（与控制台版共用）。
run-gui: gui
	cd $(GUI_DIR) && ./release/personnel_gui.exe

clean-gui:
	cd $(GUI_DIR) && (mingw32-make distclean || true)
	rm -rf $(GUI_DIR)/release $(GUI_DIR)/debug $(GUI_DIR)/.qmake.stash $(GUI_DIR)/Makefile*
