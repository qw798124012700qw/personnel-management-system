#include "personnel_system.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace std;

namespace {

// 去掉输入字符串首尾空白，避免用户多输入空格导致匹配失败。
string trim(const string &value) {
    size_t first = value.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        return "";
    }
    size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

// 英文内容统一转小写，便于做不区分大小写的查询。
string lowerCopy(string value) {
    transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(tolower(ch));
    });
    return value;
}

string readLine(const string &prompt);

// 判断薪水输入是否合法：允许整数或一位小数点的非负数字。
bool isMoney(const string &value) {
    if (value.empty()) {
        return false;
    }
    bool dotSeen = false;
    bool digitSeen = false;
    for (char ch : value) {
        if (isdigit(static_cast<unsigned char>(ch))) {
            digitSeen = true;
        } else if (ch == '.' && !dotSeen) {
            dotSeen = true;
        } else {
            return false;
        }
    }
    return digitSeen;
}

// 将字符串形式的薪水转换成 double，便于统计和排序。
double toDouble(const string &value) {
    stringstream ss(value);
    double result = 0.0;
    ss >> result;
    return result;
}

// 读取一个合法金额，用于薪水区间查询。
double readMoneyValue(const string &prompt) {
    while (true) {
        string value = readLine(prompt);
        if (isMoney(value)) {
            return toDouble(value);
        }
        cout << "请输入非负数字，例如 5000 或 5000.50。\n";
    }
}

// 文件中用 | 分隔字段；如果字段本身含有 | 或 \，先加反斜杠转义。
string escapeField(const string &value) {
    string result;
    for (char ch : value) {
        if (ch == '\\' || ch == '|') {
            result.push_back('\\');
        }
        result.push_back(ch);
    }
    return result;
}

