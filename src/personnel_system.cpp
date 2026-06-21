#include "personnel_system.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <sqlite3.h> // SQLite 嵌入式数据库 C API

#include "../common/db_schema.h"      // 与图形界面版共用的数据库表结构
#include "../common/employee_rules.h" // 与图形界面版共用的字段校验规则

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
    transform(value.begin(), value.end(), value.begin(),
              [](unsigned char ch) { return static_cast<char>(tolower(ch)); });
    return value;
}

string readLine(const string &prompt);

// 判断薪水输入是否合法：复用共享规则 pms::isMoney（单一事实来源）。
bool isMoney(const string &value) {
    return pms::isMoney(value);
}

// 将字符串形式的薪水转换成 double，便于统计和排序。
double toDouble(const string &value) {
    stringstream ss(value);
    double result = 0.0;
    ss >> result;
    return result;
}

// 把数据库中的 REAL 薪水格式化成字符串：整数不带小数点，否则保留两位小数。
string formatMoney(double value) {
    long long whole = static_cast<long long>(value);
    if (static_cast<double>(whole) == value) {
        return to_string(whole);
    }
    ostringstream ss;
    ss << fixed << setprecision(2) << value;
    return ss.str();
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

} // namespace

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
    out << left << setw(5) << index << setw(14) << name_ << setw(22) << id_ << setw(16) << number_
        << setw(14) << birthday_.toString() << setw(16) << department_ << '\n';
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
    vector<string> fields = {name_,    sex_,    id_,   birthday_.toString(), telephone_, number_,
                             address_, salary_, post_, department_};
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

// 身份证校验复用共享规则 pms::isValidId；合法时把末位 x 统一成大写 X。
string Employee::readIdentity(const string &prompt) {
    while (true) {
        string value = readRequired(prompt);
        if (pms::isValidId(value)) {
            if (!value.empty() && value.back() == 'x') {
                value.back() = 'X';
            }
            return value;
        }
        cout << "身份证号码应为 15 位或 18 位，最后一位可为 X。\n";
    }
}

