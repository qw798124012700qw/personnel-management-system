#include "personnel_gui.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QScreen>
#include <QDate>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSize>
#include <QSortFilterProxyModel>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

#include "../common/db_schema.h" // 与控制台版共用的数据库表结构

// 读取旧文本数据文件时按 | 拆分，同时识别反斜杠转义(仅用于首次从 .txt 自动迁移到数据库)。
static QStringList splitRecordLine(const QString &line) {
    QStringList fields;
    QString current;
    bool escaped = false;
    for (QChar ch : line) {
        if (escaped) {
            current.append(ch);
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '|') {
            fields.append(current);
            current.clear();
        } else {
            current.append(ch);
        }
    }
    if (escaped) {
        current.append('\\');
    }
    fields.append(current);
    return fields;
}

// 主窗口类，负责界面创建、按钮事件、表格刷新和文件读写。

MainWindow::MainWindow() {
    setWindowTitle("人事管理系统 - Qt 图形界面");
    // 默认尺寸自适应屏幕：不超过可用屏幕区域，避免在小屏 / 高分屏上窗口超出屏幕、底部按钮被挤出。
    QSize avail = QApplication::primaryScreen()->availableSize();
    resize(qMin(1180, avail.width() - 40), qMin(760, avail.height() - 50));
    setMinimumSize(qMin(880, avail.width() - 60), qMin(560, avail.height() - 70));
    setWindowIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    setStyleSheet(appStyleSheet());

    // model 保存真正显示到表格中的数据。
    model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({"姓名", "性别", "身份证号码", "生日", "电话", "工作证号",
                                      "部门", "职务", "薪水", "家庭地址"});

    // proxy 位于 model 和 table 中间，用于筛选和排序。
    proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1);
    proxy->setSortRole(Qt::UserRole);

    // 表格区域：整行选择、允许点击表头排序。
    table = new QTableView(this);
    table->setModel(proxy);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setSortingEnabled(true);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFocusPolicy(Qt::NoFocus);
    table->setFrameShape(QFrame::NoFrame);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(38);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setMinimumSectionSize(92);

    // 下方表单和按钮分开构建，主构造函数保持清晰。
    buildForm();
    buildButtons();

    // 主界面从上到下依次是查询区、表格、表单、按钮区、状态栏。
    QWidget *central = new QWidget(this);
    central->setObjectName("appRoot");
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(18, 18, 18, 14);
    mainLayout->setSpacing(12);
    mainLayout->addWidget(buildHeader());
    mainLayout->addWidget(buildSearchBox());
    mainLayout->addWidget(table, 1);
    mainLayout->addWidget(formBox);
    mainLayout->addWidget(buttonBox);
    setCentralWidget(central);

    // 点击表格某一行时，把该员工信息填入表单，方便修改。
    connect(table->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex &current) { fillFormFromCurrentRow(current); });

    loadFromFile();
    refreshTable();
}

// 关闭窗口时，若存在未保存的改动，提示用户 保存 / 放弃 / 取消。
void MainWindow::closeEvent(QCloseEvent *event) {
    if (!dirty) {
        event->accept(); // 没有改动，直接关闭
        return;
    }
    QMessageBox::StandardButton choice = QMessageBox::question(
        this, "退出", "有未保存的改动，是否保存后退出？",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Save) {
        saveToFile(); // 保存（成功后 dirty 会被清零）
        event->accept();
    } else if (choice == QMessageBox::Discard) {
        event->accept(); // 放弃改动，直接关闭
    } else {
        event->ignore(); // 取消，留在程序中
    }
}

