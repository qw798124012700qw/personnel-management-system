#ifndef PERSONNEL_SYSTEM_H
#define PERSONNEL_SYSTEM_H

// 本头文件声明人事管理系统(控制台版)的三个核心类：
//   Date         —— 日期（生日），负责校验与格式化
//   Employee     —— 单个员工，封装全部字段与录入/输出/序列化
//   EmployeeList —— 员工集合，负责增删改查、排序、统计与文件读写
// 以及若干供菜单使用的全局辅助函数。

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

// SQLite 语句句柄的前置声明（避免在头文件里包含 sqlite3.h）。
struct sqlite3_stmt;

// Date 类只负责日期本身：输入、格式化、闰年判断和合法性检查。
class Date {
  public:
    Date();                             // 默认构造，初始为 2000-01-01
    Date(int year, int month, int day); // 按年月日构造（非法则抛异常）

    void set(int year, int month, int day); // 设置日期并做合法性校验
    void input(const std::string &prompt);  // 从键盘循环读取，直到输入合法日期
    std::string toString() const;           // 转为 "YYYY-MM-DD" 字符串
    int toNumber() const;                   // 转为整数 YYYYMMDD，便于比较与排序

    static Date parse(const std::string &text); // 从字符串解析出日期对象

    friend std::istream &operator>>(std::istream &in, Date &date);        // 重载输入运算符
    friend std::ostream &operator<<(std::ostream &out, const Date &date); // 重载输出运算符

  private:
    static bool isLeapYear(int year);                  // 判断闰年
    static int daysOfMonth(int year, int month);       // 返回某年某月的天数
    static bool isValid(int year, int month, int day); // 判断年月日是否合法

    int year_;  // 年
    int month_; // 月
    int day_;   // 日
};

std::istream &operator>>(std::istream &in, Date &date);
std::ostream &operator<<(std::ostream &out, const Date &date);

// Employee 类表示单个员工，所有员工字段都封装在这个类里。
class Employee {
  public:
    Employee();

    // 以下为各字段的只读访问器（getter），返回对应字段的值。
    const std::string &name() const;       // 姓名
    const std::string &sex() const;        // 性别
    const std::string &id() const;         // 身份证号
    const std::string &telephone() const;  // 电话号码
    const Date &birthday() const;          // 生日
    const std::string &number() const;     // 工作证号
    const std::string &address() const;    // 家庭地址
    const std::string &salary() const;     // 薪水（字符串形式）
    const std::string &post() const;       // 职务
    const std::string &department() const; // 部门
    double salaryValue() const;            // 薪水的数值形式（用于统计/排序）

    void input();                                         // 从键盘录入员工完整信息
    void printBrief(std::ostream &out, int index) const;  // 输出摘要（用于列表）
    void printDetail(std::ostream &out) const;            // 输出全部详细信息
    std::string serialize() const;                        // 序列化为文件中的一行
    static Employee deserialize(const std::string &line); // 从文件一行还原为对象
    void modify();                                        // 交互式逐项修改信息
    void modifyNumberOnly(const std::string &number);     // 仅修改工作证号

    friend std::ostream &operator<<(std::ostream &out,
                                    const Employee &employee); // 重载输出（=printDetail）

  private:
    static std::string readSex(const std::string &prompt);       // 读取并校验性别
    static std::string readIdentity(const std::string &prompt);  // 读取并校验身份证号
    static std::string readTelephone(const std::string &prompt); // 读取并校验电话
    static std::string readSalary(const std::string &prompt);    // 读取并校验薪水
    void validate() const;                                       // 读文件后校验字段是否合法

    std::string name_;       // 姓名
    std::string sex_;        // 性别
    std::string id_;         // 身份证号
    std::string telephone_;  // 电话号码
    Date birthday_;          // 生日
    std::string number_;     // 工作证号
    std::string address_;    // 家庭地址
    std::string salary_;     // 薪水
    std::string post_;       // 职务
    std::string department_; // 部门
};