// 读取文件时按照 | 拆分一行记录，同时还原上面的转义字符。
vector<string> splitRecordLine(const string &line) {
    vector<string> fields;
    string current;
    bool escaped = false;
    for (char ch : line) {
        if (escaped) {
            current.push_back(ch);
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '|') {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (escaped) {
        current.push_back('\\');
    }
    fields.push_back(current);
    return fields;
}

// 读取一整行输入；如果输入流结束，抛出异常让主程序正常退出。
string readLine(const string &prompt) {
    cout << prompt;
    string value;
    if (!getline(cin, value)) {
        throw runtime_error("输入结束");
    }
    return trim(value);
}

// 读取必填项，空字符串不允许通过。
string readRequired(const string &prompt) {
    while (true) {
        string value = readLine(prompt);
        if (!value.empty()) {
            return value;
        }
        cout << "该项不能为空，请重新输入。\n";
    }
}

}  // namespace

// 统一处理 y/n 确认，删除、清空、退出保存都会用到。
bool askYesNo(const string &prompt) {
    while (true) {
        string answer = lowerCopy(readLine(prompt));
        if (answer == "y" || answer == "yes") {
            return true;
        }
        if (answer == "n" || answer == "no") {
            return false;
        }
        cout << "请输入 y 或 n。\n";
    }
}

// 读取菜单编号，并保证编号在指定范围内。
int readMenuChoice(int minValue, int maxValue) {
    while (true) {
        string value = readLine("请选择: ");
        stringstream ss(value);
        int choice = 0;
        char extra = '\0';
        if ((ss >> choice) && !(ss >> extra) && choice >= minValue && choice <= maxValue) {
            return choice;
        }
        cout << "输入无效，请输入 " << minValue << "-" << maxValue << " 之间的数字。\n";
    }
}

Date::Date() : year_(2000), month_(1), day_(1) {}

Date::Date(int year, int month, int day) {
    set(year, month, day);
}

void Date::set(int year, int month, int day) {
    if (!isValid(year, month, day)) {
        throw invalid_argument("日期不合法");
    }
    year_ = year;
    month_ = month;
    day_ = day;
}

void Date::input(const string &prompt) {
    while (true) {
        string text = readLine(prompt);
        try {
            *this = parse(text);
            return;
        } catch (const exception &) {
            cout << "日期格式应为 YYYY-MM-DD，且必须是有效日期。\n";
        }
    }
}

string Date::toString() const {
    stringstream ss;
    ss << setfill('0') << setw(4) << year_ << "-" << setw(2) << month_ << "-" << setw(2) << day_;
    return ss.str();
}

// 转成 20260523 这种数字，方便按生日排序。
int Date::toNumber() const {
    return year_ * 10000 + month_ * 100 + day_;
}

Date Date::parse(const string &text) {
    int year = 0;
    int month = 0;
    int day = 0;
    char dash1 = '\0';
    char dash2 = '\0';
    stringstream ss(text);
    // 按 "年-月-日" 依次读取，并要求两个分隔符确实是 '-'。
    if (!(ss >> year >> dash1 >> month >> dash2 >> day) || dash1 != '-' || dash2 != '-') {
        throw invalid_argument("日期格式错误");
    }
    // 若后面还残留多余字符（如 "2020-1-1x"），同样视为非法。
    char extra = '\0';
    if (ss >> extra) {
        throw invalid_argument("日期包含多余字符");
    }
    return Date(year, month, day);
}

istream &operator>>(istream &in, Date &date) {
    string text;
    in >> text;
    date = Date::parse(text);
    return in;
}

ostream &operator<<(ostream &out, const Date &date) {
    out << date.toString();
    return out;
}

// 闰年规则：能被 400 整除，或能被 4 整除但不能被 100 整除。
bool Date::isLeapYear(int year) {
    return (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
}

// 根据月份返回该月天数，二月需要额外判断闰年。
int Date::daysOfMonth(int year, int month) {
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return days[month];
}

bool Date::isValid(int year, int month, int day) {
    if (year < 1900 || year > 2100 || month < 1 || month > 12) {
        return false;
    }
    return day >= 1 && day <= daysOfMonth(year, month);
}

Employee::Employee() = default;

const string &Employee::name() const {
    return name_;
}

const string &Employee::sex() const {
    return sex_;
}

const string &Employee::id() const {
    return id_;
}

const string &Employee::telephone() const {
    return telephone_;
}

const Date &Employee::birthday() const {
    return birthday_;
}

const string &Employee::number() const {
    return number_;
}

const string &Employee::address() const {
    return address_;
}

const string &Employee::salary() const {
    return salary_;
}

const string &Employee::post() const {
    return post_;
}

const string &Employee::department() const {
    return department_;
}

double Employee::salaryValue() const {
    return toDouble(salary_);
}

// 从键盘录入一个员工的完整信息。
void Employee::input() {
    cout << "\n请输入员工信息\n";
    name_ = readRequired("姓名: ");
    sex_ = readSex("性别(男/女): ");
    id_ = readIdentity("身份证号码: ");
    birthday_.input("生日(YYYY-MM-DD): ");
    telephone_ = readTelephone("电话号码: ");
    number_ = readRequired("工作证号: ");
    department_ = readRequired("部门: ");
    post_ = readRequired("职务: ");
    salary_ = readSalary("薪水: ");
    address_ = readRequired("家庭地址: ");
}

// 用表格形式输出主要信息，适合列表展示。
void Employee::printBrief(ostream &out, int index) const {
    out << left << setw(5) << index
        << setw(14) << name_
        << setw(22) << id_
        << setw(16) << number_
        << setw(14) << birthday_.toString()
        << setw(16) << department_
        << '\n';
}

// 输出完整信息，适合查询结果展示。
void Employee::printDetail(ostream &out) const {
    out << "姓名: " << name_ << '\n'
        << "性别: " << sex_ << '\n'
        << "身份证号码: " << id_ << '\n'
        << "生日: " << birthday_ << '\n'
        << "电话号码: " << telephone_ << '\n'
        << "工作证号: " << number_ << '\n'
        << "部门: " << department_ << '\n'
        << "职务: " << post_ << '\n'
        << "薪水: " << salary_ << '\n'
        << "家庭地址: " << address_ << '\n';
}

// 将员工对象转换成一行文本，写入 data/employees.txt。
string Employee::serialize() const {
    vector<string> fields = {
        name_, sex_, id_, birthday_.toString(), telephone_, number_, address_, salary_, post_, department_};
    stringstream ss;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            ss << '|';
        }
        ss << escapeField(fields[i]);
    }
    return ss.str();
}

// 从文件中的一行文本还原成 Employee 对象。
Employee Employee::deserialize(const string &line) {
    vector<string> fields = splitRecordLine(line);
    if (fields.size() != 10) {
        throw invalid_argument("字段数量错误");
    }
    Employee employee;
    employee.name_ = fields[0];
    employee.sex_ = fields[1];
    employee.id_ = fields[2];
    employee.birthday_ = Date::parse(fields[3]);
    employee.telephone_ = fields[4];
    employee.number_ = fields[5];
    employee.address_ = fields[6];
    employee.salary_ = fields[7];
    employee.post_ = fields[8];
    employee.department_ = fields[9];
    employee.validate();
    return employee;
}

