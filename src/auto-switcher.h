#pragma once
/*
 * OBS 自动切换插件 - 核心切换器
 * 负责场景循环与来源轮播的核心逻辑
 */

#include <obs.h>
#include <string>
#include <vector>
#include <functional>

#include <QObject>

// 前向声明
class QTimer;

/* ============ 数据条目定义 ============ */

// 场景循环条目
struct SceneCycleEntry {
    std::string scene_name;  // 场景名称
    int duration_seconds = 10; // 停留秒数
};

// 来源轮播条目
struct SourceRotateEntry {
    std::string source_name; // 来源名称
    int duration_seconds = 10; // 停留秒数
};

// 工作模式枚举
enum class SwitchMode : int {
    SceneCycle = 0,    // 模式A：全局场景循环
    SourceRotate = 1   // 模式B：单场景来源轮播
};

/* ============ 跨线程信号通知器 ============ */

// 用于从热键线程安全通知主线程UI更新
class HotkeyNotifier : public QObject {
    Q_OBJECT
public:
    explicit HotkeyNotifier(QObject *parent = nullptr) : QObject(parent) {}
signals:
    void pause_toggled();  // 暂停/恢复
    void reset_triggered(); // 重置序列
};

/* ============ 核心自动切换器 ============ */

class AutoSwitcher {
public:
    // 获取单例
    static AutoSwitcher &Instance();

    // --- 基本控制 ---
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return enabled_; }

    void TogglePause();
    bool IsPaused() const { return paused_; }

    void Reset();

    // --- 模式 ---
    void SetMode(SwitchMode mode);
    SwitchMode GetMode() const { return mode_; }

    // --- 冷却时间 ---
    void SetCooldown(int seconds);
    int GetCooldown() const { return cooldown_seconds_; }

    // --- 场景循环列表（模式A）---
    void SetCycleList(const std::vector<SceneCycleEntry> &list);
    const std::vector<SceneCycleEntry> &GetCycleList() const { return cycle_list_; }

    // --- 来源轮播配置（模式B）---
    void SetTargetScene(const std::string &name);
    std::string GetTargetScene() const { return target_scene_; }

    void SetRotateList(const std::vector<SourceRotateEntry> &list);
    const std::vector<SourceRotateEntry> &GetRotateList() const { return rotate_list_; }

    // --- 状态查询 ---
    int GetRemainingSeconds() const;
    std::string GetCurrentSceneName() const;
    std::string GetCurrentSourceName() const;

    // --- 配置持久化 ---
    void Save();
    void Load();

    // --- UI更新回调 ---
    void SetUpdateCallback(std::function<void()> cb) { update_cb_ = cb; }

    // --- 跨线程通知器（供热键使用）---
    HotkeyNotifier *GetNotifier() { return &notifier_; }

private:
    AutoSwitcher();
    ~AutoSwitcher();
    AutoSwitcher(const AutoSwitcher &) = delete;
    AutoSwitcher &operator=(const AutoSwitcher &) = delete;

    // 定时器控制
    void StartTimer();
    void StopTimer();
    void OnTimerTick();

    // 执行切换
    void DoSwitch();
    void SwitchSceneCycle();
    void SwitchSourceRotate();

    // 初始化当前状态
    void InitCurrentState();

    // 设置回调桥接（Qt信号槽）
    void SetupNotifierBridge();

    // --- 成员变量 ---
    bool enabled_ = false;
    bool paused_ = false;
    SwitchMode mode_ = SwitchMode::SceneCycle;
    int cooldown_seconds_ = 0;

    // 模式A 数据
    std::vector<SceneCycleEntry> cycle_list_;
    int cycle_index_ = 0;

    // 模式B 数据
    std::string target_scene_;
    std::vector<SourceRotateEntry> rotate_list_;
    int rotate_index_ = 0;

    // 运行时状态
    int remaining_ms_ = 0;
    int cooldown_remaining_ms_ = 0;
    std::string current_scene_name_;
    std::string current_source_name_;

    // 定时器
    QTimer *timer_ = nullptr;

    // UI更新回调
    std::function<void()> update_cb_;

    // 跨线程信号通知器
    HotkeyNotifier notifier_;
};
