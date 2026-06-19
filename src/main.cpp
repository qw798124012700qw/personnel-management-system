#include "personnel_system.h"

#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    // Windows 控制台默认使用本地代码页(中文为 GBK)，切换为 UTF-8 以正确显示中文。
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    try {
        // 程序启动时自动读取数据文件，退出时可选择是否保存。
        EmployeeList employees("data/employees.txt");
        employees.load();

        // 主循环：显示菜单 → 读取用户选择(0-11) → 调用对应功能；选 0 时询问是否保存并退出。
        while (true) {
            printMenu();
            int choice = readMenuChoice(0, 11);
            switch (choice) {
                case 1:
                    employees.add();
                    break;
                case 2:
                    employees.remove();
                    break;
                case 3:
                    employees.deleteAll();
                    break;
                case 4:
                    employees.display();
                    break;
                case 5:
                    employees.find();
                    break;
                case 6:
                    employees.modify();
                    break;
                case 7:
                    employees.save();
                    break;
                case 8:
                    employees.load();
                    break;
                case 9:
                    employees.advancedSearch();
                    break;
                case 10:
                    employees.sortMenu();
                    break;
                case 11:
                    employees.statisticsMenu();
                    break;
                case 0:
                    if (askYesNo("是否保存后退出? (y/n): ")) {
                        employees.save();
                    }
                    std::cout << "系统已退出。\n";
                    return 0;
            }
        }
    } catch (const std::runtime_error &ex) {
        // 输入流结束(EOF / Ctrl+Z)时 readLine 会抛出"输入结束"，这里视为正常退出。
        if (std::string(ex.what()) == "输入结束") {
            std::cout << "\n输入结束，系统已退出。\n";
            return 0;
        }
        std::cout << "运行错误: " << ex.what() << '\n';
        return 1;
    }
}