std::ostream &operator<<(std::ostream &out, const Employee &employee);

// EmployeeList 类管理员工集合，内部使用 vector 顺序表保存所有员工。
class EmployeeList {
  public:
    explicit EmployeeList(std::string fileName); // 构造时指定数据文件路径

    void add();                     // 添加员工（工作证号不可重复）
    void display() const;           // 显示全部员工摘要
    void find() const;              // 按姓名或工作证号查找并显示详细信息
    void modify();                  // 选择某员工并修改
    void remove();                  // 删除某员工
    void deleteAll();               // 清空全部员工（需确认）
    void save() const;              // 保存到数据文件
    void load();                    // 从数据文件读取
    void sortByNumber();            // 按工作证号排序
    void advancedSearch() const;    // 高级查询子菜单（部门/薪水区间/职务关键字）
    void sortMenu();                // 排序管理子菜单（工作证号/生日/薪水）
    void statisticsMenu() const;    // 统计分析子菜单
    void countByDepartment() const; // 按部门统计人数

  private:
    void searchByDepartment() const;  // 按部门精确查询（SQL）
    void searchBySalaryRange() const; // 按薪水区间查询（SQL）
    void searchByPostKeyword() const; // 按职务关键字模糊查询（SQL）
    // 在数据库上执行 kSelectAll + whereOrder 的查询，bind 负责绑定占位符，返回结果集。
    std::vector<Employee> runQuery(const std::string &whereOrder,
                                   const std::function<void(sqlite3_stmt *)> &bind) const;
    void printQueryResult(const std::vector<Employee> &matches) const; // 统一输出查询结果
    // 执行一条写语句(INSERT/UPDATE/DELETE)，bind 负责绑定占位符；用于增量单行写入。
    bool execWrite(const std::string &sql, const std::function<void(sqlite3_stmt *)> &bind) const;
    bool writeAllToDb() const;     // 整表写回数据库（供 save 与首次迁移使用）
    void salaryStatistics() const; // 统计薪水总额/平均/最高/最低
    std::vector<Employee>::iterator
    findByNumber(const std::string &number); // 按工作证号查找（可写）
    std::vector<Employee>::const_iterator
    findByNumber(const std::string &number) const; // 按工作证号查找（只读）
    std::vector<const Employee *>
    findMatches(const std::string &key) const; // 按姓名/工作证号找出所有匹配（返回指针）
    std::vector<std::size_t>
    findMatchIndexes(const std::string &key) const;      // 按姓名/工作证号找出所有匹配（返回下标）
    Employee *chooseEmployee(const std::string &prompt); // 交互式选定一名员工
    std::size_t
    chooseIndex(const std::vector<std::size_t> &matches) const; // 多条匹配时让用户选一条
    static void printBriefHeader(std::ostream &out);            // 输出摘要表的表头
    static bool compareByNumber(const Employee &left,
                                const Employee &right); // 排序比较：工作证号升序
    static bool compareByBirthday(const Employee &left,
                                  const Employee &right); // 排序比较：生日升序
    static bool compareBySalaryDesc(const Employee &left,
                                    const Employee &right); // 排序比较：薪水降序
    void rebuildIndex(); // 根据 employees_ 重建“工作证号 -> 下标”哈希索引

    std::string fileName_;            // 数据文件路径
    std::vector<Employee> employees_; // 顺序表：保存全部员工
    // 工作证号 -> 在 employees_ 中的下标；把查找/查重从 O(n) 降到 O(1)。
    std::unordered_map<std::string, std::size_t> numberIndex_;
};

bool askYesNo(const std::string &prompt);       // 统一的 y/n 确认输入
int readMenuChoice(int minValue, int maxValue); // 读取并校验菜单编号（限定范围）
void printMenu();                               // 输出主菜单

#endif
