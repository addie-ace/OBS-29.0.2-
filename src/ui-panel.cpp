/*
 * OBS 自动切换插件 - 可视化设置面板实现
 * 全中文界面，嵌入OBS原生Dock
 */

#include "ui-panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QButtonGroup>
#include <QScrollArea>
#include <QMessageBox>
#include <QTimer>
#include <QSizePolicy>
#include <obs-frontend-api.h>
#include <obs.h>

/* ============ 构造函数 ============ */

AutoSwitchPanel::AutoSwitchPanel(QWidget *parent)
    : QDockWidget(QStringLiteral("自动场景切换"), parent)
{
    setObjectName(QStringLiteral("AutoSwitchDock"));

    // 设置最小大小，防止OBS给子Dock极小的默认尺寸
    setMinimumWidth(800);
    setMinimumHeight(600);

    // 允许Dock浮动、移动和关闭
    setFeatures(QDockWidget::DockWidgetMovable |
                QDockWidget::DockWidgetFloatable |
                QDockWidget::DockWidgetClosable);

    BuildUI();

    // 延时设置浮动和大小，确保OBS完成Dock布局后再生效
    // OBS的obs_frontend_add_dock会恢复之前保存的极小尺寸，
    // 必须等事件循环处理完毕后再强制覆盖
    QTimer::singleShot(500, this, [this]() {
        setFloating(true);
        resize(1187, 891);
        show();
        raise();
    });

    // 注册AutoSwitcher的UI更新回调
    AutoSwitcher::Instance().SetUpdateCallback([this]() {
        // 通过Qt事件循环安全调用UI更新
        QMetaObject::invokeMethod(this, "UpdateStatusDisplay",
                                  Qt::QueuedConnection);
    });

    // 连接热键信号到UI槽
    HotkeyNotifier *notifier = AutoSwitcher::Instance().GetNotifier();
    connect(notifier, &HotkeyNotifier::pause_toggled,
            this, &AutoSwitchPanel::OnPauseToggled, Qt::QueuedConnection);
    connect(notifier, &HotkeyNotifier::reset_triggered,
            this, &AutoSwitchPanel::OnResetTriggered, Qt::QueuedConnection);

    // 加载已保存的配置到UI
    LoadFromSwitcher();
}

/* ============ 构建UI ============ */