// 修改员工信息，采用二级菜单逐项修改。
void Employee::modify() {
    while (true) {
        cout << "\n正在修改员工: " << name_ << "\n"
             << "1. 姓名\n"
             << "2. 性别\n"
             << "3. 身份证号码\n"
             << "4. 生日\n"
             << "5. 电话号码\n"
             << "6. 工作证号\n"
             << "7. 部门\n"
             << "8. 职务\n"
             << "9. 薪水\n"
             << "10. 家庭地址\n"
             << "0. 返回\n";
        int choice = readMenuChoice(0, 10);
        if (choice == 0) {
            return;
        }
        // 修改选中的字段，每一项都复用与录入时相同的校验函数。
        switch (choice) {
            case 1:
                name_ = readRequired("新姓名: ");
                break;
            case 2:
                sex_ = readSex("新性别(男/女): ");
                break;
            case 3:
                id_ = readIdentity("新身份证号码: ");
                break;
            case 4:
                birthday_.input("新生日(YYYY-MM-DD): ");
                break;
            case 5:
                telephone_ = readTelephone("新电话号码: ");
                break;
            case 6:
                number_ = readRequired("新工作证号: ");
                break;
            case 7:
                department_ = readRequired("新部门: ");
                break;
            case 8:
                post_ = readRequired("新职务: ");
                break;
            case 9:
                salary_ = readSalary("新薪水: ");
                break;
            case 10:
                address_ = readRequired("新家庭地址: ");
                break;
        }
        cout << "修改成功。\n";
    }
}

void Employee::modifyNumberOnly(const string &number) {
    number_ = number;
}

ostream &operator<<(ostream &out, const Employee &employee) {
    employee.printDetail(out);
    return out;
}

string Employee::readSex(const string &prompt) {
    while (true) {
        string value = readRequired(prompt);
        string lowerValue = lowerCopy(value);
        if (value == "男" || value == "女" || lowerValue == "m" || lowerValue == "f") {
            if (lowerValue == "m") {
                return "男";
            }
            if (lowerValue == "f") {
                return "女";
            }
            return value;
        }
        cout << "性别只能输入 男 或 女。\n";
    }
}

// 身份证简单校验：15 位或 18 位，18 位最后一位可以是 X。
string Employee::readIdentity(const string &prompt) {
    while (true) {
        string value = readRequired(prompt);
        bool lengthOk = value.size() == 15 || value.size() == 18;
        bool charsOk = true;
        for (size_t i = 0; i < value.size(); ++i) {
            char ch = value[i];
            if (!isdigit(static_cast<unsigned char>(ch)) && !(i == value.size() - 1 && (ch == 'X' || ch == 'x'))) {
                charsOk = false;
                break;
            }
        }
        if (lengthOk && charsOk) {
            if (!value.empty() && value.back() == 'x') {
                value.back() = 'X';
            }
            return value;
        }
        cout << "身份证号码应为 15 位或 18 位，最后一位可为 X。\n";
    }
}

// 电话号码允许数字和短横线，长度控制在常见范围内。
string Employee::readTelephone(const string &prompt) {
    while (true) {
        string value = readRequired(prompt);
        bool ok = value.size() >= 7 && value.size() <= 15;
        for (char ch : value) {
            if (!isdigit(static_cast<unsigned char>(ch)) && ch != '-') {
                ok = false;
            }
        }
        if (ok) {
            return value;
        }
        cout << "电话号码长度应为 7-15 位，只能包含数字和短横线。\n";
    }
}

// 薪水保存为字符串，便于文件格式统一；统计时再转 double。
string Employee::readSalary(const string &prompt) {
    while (true) {
        string value = readRequired(prompt);
        if (isMoney(value)) {
            return value;
        }
        cout << "薪水应为非负数字，例如 8500 或 8500.50。\n";
    }
}

// 从文件读取后做一次基本校验，防止异常数据进入系统。
void Employee::validate() const {
    if (name_.empty() || sex_.empty() || id_.empty() || telephone_.empty() || number_.empty() ||
        address_.empty() || salary_.empty() || post_.empty() || department_.empty()) {
        throw invalid_argument("员工记录包含空字段");
    }
    if (!isMoney(salary_)) {
        throw invalid_argument("薪水格式错误");
    }
}

