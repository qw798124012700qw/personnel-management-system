#ifndef PERSONNEL_GUI_H
#define PERSONNEL_GUI_H

// 本头文件声明 Qt 图形界面版的数据结构与主窗口类。
// 数据用结构体 Employee 保存，界面与逻辑集中在 MainWindow 中。

#include <QMainWindow>
#include <QString>
#include <QStyle>
#include <QVector>

#include <functional>

// 前置声明用到的 Qt 控件类型，减少头文件依赖、加快编译。
class QCloseEvent;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class QTableView;
class QWidget;

// Qt 版本中使用一个简单结构体保存员工数据。
// 字段顺序与 data/employees.txt 中的保存顺序保持一致。
struct Employee {
    QString name;        // 姓名
    QString sex;         // 性别
    QString id;          // 身份证号
    QString birthday;    // 生日
    QString telephone;   // 电话号码
    QString number;      // 工作证号
    QString address;     // 家庭地址
    double salary = 0.0; // 薪水
    QString post;        // 职务
    QString department;  // 部门
};

// 主窗口类，负责界面创建、按钮事件、表格刷新和文件读写。
class MainWindow : public QMainWindow {
  public:
    // readOnly=true 时为只读访客：禁用增 / 删 / 改 / 导入 / 撤销 / 重做等写操作。
    explicit MainWindow(bool readOnly = false); // 构造时搭建整个界面并加载数据文件

  protected:
    // 关闭窗口时若有未保存改动，弹窗询问 保存 / 放弃 / 取消。
    void closeEvent(QCloseEvent *event) override;

  private:
    static QString appStyleSheet(); // 返回全局样式表（统一外观）

    QWidget *buildHeader();    // 构建顶部标题栏
    void buildForm();          // 构建下方“编辑员工”表单
    QWidget *buildSearchBox(); // 构建左上“筛选/查询”区
    QWidget *buildPager();     // 构建表格下方的分页控件
    void buildButtons();       // 构建操作按钮区
    void applyPaging();        // 按当前页大小/页码,隐藏当前页以外的表格行
    void setupButton(QPushButton *button, QStyle::StandardPixmap icon,
                     const QString &variant); // 统一设置按钮图标与样式
    Employee formToEmployee() const;          // 把表单内容收集成一个 Employee
    bool validateEmployee(const Employee &employee,
                          int ignoreIndex = -1);             // 校验表单（含工作证号查重）
    int currentSourceRow() const;                            // 取当前选中行在原始数据中的下标
    void addEmployee();                                      // “添加”按钮：新增员工
    void updateEmployee();                                   // “修改”按钮：更新选中员工
    void deleteEmployee();                                   // “删除”按钮：删除选中员工
    void fillFormFromCurrentRow(const QModelIndex &current); // 选中行变化时回填表单
    void clearForm();                                        // 清空表单
    void refreshTable();                                     // 用 employees 重建表格模型
    QStandardItem *makeItem(const QString &text) const;      // 生成只读文本单元格
    QStandardItem *makeNumberItem(double value) const;       // 生成数值（右对齐）单元格
    void applyFilter();                                      // 按关键字筛选表格
    void salaryRangeSearch();                                // 按薪水区间筛选
    void showDepartmentStatistics();                         // 弹窗显示部门人数统计
    void showSalaryStatistics();                             // 弹窗显示薪水统计
    void loadFromFile();                                     // 从数据文件读取员工
    bool persist();               // 整表写回数据库(写穿透核心,返回是否成功)
    void saveToFile();            // 「保存文件」按钮:手动写回 + 状态提示
    void exportCsv();             // 导出为 CSV(可用 Excel 打开)
    void importCsv();             // 从 CSV 导入(校验+去重)
    void pushUndo();              // 增/删/改前记录一次快照(并清空重做栈)
    void undo();                  // 撤销上一次增/删/改(支持多级)
    void redo();                  // 重做被撤销的操作(支持多级)
    void updateUndoRedoButtons(); // 按栈空/非空刷新撤销/重做按钮状态
    void restoreTableState();     // 启动时恢复列宽与排序状态
    void saveTableState() const;  // 退出时保存列宽与排序状态
    void logAudit(const QString &action, const QString &detail); // 写一条审计日志
    void showAuditLog();                                         // 弹窗查看最近审计日志
    // 国际化：注册一个"刷新某控件文字"的回调；retranslateUi() 切换语言时统一调用。
    void addTr(const std::function<void()> &fn);
    void retranslateUi();                              // 按当前语言刷新全部已注册控件文字
    QString tr2(const char *zh, const char *en) const; // 按当前语言二选一

    QTableView *table = nullptr;            // 员工表格视图
    QStandardItemModel *model = nullptr;    // 表格数据模型
    QSortFilterProxyModel *proxy = nullptr; // 排序/筛选代理模型
    QGroupBox *formBox = nullptr;           // 表单分组框
    QGroupBox *buttonBox = nullptr;         // 按钮分组框
    QLabel *statusLabel = nullptr;          // 状态提示标签
    QLabel *summaryLabel = nullptr;         // 汇总信息标签
    QLabel *pageLabel = nullptr;            // 分页页码标签(第 X/Y 页)
    int pageSize = 50;                      // 每页行数(0 表示不分页/全部)
    int currentPage = 0;                    // 当前页(从 0 计)

    // 表单中的各输入控件
    QLineEdit *nameEdit = nullptr;        // 姓名输入
    QComboBox *sexBox = nullptr;          // 性别下拉
    QLineEdit *idEdit = nullptr;          // 身份证号输入
    QDateEdit *birthdayEdit = nullptr;    // 生日选择
    QLineEdit *telephoneEdit = nullptr;   // 电话输入
    QLineEdit *numberEdit = nullptr;      // 工作证号输入
    QLineEdit *departmentEdit = nullptr;  // 部门输入
    QLineEdit *postEdit = nullptr;        // 职务输入
    QDoubleSpinBox *salarySpin = nullptr; // 薪水输入
    QLineEdit *addressEdit = nullptr;     // 地址输入
    QLineEdit *keywordEdit = nullptr;     // 筛选关键字输入
    QComboBox *queryModeBox = nullptr;    // 筛选方式下拉

    QVector<Employee> employees;                 // 内存中的全部员工数据
    QVector<QVector<Employee>> undoStack;        // 撤销栈：每次增/删/改前压入一份快照(支持多级)
    QVector<QVector<Employee>> redoStack;        // 重做栈：撤销时把当前状态压入
    QPushButton *undoButton = nullptr;           // 撤销按钮(随栈空/非空启用或禁用)
    QPushButton *redoButton = nullptr;           // 重做按钮(随栈空/非空启用或禁用)
    bool dirty = false;                          // 是否有未保存改动（增删改后置 true，存/读后清零）
    bool readOnly = false;                       // 只读访客：禁用所有写操作
    QString roleName;                            // 当前身份名称(写入审计日志)
    bool english = false;                        // 当前界面语言：false=中文, true=English
    QVector<std::function<void()>> translators_; // 各控件的"按当前语言刷新文字"回调
    // 数据库路径在构造时由 resolveDataFile() 相对 exe 位置智能定位（兼容开发布局与便携包）。
    QString dataFile;
};

#endif
