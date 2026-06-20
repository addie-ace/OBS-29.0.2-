/*
 * OBS 自动切换插件 - 模块入口
 * 注册插件元数据、前端事件回调、全局热键与保存回调
 * 适配 OBS 29.0.2 API
 */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <util/platform.h>
#include <util/bmem.h>

#include <QString>
#include <QWidget>
#include <QAction>
#include <QDir>
#include <QStandardPaths>

#include "auto-switcher.h"
#include "ui-panel.h"

/* ============ 全局变量 ============ */

static AutoSwitchPanel *g_panel = nullptr;
static obs_hotkey_id g_hotkey_pause = (obs_hotkey_id)-1;
static obs_hotkey_id g_hotkey_reset = (obs_hotkey_id)-1;

// 插件配置文件路径
static std::string g_config_path;

static std::string get_config_path()
{
    if (!g_config_path.empty())
        return g_config_path;

    // 使用 OBS 配置目录
    char *config_dir = obs_module_config_path(nullptr);
    if (config_dir) {
        g_config_path = config_dir;
        g_config_path += "/settings.json";
        bfree(config_dir);
    }
    return g_config_path;
}

/* ============ 模块元数据 ============ */

OBS_DECLARE_MODULE()

OBS_MODULE_USE_DEFAULT_LOCALE("auto_scene_switcher", "zh-CN")

MODULE_EXPORT const char *obs_module_name(void)
{
    return u8"自动场景切换插件";
}

MODULE_EXPORT const char *obs_module_description(void)
{
    return u8"双模式自动轮换：场景循环 / 单场景来源轮播";
}

/* ============ 热键回调（OBS 29 要求返回 void）============ */

static void hotkey_pause_cb(void *data, obs_hotkey_id id,
                            obs_hotkey_t *hotkey, bool pressed)
{
    (void)data; (void)id; (void)hotkey;
    if (!pressed) return;

    blog(LOG_INFO, "[自动切换] 热键触发：暂停/恢复");
    AutoSwitcher::Instance().GetNotifier()->pause_toggled();
}

static void hotkey_reset_cb(void *data, obs_hotkey_id id,
                            obs_hotkey_t *hotkey, bool pressed)
{
    (void)data; (void)id; (void)hotkey;
    if (!pressed) return;

    blog(LOG_INFO, "[自动切换] 热键触发：重置序列");
    AutoSwitcher::Instance().GetNotifier()->reset_triggered();
}

/* ============ 前端事件回调 ============ */

static void frontend_event_cb(enum obs_frontend_event event, void *data)
{
    (void)data;

    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
    {
        blog(LOG_INFO, "[自动切换] OBS前端加载完成，注册面板");

        QWidget *main_window = static_cast<QWidget *>(
            obs_frontend_get_main_window());
        if (!main_window) {
            blog(LOG_ERROR, "[自动切换] 错误：无法获取OBS主窗口");
            return;
        }

        // 必须先加载配置，再创建面板（面板构造函数会读取配置）
        AutoSwitcher::Instance().Load();

        g_panel = new AutoSwitchPanel(main_window);
        obs_frontend_add_dock(g_panel);
        g_panel->show();

        // 添加工具菜单项，并直接用返回的QAction指针连接信号
        // obs_frontend_add_tools_menu_qaction 返回 void* (实际是QAction*)
        void *action_ptr = obs_frontend_add_tools_menu_qaction(
            u8"自动场景切换面板");
        if (action_ptr) {
            QAction *menu_action = static_cast<QAction *>(action_ptr);
            QObject::connect(menu_action, &QAction::triggered, []() {
                if (g_panel) {
                    // QDockWidget关闭后需要强制显示并提升
                    g_panel->setVisible(true);
                    if (g_panel->isFloating()) {
                        g_panel->showNormal();
                    }
                    g_panel->raise();
                    g_panel->activateWindow();
                    blog(LOG_INFO, "[自动切换] 工具菜单点击，显示面板");
                }
            });
            blog(LOG_INFO, "[自动切换] 工具菜单项已连接");
        } else {
            blog(LOG_WARNING, "[自动切换] 警告：无法获取菜单QAction指针");
        }

        g_panel->LoadFromSwitcher();

        blog(LOG_INFO, "[自动切换] 面板注册成功");
        break;
    }

    case OBS_FRONTEND_EVENT_EXIT:
    {
        blog(LOG_INFO, "[自动切换] OBS即将退出，保存配置");
        AutoSwitcher::Instance().SetEnabled(false);
        AutoSwitcher::Instance().Save();
        break;
    }

    case OBS_FRONTEND_EVENT_SCENE_CHANGED:
    {
        if (g_panel) {
            g_panel->UpdateStatusDisplay();
        }
        break;
    }

    default:
        break;
    }
}