EmployeeList::EmployeeList(string fileName) : fileName_(std::move(fileName)) {}

void EmployeeList::add() {
    Employee employee;
    employee.input();
    if (findByNumber(employee.number()) != employees_.end()) {
        cout << "工作证号已存在，添加失败。\n";
        return;
    }
    employees_.push_back(employee);
    cout << "添加成功。\n";
}

// 显示所有员工的摘要信息。
void EmployeeList::display() const {
    if (employees_.empty()) {
        cout << "暂无员工信息。\n";
        return;
    }
    printBriefHeader(cout);
    for (size_t i = 0; i < employees_.size(); ++i) {
        employees_[i].printBrief(cout, static_cast<int>(i + 1));
    }
}

// 普通查找：姓名模糊匹配，工作证号精确匹配。
void EmployeeList::find() const {
    string key = readRequired("请输入要查找的员工姓名或工作证号: ");
    vector<const Employee *> matches = findMatches(key);
    if (matches.empty()) {
        cout << "未找到匹配员工。\n";
        return;
    }
    cout << "找到 " << matches.size() << " 条记录:\n";
    for (size_t i = 0; i < matches.size(); ++i) {
        cout << "\n[" << i + 1 << "]\n";
        matches[i]->printDetail(cout);
    }
}

// 修改后额外检查工作证号是否重复，重复则恢复旧工作证号。
void EmployeeList::modify() {
    Employee *employee = chooseEmployee("请输入要修改的员工姓名或工作证号: ");
    if (employee == nullptr) {
        return;
    }
    string oldNumber = employee->number();  // 记下旧工作证号，便于改重复后回滚
    employee->modify();
    // 修改后检查新的工作证号是否与其他员工重复。
    bool duplicated = false;
    for (size_t i = 0; i < employees_.size(); ++i) {
        if (&employees_[i] != employee && employees_[i].number() == employee->number()) {
            duplicated = true;
            break;
        }
    }
    if (duplicated) {
        cout << "修改后的工作证号与其他员工重复，已恢复原工作证号。\n";
        employee->modifyNumberOnly(oldNumber);
    }
}

// 删除时先查找，再让用户确认，避免误删。
void EmployeeList::remove() {
    string key = readRequired("请输入要删除的员工姓名或工作证号: ");
    vector<size_t> matches = findMatchIndexes(key);
    if (matches.empty()) {
        cout << "未找到匹配员工。\n";
        return;
    }
    size_t index = chooseIndex(matches);
    if (index == employees_.size()) {
        return;
    }
    if (askYesNo("确认删除员工 " + employees_[index].name() + " ? (y/n): ")) {
        employees_.erase(employees_.begin() + static_cast<long>(index));
        cout << "删除成功。\n";
    } else {
        cout << "已取消删除。\n";
    }
}

// 清空所有员工信息，属于危险操作，必须确认。
void EmployeeList::deleteAll() {
    if (employees_.empty()) {
        cout << "暂无员工信息。\n";
        return;
    }
    if (askYesNo("确认清空全部员工信息? 此操作不可撤销 (y/n): ")) {
        employees_.clear();
        cout << "全部员工信息已清空。\n";
    } else {
        cout << "已取消清空。\n";
    }
}

// 保存到文本文件，每个员工占一行。
void EmployeeList::save() const {
    ofstream out(fileName_);
    if (!out) {
        cout << "无法打开文件 " << fileName_ << " 进行保存。\n";
        return;
    }
    for (const Employee &employee : employees_) {
        out << employee.serialize() << '\n';
    }
    cout << "已保存 " << employees_.size() << " 条员工信息到 " << fileName_ << "。\n";
}