void AutoSwitchPanel::BuildUI()
{
    QWidget *main_widget = new QWidget(this);
    QVBoxLayout *main_layout = new QVBoxLayout(main_widget);
    main_layout->setContentsMargins(6, 6, 6, 6);
    main_layout->setSpacing(4);

    // === 全局控制区 ===
    QGroupBox *ctrl_group = new QGroupBox(QStringLiteral("全局控制"), main_widget);
    QVBoxLayout *ctrl_layout = new QVBoxLayout(ctrl_group);

    // 主开关
    enable_check_ = new QCheckBox(QStringLiteral("启用自动轮换"), ctrl_group);
    connect(enable_check_, &QCheckBox::toggled, this, &AutoSwitchPanel::OnEnableChanged);
    ctrl_layout->addWidget(enable_check_);

    // 模式选择
    QHBoxLayout *mode_layout = new QHBoxLayout();
    radio_cycle_ = new QRadioButton(QStringLiteral("场景循环"), ctrl_group);
    radio_rotate_ = new QRadioButton(QStringLiteral("来源轮播"), ctrl_group);
    radio_cycle_->setChecked(true);
    QButtonGroup *mode_group = new QButtonGroup(ctrl_group);
    mode_group->addButton(radio_cycle_, 0);
    mode_group->addButton(radio_rotate_, 1);
    mode_layout->addWidget(new QLabel(QStringLiteral("模式："), ctrl_group));
    mode_layout->addWidget(radio_cycle_);
    mode_layout->addWidget(radio_rotate_);
    mode_layout->addStretch();
    ctrl_layout->addLayout(mode_layout);

    connect(mode_group, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &AutoSwitchPanel::OnModeChanged);

    // 冷却时间
    QHBoxLayout *cd_layout = new QHBoxLayout();
    cd_layout->addWidget(new QLabel(QStringLiteral("切换冷却(秒)："), ctrl_group));
    cooldown_spin_ = new QSpinBox(ctrl_group);
    cooldown_spin_->setRange(0, 60);
    cooldown_spin_->setValue(0);
    cooldown_spin_->setToolTip(QStringLiteral("每次切换后的防抖冷却时间"));
    connect(cooldown_spin_, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int val) { AutoSwitcher::Instance().SetCooldown(val); });
    cd_layout->addWidget(cooldown_spin_);
    cd_layout->addStretch();
    ctrl_layout->addLayout(cd_layout);

    // 刷新按钮
    QPushButton *refresh_btn = new QPushButton(QStringLiteral("刷新场景/来源列表"), ctrl_group);
    connect(refresh_btn, &QPushButton::clicked, this, &AutoSwitchPanel::RefreshSceneLists);
    ctrl_layout->addWidget(refresh_btn);

    main_layout->addWidget(ctrl_group);

    // === 模式配置区（堆叠切换）===
    mode_stack_ = new QStackedWidget(main_widget);
    mode_stack_->addWidget(BuildModeAPanel());  // 索引0：场景循环
    mode_stack_->addWidget(BuildModeBPanel());  // 索引1：来源轮播
    main_layout->addWidget(mode_stack_, 1);

    // === 状态显示区 ===
    QGroupBox *status_group = new QGroupBox(QStringLiteral("运行状态"), main_widget);
    QVBoxLayout *status_layout = new QVBoxLayout(status_group);
    status_label_ = new QLabel(QStringLiteral("状态：已停止"), status_group);
    countdown_label_ = new QLabel(QStringLiteral("倒计时：--"), status_group);
    status_layout->addWidget(status_label_);
    status_layout->addWidget(countdown_label_);
    main_layout->addWidget(status_group);

    // 放入滚动区域
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidget(main_widget);
    scroll->setWidgetResizable(true);
    setWidget(scroll);
}

/* ============ 模式A面板 ============ */

QWidget *AutoSwitchPanel::BuildModeAPanel()
{
    QWidget *panel = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(panel);

    QGroupBox *group = new QGroupBox(QStringLiteral("场景循环配置"), panel);
    QVBoxLayout *group_layout = new QVBoxLayout(group);

    group_layout->addWidget(new QLabel(
        QStringLiteral("依次切换场景，循环轮播："), group));

    // 场景列表表格
    cycle_table_ = new QTableWidget(0, 2, group);
    cycle_table_->setHorizontalHeaderLabels(
        {QStringLiteral("场景"), QStringLiteral("停留秒数")});
    cycle_table_->horizontalHeader()->setStretchLastSection(true);
    cycle_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    cycle_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    group_layout->addWidget(cycle_table_, 1);

    // 操作按钮
    QHBoxLayout *btn_layout = new QHBoxLayout();
    cycle_add_btn_ = new QPushButton(QStringLiteral("添加"), group);
    cycle_del_btn_ = new QPushButton(QStringLiteral("删除"), group);
    cycle_up_btn_ = new QPushButton(QStringLiteral("上移"), group);
    cycle_down_btn_ = new QPushButton(QStringLiteral("下移"), group);
    btn_layout->addWidget(cycle_add_btn_);
    btn_layout->addWidget(cycle_del_btn_);
    btn_layout->addWidget(cycle_up_btn_);
    btn_layout->addWidget(cycle_down_btn_);
    group_layout->addLayout(btn_layout);

    connect(cycle_add_btn_, &QPushButton::clicked, this, &AutoSwitchPanel::AddCycleEntry);
    connect(cycle_del_btn_, &QPushButton::clicked, this, &AutoSwitchPanel::RemoveCycleEntry);
    connect(cycle_up_btn_, &QPushButton::clicked, this, &AutoSwitchPanel::MoveCycleUp);
    connect(cycle_down_btn_, &QPushButton::clicked, this, &AutoSwitchPanel::MoveCycleDown);

    layout->addWidget(group);
    return panel;
}