// 电话号码校验复用共享规则 pms::isValidPhone。
string Employee::readTelephone(const string &prompt) {
    while (true) {
        string value = readRequired(prompt);
        if (pms::isValidPhone(value)) {
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

// 从文件/数据库读取后做一次校验，防止异常数据进入系统。
// 校验规则与录入时一致（均来自 common/employee_rules.h），保证两端及读写口径统一。
void Employee::validate() const {
    if (name_.empty() || sex_.empty() || id_.empty() || telephone_.empty() || number_.empty() ||
        address_.empty() || salary_.empty() || post_.empty() || department_.empty()) {
        throw invalid_argument("员工记录包含空字段");
    }
    if (!pms::isValidSex(sex_)) {
        throw invalid_argument("性别格式错误");
    }
    if (!pms::isValidId(id_)) {
        throw invalid_argument("身份证号码格式错误");
    }
    if (!pms::isValidPhone(telephone_)) {
        throw invalid_argument("电话号码格式错误");
    }
    if (!isMoney(salary_)) {
        throw invalid_argument("薪水格式错误");
    }
}

// INSERT 语句及其列序；bindEmployee 按此顺序绑定占位符 1..10。
static const char *kInsertSql =
    "INSERT INTO employees (number, name, sex, id, birthday, telephone, "
    "address, salary, post, department) VALUES (?,?,?,?,?,?,?,?,?,?);";

// UPDATE 语句：占位符 1..10 为新值(列序同 INSERT)，11 为 WHERE 用的旧工作证号。
static const char *kUpdateSql =
    "UPDATE employees SET number=?, name=?, sex=?, id=?, birthday=?, telephone=?, "
    "address=?, salary=?, post=?, department=? WHERE number=?;";

// 把一个 Employee 的字段按 INSERT 列序(占位符 1..10)绑定到已准备好的语句。
// 用 SQLITE_TRANSIENT 让 SQLite 立即复制，避免临时串(如 birthday)悬空。
static void bindEmployee(sqlite3_stmt *stmt, const Employee &e) {
    const string bd = e.birthday().toString();
    sqlite3_bind_text(stmt, 1, e.number().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, e.name().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, e.sex().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, e.id().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, bd.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, e.telephone().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, e.address().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 8, e.salaryValue()); // salary 列为 REAL，按数值存储
    sqlite3_bind_text(stmt, 9, e.post().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, e.department().c_str(), -1, SQLITE_TRANSIENT);
}

EmployeeList::EmployeeList(string fileName) : fileName_(std::move(fileName)) {}

void EmployeeList::add() {
    Employee employee;
    employee.input();
    if (findByNumber(employee.number()) != employees_.end()) { // O(1) 查重
        cout << "工作证号已存在，添加失败。\n";
        return;
    }
    employees_.push_back(employee);
    numberIndex_[employee.number()] = employees_.size() - 1; // 增量维护索引
    // 增量单行写入：只 INSERT 这一条(数据库为实时数据源)。
    execWrite(kInsertSql, [&](sqlite3_stmt *s) { bindEmployee(s, employee); });
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
    string oldNumber = employee->number(); // 记下旧工作证号，便于改重复后回滚
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
    rebuildIndex(); // 修改可能改动了工作证号，重建索引
    // 增量单行写入：UPDATE 该行(占位符 11 为旧工作证号,支持工作证号被改的情况)。
    execWrite(kUpdateSql, [&](sqlite3_stmt *s) {
        bindEmployee(s, *employee);
        sqlite3_bind_text(s, 11, oldNumber.c_str(), -1, SQLITE_TRANSIENT);
    });
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
        const string num = employees_[index].number(); // 删除前记下工作证号(用于 SQL)
        employees_.erase(employees_.begin() + static_cast<long>(index));
        rebuildIndex(); // 删除导致后续下标移动，重建索引
        // 增量单行写入：只 DELETE 这一条。
        execWrite("DELETE FROM employees WHERE number = ?;", [&](sqlite3_stmt *s) {
            sqlite3_bind_text(s, 1, num.c_str(), -1, SQLITE_TRANSIENT);
        });
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
        numberIndex_.clear();
        execWrite("DELETE FROM employees;", [](sqlite3_stmt *) {}); // 清空表
        cout << "全部员工信息已清空。\n";
    } else {
        cout << "已取消清空。\n";
    }
}

// ============ SQLite 数据库存储 ============
// 表结构定义见 common/db_schema.h（与图形界面版共用，避免 schema 漂移）。

// 建表 + 建索引：两处入口（save / load）共用。
static void ensureSchema(sqlite3 *db) {
    sqlite3_exec(db, pms::kCreateTableSql, nullptr, nullptr, nullptr);
    sqlite3_exec(db, pms::kCreateDeptIndexSql, nullptr, nullptr, nullptr);
}

// 由数据库路径(.db)推导出旧文本文件路径(.txt),用于首次运行时自动迁移。
static string textPathOf(const string &dbPath) {
    if (dbPath.size() >= 3 && dbPath.compare(dbPath.size() - 3, 3, ".db") == 0) {
        return dbPath.substr(0, dbPath.size() - 3) + ".txt";
    }
    return dbPath + ".txt";
}

// 从查询结果的一行构造 Employee。列顺序须与下面各 SELECT 一致：
// (name, sex, id, birthday, telephone, number, address, salary, post, department)。
// salary 列为 REAL，按数值读出再格式化；其余按文本读出。拼成竖线分隔串后复用 deserialize 校验。
static Employee rowToEmployee(sqlite3_stmt *stmt) {
    stringstream ss;
    for (int i = 0; i < 10; ++i) {
        if (i > 0) {
            ss << '|';
        }
        if (i == 7) {
            ss << escapeField(formatMoney(sqlite3_column_double(stmt, i)));
        } else {
            const unsigned char *t = sqlite3_column_text(stmt, i);
            ss << escapeField(t ? reinterpret_cast<const char *>(t) : "");
        }
    }
    return Employee::deserialize(ss.str());
}

// 列出全部字段、保持统一顺序的 SELECT 前缀；各查询在其后追加 WHERE / ORDER BY。
static const char *kSelectAll =
    "SELECT name, sex, id, birthday, telephone, number, address, salary, post, department "
    "FROM employees";

// 执行一条写语句(INSERT/UPDATE/DELETE)：打开库、建表、绑定、执行。bind 负责绑定占位符。
// 用于每次增删改的"增量单行 SQL"写入(数据库为实时数据源)。
bool EmployeeList::execWrite(const string &sql, const function<void(sqlite3_stmt *)> &bind) const {
    sqlite3 *db = nullptr;
    if (sqlite3_open(fileName_.c_str(), &db) != SQLITE_OK) {
        cout << "无法打开数据库 " << fileName_ << "：" << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return false;
    }
    ensureSchema(db);
    sqlite3_stmt *stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        bind(stmt);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    if (!ok) {
        cout << "写入数据库失败：" << sqlite3_errmsg(db) << "\n";
    }
    sqlite3_close(db);
    return ok;
}

// 整表写回数据库（清空后重插，事务保证一致性）。供 save() 与首次迁移使用。
bool EmployeeList::writeAllToDb() const {
    sqlite3 *db = nullptr;
    if (sqlite3_open(fileName_.c_str(), &db) != SQLITE_OK) {
        cout << "无法打开数据库 " << fileName_ << "：" << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return false;
    }
    ensureSchema(db);
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "DELETE FROM employees;", nullptr, nullptr, nullptr);

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, kInsertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        cout << "写入数据库失败：" << sqlite3_errmsg(db) << "\n";
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }
    for (const Employee &e : employees_) {
        bindEmployee(stmt, e);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return true;
}

// 手动保存（菜单项）：写回数据库并提示。增删改已实时写库，本项相当于再确认一次。
void EmployeeList::save() const {
    if (writeAllToDb()) {
        cout << "已保存 " << employees_.size() << " 条员工信息到数据库 " << fileName_ << "。\n";
    }
}

// 读取：从数据库读取全部员工。仅当数据库文件原本不存在(真正首次运行)时，才从同名 .txt
// 种子文件迁移；数据库一旦建立，其内容(哪怕被清空)即为权威，不会再被种子还原。
void EmployeeList::load() {
    const bool dbExisted = ifstream(fileName_).good(); // 必须在 sqlite3_open 建文件前判断
    sqlite3 *db = nullptr;
    if (sqlite3_open(fileName_.c_str(), &db) != SQLITE_OK) {
        cout << "无法打开数据库 " << fileName_ << "：" << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return;
    }
    ensureSchema(db); // 首次运行自动建表 + 建索引

    vector<Employee> loaded;
    numberIndex_.clear();
    int skipped = 0;

    // 逐行读取并复用 rowToEmployee（内部走 deserialize 校验）。
    sqlite3_stmt *stmt = nullptr;
    const string sql = string(kSelectAll) + ";";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            try {
                Employee employee = rowToEmployee(stmt);
                if (numberIndex_.count(employee.number()) == 0) {
                    numberIndex_[employee.number()] = loaded.size();
                    loaded.push_back(employee);
                } else {
                    ++skipped;
                }
            } catch (const exception &) {
                ++skipped;
            }
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    employees_ = loaded;

    // 仅"首次运行(数据库文件原本不存在)且当前为空"才从种子文本迁移；
    // 已存在的数据库即使为空也尊重其状态(例如用户主动清空后不再被种子还原)。
    if (employees_.empty() && !dbExisted) {
        ifstream in(textPathOf(fileName_));
        if (in) {
            string line;
            while (getline(in, line)) {
                line = trim(line);
                if (line.empty()) {
                    continue;
                }
                try {
                    Employee employee = Employee::deserialize(line);
                    if (numberIndex_.count(employee.number()) == 0) {
                        numberIndex_[employee.number()] = employees_.size();
                        employees_.push_back(employee);
                    }
                } catch (const exception &) {
                }
            }
            if (!employees_.empty()) {
                writeAllToDb(); // 写入数据库,完成迁移
                cout << "(已从文本文件 " << textPathOf(fileName_) << " 自动迁移 "
                     << employees_.size() << " 条到数据库)\n";
                return;
            }
        }
        cout << "数据库为空，将从空列表开始。\n";
        return;
    }

    cout << "已从数据库读取 " << employees_.size() << " 条员工信息";
    if (skipped > 0) {
        cout << "，跳过 " << skipped << " 条异常记录";
    }
    cout << "。\n";
}

void EmployeeList::sortByNumber() {
    sort(employees_.begin(), employees_.end(), compareByNumber);
    rebuildIndex(); // 排序改变了下标，重建索引
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
        rebuildIndex(); // 排序改变了下标，重建索引
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

// 高级查询统一走数据库 SQL：在 kSelectAll 后追加 whereOrder 子句并执行，bind 负责绑定占位符。
// 增删改已实时写库（writeAllToDb），故数据库即当前数据，查询结果始终准确。
vector<Employee> EmployeeList::runQuery(const string &whereOrder,
                                        const function<void(sqlite3_stmt *)> &bind) const {
    vector<Employee> result;
    sqlite3 *db = nullptr;
    if (sqlite3_open(fileName_.c_str(), &db) != SQLITE_OK) {
        cout << "无法打开数据库 " << fileName_ << "：" << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return result;
    }
    ensureSchema(db);
    const string sql = string(kSelectAll) + " " + whereOrder + ";";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        bind(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            try {
                result.push_back(rowToEmployee(stmt));
            } catch (const exception &) {
                // 跳过异常行
            }
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return result;
}

// 高级查询结果统一用简表输出。
void EmployeeList::printQueryResult(const vector<Employee> &matches) const {
    if (matches.empty()) {
        cout << "未找到匹配员工。\n";
        return;
    }
    cout << "找到 " << matches.size() << " 条记录:\n";
    printBriefHeader(cout);
    for (size_t i = 0; i < matches.size(); ++i) {
        matches[i].printBrief(cout, static_cast<int>(i + 1));
    }
}

// 部门查询：SQL 精确匹配（WHERE department = ?，走部门索引）。
void EmployeeList::searchByDepartment() const {
    string department = readRequired("请输入部门名称: ");
    vector<Employee> matches =
        runQuery("WHERE department = ? ORDER BY number", [&](sqlite3_stmt *s) {
            sqlite3_bind_text(s, 1, department.c_str(), -1, SQLITE_TRANSIENT);
        });
    printQueryResult(matches);
}

// 薪水区间查询：SQL 数值区间（WHERE salary BETWEEN ? AND ?）；最低值大于最高值时自动交换。
void EmployeeList::searchBySalaryRange() const {
    double minSalary = readMoneyValue("请输入最低薪水: ");
    double maxSalary = readMoneyValue("请输入最高薪水: ");
    if (minSalary > maxSalary) {
        std::swap(minSalary, maxSalary);
    }
    vector<Employee> matches =
        runQuery("WHERE salary BETWEEN ? AND ? ORDER BY salary", [&](sqlite3_stmt *s) {
            sqlite3_bind_double(s, 1, minSalary);
            sqlite3_bind_double(s, 2, maxSalary);
        });
    printQueryResult(matches);
}

// 职务关键字查询：SQL 模糊匹配（WHERE post LIKE '%关键字%'），输入“工程”可匹配“软件工程师”。
void EmployeeList::searchByPostKeyword() const {
    string keyword = readRequired("请输入职务关键字: ");
    string pattern = "%" + keyword + "%";
    vector<Employee> matches = runQuery("WHERE post LIKE ? ORDER BY number", [&](sqlite3_stmt *s) {
        sqlite3_bind_text(s, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    });
    printQueryResult(matches);
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
    double average = total / employees_.size(); // 平均薪水 = 总额 / 人数

    cout << fixed << setprecision(2);
    cout << "\n薪水统计\n";
    cout << "员工总数: " << employees_.size() << '\n';
    cout << "薪水总额: " << total << '\n';
    cout << "平均薪水: " << average << '\n';
    cout << "最高薪水: " << employees_[static_cast<size_t>(maxIndex)].name() << " ("
         << employees_[static_cast<size_t>(maxIndex)].salaryValue() << ")\n";
    cout << "最低薪水: " << employees_[static_cast<size_t>(minIndex)].name() << " ("
         << employees_[static_cast<size_t>(minIndex)].salaryValue() << ")\n";
    cout.unsetf(ios::floatfield);
    cout << setprecision(6);
}

// 根据当前 employees_ 重建“工作证号 -> 下标”哈希索引。
// 任何改变元素位置的操作（删除、排序、整体加载）之后调用它即可保持索引正确。
void EmployeeList::rebuildIndex() {
    numberIndex_.clear();
    numberIndex_.reserve(employees_.size());
    for (size_t i = 0; i < employees_.size(); ++i) {
        numberIndex_[employees_[i].number()] = i;
    }
}

// 按工作证号查找：借助哈希索引 O(1) 定位（添加、查重时使用）。
vector<Employee>::iterator EmployeeList::findByNumber(const string &number) {
    auto it = numberIndex_.find(number);
    if (it == numberIndex_.end()) {
        return employees_.end();
    }
    return employees_.begin() + static_cast<long>(it->second);
}

// const 版本：同样用哈希索引 O(1) 查找。
vector<Employee>::const_iterator EmployeeList::findByNumber(const string &number) const {
    auto it = numberIndex_.find(number);
    if (it == numberIndex_.end()) {
        return employees_.end();
    }
    return employees_.begin() + static_cast<long>(it->second);
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
    out << left << setw(5) << "序号" << setw(14) << "姓名" << setw(22) << "身份证号码" << setw(16)
        << "工作证号" << setw(14) << "生日" << setw(16) << "部门" << '\n';
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