// 从文本文件读取员工，跳过格式错误或工作证号重复的记录。
void EmployeeList::load() {
    ifstream in(fileName_);
    if (!in) {
        cout << "数据文件 " << fileName_ << " 不存在，将从空列表开始。\n";
        return;
    }
    // loaded 暂存成功解析的员工；lineNumber 记录行号便于报错；skipped 统计被跳过的坏行。
    vector<Employee> loaded;
    string line;
    int lineNumber = 0;
    int skipped = 0;
    while (getline(in, line)) {
        ++lineNumber;
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        try {
            // 解析这一行；字段数、日期或薪水非法时 deserialize 会抛异常。
            Employee employee = Employee::deserialize(line);
            // 检查工作证号是否与已读入的员工重复。
            bool duplicated = false;
            for (size_t i = 0; i < loaded.size(); ++i) {
                if (loaded[i].number() == employee.number()) {
                    duplicated = true;
                    break;
                }
            }
            if (!duplicated) {
                loaded.push_back(employee);
            } else {
                ++skipped;
                cout << "跳过第 " << lineNumber << " 行：工作证号重复。\n";
            }
        } catch (const exception &ex) {
            // 捕获异常并跳过这一行，不影响其余正常记录的读取。
            ++skipped;
            cout << "跳过第 " << lineNumber << " 行：" << ex.what() << "。\n";
        }
    }
    // 全部解析完成后再整体替换，避免读到一半就破坏已有数据。
    employees_ = loaded;
    cout << "已读取 " << employees_.size() << " 条员工信息";
    if (skipped > 0) {
        cout << "，跳过 " << skipped << " 条异常记录";
    }
    cout << "。\n";
}

void EmployeeList::sortByNumber() {
    sort(employees_.begin(), employees_.end(), compareByNumber);
    cout << "已按工作证号排序。\n";
}

// 高级查询子菜单：把多个查询功能放在一起，主菜单更清晰。
void EmployeeList::advancedSearch() const {
    while (true) {
        cout << "\n========== 高级查询 ==========\n"
             << "1. 按部门查询\n"
             << "2. 按薪水区间查询\n"
             << "3. 按职务关键字查询\n"
             << "0. 返回主菜单\n";
        int choice = readMenuChoice(0, 3);
        if (choice == 0) {
            return;
        }
        switch (choice) {
            case 1:
                searchByDepartment();
                break;
            case 2:
                searchBySalaryRange();
                break;
            case 3:
                searchByPostKeyword();
                break;
        }
    }
}

// 排序管理：使用标准库 sort，比较规则由下面的静态函数提供。
void EmployeeList::sortMenu() {
    if (employees_.empty()) {
        cout << "暂无员工信息。\n";
        return;
    }
    while (true) {
        cout << "\n========== 排序管理 ==========\n"
             << "1. 按工作证号升序排序\n"
             << "2. 按生日升序排序\n"
             << "3. 按薪水降序排序\n"
             << "0. 返回主菜单\n";
        int choice = readMenuChoice(0, 3);
        if (choice == 0) {
            return;
        }
        switch (choice) {
            case 1:
                sort(employees_.begin(), employees_.end(), compareByNumber);
                cout << "已按工作证号升序排序。\n";
                break;
            case 2:
                sort(employees_.begin(), employees_.end(), compareByBirthday);
                cout << "已按生日升序排序。\n";
                break;
            case 3:
                sort(employees_.begin(), employees_.end(), compareBySalaryDesc);
                cout << "已按薪水降序排序。\n";
                break;
        }
        display();
    }
}

// 统计分析子菜单：目前包含部门人数统计和薪水统计。
void EmployeeList::statisticsMenu() const {
    if (employees_.empty()) {
        cout << "暂无员工信息。\n";
        return;
    }
    while (true) {
        cout << "\n========== 统计分析 ==========\n"
             << "1. 按部门统计人数\n"
             << "2. 薪水统计\n"
             << "0. 返回主菜单\n";
        int choice = readMenuChoice(0, 2);
        if (choice == 0) {
            return;
        }
        switch (choice) {
            case 1:
                countByDepartment();
                break;
            case 2:
                salaryStatistics();
                break;
        }
    }
}

// 部门统计思路：两个 vector 分别保存部门名称和人数。
void EmployeeList::countByDepartment() const {
    if (employees_.empty()) {
        cout << "暂无员工信息。\n";
        return;
    }

    // 两个并列的顺序表：departments 存部门名，counts 存对应人数（下标一一对应）。
    vector<string> departments;
    vector<int> counts;
    for (size_t i = 0; i < employees_.size(); ++i) {
        // 在已记录的部门里顺序查找当前员工所属部门。
        int position = -1;
        for (size_t j = 0; j < departments.size(); ++j) {
            if (departments[j] == employees_[i].department()) {
                position = static_cast<int>(j);
                break;
            }
        }
        if (position == -1) {
            // 没找到：新增该部门，人数置 1。
            departments.push_back(employees_[i].department());
            counts.push_back(1);
        } else {
            // 已存在：对应部门人数加 1。
            counts[static_cast<size_t>(position)]++;
        }
    }

    cout << "\n部门人数统计\n";
    cout << left << setw(18) << "部门" << setw(8) << "人数" << '\n';
    cout << string(26, '-') << '\n';
    for (size_t i = 0; i < departments.size(); ++i) {
        cout << left << setw(18) << departments[i] << setw(8) << counts[i] << '\n';
    }
}