/* ============ 模式B面板 ============ */

QWidget *AutoSwitchPanel::BuildModeBPanel()
{
    QWidget *panel = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(panel);

    QGroupBox *group = new QGroupBox(QStringLiteral("来源轮播配置"), panel);
    QVBoxLayout *group_layout = new QVBoxLayout(group);

    // 目标场景选择
    QHBoxLayout *scene_layout = new QHBoxLayout();
    scene_layout->addWidget(new QLabel(QStringLiteral("目标场景："), group));
    target_scene_combo_ = new QComboBox(group);
    target_scene_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    scene_layout->addWidget(target_scene_combo_, 1);
    group_layout->addLayout(scene_layout);

    connect(target_scene_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AutoSwitchPanel::OnTargetSceneChanged);

    group_layout->addWidget(new QLabel(
        QStringLiteral("轮播来源列表（仅列表内来源参与轮播）："), group));

    // 来源列表表格
    rotate_table_ = new QTableWidget(0, 2, group);
    rotate_table_->setHorizontalHeaderLabels(
        {QStringLiteral("来源"), QStringLiteral("停留秒数")});
    rotate_table_->horizontalHeader()->setStretchLastSection(true);
    rotate_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rotate_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    group_layout->addWidget(rotate_table_, 1);

    // 操作按钮
    QHBoxLayout *btn_layout = new QHBoxLayout();
    rotate_add_btn_ = new QPushButton(QStringLiteral("添加"), group);
    rotate_del_btn_ = new QPushButton(QStringLiteral("删除"), group);
    rotate_up_btn_ = new QPushButton(QStringLiteral("上移"), group);
    rotate_down_btn_ = new QPushButton(QStringLiteral("下移"), group);
    btn_layout->addWidget(rotate_add_btn_);
    btn_layout->addWidget(rotate_del_btn_);
    btn_layout->addWidget(rotate_up_btn_);
    btn_layout->addWidget(rotate_down_btn_);
    group_layout->addLayout(btn_layout);

    connect(rotate_add_btn_, &QPushButton::clicked, this, &AutoSwitchPanel::AddRotateEntry);
    connect(rotate_del_btn_, &QPushButton::clicked, this, &AutoSwitchPanel::RemoveRotateEntry);
    connect(rotate_up_btn_, &QPushButton::clicked, this, &AutoSwitchPanel::MoveRotateUp);
    connect(rotate_down_btn_, &QPushButton::clicked, this, &AutoSwitchPanel::MoveRotateDown);

    layout->addWidget(group);
    return panel;
}

/* ============ OBS数据获取辅助 ============ */

std::vector<std::string> AutoSwitchPanel::GetSceneNames()
{
    std::vector<std::string> names;
    obs_frontend_source_list list = {};
    obs_frontend_get_scenes(&list);
    for (size_t i = 0; i < list.sources.num; i++) {
        const char *name = obs_source_get_name(list.sources.array[i]);
        if (name) names.push_back(name);
    }
    obs_frontend_source_list_free(&list);
    return names;
}