/* ============ 保存回调 ============ */

static void save_hotkeys_to_config()
{
    std::string path = get_config_path();
    if (path.empty()) return;

    obs_data_t *config = obs_data_create();

    // 保存热键绑定（obs_hotkey_save 返回 obs_data_array_t*）
    if (g_hotkey_pause != (obs_hotkey_id)-1) {
        obs_data_array_t *arr = obs_hotkey_save(g_hotkey_pause);
        if (arr) {
            obs_data_set_array(config, "hotkey_pause", arr);
            obs_data_array_release(arr);
        }
    }

    if (g_hotkey_reset != (obs_hotkey_id)-1) {
        obs_data_array_t *arr = obs_hotkey_save(g_hotkey_reset);
        if (arr) {
            obs_data_set_array(config, "hotkey_reset", arr);
            obs_data_array_release(arr);
        }
    }

    // 确保配置目录存在
    std::string dir = path.substr(0, path.find_last_of("/\\"));
    os_mkdirs(dir.c_str());

    // 写入文件
    obs_data_save_json(config, path.c_str());
    obs_data_release(config);
}

static void load_hotkeys_from_config()
{
    std::string path = get_config_path();
    if (path.empty()) return;

    obs_data_t *config = obs_data_create_from_json_file(path.c_str());
    if (!config) return;

    // 加载热键绑定（obs_hotkey_load 接受 obs_data_array_t*）
    if (g_hotkey_pause != (obs_hotkey_id)-1) {
        obs_data_array_t *arr = obs_data_get_array(config, "hotkey_pause");
        if (arr) {
            obs_hotkey_load(g_hotkey_pause, arr);
            obs_data_array_release(arr);
        }
    }

    if (g_hotkey_reset != (obs_hotkey_id)-1) {
        obs_data_array_t *arr = obs_data_get_array(config, "hotkey_reset");
        if (arr) {
            obs_hotkey_load(g_hotkey_reset, arr);
            obs_data_array_release(arr);
        }
    }

    obs_data_release(config);
}

static void save_callback(obs_data_t *save_data, bool saving, void *data)
{
    (void)save_data; (void)data;

    if (saving) {
        AutoSwitcher::Instance().Save();
        save_hotkeys_to_config();
        blog(LOG_INFO, "[自动切换] 配置已保存");
    }
}

/* ============ 注册热键 ============ */

static void register_hotkeys()
{
    g_hotkey_pause = obs_hotkey_register_frontend(
        "auto_switch_pause",
        u8"自动切换 - 暂停/恢复轮换",
        hotkey_pause_cb,
        nullptr);

    g_hotkey_reset = obs_hotkey_register_frontend(
        "auto_switch_reset",
        u8"自动切换 - 重置轮换序列",
        hotkey_reset_cb,
        nullptr);

    load_hotkeys_from_config();

    blog(LOG_INFO, "[自动切换] 热键注册完成");
}

/* ============ 模块加载/卸载 ============ */

bool obs_module_load(void)
{
    blog(LOG_INFO, "[自动切换] 插件加载中...");

    obs_frontend_add_event_callback(frontend_event_cb, nullptr);
    obs_frontend_add_save_callback(save_callback, nullptr);
    register_hotkeys();

    blog(LOG_INFO, "[自动切换] 插件加载成功");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[自动切换] 插件卸载中...");

    AutoSwitcher::Instance().SetEnabled(false);

    if (g_hotkey_pause != (obs_hotkey_id)-1) {
        obs_hotkey_unregister(g_hotkey_pause);
        g_hotkey_pause = (obs_hotkey_id)-1;
    }
    if (g_hotkey_reset != (obs_hotkey_id)-1) {
        obs_hotkey_unregister(g_hotkey_reset);
        g_hotkey_reset = (obs_hotkey_id)-1;
    }

    g_panel = nullptr;

    blog(LOG_INFO, "[自动切换] 插件已卸载");
}