QString MainWindow::appStyleSheet() {
    return R"(
        #appRoot {
            background: #f3f6fb;
            color: #172033;
            font-family: "Microsoft YaHei", "Noto Sans CJK SC", "PingFang SC", sans-serif;
            font-size: 14px;
        }

        QFrame#headerBar {
            background: #1d3557;
            border-radius: 8px;
        }

        QLabel#headerTitle {
            color: #ffffff;
            font-size: 24px;
            font-weight: 700;
        }

        QLabel#headerMeta {
            color: #d8e5f2;
            font-size: 13px;
        }

        QLabel#summaryBadge {
            background: #e9f5ec;
            border: 1px solid #b8dfc0;
            border-radius: 8px;
            color: #205537;
            font-weight: 600;
            padding: 7px 14px;
        }

        QGroupBox {
            background: #ffffff;
            border: 1px solid #dce4ef;
            border-radius: 8px;
            margin-top: 18px;
            font-weight: 700;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            top: 0px;
            color: #26354d;
            padding: 0 7px;
            background: #ffffff;
        }

        QLabel {
            color: #34445e;
        }

        QLineEdit, QComboBox, QDateEdit, QDoubleSpinBox {
            background: #ffffff;
            border: 1px solid #ccd8e6;
            border-radius: 6px;
            min-height: 31px;
            padding: 3px 9px;
            selection-background-color: #2f6f9f;
        }

        QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QDoubleSpinBox:focus {
            border: 1px solid #2f6f9f;
            background: #fbfdff;
        }

        QTableView {
            background: #ffffff;
            alternate-background-color: #f7fafc;
            border: 1px solid #dce4ef;
            border-radius: 8px;
            color: #1f2a3d;
            selection-background-color: #dcecff;
            selection-color: #102033;
        }

        QHeaderView::section {
            background: #e8eef6;
            color: #223148;
            border: none;
            border-right: 1px solid #d2dce8;
            padding: 9px 10px;
            font-weight: 700;
        }

        QTableView::item {
            border-bottom: 1px solid #edf1f6;
            padding-left: 7px;
            padding-right: 7px;
        }

        QPushButton {
            background: #eef3f8;
            border: 1px solid #cfd9e6;
            border-radius: 7px;
            color: #24354e;
            font-weight: 600;
            min-height: 34px;
            padding: 6px 13px;
        }

        QPushButton:hover {
            background: #e3ebf4;
            border-color: #b9c8da;
        }

        QPushButton:pressed {
            background: #d7e2ee;
        }

        QPushButton[variant="primary"] {
            background: #2f6f9f;
            border-color: #2f6f9f;
            color: #ffffff;
        }

        QPushButton[variant="primary"]:hover {
            background: #275f89;
        }

        QPushButton[variant="danger"] {
            background: #fff0f0;
            border-color: #efb5b5;
            color: #9a2f2f;
        }

        QPushButton[variant="danger"]:hover {
            background: #ffe3e3;
        }

        QPushButton[variant="quiet"] {
            background: #ffffff;
        }

        QLabel#statusText {
            color: #53647d;
            font-weight: 600;
            padding-left: 8px;
        }
    )";
}