std::vector<std::string> AutoSwitchPanel::GetSourceNames(const std::string &scene_name)
{
    std::vector<std::string> names;
    if (scene_name.empty()) {
        blog(LOG_INFO, "[自动切换] GetSourceNames: 场景名为空，跳过");
        return names;
    }

    obs_source_t *src = obs_get_source_by_name(scene_name.c_str());
    if (!src) {
        blog(LOG_WARNING, "[自动切换] GetSourceNames: 找不到场景「%s」",
             scene_name.c_str());
        return names;
    }

    obs_scene_t *scene = obs_scene_from_source(src);
    if (!scene) {
        blog(LOG_WARNING, "[自动切换] GetSourceNames: 「%s」不是场景类型",
             scene_name.c_str());
        obs_source_release(src);
        return names;
    }

    // 递归枚举场景项目，支持群组内的来源
    struct EnumData {
        std::vector<std::string> *names;
    } data = {&names};

    obs_scene_enum_items(scene,
        [](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
            auto *d = static_cast<EnumData *>(param);
            obs_source_t *s = obs_sceneitem_get_source(item);
            if (s) {
                const char *name = obs_source_get_name(s);
                const char *id = obs_source_get_id(s);
                if (name) {
                    d->names->push_back(name);
                    blog(LOG_INFO, "[自动切换]   发现来源: 「%s」 (类型: %s)",
                         name, id ? id : "unknown");
                }
                // 如果是群组，递归进入查找子来源
                if (obs_sceneitem_is_group(item)) {
                    obs_scene_t *group_scene = obs_sceneitem_get_scene(item);
                    if (group_scene) {
                        obs_scene_enum_items(group_scene,
                            [](obs_scene_t *, obs_sceneitem_t *sub_item, void *p) -> bool {
                                auto *dd = static_cast<EnumData *>(p);
                                obs_source_t *ss = obs_sceneitem_get_source(sub_item);
                                if (ss) {
                                    const char *nn = obs_source_get_name(ss);
                                    const char *sid = obs_source_get_id(ss);
                                    if (nn) {
                                        dd->names->push_back(nn);
                                        blog(LOG_INFO,
                                            "[自动切换]   发现群组内来源: 「%s」 (类型: %s)",
                                            nn, sid ? sid : "unknown");
                                    }
                                }
                                return true;
                            }, d);
                    }
                }
            }
            return true;
        }, &data);

    blog(LOG_INFO, "[自动切换] GetSourceNames: 场景「%s」共发现 %zu 个来源",
         scene_name.c_str(), names.size());
    obs_source_release(src);
    return names;
}

/* ============ 填充下拉框 ============ */

void AutoSwitchPanel::PopulateSceneCombo(QComboBox *combo,
                                         const std::string &selected)
{
    combo->blockSignals(true);
    combo->clear();
    auto names = GetSceneNames();
    for (auto &n : names) {
        combo->addItem(QString::fromStdString(n));
    }
    // 恢复选中项
    if (!selected.empty()) {
        int idx = combo->findText(QString::fromStdString(selected));
        if (idx >= 0) combo->setCurrentIndex(idx);
    }
    combo->blockSignals(false);
}

void AutoSwitchPanel::PopulateSourceCombo(QComboBox *combo,
                                          const std::string &scene_name,
                                          const std::string &selected)
{
    combo->blockSignals(true);
    combo->clear();
    auto names = GetSourceNames(scene_name);
    for (auto &n : names) {
        combo->addItem(QString::fromStdString(n));
    }
    if (!selected.empty()) {
        int idx = combo->findText(QString::fromStdString(selected));
        if (idx >= 0) combo->setCurrentIndex(idx);
    }
    combo->blockSignals(false);
}

/* ============ 表格行设置 ============ */

void AutoSwitchPanel::SetupCycleRow(int row, const std::string &scene_name,
                                    int duration)
{
    // 场景下拉框
    QComboBox *combo = new QComboBox();
    PopulateSceneCombo(combo, scene_name);
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this]() { if (!loading_) SyncCycleListFromTable(); });

    // 秒数输入
    QSpinBox *spin = new QSpinBox();
    spin->setRange(1, 86400);
    spin->setValue(duration > 0 ? duration : 10);
    spin->setSuffix(QStringLiteral(" 秒"));
    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
            [this]() { if (!loading_) SyncCycleListFromTable(); });

    cycle_table_->setCellWidget(row, 0, combo);
    cycle_table_->setCellWidget(row, 1, spin);
}