// 部门查询采用精确匹配，适合查找某个部门全部员工。
void EmployeeList::searchByDepartment() const {
    string department = readRequired("请输入部门名称: ");
    vector<const Employee *> matches;
    for (size_t i = 0; i < employees_.size(); ++i) {
        if (employees_[i].department() == department) {
            matches.push_back(&employees_[i]);
        }
    }
    printSearchResult(matches);
}

// 薪水区间查询：如果最低值大于最高值，自动交换。
void EmployeeList::searchBySalaryRange() const {
    double minSalary = readMoneyValue("请输入最低薪水: ");
    double maxSalary = readMoneyValue("请输入最高薪水: ");
    if (minSalary > maxSalary) {
        double temp = minSalary;
        minSalary = maxSalary;
        maxSalary = temp;
    }

    vector<const Employee *> matches;
    for (size_t i = 0; i < employees_.size(); ++i) {
        double salary = employees_[i].salaryValue();
        if (salary >= minSalary && salary <= maxSalary) {
            matches.push_back(&employees_[i]);
        }
    }
    printSearchResult(matches);
}

// 职务关键字查询采用模糊匹配，输入“工程”可匹配“软件工程师”等。
void EmployeeList::searchByPostKeyword() const {
    string keyword = lowerCopy(readRequired("请输入职务关键字: "));
    vector<const Employee *> matches;
    for (size_t i = 0; i < employees_.size(); ++i) {
        string post = lowerCopy(employees_[i].post());
        if (post.find(keyword) != string::npos) {
            matches.push_back(&employees_[i]);
        }
    }
    printSearchResult(matches);
}

// 高级查询结果统一用简表输出，避免重复代码。
void EmployeeList::printSearchResult(const vector<const Employee *> &matches) const {
    if (matches.empty()) {
        cout << "未找到匹配员工。\n";
        return;
    }
    cout << "找到 " << matches.size() << " 条记录:\n";
    printBriefHeader(cout);
    for (size_t i = 0; i < matches.size(); ++i) {
        matches[i]->printBrief(cout, static_cast<int>(i + 1));
    }
}

// 统计总薪水、平均薪水、最高薪水和最低薪水。
void EmployeeList::salaryStatistics() const {
    // total 累加薪水总额；maxIndex/minIndex 记录最高、最低薪水所在的下标。
    double total = 0.0;
    int maxIndex = 0;
    int minIndex = 0;
    // 一次遍历同时累加总额，并更新最高、最低薪水的下标。
    for (size_t i = 0; i < employees_.size(); ++i) {
        double salary = employees_[i].salaryValue();
        total += salary;
        if (salary > employees_[static_cast<size_t>(maxIndex)].salaryValue()) {
            maxIndex = static_cast<int>(i);
        }
        if (salary < employees_[static_cast<size_t>(minIndex)].salaryValue()) {
            minIndex = static_cast<int>(i);
        }
    }
    double average = total / employees_.size();  // 平均薪水 = 总额 / 人数

    cout << fixed << setprecision(2);
    cout << "\n薪水统计\n";
    cout << "员工总数: " << employees_.size() << '\n';
    cout << "薪水总额: " << total << '\n';
    cout << "平均薪水: " << average << '\n';
    cout << "最高薪水: " << employees_[static_cast<size_t>(maxIndex)].name()
         << " (" << employees_[static_cast<size_t>(maxIndex)].salaryValue() << ")\n";
    cout << "最低薪水: " << employees_[static_cast<size_t>(minIndex)].name()
         << " (" << employees_[static_cast<size_t>(minIndex)].salaryValue() << ")\n";
    cout.unsetf(ios::floatfield);
    cout << setprecision(6);
}

// 按工作证号查找，返回迭代器；添加和查重时会用到。
vector<Employee>::iterator EmployeeList::findByNumber(const string &number) {
    for (vector<Employee>::iterator it = employees_.begin(); it != employees_.end(); ++it) {
        if (it->number() == number) {
            return it;
        }
    }
    return employees_.end();
}

