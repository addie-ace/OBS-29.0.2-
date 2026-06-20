#pragma once
/*
 * OBS 自动切换插件 - 可视化设置面板
 * 基于Qt的DockWidget，嵌入OBS界面
 */

#include <QDockWidget>
#include <QLabel>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QGroupBox>
#include <QStackedWidget>

#include "auto-switcher.h"

class AutoSwitchPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit AutoSwitchPanel(QWidget *parent = nullptr);
    ~AutoSwitchPanel() override = default;

    // 从AutoSwitcher读取配置并更新UI
    void LoadFromSwitcher();
    // 从UI写入AutoSwitcher配置
    void ApplyToSwitcher();

public slots:
    // 更新状态显示（由定时器回调触发）
    void UpdateStatusDisplay();
    // 热键触发的暂停/重置
    void OnPauseToggled();
    void OnResetTriggered();

private slots:
    // 主开关变更
    void OnEnableChanged(bool checked);
    // 模式切换
    void OnModeChanged();
    // 目标场景变更（模式B）
    void OnTargetSceneChanged(int index);
    // 场景列表刷新
    void RefreshSceneLists();
    // 模式A：添加/删除/上移/下移场景条目
    void AddCycleEntry();
    void RemoveCycleEntry();
    void MoveCycleUp();
    void MoveCycleDown();
    // 模式B：添加/删除/上移/下移来源条目
    void AddRotateEntry();
    void RemoveRotateEntry();
    void MoveRotateUp();
    void MoveRotateDown();

private:
    // 构建UI布局
    void BuildUI();
    // 构建模式A面板
    QWidget *BuildModeAPanel();
    // 构建模式B面板
    QWidget *BuildModeBPanel();

    // 从表格读取数据到AutoSwitcher
    void SyncCycleListFromTable();
    void SyncRotateListFromTable();

    // 刷新场景下拉框内容
    void PopulateSceneCombo(QComboBox *combo, const std::string &selected);
    // 刷新来源下拉框内容
    void PopulateSourceCombo(QComboBox *combo, const std::string &scene_name,
                             const std::string &selected);

    // 获取OBS中所有场景名
    std::vector<std::string> GetSceneNames();
    // 获取指定场景中的所有来源名
    std::vector<std::string> GetSourceNames(const std::string &scene_name);

    // 在表格行中创建场景下拉+秒数控件
    void SetupCycleRow(int row, const std::string &scene_name, int duration);
    void SetupRotateRow(int row, const std::string &source_name, int duration);

    /* ============ UI控件 ============ */

    // 全局控制
    QCheckBox       *enable_check_   = nullptr;
    QRadioButton    *radio_cycle_    = nullptr;
    QRadioButton    *radio_rotate_   = nullptr;
    QSpinBox        *cooldown_spin_  = nullptr;
    QStackedWidget  *mode_stack_     = nullptr;

    // 模式A：场景循环
    QTableWidget    *cycle_table_    = nullptr;
    QPushButton     *cycle_add_btn_  = nullptr;
    QPushButton     *cycle_del_btn_  = nullptr;
    QPushButton     *cycle_up_btn_   = nullptr;
    QPushButton     *cycle_down_btn_ = nullptr;

    // 模式B：来源轮播
    QComboBox       *target_scene_combo_ = nullptr;
    QTableWidget    *rotate_table_   = nullptr;
    QPushButton     *rotate_add_btn_ = nullptr;
    QPushButton     *rotate_del_btn_ = nullptr;
    QPushButton     *rotate_up_btn_  = nullptr;
    QPushButton     *rotate_down_btn_= nullptr;

    // 状态显示
    QLabel          *status_label_   = nullptr;
    QLabel          *countdown_label_= nullptr;

    // 防止信号循环的标志
    bool loading_ = false;
};
