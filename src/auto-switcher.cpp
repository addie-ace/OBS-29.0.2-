/*
 * OBS 自动切换插件 - 核心切换逻辑实现
 * 包含场景循环与来源轮播的全部切换逻辑
 */

#include "auto-switcher.h"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <util/bmem.h>
#include <QTimer>
#include <QMetaObject>

// 定时器刷新间隔（毫秒），200ms足够显示秒级倒计时
#define TIMER_INTERVAL_MS 200

// 前向声明：递归查找场景项目（支持群组内的来源）
static obs_sceneitem_t *find_sceneitem_recursive(obs_scene_t *scene,
                                                  const char *name);

/* ============ 单例获取 ============ */

AutoSwitcher &AutoSwitcher::Instance()
{
    static AutoSwitcher instance;
    return instance;
}

/* ============ 构造与析构 ============ */

AutoSwitcher::AutoSwitcher()
{
    // 创建Qt定时器，用于倒计时刷新
    timer_ = new QTimer();
    timer_->setInterval(TIMER_INTERVAL_MS);

    // 连接定时器信号到主逻辑
    QObject::connect(timer_, &QTimer::timeout, [this]() {
        OnTimerTick();
    });

    // 设置跨线程通知桥接
    SetupNotifierBridge();

    // 加载已保存的配置
    Load();
}

AutoSwitcher::~AutoSwitcher()
{
    StopTimer();
    if (timer_) {
        delete timer_;
        timer_ = nullptr;
    }
}

/* ============ 跨线程通知桥接 ============ */

void AutoSwitcher::SetupNotifierBridge()
{
    // 热键线程发出的信号，通过Qt队列连接到主线程执行
    QObject::connect(&notifier_, &HotkeyNotifier::pause_toggled,
        timer_, [this]() { TogglePause(); }, Qt::QueuedConnection);
    QObject::connect(&notifier_, &HotkeyNotifier::reset_triggered,
        timer_, [this]() { Reset(); }, Qt::QueuedConnection);
}

/* ============ 基本控制 ============ */

void AutoSwitcher::SetEnabled(bool enabled)
{
    if (enabled_ == enabled)
        return;

    enabled_ = enabled;

    if (enabled_) {
        paused_ = false;
        InitCurrentState();
        StartTimer();
        blog(LOG_INFO, "[自动切换] 已启用自动轮换");
    } else {
        StopTimer();
        paused_ = false;
        current_scene_name_.clear();
        current_source_name_.clear();
        remaining_ms_ = 0;
        blog(LOG_INFO, "[自动切换] 已停用自动轮换");
    }

    Save();
    if (update_cb_) update_cb_();
}

void AutoSwitcher::TogglePause()
{
    if (!enabled_) return;

    paused_ = !paused_;

    if (paused_) {
        blog(LOG_INFO, "[自动切换] 已暂停轮换");
    } else {
        blog(LOG_INFO, "[自动切换] 已恢复轮换");
    }

    if (update_cb_) update_cb_();
}

void AutoSwitcher::Reset()
{
    if (!enabled_) return;

    blog(LOG_INFO, "[自动切换] 重置轮换序列，回到第一条");

    InitCurrentState();

    if (paused_) {
        paused_ = false;
    }

    if (update_cb_) update_cb_();
}

void AutoSwitcher::SetMode(SwitchMode mode)
{
    if (mode_ == mode) return;

    mode_ = mode;

    // 如果正在运行，重新初始化当前模式的状态
    if (enabled_) {
        InitCurrentState();
    }

    Save();
    if (update_cb_) update_cb_();
}

void AutoSwitcher::SetCooldown(int seconds)
{
    cooldown_seconds_ = (seconds < 0) ? 0 : seconds;
    Save();
}

/* ============ 列表设置 ============ */

void AutoSwitcher::SetCycleList(const std::vector<SceneCycleEntry> &list)
{
    cycle_list_ = list;

    // 修正索引越界
    if (!cycle_list_.empty() && cycle_index_ >= (int)cycle_list_.size())
        cycle_index_ = 0;

    // 如果正在运行模式A，更新剩余时间
    if (enabled_ && mode_ == SwitchMode::SceneCycle && !cycle_list_.empty()) {
        remaining_ms_ = cycle_list_[cycle_index_].duration_seconds * 1000;
    }

    Save();
}

void AutoSwitcher::SetTargetScene(const std::string &name)
{
    target_scene_ = name;
    Save();
}

void AutoSwitcher::SetRotateList(const std::vector<SourceRotateEntry> &list)
{
    rotate_list_ = list;

    // 修正索引越界（新逻辑范围: 0 ~ n）
    int n = (int)rotate_list_.size();
    if (n > 0 && rotate_index_ > n)
        rotate_index_ = 0;
    if (n == 0)
        rotate_index_ = 0;

    // 如果正在运行模式B，更新剩余时间
    if (enabled_ && mode_ == SwitchMode::SourceRotate && n > 0) {
        int dur_idx = (rotate_index_ == 0) ? 0 : (rotate_index_ - 1);
        if (dur_idx < n)
            remaining_ms_ = rotate_list_[dur_idx].duration_seconds * 1000;
    }

    Save();
}