QWidget *MainWindow::buildHeader() {
    QFrame *header = new QFrame(this);
    header->setObjectName("headerBar");
    QHBoxLayout *layout = new QHBoxLayout(header);
    layout->setContentsMargins(20, 14, 20, 14);
    layout->setSpacing(14);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(2);

    QLabel *title = new QLabel("人事管理系统", header);
    title->setObjectName("headerTitle");
    QLabel *meta = new QLabel("Personnel Management", header);
    meta->setObjectName("headerMeta");
    titleLayout->addWidget(title);
    titleLayout->addWidget(meta);

    summaryLabel = new QLabel("0 条记录", header);
    summaryLabel->setObjectName("summaryBadge");
    summaryLabel->setAlignment(Qt::AlignCenter);

    layout->addLayout(titleLayout, 1);
    layout->addWidget(summaryLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    return header;
}

// 创建员工信息表单。
void MainWindow::buildForm() {
    formBox = new QGroupBox("员工信息", this);
    QGridLayout *layout = new QGridLayout(formBox);
    layout->setContentsMargins(16, 22, 16, 14);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(10);

    nameEdit = new QLineEdit(formBox);
    nameEdit->setPlaceholderText("姓名");
    nameEdit->setClearButtonEnabled(true);
    sexBox = new QComboBox(formBox);
    sexBox->addItems({"男", "女"});
    idEdit = new QLineEdit(formBox);
    idEdit->setPlaceholderText("15 位或 18 位身份证号码");
    idEdit->setClearButtonEnabled(true);
    birthdayEdit = new QDateEdit(QDate(2000, 1, 1), formBox);
    birthdayEdit->setCalendarPopup(true);
    birthdayEdit->setDisplayFormat("yyyy-MM-dd");
    telephoneEdit = new QLineEdit(formBox);
    telephoneEdit->setPlaceholderText("电话号码");
    telephoneEdit->setClearButtonEnabled(true);
    numberEdit = new QLineEdit(formBox);
    numberEdit->setPlaceholderText("工作证号");
    numberEdit->setClearButtonEnabled(true);
    departmentEdit = new QLineEdit(formBox);
    departmentEdit->setPlaceholderText("部门");
    departmentEdit->setClearButtonEnabled(true);
    postEdit = new QLineEdit(formBox);
    postEdit->setPlaceholderText("职务");
    postEdit->setClearButtonEnabled(true);
    salarySpin = new QDoubleSpinBox(formBox);
    salarySpin->setRange(0, 1000000);
    salarySpin->setDecimals(2);
    salarySpin->setSingleStep(500);
    salarySpin->setPrefix("¥ ");
    addressEdit = new QLineEdit(formBox);
    addressEdit->setPlaceholderText("家庭地址");
    addressEdit->setClearButtonEnabled(true);

    // 使用正则表达式做简单输入限制，减少明显错误输入。
    idEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression("[0-9]{15}|[0-9]{17}[0-9Xx]"), idEdit));
    telephoneEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression("[0-9-]{7,15}"), telephoneEdit));

    layout->addWidget(new QLabel("姓名"), 0, 0);
    layout->addWidget(nameEdit, 0, 1);
    layout->addWidget(new QLabel("性别"), 0, 2);
    layout->addWidget(sexBox, 0, 3);
    layout->addWidget(new QLabel("身份证号码"), 0, 4);
    layout->addWidget(idEdit, 0, 5);

    layout->addWidget(new QLabel("生日"), 1, 0);
    layout->addWidget(birthdayEdit, 1, 1);
    layout->addWidget(new QLabel("电话"), 1, 2);
    layout->addWidget(telephoneEdit, 1, 3);
    layout->addWidget(new QLabel("工作证号"), 1, 4);
    layout->addWidget(numberEdit, 1, 5);

    layout->addWidget(new QLabel("部门"), 2, 0);
    layout->addWidget(departmentEdit, 2, 1);
    layout->addWidget(new QLabel("职务"), 2, 2);
    layout->addWidget(postEdit, 2, 3);
    layout->addWidget(new QLabel("薪水"), 2, 4);
    layout->addWidget(salarySpin, 2, 5);

    layout->addWidget(new QLabel("家庭地址"), 3, 0);
    layout->addWidget(addressEdit, 3, 1, 1, 5);
}

// 创建查询区域：关键字筛选和薪水区间查询。
QWidget *MainWindow::buildSearchBox() {
    QGroupBox *box = new QGroupBox("查询与筛选", this);
    QGridLayout *layout = new QGridLayout(box);
    layout->setContentsMargins(16, 22, 16, 14);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(8);

    queryModeBox = new QComboBox(box);
    queryModeBox->addItems({"全部字段关键字", "姓名", "部门", "职务", "工作证号"});
    keywordEdit = new QLineEdit(box);
    keywordEdit->setPlaceholderText("输入关键字");
    keywordEdit->setClearButtonEnabled(true);

    QPushButton *clearFilterButton = new QPushButton("清除筛选", box);
    QPushButton *salaryRangeButton = new QPushButton("薪水区间查询", box);
    setupButton(clearFilterButton, QStyle::SP_DialogResetButton, "quiet");
    setupButton(salaryRangeButton, QStyle::SP_FileDialogInfoView, "primary");

    layout->addWidget(new QLabel("查询方式"), 0, 0);
    layout->addWidget(queryModeBox, 0, 1);
    layout->addWidget(keywordEdit, 0, 2);
    layout->addWidget(clearFilterButton, 0, 3);
    layout->addWidget(salaryRangeButton, 0, 4);

    // 文本变化时立即筛选，不需要额外点击查询按钮。
    connect(keywordEdit, &QLineEdit::textChanged, this, [this](const QString &) { applyFilter(); });
    connect(queryModeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { applyFilter(); });
    connect(clearFilterButton, &QPushButton::clicked, this, [this]() {
        keywordEdit->clear();
        queryModeBox->setCurrentIndex(0);
        applyFilter();
    });
    connect(salaryRangeButton, &QPushButton::clicked, this, [this]() { salaryRangeSearch(); });

    return box;
}