void AutoSwitchPanel::SetupRotateRow(int row, const std::string &source_name,
                                     int duration)
{
    std::string target = AutoSwitcher::Instance().GetTargetScene();

    // 安全回退：如果AutoSwitcher中目标场景为空，从下拉框获取
    if (target.empty() && target_scene_combo_ && target_scene_combo_->count() > 0) {
        target = target_scene_combo_->currentText().toStdString();
        blog(LOG_INFO, "[自动切换] SetupRotateRow: GetTargetScene为空，回退使用下拉框值「%s」",
             target.c_str());
    }

    // 来源下拉框
    QComboBox *combo = new QComboBox();
    PopulateSourceCombo(combo, target, source_name);
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this]() { if (!loading_) SyncRotateListFromTable(); });

    // 秒数输入
    QSpinBox *spin = new QSpinBox();
    spin->setRange(1, 86400);
    spin->setValue(duration > 0 ? duration : 10);
    spin->setSuffix(QStringLiteral(" 秒"));
    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
            [this]() { if (!loading_) SyncRotateListFromTable(); });

    rotate_table_->setCellWidget(row, 0, combo);
    rotate_table_->setCellWidget(row, 1, spin);
}

/* ============ 模式A条目操作 ============ */

void AutoSwitchPanel::AddCycleEntry()
{
    int row = cycle_table_->rowCount();
    cycle_table_->insertRow(row);
    SetupCycleRow(row, "", 10);
    SyncCycleListFromTable();
}

void AutoSwitchPanel::RemoveCycleEntry()
{
    int row = cycle_table_->currentRow();
    if (row >= 0) {
        cycle_table_->removeRow(row);
        SyncCycleListFromTable();
    }
}

void AutoSwitchPanel::MoveCycleUp()
{
    int row = cycle_table_->currentRow();
    if (row <= 0) return;
    // 交换两行数据
    auto &list = const_cast<std::vector<SceneCycleEntry>&>(
        AutoSwitcher::Instance().GetCycleList());
    if (row < (int)list.size()) {
        std::swap(list[row], list[row - 1]);
        AutoSwitcher::Instance().SetCycleList(list);
        LoadFromSwitcher();
        cycle_table_->selectRow(row - 1);
    }
}

void AutoSwitchPanel::MoveCycleDown()
{
    int row = cycle_table_->currentRow();
    if (row < 0 || row >= cycle_table_->rowCount() - 1) return;
    auto &list = const_cast<std::vector<SceneCycleEntry>&>(
        AutoSwitcher::Instance().GetCycleList());
    if (row + 1 < (int)list.size()) {
        std::swap(list[row], list[row + 1]);
        AutoSwitcher::Instance().SetCycleList(list);
        LoadFromSwitcher();
        cycle_table_->selectRow(row + 1);
    }
}

/* ============ 模式B条目操作 ============ */

void AutoSwitchPanel::AddRotateEntry()
{
    int row = rotate_table_->rowCount();
    rotate_table_->insertRow(row);
    SetupRotateRow(row, "", 10);
    SyncRotateListFromTable();
}

void AutoSwitchPanel::RemoveRotateEntry()
{
    int row = rotate_table_->currentRow();
    if (row >= 0) {
        rotate_table_->removeRow(row);
        SyncRotateListFromTable();
    }
}

void AutoSwitchPanel::MoveRotateUp()
{
    int row = rotate_table_->currentRow();
    if (row <= 0) return;
    auto &list = const_cast<std::vector<SourceRotateEntry>&>(
        AutoSwitcher::Instance().GetRotateList());
    if (row < (int)list.size()) {
        std::swap(list[row], list[row - 1]);
        AutoSwitcher::Instance().SetRotateList(list);
        LoadFromSwitcher();
        rotate_table_->selectRow(row - 1);
    }
}

