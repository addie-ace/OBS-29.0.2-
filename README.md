# OBS 自动场景切换插件

> 适配 OBS Studio 29.0.2 / Windows 64位

---

## 一、功能介绍

本插件为 OBS Studio 提供**双核心自动轮换**功能，两种模式独立配置、互不干扰，可通过可视化面板一键切换。

### 模式A：全局场景循环

- 多场景依次定时切换，每个场景可自定义停留秒数
- 循环末尾自动回到首个场景，无限轮播
- 支持增删、上下移动排序

### 模式B：单场景来源轮播（核心功能）

- 先选择一个目标场景
- 手动添加「参与轮播的来源」到列表，可自由增删、排序
- **图层逐层隐藏逻辑**：所有来源初始全部可见（用户预先叠好图层），按列表顺序从上往下依次隐藏一个来源，全部隐藏后重置显示全部
- **未加入列表的来源保持原有状态完全不受影响**
- 举例：场景有 A（顶层）、A1、B、C 四个来源，仅 A 和 A1 加入轮播列表，则循环为：全部显示 → 隐藏A → 隐藏A1 → 全部显示（重置）
- 支持群组（Group）内来源递归查找
- 每个来源可单独设置停留秒数

### 通用功能

| 功能 | 说明 |
|------|------|
| 运行总开关 | 一键启用/停用自动轮换 |
| 全局冷却时间 | 每次切换后的防抖等待秒数 |
| 实时状态显示 | 当前倒计时、当前激活场景/来源 |
| 中文热键 | 暂停/恢复 + 重置序列，配置永久保存 |
| 容错机制 | 场景/来源被删时自动跳过，中文日志警告 |
| 低CPU占用 | 停用时零额外消耗 |
| 工具菜单 | 点击菜单项可随时重新打开已关闭的面板 |

---

## 二、编译环境搭建

### 前置条件