// 创建操作按钮，并把按钮点击事件连接到对应函数。
void MainWindow::buildButtons() {
    buttonBox = new QGroupBox("操作", this);
    QGridLayout *layout = new QGridLayout(buttonBox);
    layout->setContentsMargins(16, 22, 16, 14);
    layout->setHorizontalSpacing(10);
    layout->setVerticalSpacing(10);

    QPushButton *addButton = new QPushButton("添加", buttonBox);
    QPushButton *updateButton = new QPushButton("修改", buttonBox);
    QPushButton *deleteButton = new QPushButton("删除", buttonBox);
    QPushButton *clearFormButton = new QPushButton("清空表单", buttonBox);
    QPushButton *loadButton = new QPushButton("读取文件", buttonBox);
    QPushButton *saveButton = new QPushButton("保存文件", buttonBox);
    QPushButton *departmentStatButton = new QPushButton("部门统计", buttonBox);
    QPushButton *salaryStatButton = new QPushButton("薪水统计", buttonBox);
    QPushButton *sortNumberButton = new QPushButton("按工号排序", buttonBox);
    QPushButton *sortSalaryButton = new QPushButton("按薪水排序", buttonBox);
    QPushButton *exportButton = new QPushButton("导出CSV", buttonBox);
    undoButton = new QPushButton("撤销", buttonBox);
    undoButton->setEnabled(false); // 无可撤销操作时禁用

    setupButton(addButton, QStyle::SP_DialogApplyButton, "primary");
    setupButton(updateButton, QStyle::SP_BrowserReload, "primary");
    setupButton(deleteButton, QStyle::SP_TrashIcon, "danger");
    setupButton(clearFormButton, QStyle::SP_DialogResetButton, "quiet");
    setupButton(loadButton, QStyle::SP_DialogOpenButton, "quiet");
    setupButton(saveButton, QStyle::SP_DialogSaveButton, "primary");
    setupButton(departmentStatButton, QStyle::SP_FileDialogDetailedView, "quiet");
    setupButton(salaryStatButton, QStyle::SP_FileDialogInfoView, "quiet");
    setupButton(sortNumberButton, QStyle::SP_ArrowUp, "quiet");
    setupButton(sortSalaryButton, QStyle::SP_ArrowDown, "quiet");
    setupButton(exportButton, QStyle::SP_DialogSaveButton, "quiet");
    setupButton(undoButton, QStyle::SP_ArrowBack, "quiet");

    layout->addWidget(addButton, 0, 0);
    layout->addWidget(updateButton, 0, 1);
    layout->addWidget(deleteButton, 0, 2);
    layout->addWidget(clearFormButton, 0, 3);
    layout->addWidget(loadButton, 0, 4);
    layout->addWidget(saveButton, 0, 5);
    layout->addWidget(departmentStatButton, 1, 0);
    layout->addWidget(salaryStatButton, 1, 1);
    layout->addWidget(sortNumberButton, 1, 2);
    layout->addWidget(sortSalaryButton, 1, 3);
    layout->addWidget(exportButton, 1, 4);
    layout->addWidget(undoButton, 1, 5);

    statusLabel = new QLabel("就绪", buttonBox);
    statusLabel->setObjectName("statusText");
    layout->addWidget(statusLabel, 2, 0, 1, 6);

    // Qt 的信号槽机制：按钮 clicked 信号触发对应的业务函数。
    connect(addButton, &QPushButton::clicked, this, [this]() { addEmployee(); });
    connect(updateButton, &QPushButton::clicked, this, [this]() { updateEmployee(); });
    connect(deleteButton, &QPushButton::clicked, this, [this]() { deleteEmployee(); });
    connect(clearFormButton, &QPushButton::clicked, this, [this]() { clearForm(); });
    connect(loadButton, &QPushButton::clicked, this, [this]() {
        loadFromFile();
        refreshTable();
    });
    connect(saveButton, &QPushButton::clicked, this, [this]() { saveToFile(); });
    connect(departmentStatButton, &QPushButton::clicked, this,
            [this]() { showDepartmentStatistics(); });
    connect(salaryStatButton, &QPushButton::clicked, this, [this]() { showSalaryStatistics(); });
    connect(sortNumberButton, &QPushButton::clicked, this, [this]() {
        // QVector 也可以使用 std::sort 排序。
        std::sort(employees.begin(), employees.end(),
                  [](const Employee &a, const Employee &b) { return a.number < b.number; });
        refreshTable();
    });
    connect(sortSalaryButton, &QPushButton::clicked, this, [this]() {
        std::sort(employees.begin(), employees.end(),
                  [](const Employee &a, const Employee &b) { return a.salary > b.salary; });
        refreshTable();
    });
    connect(exportButton, &QPushButton::clicked, this, [this]() { exportCsv(); });
    connect(undoButton, &QPushButton::clicked, this, [this]() { undo(); });
}