void AutoSwitchPanel::MoveRotateDown()
{
    int row = rotate_table_->currentRow();
    if (row < 0 || row >= rotate_table_->rowCount() - 1) return;
    auto &list = const_cast<std::vector<SourceRotateEntry>&>(
        AutoSwitcher::Instance().GetRotateList());
    if (row + 1 < (int)list.size()) {
        std::swap(list[row], list[row + 1]);
        AutoSwitcher::Instance().SetRotateList(list);
        LoadFromSwitcher();
        rotate_table_->selectRow(row + 1);
    }
}

/* ============ 数据同步 ============ */

void AutoSwitchPanel::SyncCycleListFromTable()
{
    std::vector<SceneCycleEntry> list;
    for (int i = 0; i < cycle_table_->rowCount(); i++) {
        SceneCycleEntry entry;
        QComboBox *combo = qobject_cast<QComboBox *>(
            cycle_table_->cellWidget(i, 0));
        QSpinBox *spin = qobject_cast<QSpinBox *>(
            cycle_table_->cellWidget(i, 1));
        if (combo)
            entry.scene_name = combo->currentText().toStdString();
        if (spin)
            entry.duration_seconds = spin->value();
        list.push_back(entry);
    }
    AutoSwitcher::Instance().SetCycleList(list);
}

void AutoSwitchPanel::SyncRotateListFromTable()
{
    std::vector<SourceRotateEntry> list;
    for (int i = 0; i < rotate_table_->rowCount(); i++) {
        SourceRotateEntry entry;
        QComboBox *combo = qobject_cast<QComboBox *>(
            rotate_table_->cellWidget(i, 0));
        QSpinBox *spin = qobject_cast<QSpinBox *>(
            rotate_table_->cellWidget(i, 1));
        if (combo)
            entry.source_name = combo->currentText().toStdString();
        if (spin)
            entry.duration_seconds = spin->value();
        list.push_back(entry);
    }
    AutoSwitcher::Instance().SetRotateList(list);
}

/* ============ 配置加载/应用 ============ */

void AutoSwitchPanel::LoadFromSwitcher()
{
    loading_ = true;
    auto &sw = AutoSwitcher::Instance();

    // 基本设置
    enable_check_->setChecked(sw.IsEnabled());
    if (sw.GetMode() == SwitchMode::SceneCycle)
        radio_cycle_->setChecked(true);
    else
        radio_rotate_->setChecked(true);
    cooldown_spin_->setValue(sw.GetCooldown());
    mode_stack_->setCurrentIndex(static_cast<int>(sw.GetMode()));

    // 刷新所有场景下拉框
    PopulateSceneCombo(target_scene_combo_, sw.GetTargetScene());

    // 关键：将目标场景同步回AutoSwitcher
    // PopulateSceneCombo使用blockSignals，OnTargetSceneChanged不会触发，
    // 必须手动设置，否则SetupRotateRow中的GetTargetScene()会返回空
    if (target_scene_combo_->count() > 0) {
        std::string target = target_scene_combo_->currentText().toStdString();
        if (!target.empty()) {
            sw.SetTargetScene(target);
            blog(LOG_INFO, "[自动切换] LoadFromSwitcher: 同步目标场景「%s」",
                 target.c_str());
        }
    }

    // 模式A表格
    cycle_table_->setRowCount(0);
    for (auto &entry : sw.GetCycleList()) {
        int row = cycle_table_->rowCount();
        cycle_table_->insertRow(row);
        SetupCycleRow(row, entry.scene_name, entry.duration_seconds);
    }

    // 模式B表格
    rotate_table_->setRowCount(0);
    for (auto &entry : sw.GetRotateList()) {
        int row = rotate_table_->rowCount();
        rotate_table_->insertRow(row);
        SetupRotateRow(row, entry.source_name, entry.duration_seconds);
    }

    loading_ = false;
    UpdateStatusDisplay();
}

