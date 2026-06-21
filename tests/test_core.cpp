// 人事管理系统核心逻辑单元测试（零第三方依赖，自带极简断言框架）。
//
// 覆盖范围：
//   - 共享校验规则 pms::*（性别/身份证/电话/薪水）
//   - Date：闰年/合法性校验、toString / toNumber / parse
//   - Employee：serialize / deserialize 往返一致性、字段转义、非法输入拒绝
//
// 构建与运行（项目根目录）：
//   make test
// 退出码 0 表示全部通过，非 0 表示有失败用例（便于 CI 判定）。

#include "../src/personnel_system.h"

#include "../common/employee_rules.h"

#include <exception>
#include <iostream>
#include <string>

namespace {

int g_total = 0;
int g_failed = 0;

// 基础断言：条件为假即记一次失败并打印位置。
void checkImpl(bool cond, const char *expr, const char *file, int line) {
    ++g_total;
    if (!cond) {
        ++g_failed;
        std::cerr << "  [FAIL] " << file << ":" << line << "  " << expr << "\n";
    }
}
#define CHECK(cond) checkImpl((cond), #cond, __FILE__, __LINE__)

// 断言某段代码会抛异常（用于校验非法输入被正确拒绝）。
template <typename F> void checkThrows(F &&fn, const char *what, const char *file, int line) {
    ++g_total;
    bool thrown = false;
    try {
        fn();
    } catch (const std::exception &) {
        thrown = true;
    }
    if (!thrown) {
        ++g_failed;
        std::cerr << "  [FAIL] " << file << ":" << line << "  expected throw: " << what << "\n";
    }
}
#define CHECK_THROWS(fn) checkThrows([&]() { fn; }, #fn, __FILE__, __LINE__)

void testRules() {
    std::cout << "[共享校验规则 pms::*]\n";
    // 性别
    CHECK(pms::isValidSex("男"));
    CHECK(pms::isValidSex("女"));
    CHECK(!pms::isValidSex("M"));
    CHECK(!pms::isValidSex(""));
    // 身份证：15 / 18 位，末位可 X/x
    CHECK(pms::isValidId("110101199001011234"));  // 18 位
    CHECK(pms::isValidId("11010119900101123X"));  // 18 位末位 X
    CHECK(pms::isValidId("11010119900101x"));     // 15 位末位 x
    CHECK(!pms::isValidId("12345"));              // 长度不对
    CHECK(!pms::isValidId("11010119900101123A")); // 含非法字母
    CHECK(!pms::isValidId("1101011990010112X4")); // X 不在末位
    // 电话：7-15 位，数字与短横线
    CHECK(pms::isValidPhone("13800138000"));
    CHECK(pms::isValidPhone("010-1234567"));
    CHECK(!pms::isValidPhone("123"));              // 太短
    CHECK(!pms::isValidPhone("1234567890123456")); // 太长
    CHECK(!pms::isValidPhone("138a0013800"));      // 含字母
    // 薪水：非负数字，至多一个小数点
    CHECK(pms::isMoney("8500"));
    CHECK(pms::isMoney("8500.50"));
    CHECK(!pms::isMoney(""));
    CHECK(!pms::isMoney("8500.5.0")); // 两个小数点
    CHECK(!pms::isMoney("-100"));     // 负号
    CHECK(!pms::isMoney("8500abc"));  // 含字母
}

void testDate() {
    std::cout << "[Date]\n";
    // 闰年 2 月 29 日合法；平年同日非法。
    CHECK_THROWS(Date(2021, 2, 29)); // 2021 非闰年
    Date leap(2020, 2, 29);          // 2020 闰年，构造不应抛异常
    CHECK(leap.toString() == "2020-02-29");
    CHECK(leap.toNumber() == 20200229);

    // 常规日期格式化。
    Date d(1990, 1, 1);
    CHECK(d.toString() == "1990-01-01");
    CHECK(d.toNumber() == 19900101);

    // parse 往返。
    Date parsed = Date::parse("1988-05-05");
    CHECK(parsed.toString() == "1988-05-05");

    // 非法月份 / 日期。
    CHECK_THROWS(Date(1990, 13, 1));
    CHECK_THROWS(Date(1990, 0, 10));
    CHECK_THROWS(Date(1990, 4, 31)); // 4 月只有 30 天
    CHECK_THROWS(Date::parse("not-a-date"));
}

void testEmployeeRoundTrip() {
    std::cout << "[Employee serialize/deserialize]\n";
    const std::string line = "张三|男|110101199001011234|1990-01-01|13800138000|1001|北京市海淀区|"
                             "8500|软件工程师|研发部";
    Employee e = Employee::deserialize(line);
    CHECK(e.name() == "张三");
    CHECK(e.number() == "1001");
    CHECK(e.salary() == "8500");
    CHECK(e.department() == "研发部");
    CHECK(e.birthday().toString() == "1990-01-01");
    // 序列化应还原成完全相同的一行。
    CHECK(e.serialize() == line);
}

void testEmployeeEscaping() {
    std::cout << "[Employee escaping]\n";
    // 地址中包含分隔符 | ，文件里以 \| 转义；反序列化应还原为真实的 | 。
    const std::string line =
        "李四|女|310101199203044567|1992-03-04|13900139000|1002|上海\\|浦东|9800|"
        "产品主管|市场部";
    Employee e = Employee::deserialize(line);
    CHECK(e.address() == "上海|浦东"); // 转义被正确还原
    CHECK(e.serialize() == line);      // 再次序列化应重新转义为 \|
}

void testEmployeeRejectsBadInput() {
    std::cout << "[Employee invalid input]\n";
    CHECK_THROWS(Employee::deserialize("字段太少|男|110101199001011234")); // 字段数不足
    // 非法薪水（含字母）应被拒绝。
    CHECK_THROWS(Employee::deserialize(
        "王五|男|110101199001011234|1990-01-01|13800138000|1003|地址|abc|职务|部门"));
    // 非法生日应被拒绝。
    CHECK_THROWS(Employee::deserialize(
        "王五|男|110101199001011234|1990-13-99|13800138000|1003|地址|8500|职务|部门"));
}

} // namespace

int main() {
    std::cout << "运行核心逻辑单元测试...\n";
    testRules();
    testDate();
    testEmployeeRoundTrip();
    testEmployeeEscaping();
    testEmployeeRejectsBadInput();

    std::cout << "\n通过 " << (g_total - g_failed) << "/" << g_total << " 个断言。\n";
    if (g_failed == 0) {
        std::cout << "全部测试通过 ✅\n";
        return 0;
    }
    std::cout << g_failed << " 个断言失败 ❌\n";
    return 1;
}