void MainWindow::setupButton(QPushButton *button, QStyle::StandardPixmap icon,
                             const QString &variant) {
    button->setIcon(style()->standardIcon(icon));
    button->setIconSize(QSize(16, 16));
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("variant", variant);
}

// 将界面表单中的内容整理成 Employee 结构体。
Employee MainWindow::formToEmployee() const {
    Employee employee;
    employee.name = nameEdit->text().trimmed();
    employee.sex = sexBox->currentText();
    employee.id = idEdit->text().trimmed().toUpper();
    employee.birthday = birthdayEdit->date().toString("yyyy-MM-dd");
    employee.telephone = telephoneEdit->text().trimmed();
    employee.number = numberEdit->text().trimmed();
    employee.department = departmentEdit->text().trimmed();
    employee.post = postEdit->text().trimmed();
    employee.salary = salarySpin->value();
    employee.address = addressEdit->text().trimmed();
    return employee;
}

// 添加和修改前做基本校验：必填项、身份证长度、工作证号唯一。
bool MainWindow::validateEmployee(const Employee &employee, int ignoreIndex) {
    if (employee.name.isEmpty() || employee.id.isEmpty() || employee.telephone.isEmpty() ||
        employee.number.isEmpty() || employee.department.isEmpty() || employee.post.isEmpty() ||
        employee.address.isEmpty()) {
        QMessageBox::warning(this, "输入不完整", "请填写所有员工信息。");
        return false;
    }
    if (!(employee.id.size() == 15 || employee.id.size() == 18)) {
        QMessageBox::warning(this, "身份证号码错误", "身份证号码应为 15 位或 18 位。");
        return false;
    }
    for (int i = 0; i < employees.size(); ++i) {
        if (i != ignoreIndex && employees[i].number == employee.number) {
            QMessageBox::warning(this, "工作证号重复", "该工作证号已经存在。");
            return false;
        }
    }
    return true;
}

// 表格显示的是 proxy 下标，需要转换成原始 employees 中的下标。
int MainWindow::currentSourceRow() const {
    QModelIndex proxyIndex = table->currentIndex();
    if (!proxyIndex.isValid()) {
        return -1;
    }
    QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);
    return sourceIndex.row();
}

// 添加员工：表单 -> 校验 -> 加入 QVector -> 刷新表格。
void MainWindow::addEmployee() {
    Employee employee = formToEmployee();
    if (!validateEmployee(employee)) {
        return;
    }
    pushUndo(); // 改动前留快照,支持撤销
    employees.append(employee);
    dirty = true; // 标记有未保存改动
    refreshTable();
    statusLabel->setText("添加成功");
}

// 修改当前选中员工。
void MainWindow::updateEmployee() {
    int row = currentSourceRow();
    if (row < 0 || row >= employees.size()) {
        QMessageBox::information(this, "未选择", "请先在表格中选择要修改的员工。");
        return;
    }
    Employee employee = formToEmployee();
    if (!validateEmployee(employee, row)) {
        return;
    }
    pushUndo(); // 改动前留快照,支持撤销
    employees[row] = employee;
    dirty = true; // 标记有未保存改动
    refreshTable();
    statusLabel->setText("修改成功");
}