/* ============ 状态查询 ============ */

int AutoSwitcher::GetRemainingSeconds() const
{
    // 向上取整，显示更直观
    return (remaining_ms_ + 999) / 1000;
}

std::string AutoSwitcher::GetCurrentSceneName() const
{
    return current_scene_name_;
}

std::string AutoSwitcher::GetCurrentSourceName() const
{
    return current_source_name_;
}

/* ============ 定时器控制 ============ */

void AutoSwitcher::StartTimer()
{
    if (timer_ && !timer_->isActive()) {
        timer_->start();
    }
}

void AutoSwitcher::StopTimer()
{
    if (timer_ && timer_->isActive()) {
        timer_->stop();
    }
}

/* ============ 初始化当前状态 ============ */

void AutoSwitcher::InitCurrentState()
{
    cooldown_remaining_ms_ = 0;
    current_source_name_.clear();

    if (mode_ == SwitchMode::SceneCycle) {
        // 模式A：重置到第一个场景
        cycle_index_ = 0;
        current_scene_name_.clear();

        if (!cycle_list_.empty()) {
            // 尝试切换到第一个场景
            obs_source_t *scene = obs_get_source_by_name(
                cycle_list_[0].scene_name.c_str());
            if (scene) {
                obs_frontend_set_current_scene(scene);
                current_scene_name_ = cycle_list_[0].scene_name;
                obs_source_release(scene);
            } else {
                blog(LOG_WARNING, "[自动切换] 警告：场景「%s」不存在，跳过",
                     cycle_list_[0].scene_name.c_str());
                // 跳过无效条目，直接尝试切换
                DoSwitch();
                return;
            }
            remaining_ms_ = cycle_list_[0].duration_seconds * 1000;
        } else {
            remaining_ms_ = 0;
        }
    } else {
        // 模式B：图层隐藏逻辑 - 初始状态显示全部来源
        rotate_index_ = 0;
        current_scene_name_ = target_scene_;

        if (!rotate_list_.empty() && !target_scene_.empty()) {
            obs_source_t *scene_src = obs_get_source_by_name(
                target_scene_.c_str());
            if (scene_src) {
                obs_scene_t *scene = obs_scene_from_source(scene_src);
                if (scene) {
                    // 显示所有轮播来源（初始状态）
                    for (auto &e : rotate_list_) {
                        obs_sceneitem_t *it = find_sceneitem_recursive(
                            scene, e.source_name.c_str());
                        if (it)
                            obs_sceneitem_set_visible(it, true);
                    }
                    current_source_name_ = u8"全部显示";
                    remaining_ms_ = rotate_list_[0].duration_seconds * 1000;
                    blog(LOG_INFO,
                         "[自动切换] 来源轮播初始化：显示全部 %d 个来源",
                         (int)rotate_list_.size());
                }
                obs_source_release(scene_src);
            } else {
                blog(LOG_WARNING, "[自动切换] 警告：目标场景「%s」不存在",
                     target_scene_.c_str());
                remaining_ms_ = 0;
            }
        } else {
            remaining_ms_ = 0;
        }
    }
}

/* ============ 定时器回调 ============ */

void AutoSwitcher::OnTimerTick()
{
    if (!enabled_ || paused_)
        return;

    // 冷却倒计时
    if (cooldown_remaining_ms_ > 0) {
        cooldown_remaining_ms_ -= TIMER_INTERVAL_MS;
        if (cooldown_remaining_ms_ < 0)
            cooldown_remaining_ms_ = 0;
        return; // 冷却期间不倒计时
    }

    // 正常倒计时
    remaining_ms_ -= TIMER_INTERVAL_MS;

    if (remaining_ms_ <= 0) {
        DoSwitch();
    }

    // 通知UI更新状态显示
    if (update_cb_) update_cb_();
}

/* ============ 执行切换 ============ */

void AutoSwitcher::DoSwitch()
{
    if (mode_ == SwitchMode::SceneCycle) {
        SwitchSceneCycle();
    } else {
        SwitchSourceRotate();
    }

    // 冷却时间
    if (cooldown_seconds_ > 0) {
        cooldown_remaining_ms_ = cooldown_seconds_ * 1000;
    }
}

/* ============ 模式A：场景循环切换 ============ */