| 工具 | 版本要求 | 说明 |
|------|---------|------|
| **OBS Studio** | 29.0.2 | [下载](https://obsproject.com/download) |
| **OBS SDK 头文件+导入库** | 29.0.2 | 从 GitHub 源码提取头文件，从 DLL 生成导入库 |
| **Visual Studio** | 2022 Build Tools | 安装「C++桌面开发」工作负载 |
| **CMake** | 3.16+ | VS Build Tools 自带 |
| **Qt6** | 6.3.1 (MSVC 2019 64-bit) | **必须与 OBS 运行时 Qt 版本一致** |

> **重要**：OBS 29.0.2 运行时使用 **Qt 6.3.1**，插件必须用相同版本编译，否则会出现 "Module not loaded" 错误。

### 安装 Qt6

推荐使用 `aqtinstall`（Python 命令行 Qt 安装器）：

```cmd
pip install aqtinstall
aqt install-qt windows desktop 6.3.1 win64_msvc2019_64 --outputdir H:\Qt
```

或使用 Qt 官方在线安装器，选择 Qt 6.3.1 → MSVC 2019 64-bit。

---

## 三、编译步骤

### 方式一：一键编译（推荐）

运行 `do_build.bat`（已配置好本机环境路径）：

```cmd
do_build.bat
```

该脚本自动执行：
1. 初始化 VC 编译环境（vcvarsall.bat x64）
2. CMake 配置（NMake Makefiles 生成器）
3. Release 编译
4. 自动部署 DLL 到 OBS 插件目录

### 方式二：增量编译

运行 `quick_build.bat`，不删除构建目录，仅重新编译变更的文件，速度更快。

### 方式三：手动编译

```cmd
:: 1. 初始化环境
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

:: 2. CMake 配置
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="H:\Qt\6.3.1\msvc2019_64" ^
    -DOBS_INSTALL_DIR="H:\obs29\obs-studio" ^
    -DOBS_INCLUDE_DIR="H:\obs29\obs-studio\include\obs" ^
    -DOBS_LIB_DIR="H:\obs29\obs-studio\lib" ^
    -DOBS_BIN_DIR="H:\obs29\obs-studio\bin\64bit" .

:: 3. 编译
cmake --build build --config Release
```

---

## 四、插件安装

### 部署路径

```
源文件:  build\obs-auto-switcher.dll
目标:    H:\obs29\obs-studio\obs-plugins\64bit\obs-auto-switcher.dll
```

> **注意**：OBS 29+ 插件加载路径为 OBS 根目录下的 `obs-plugins\64bit\`，**不是** `bin\64bit\obs-plugins\64bit\`。

### 目录结构验证

```
H:\obs29\obs-studio\
├── bin\64bit\
│   ├── obs64.exe
│   ├── obs.dll
│   └── obs-frontend-api.dll
├── obs-plugins\
│   └── 64bit\
│       ├── frontend-tools.dll          ← OBS自带插件
│       └── obs-auto-switcher.dll       ← 我们的插件
```

> **部署前提**：必须先关闭 OBS，否则 DLL 会被锁定无法覆盖。

---

## 五、使用教程

### 基本使用流程

1. **启动 OBS** → 菜单栏「工具」→ 点击「自动场景切换面板」
2. 面板打开后，勾选「启用自动轮换」总开关
3. 关闭面板后，随时可通过工具菜单重新打开

### 模式A：场景循环

1. 选择「场景循环」单选框
2. 点击「添加」按钮，从下拉框选择要循环的场景
3. 设置每个场景的停留秒数
4. 可上下移动调整切换顺序
5. 勾选主开关即开始循环

### 模式B：来源轮播

1. 选择「来源轮播」单选框
2. 在「目标场景」下拉框中选择要操作的场景
3. 点击「添加」，从下拉框选择要参与轮播的来源
4. 设置每个来源的停留秒数
5. 勾选主开关开始轮播
6. **轮播逻辑**：所有来源先全部显示 → 按顺序逐层隐藏 → 全部隐藏后重置显示全部

### 热键使用

1. OBS 菜单「文件」→「设置」→「热键」
2. 找到以下热键并绑定快捷键：
   - **自动切换 - 暂停/恢复轮换**：临时暂停自动切换
   - **自动切换 - 重置轮换序列**：立刻跳回第一条重新倒计时
3. 热键配置自动保存，重启 OBS 不丢失

### 冷却时间

- 在面板中设置「切换冷却」秒数
- 每次自动切换后，等待冷却时间结束才开始下一轮倒计时
- 防止频繁切换造成画面闪烁

---

## 六、故障排查

### 插件未加载

| 问题 | 解决方案 |
|------|---------|
| OBS 工具菜单中无面板选项 | 确认 DLL 放在 `obs-plugins\64bit\` 目录下（非 `bin\64bit\` 下） |
| 加载后提示 Module not loaded | Qt 版本不匹配，OBS 29.0.2 使用 **Qt 6.3.1**，必须用相同版本编译 |
| 提示缺少 DLL | 确认编译配置为 Release x64 |

### 编译错误

| 错误信息 | 解决方案 |
|---------|---------|
| `Could not find package Qt6` | 设置 `CMAKE_PREFIX_PATH` 指向 Qt6 安装路径 |
| `Cannot open include file: obs.h` | 安装 OBS SDK 头文件，或检查 `OBS_INCLUDE_DIR` |
| `unresolved external symbol` | 确认链接了 `obs.lib` 和 `obs-frontend-api.lib`（可用 `gen_libs2.ps1` 生成） |
| 路径含中文导致 LNK1201 | 将项目复制到纯英文路径编译 |

### 运行时问题

| 问题 | 解决方案 |
|------|---------|
| 倒计时不更新 | 确认主开关已勾选，且有有效的场景/来源 |
| 来源轮播不工作 | 确认目标场景存在且包含所添加的来源 |
| 来源下拉框为空 | 点击「刷新场景/来源列表」按钮，或检查 OBS 日志搜索 `[自动切换]` |
| 切换后其他来源也被隐藏 | 检查来源是否正确添加到轮播列表（仅列表内来源受影响） |
| 日志报「场景不存在」 | 场景被删除或重命名，插件自动跳过，可在面板中刷新列表 |
| 热键不生效 | 在 OBS 热键设置中确认已绑定按键 |
| 关闭面板后无法重新打开 | 通过工具菜单「自动场景切换面板」重新打开 |

### 查看日志

OBS 菜单「帮助」→「日志文件」→「查看当前日志」，搜索 `[自动切换]` 关键字查看插件运行日志。

---

## 七、项目文件结构

```
obs自动切换插件/
├── CMakeLists.txt          # CMake 构建配置
├── do_build.bat            # 一键编译脚本（完整编译）
├── quick_build.bat         # 增量编译脚本（快速）
├── gen_libs2.ps1           # OBS 导入库(.lib)生成脚本
├── README.md               # 本文档
├── INSTALL.md              # 详细安装手册
└── src/
    ├── auto-switcher.h     # 核心切换器头文件
    ├── auto-switcher.cpp   # 核心切换逻辑实现
    ├── ui-panel.h          # UI 面板头文件
    ├── ui-panel.cpp        # UI 面板实现
    └── plugin-main.cpp     # 插件入口与热键注册
```