// 删除当前选中员工，删除前弹窗确认。
void MainWindow::deleteEmployee() {
    int row = currentSourceRow();
    if (row < 0 || row >= employees.size()) {
        QMessageBox::information(this, "未选择", "请先在表格中选择要删除的员工。");
        return;
    }
    if (QMessageBox::question(this, "确认删除", "确认删除选中的员工信息吗？") == QMessageBox::Yes) {
        pushUndo(); // 改动前留快照,支持撤销
        employees.removeAt(row);
        dirty = true; // 标记有未保存改动
        refreshTable();
        clearForm();
        statusLabel->setText("删除成功");
    }
}

// 表格选中行变化时，把数据显示到下方表单。
void MainWindow::fillFormFromCurrentRow(const QModelIndex &current) {
    if (!current.isValid()) {
        return;
    }
    int row = proxy->mapToSource(current).row();
    if (row < 0 || row >= employees.size()) {
        return;
    }
    const Employee &employee = employees[row];
    nameEdit->setText(employee.name);
    sexBox->setCurrentText(employee.sex);
    idEdit->setText(employee.id);
    birthdayEdit->setDate(QDate::fromString(employee.birthday, "yyyy-MM-dd"));
    telephoneEdit->setText(employee.telephone);
    numberEdit->setText(employee.number);
    departmentEdit->setText(employee.department);
    postEdit->setText(employee.post);
    salarySpin->setValue(employee.salary);
    addressEdit->setText(employee.address);
}

// 清空表单，方便重新录入。
void MainWindow::clearForm() {
    nameEdit->clear();
    sexBox->setCurrentIndex(0);
    idEdit->clear();
    birthdayEdit->setDate(QDate(2000, 1, 1));
    telephoneEdit->clear();
    numberEdit->clear();
    departmentEdit->clear();
    postEdit->clear();
    salarySpin->setValue(0);
    addressEdit->clear();
    table->clearSelection();
}

// 用 employees 刷新表格 model。所有增删改排序后都调用它。
void MainWindow::refreshTable() {
    model->removeRows(0, model->rowCount());
    for (const Employee &employee : employees) {
        QList<QStandardItem *> row;
        row << makeItem(employee.name) << makeItem(employee.sex) << makeItem(employee.id)
            << makeItem(employee.birthday) << makeItem(employee.telephone)
            << makeItem(employee.number) << makeItem(employee.department) << makeItem(employee.post)
            << makeNumberItem(employee.salary) << makeItem(employee.address);
        model->appendRow(row);
    }
    applyFilter();
    QString countText = QString("%1 条记录").arg(employees.size());
    summaryLabel->setText(countText);
    statusLabel->setText(QString("当前共有 %1 条员工信息").arg(employees.size()));
}

// 普通文本单元格。UserRole 用于排序时取原始值。
QStandardItem *MainWindow::makeItem(const QString &text) const {
    QStandardItem *item = new QStandardItem(text);
    item->setData(text, Qt::UserRole);
    return item;
}

// 数字单元格把 double 放入 UserRole，避免按字符串排序。
QStandardItem *MainWindow::makeNumberItem(double value) const {
    QStandardItem *item = new QStandardItem(QString::number(value, 'f', 2));
    item->setData(value, Qt::UserRole);
    return item;
}

// 根据查询方式决定筛选哪一列；-1 表示所有列一起筛选。
void MainWindow::applyFilter() {
    int column = -1;
    switch (queryModeBox->currentIndex()) {
    case 1:
        column = 0;
        break;
    case 2:
        column = 6;
        break;
    case 3:
        column = 7;
        break;
    case 4:
        column = 5;
        break;
    default:
        column = -1;
    }
    proxy->setFilterKeyColumn(column);
    proxy->setFilterFixedString(keywordEdit->text().trimmed());
}

// 薪水区间查询使用两个输入框，并用弹窗显示结果。
void MainWindow::salaryRangeSearch() {
    bool ok = false;
    double minSalary =
        QInputDialog::getDouble(this, "薪水区间查询", "最低薪水", 0, 0, 1000000, 2, &ok);
    if (!ok) {
        return;
    }
    double maxSalary =
        QInputDialog::getDouble(this, "薪水区间查询", "最高薪水", 10000, 0, 1000000, 2, &ok);
    if (!ok) {
        return;
    }
    if (minSalary > maxSalary) {
        std::swap(minSalary, maxSalary);
    }

    QString message;
    int count = 0;
    for (const Employee &employee : employees) {
        if (employee.salary >= minSalary && employee.salary <= maxSalary) {
            message += QString("%1  %2  %3元\n")
                           .arg(employee.name, employee.department)
                           .arg(employee.salary, 0, 'f', 2);
            ++count;
        }
    }
    if (message.isEmpty()) {
        message = "没有符合条件的员工。";
    }
    QMessageBox::information(this, "薪水区间查询",
                             QString("找到 %1 条记录：\n\n%2").arg(count).arg(message));
}