void AutoSwitcher::SwitchSceneCycle()
{
    if (cycle_list_.empty()) {
        current_scene_name_.clear();
        remaining_ms_ = 0;
        return;
    }

    // 寻找下一个有效场景（循环查找）
    int start_index = cycle_index_;
    bool found = false;

    do {
        cycle_index_ = (cycle_index_ + 1) % (int)cycle_list_.size();

        obs_source_t *scene = obs_get_source_by_name(
            cycle_list_[cycle_index_].scene_name.c_str());

        if (scene) {
            obs_frontend_set_current_scene(scene);
            current_scene_name_ = cycle_list_[cycle_index_].scene_name;
            remaining_ms_ = cycle_list_[cycle_index_].duration_seconds * 1000;
            obs_source_release(scene);

            blog(LOG_INFO, "[自动切换] 场景循环：切换到「%s」，停留%d秒",
                 current_scene_name_.c_str(),
                 cycle_list_[cycle_index_].duration_seconds);
            found = true;
            break;
        } else {
            blog(LOG_WARNING, "[自动切换] 警告：场景「%s」已被删除或不存在，跳过",
                 cycle_list_[cycle_index_].scene_name.c_str());
        }
    } while (cycle_index_ != start_index);

    if (!found) {
        blog(LOG_WARNING, "[自动切换] 警告：所有场景均无效，循环暂停");
        remaining_ms_ = 0;
        current_scene_name_.clear();
    }
}

/* ============ 模式B：来源轮播切换（图层隐藏逻辑） ============ */

// 辅助函数：递归查找场景项目（支持群组内的来源）
static obs_sceneitem_t *find_sceneitem_recursive(obs_scene_t *scene,
                                                  const char *name)
{
    if (!scene || !name) return nullptr;

    // 先在当前场景层级查找
    obs_sceneitem_t *item = obs_scene_find_source(scene, name);
    if (item) return item;

    // 遍历所有场景项目，进入群组递归查找
    struct FindData {
        const char *name;
        obs_sceneitem_t *found;
    } fdata = {name, nullptr};

    obs_scene_enum_items(scene,
        [](obs_scene_t *, obs_sceneitem_t *si, void *param) -> bool {
            auto *fd = static_cast<FindData *>(param);
            if (obs_sceneitem_is_group(si)) {
                obs_scene_t *group_scene = obs_sceneitem_get_scene(si);
                if (group_scene) {
                    obs_sceneitem_t *sub = obs_scene_find_source(
                        group_scene, fd->name);
                    if (sub) {
                        fd->found = sub;
                        return false; // 停止遍历
                    }
                }
            }
            return true; // 继续遍历
        }, &fdata);

    return fdata.found;
}

void AutoSwitcher::SwitchSourceRotate()
{
    if (rotate_list_.empty() || target_scene_.empty()) {
        current_source_name_.clear();
        remaining_ms_ = 0;
        return;
    }

    // 获取目标场景
    obs_source_t *scene_src = obs_get_source_by_name(target_scene_.c_str());
    if (!scene_src) {
        blog(LOG_WARNING, "[自动切换] 警告：目标场景「%s」已被删除",
             target_scene_.c_str());
        remaining_ms_ = 0;
        current_source_name_.clear();
        return;
    }

    obs_scene_t *scene = obs_scene_from_source(scene_src);
    if (!scene) {
        obs_source_release(scene_src);
        remaining_ms_ = 0;
        return;
    }

    int n = (int)rotate_list_.size();

    // === 图层隐藏逻辑 ===
    // 轮播列表中的来源按从上到下的图层顺序排列
    // rotate_index_=0 时：显示全部来源（新周期开始）
    // rotate_index_=1..n 时：隐藏 rotate_list_[index-1]，其余保持当前状态
    // 到达 n+1 时：自动重置为 0（显示全部），开始新周期

    if (rotate_index_ == 0) {
        // 显示所有轮播来源（重置周期）
        for (auto &entry : rotate_list_) {
            obs_sceneitem_t *it = find_sceneitem_recursive(scene,
                entry.source_name.c_str());
            if (it) {
                obs_sceneitem_set_visible(it, true);
            }
        }
        current_source_name_ = u8"全部显示";
        remaining_ms_ = rotate_list_[0].duration_seconds * 1000;

        blog(LOG_INFO, "[自动切换] 来源轮播：重置周期，显示全部 %d 个来源",
             n);
    } else {
        // 隐藏 rotate_list_[rotate_index_ - 1]
        int hide_idx = rotate_index_ - 1;
        obs_sceneitem_t *item = find_sceneitem_recursive(scene,
            rotate_list_[hide_idx].source_name.c_str());
        if (item) {
            obs_sceneitem_set_visible(item, false);
            blog(LOG_INFO, "[自动切换] 来源轮播：隐藏图层「%s」",
                 rotate_list_[hide_idx].source_name.c_str());
        } else {
            blog(LOG_WARNING, "[自动切换] 警告：来源「%s」在场景中不存在，跳过",
                 rotate_list_[hide_idx].source_name.c_str());
        }
        current_source_name_ = rotate_list_[hide_idx].source_name;
        remaining_ms_ = rotate_list_[hide_idx].duration_seconds * 1000;
    }

    // 推进索引：0 -> 1 -> 2 -> ... -> n -> 0
    rotate_index_ = (rotate_index_ + 1) % (n + 1);

    obs_source_release(scene_src);
}

