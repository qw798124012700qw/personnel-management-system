#ifndef PMS_DB_SCHEMA_H
#define PMS_DB_SCHEMA_H

// 控制台版与图形界面版共用的数据库表结构定义。
// 单一事实来源（single source of truth）：两端都 include 本文件，避免各自硬编码
// CREATE TABLE 语句而导致 schema 漂移（字段顺序 / 类型不一致）。
//
// 设计要点：
//   - 工作证号 number 设为主键，天然保证唯一；
//   - salary 用 REAL 而非 TEXT，符合关系型存储的数值语义，便于将来用 SQL 做
//     排序 / 区间查询 / 聚合（当前业务在内存中完成，数据量大时可下推到数据库）；
//   - 在 department 上建索引，为“按部门查询”这类高频检索预留 O(log n) 能力。

namespace pms {

// 员工表结构（字段顺序固定，序列化 / 反序列化均以此为准）。
inline const char *const kCreateTableSql =
    "CREATE TABLE IF NOT EXISTS employees ("
    "number TEXT PRIMARY KEY, name TEXT, sex TEXT, id TEXT, birthday TEXT, "
    "telephone TEXT, address TEXT, salary REAL, post TEXT, department TEXT);";

// 部门索引：支撑按部门检索的扩展能力。
inline const char *const kCreateDeptIndexSql =
    "CREATE INDEX IF NOT EXISTS idx_emp_department ON employees(department);";

// 审计日志表：记录谁(角色)在何时做了什么操作，便于追溯。
inline const char *const kCreateAuditSql =
    "CREATE TABLE IF NOT EXISTS audit_log ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, ts TEXT, role TEXT, action TEXT, detail TEXT);";

} // namespace pms

#endif // PMS_DB_SCHEMA_H