// 部门统计：统计每个部门出现次数。
void MainWindow::showDepartmentStatistics() {
    QStringList departments;
    QVector<int> counts;
    for (const Employee &employee : employees) {
        int index = departments.indexOf(employee.department);
        if (index < 0) {
            departments.append(employee.department);
            counts.append(1);
        } else {
            counts[index]++;
        }
    }

    QString message;
    for (int i = 0; i < departments.size(); ++i) {
        message += QString("%1：%2 人\n").arg(departments[i]).arg(counts[i]);
    }
    QMessageBox::information(this, "部门人数统计", message.isEmpty() ? "暂无员工信息。" : message);
}

// 薪水统计：总额、平均值、最高、最低。
void MainWindow::showSalaryStatistics() {
    if (employees.isEmpty()) {
        QMessageBox::information(this, "薪水统计", "暂无员工信息。");
        return;
    }
    double total = 0;
    int maxIndex = 0;
    int minIndex = 0;
    for (int i = 0; i < employees.size(); ++i) {
        total += employees[i].salary;
        if (employees[i].salary > employees[maxIndex].salary) {
            maxIndex = i;
        }
        if (employees[i].salary < employees[minIndex].salary) {
            minIndex = i;
        }
    }
    QString message =
        QString("员工总数：%1\n薪水总额：%2\n平均薪水：%3\n最高薪水：%4（%5）\n最低薪水：%6（%7）")
            .arg(employees.size())
            .arg(total, 0, 'f', 2)
            .arg(total / employees.size(), 0, 'f', 2)
            .arg(employees[maxIndex].name)
            .arg(employees[maxIndex].salary, 0, 'f', 2)
            .arg(employees[minIndex].name)
            .arg(employees[minIndex].salary, 0, 'f', 2);
    QMessageBox::information(this, "薪水统计", message);
}

// 从 SQLite 数据库读取员工(与控制台版共用同一数据库)。库为空时自动从旧的 .txt 文本文件迁移。
void MainWindow::loadFromFile() {
    employees.clear();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "pms");
        db.setDatabaseName(dataFile);
        if (!db.open()) {
            statusLabel->setText("无法打开数据库");
        } else {
            QSqlQuery q(db);
            q.exec(pms::kCreateTableSql);
            q.exec(pms::kCreateDeptIndexSql);
            q.exec("SELECT name, sex, id, birthday, telephone, number, address, salary, post, "
                   "department FROM employees;");
            while (q.next()) {
                Employee e;
                e.name = q.value(0).toString();
                e.sex = q.value(1).toString();
                e.id = q.value(2).toString();
                e.birthday = q.value(3).toString();
                e.telephone = q.value(4).toString();
                e.number = q.value(5).toString();
                e.address = q.value(6).toString();
                e.salary = q.value(7).toDouble();
                e.post = q.value(8).toString();
                e.department = q.value(9).toString();
                employees.append(e);
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase("pms");

    // 数据库为空 -> 尝试从旧的同名 .txt 文本文件迁移。
    if (employees.isEmpty()) {
        QString txt = dataFile;
        if (txt.endsWith(".db")) {
            txt.chop(3);
            txt += ".txt";
        }
        QFile file(txt);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            in.setCodec("UTF-8");
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.isEmpty()) {
                    continue;
                }
                QStringList f = splitRecordLine(line);
                if (f.size() != 10) {
                    continue;
                }
                Employee e;
                e.name = f[0];
                e.sex = f[1];
                e.id = f[2];
                e.birthday = f[3];
                e.telephone = f[4];
                e.number = f[5];
                e.address = f[6];
                e.salary = f[7].toDouble();
                e.post = f[8];
                e.department = f[9];
                employees.append(e);
            }
            file.close();
            if (!employees.isEmpty()) {
                saveToFile(); // 写入数据库,完成迁移
                statusLabel->setText(
                    QString("已从文本文件迁移 %1 条到数据库").arg(employees.size()));
                return;
            }
        }
    }
    dirty = false;
    statusLabel->setText(QString("已从数据库读取 %1 条员工信息").arg(employees.size()));
}

