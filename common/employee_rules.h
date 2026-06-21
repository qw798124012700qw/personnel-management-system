#ifndef PMS_EMPLOYEE_RULES_H
#define PMS_EMPLOYEE_RULES_H

// 控制台版与图形界面版共用的员工字段校验规则（单一事实来源）。
// 仅依赖标准库、以 std::string 表达，不绑定 Qt / SQLite，两端都可包含：
//   - 控制台版直接以 std::string 调用；
//   - 图形界面版把 QString 转 std::string 后调用，确保两端规则完全一致。
// 这样字段格式规则只在这一处定义，避免两套实现产生分歧。

#include <cctype>
#include <string>

namespace pms {

// 性别：只能是“男”或“女”。
inline bool isValidSex(const std::string &value) {
    return value == "男" || value == "女";
}

// 身份证号：15 位或 18 位；除末位（18 位时可为 X/x）外全为数字。
inline bool isValidId(const std::string &value) {
    if (value.size() != 15 && value.size() != 18) {
        return false;
    }
    for (std::size_t i = 0; i < value.size(); ++i) {
        char ch = value[i];
        bool tailX = (i + 1 == value.size()) && (ch == 'X' || ch == 'x');
        if (!std::isdigit(static_cast<unsigned char>(ch)) && !tailX) {
            return false;
        }
    }
    return true;
}

// 电话号码：长度 7–15，只含数字和短横线 '-'。
inline bool isValidPhone(const std::string &value) {
    if (value.size() < 7 || value.size() > 15) {
        return false;
    }
    for (char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch)) && ch != '-') {
            return false;
        }
    }
    return true;
}

// 薪水：非负数字，最多一个小数点，且至少含一位数字（如 8500、8500.50）。
inline bool isMoney(const std::string &value) {
    if (value.empty()) {
        return false;
    }
    bool dotSeen = false;
    bool digitSeen = false;
    for (char ch : value) {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            digitSeen = true;
        } else if (ch == '.' && !dotSeen) {
            dotSeen = true;
        } else {
            return false;
        }
    }
    return digitSeen;
}

} // namespace pms

#endif // PMS_EMPLOYEE_RULES_H