// const 版本用于只读场景。
vector<Employee>::const_iterator EmployeeList::findByNumber(const string &number) const {
    for (vector<Employee>::const_iterator it = employees_.begin(); it != employees_.end(); ++it) {
        if (it->number() == number) {
            return it;
        }
    }
    return employees_.end();
}

// 普通查找：姓名包含关键字或工作证号完全相同。
vector<const Employee *> EmployeeList::findMatches(const string &key) const {
    string lowerKey = lowerCopy(key);
    vector<const Employee *> matches;
    for (const Employee &employee : employees_) {
        string lowerName = lowerCopy(employee.name());
        string lowerNumber = lowerCopy(employee.number());
        if (lowerName.find(lowerKey) != string::npos || lowerNumber == lowerKey) {
            matches.push_back(&employee);
        }
    }
    return matches;
}

// 返回匹配下标，删除和修改时需要定位原 vector 中的位置。
vector<size_t> EmployeeList::findMatchIndexes(const string &key) const {
    string lowerKey = lowerCopy(key);
    vector<size_t> matches;
    for (size_t i = 0; i < employees_.size(); ++i) {
        string lowerName = lowerCopy(employees_[i].name());
        string lowerNumber = lowerCopy(employees_[i].number());
        if (lowerName.find(lowerKey) != string::npos || lowerNumber == lowerKey) {
            matches.push_back(i);
        }
    }
    return matches;
}

// 根据用户输入选择一个员工；如果有多条匹配，会继续让用户选择序号。
Employee *EmployeeList::chooseEmployee(const string &prompt) {
    string key = readRequired(prompt);
    vector<size_t> matches = findMatchIndexes(key);
    if (matches.empty()) {
        cout << "未找到匹配员工。\n";
        return nullptr;
    }
    size_t index = chooseIndex(matches);
    if (index == employees_.size()) {
        return nullptr;
    }
    return &employees_[index];
}

// 多条记录重名时，让用户从候选列表里选择一条。
size_t EmployeeList::chooseIndex(const vector<size_t> &matches) const {
    // 只有一条匹配时直接返回，无需让用户选择。
    if (matches.size() == 1) {
        return matches[0];
    }
    // 多条匹配：列出候选，让用户按序号选择（输入 0 取消）。
    cout << "找到多条记录，请选择:\n";
    printBriefHeader(cout);
    for (size_t i = 0; i < matches.size(); ++i) {
        employees_[matches[i]].printBrief(cout, static_cast<int>(i + 1));
    }
    cout << "0. 取消\n";
    int choice = readMenuChoice(0, static_cast<int>(matches.size()));
    if (choice == 0) {
        return employees_.size();
    }
    return matches[static_cast<size_t>(choice - 1)];
}

// 输出员工简表的表头。
void EmployeeList::printBriefHeader(ostream &out) {
    out << left << setw(5) << "序号"
        << setw(14) << "姓名"
        << setw(22) << "身份证号码"
        << setw(16) << "工作证号"
        << setw(14) << "生日"
        << setw(16) << "部门"
        << '\n';
    out << string(87, '-') << '\n';
}

// sort 的比较函数：按工作证号升序。
bool EmployeeList::compareByNumber(const Employee &left, const Employee &right) {
    return left.number() < right.number();
}

// sort 的比较函数：生日越早越靠前。
bool EmployeeList::compareByBirthday(const Employee &left, const Employee &right) {
    return left.birthday().toNumber() < right.birthday().toNumber();
}

// sort 的比较函数：薪水越高越靠前。
bool EmployeeList::compareBySalaryDesc(const Employee &left, const Employee &right) {
    return left.salaryValue() > right.salaryValue();
}

// 主菜单只负责展示选项，具体功能由 EmployeeList 完成。
void printMenu() {
    cout << "\n========== 人事管理系统 ==========\n"
         << "1. 添加员工信息\n"
         << "2. 删除员工信息\n"
         << "3. 清空全部员工信息\n"
         << "4. 显示全部员工信息\n"
         << "5. 查找员工信息\n"
         << "6. 修改员工信息\n"
         << "7. 保存员工信息\n"
         << "8. 读取员工信息\n"
         << "9. 高级查询\n"
         << "10. 排序管理\n"
         << "11. 统计分析\n"
         << "0. 退出系统\n";
}