// 保存到 SQLite 数据库：清空表后整表重写(放在事务里)。与控制台版共用同一数据库。
void MainWindow::saveToFile() {
    bool ok = false;
    // 把 db 限制在内层作用域：必须在 removeDatabase 之前析构，否则 Qt 会报
    // “connection 'pms' is still in use”。
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "pms");
        db.setDatabaseName(dataFile);
        if (!db.open()) {
            QMessageBox::warning(this, "保存失败", "无法打开数据库。");
        } else {
            QSqlQuery q(db);
            q.exec(pms::kCreateTableSql);
            q.exec(pms::kCreateDeptIndexSql);
            db.transaction();
            q.exec("DELETE FROM employees;");
            q.prepare("INSERT INTO employees (number, name, sex, id, birthday, telephone, address, "
                      "salary, post, department) VALUES (?,?,?,?,?,?,?,?,?,?);");
            for (const Employee &e : employees) {
                q.addBindValue(e.number);
                q.addBindValue(e.name);
                q.addBindValue(e.sex);
                q.addBindValue(e.id);
                q.addBindValue(e.birthday);
                q.addBindValue(e.telephone);
                q.addBindValue(e.address);
                q.addBindValue(e.salary); // salary 列为 REAL，按数值存储
                q.addBindValue(e.post);
                q.addBindValue(e.department);
                q.exec();
            }
            db.commit();
            db.close();
            ok = true;
        }
    }
    QSqlDatabase::removeDatabase("pms");
    if (!ok) {
        return;
    }
    dirty = false;
    statusLabel->setText(QString("已保存 %1 条员工信息到数据库").arg(employees.size()));
}

// 按 CSV 规则转义单个字段：含逗号/引号/换行时用双引号包裹，内部双引号翻倍。
static QString csvCell(const QString &value) {
    if (value.contains(',') || value.contains('"') || value.contains('\n') ||
        value.contains('\r')) {
        QString escaped = value;
        escaped.replace('"', "\"\"");
        return '"' + escaped + '"';
    }
    return value;
}

// 导出当前(内存中)的全部员工为 CSV 文件，可直接用 Excel / WPS 打开。
void MainWindow::exportCsv() {
    if (employees.isEmpty()) {
        QMessageBox::information(this, "导出CSV", "当前没有可导出的员工数据。");
        return;
    }
    QString path =
        QFileDialog::getSaveFileName(this, "导出为 CSV", "employees.csv", "CSV 文件 (*.csv)");
    if (path.isEmpty()) {
        return; // 用户取消
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法写入所选文件。");
        return;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(true); // 写 BOM，确保 Excel 正确识别中文
    out << "姓名,性别,身份证号,生日,电话,工作证号,地址,薪水,职务,部门\n";
    for (const Employee &e : employees) {
        QStringList cols{e.name,      e.sex,       e.id,      e.birthday,
                         e.telephone, e.number,    e.address, QString::number(e.salary, 'f', 2),
                         e.post,      e.department};
        QStringList escaped;
        for (const QString &c : cols) {
            escaped << csvCell(c);
        }
        out << escaped.join(',') << '\n';
    }
    file.close();
    statusLabel->setText(QString("已导出 %1 条到 %2").arg(employees.size()).arg(path));
}

// 在一次会增/删/改数据的操作前调用：保存当前数据快照，供撤销使用。
void MainWindow::pushUndo() {
    undoStack.append(employees);
    if (undoButton) {
        undoButton->setEnabled(true);
    }
}

// 撤销上一次增/删/改：弹出最近的快照并恢复。
void MainWindow::undo() {
    if (undoStack.isEmpty()) {
        return;
    }
    employees = undoStack.takeLast();
    dirty = true; // 撤销后内存与磁盘可能不一致
    refreshTable();
    clearForm();
    if (undoButton) {
        undoButton->setEnabled(!undoStack.isEmpty());
    }
    statusLabel->setText("已撤销上一步操作");
}