/* ============ 配置持久化 ============ */

// 获取插件配置文件路径
static std::string get_plugin_config_path()
{
    char *path = obs_module_config_path(nullptr);
    if (path) {
        std::string result = path;
        result += "/plugin_settings.json";
        bfree(path);
        return result;
    }
    return "";
}

void AutoSwitcher::Save()
{
    std::string path = get_plugin_config_path();
    if (path.empty()) return;

    obs_data_t *data = obs_data_create();

    // 基本设置
    obs_data_set_bool(data, "enabled", enabled_);
    obs_data_set_int(data, "mode", static_cast<int>(mode_));
    obs_data_set_int(data, "cooldown", cooldown_seconds_);
    obs_data_set_string(data, "target_scene", target_scene_.c_str());

    // 保存场景循环列表
    obs_data_array_t *cycle_arr = obs_data_array_create();
    for (const auto &entry : cycle_list_) {
        obs_data_t *item = obs_data_create();
        obs_data_set_string(item, "scene", entry.scene_name.c_str());
        obs_data_set_int(item, "duration", entry.duration_seconds);
        obs_data_array_push_back(cycle_arr, item);
        obs_data_release(item);
    }
    obs_data_set_array(data, "cycle_list", cycle_arr);
    obs_data_array_release(cycle_arr);

    // 保存来源轮播列表
    obs_data_array_t *rotate_arr = obs_data_array_create();
    for (const auto &entry : rotate_list_) {
        obs_data_t *item = obs_data_create();
        obs_data_set_string(item, "source", entry.source_name.c_str());
        obs_data_set_int(item, "duration", entry.duration_seconds);
        obs_data_array_push_back(rotate_arr, item);
        obs_data_release(item);
    }
    obs_data_set_array(data, "rotate_list", rotate_arr);
    obs_data_array_release(rotate_arr);

    // 确保目录存在并保存
    std::string dir = path.substr(0, path.find_last_of("/\\"));
    os_mkdirs(dir.c_str());
    obs_data_save_json(data, path.c_str());
    obs_data_release(data);
}

void AutoSwitcher::Load()
{
    std::string path = get_plugin_config_path();
    if (path.empty()) return;

    obs_data_t *data = obs_data_create_from_json_file(path.c_str());
    if (!data) return;

    // 读取基本设置
    enabled_ = obs_data_get_bool(data, "enabled");
    mode_ = static_cast<SwitchMode>(obs_data_get_int(data, "mode"));
    cooldown_seconds_ = (int)obs_data_get_int(data, "cooldown");

    const char *ts = obs_data_get_string(data, "target_scene");
    target_scene_ = ts ? ts : "";

    // 读取场景循环列表
    cycle_list_.clear();
    obs_data_array_t *cycle_arr = obs_data_get_array(data, "cycle_list");
    if (cycle_arr) {
        size_t count = obs_data_array_count(cycle_arr);
        for (size_t i = 0; i < count; i++) {
            obs_data_t *item = obs_data_array_item(cycle_arr, i);
            SceneCycleEntry entry;
            const char *name = obs_data_get_string(item, "scene");
            entry.scene_name = name ? name : "";
            entry.duration_seconds = (int)obs_data_get_int(item, "duration");
            if (entry.duration_seconds <= 0)
                entry.duration_seconds = 10;
            cycle_list_.push_back(entry);
            obs_data_release(item);
        }
        obs_data_array_release(cycle_arr);
    }

    // 读取来源轮播列表
    rotate_list_.clear();
    obs_data_array_t *rotate_arr = obs_data_get_array(data, "rotate_list");
    if (rotate_arr) {
        size_t count = obs_data_array_count(rotate_arr);
        for (size_t i = 0; i < count; i++) {
            obs_data_t *item = obs_data_array_item(rotate_arr, i);
            SourceRotateEntry entry;
            const char *name = obs_data_get_string(item, "source");
            entry.source_name = name ? name : "";
            entry.duration_seconds = (int)obs_data_get_int(item, "duration");
            if (entry.duration_seconds <= 0)
                entry.duration_seconds = 10;
            rotate_list_.push_back(entry);
            obs_data_release(item);
        }
        obs_data_array_release(rotate_arr);
    }

    obs_data_release(data);

    // 重置运行时索引
    cycle_index_ = 0;
    rotate_index_ = 0;
    remaining_ms_ = 0;
    cooldown_remaining_ms_ = 0;
}