void AutoSwitchPanel::ApplyToSwitcher()
{
    auto &sw = AutoSwitcher::Instance();
    sw.SetEnabled(enable_check_->isChecked());
    sw.SetMode(radio_cycle_->isChecked() ? SwitchMode::SceneCycle
                                         : SwitchMode::SourceRotate);
    sw.SetCooldown(cooldown_spin_->value());

    if (target_scene_combo_->currentText().length() > 0)
        sw.SetTargetScene(target_scene_combo_->currentText().toStdString());

    SyncCycleListFromTable();
    SyncRotateListFromTable();
}

/* ============ 事件响应 ============ */

void AutoSwitchPanel::OnEnableChanged(bool checked)
{
    if (!loading_) {
        AutoSwitcher::Instance().SetEnabled(checked);
    }
}

void AutoSwitchPanel::OnModeChanged()
{
    if (loading_) return;
    SwitchMode mode = radio_cycle_->isChecked() ? SwitchMode::SceneCycle
                                                 : SwitchMode::SourceRotate;
    AutoSwitcher::Instance().SetMode(mode);
    mode_stack_->setCurrentIndex(static_cast<int>(mode));
}

void AutoSwitchPanel::OnTargetSceneChanged(int index)
{
    if (loading_ || index < 0) return;
    std::string name = target_scene_combo_->currentText().toStdString();
    AutoSwitcher::Instance().SetTargetScene(name);

    // 刷新来源列表中的下拉框
    for (int i = 0; i < rotate_table_->rowCount(); i++) {
        QComboBox *combo = qobject_cast<QComboBox *>(
            rotate_table_->cellWidget(i, 0));
        if (combo) {
            std::string current = combo->currentText().toStdString();
            PopulateSourceCombo(combo, name, current);
        }
    }
    SyncRotateListFromTable();
}

void AutoSwitchPanel::RefreshSceneLists()
{
    // 刷新所有场景下拉框
    for (int i = 0; i < cycle_table_->rowCount(); i++) {
        QComboBox *combo = qobject_cast<QComboBox *>(
            cycle_table_->cellWidget(i, 0));
        if (combo) {
            std::string current = combo->currentText().toStdString();
            PopulateSceneCombo(combo, current);
        }
    }

    std::string target = AutoSwitcher::Instance().GetTargetScene();
    PopulateSceneCombo(target_scene_combo_, target);

    // 刷新来源下拉框
    for (int i = 0; i < rotate_table_->rowCount(); i++) {
        QComboBox *combo = qobject_cast<QComboBox *>(
            rotate_table_->cellWidget(i, 0));
        if (combo) {
            std::string current = combo->currentText().toStdString();
            PopulateSourceCombo(combo, target, current);
        }
    }
}

/* ============ 状态显示更新 ============ */

void AutoSwitchPanel::UpdateStatusDisplay()
{
    auto &sw = AutoSwitcher::Instance();

    if (!sw.IsEnabled()) {
        status_label_->setText(QStringLiteral("状态：已停止"));
        countdown_label_->setText(QStringLiteral("倒计时：--"));
        return;
    }

    if (sw.IsPaused()) {
        status_label_->setText(QStringLiteral("状态：已暂停"));
        countdown_label_->setText(QStringLiteral("倒计时：已暂停"));
        return;
    }

    // 构建状态文本
    QString status_text;
    if (sw.GetMode() == SwitchMode::SceneCycle) {
        status_text = QStringLiteral("状态：场景循环 - 当前场景「%1」")
            .arg(QString::fromStdString(sw.GetCurrentSceneName()));
    } else {
        status_text = QStringLiteral("状态：来源轮播 - 当前来源「%1」")
            .arg(QString::fromStdString(sw.GetCurrentSourceName()));
    }
    status_label_->setText(status_text);

    int remaining = sw.GetRemainingSeconds();
    countdown_label_->setText(QStringLiteral("倒计时：%1 秒").arg(remaining));
}

/* ============ 热键响应 ============ */

void AutoSwitchPanel::OnPauseToggled()
{
    UpdateStatusDisplay();
}

void AutoSwitchPanel::OnResetTriggered()
{
    LoadFromSwitcher();
    UpdateStatusDisplay();
}
