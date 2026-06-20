# OBS 自动场景切换插件 - 安装手册

> 适配 OBS Studio 29.0.2 / Windows 64位

---

## 一、编译环境检查清单

在编译前，请确认以下工具已安装：

| 序号 | 工具 | 版本 | 检查命令/方法 |
|------|------|------|--------------|
| 1 | **OBS Studio** | 29.0.2 | 运行 OBS，查看「帮助→关于」确认版本 |
| 2 | **Visual Studio 2022** | Build Tools 或 Community+ | 确认安装了「C++桌面开发」工作负载 |
| 3 | **CMake** | 3.16+ | VS Build Tools 自带，或独立安装 |
| 4 | **Qt 6.3.1** | MSVC 2019 64-bit | 通过 Qt 安装器或 `aqt install-qt` 安装（**必须与 OBS 运行时 Qt 版本一致**） |
| 5 | **OBS SDK 头文件** | 29.0.2 | 从 GitHub 源码获取 |

### 本环境实际路径

```
OBS 安装目录:     H:\obs29\obs-studio
OBS SDK 头文件:   H:\obs29\obs-studio\include\obs\
OBS 导入库:       H:\obs29\obs-studio\lib\obs.lib
                  H:\obs29\obs-studio\lib\obs-frontend-api.lib
Qt6 目录:         H:\Qt\6.3.1\msvc2019_64
VS Build Tools:   C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
CMake (内置):     VS Build Tools\Common7\IDE\...\CMake\bin\cmake.exe
```

---

## 二、首次环境搭建（仅需一次）

### 步骤 1：安装 OBS 开发者包头文件

OBS Studio 默认安装不含开发头文件，需从 GitHub 源码提取：

```powershell
# 下载 OBS 29.0.2 源码
Invoke-WebRequest -Uri "https://github.com/obsproject/obs-studio/archive/refs/tags/29.0.2.zip" -OutFile obs-29.0.2.zip

# 解压并复制头文件
Expand-Archive obs-29.0.2.zip -DestinationPath .\obs-sdk
Copy-Item .\obs-sdk\obs-studio-29.0.2\libobs\* "H:\obs29\obs-studio\include\obs\" -Recurse -Force
Copy-Item .\obs-sdk\obs-studio-29.0.2\UI\obs-frontend-api\obs-frontend-api.h "H:\obs29\obs-studio\include\" -Force
```

### 步骤 2：生成 OBS 导入库

OBS 安装目录不含 `.lib` 文件，需从运行时 DLL 生成：

```cmd
:: 设置 VC 环境
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

:: 导出 DLL 符号表
dumpbin /exports "H:\obs29\obs-studio\bin\64bit\obs.dll" > obs_exports.txt
dumpbin /exports "H:\obs29\obs-studio\bin\64bit\obs-frontend-api.dll" > frontend_exports.txt

:: 创建 .def 文件并生成 .lib（具体脚本见 gen_libs2.ps1）
lib /def:obs.def /out:"H:\obs29\obs-studio\lib\obs.lib" /machine:x64
lib /def:obs-frontend-api.def /out:"H:\obs29\obs-studio\lib\obs-frontend-api.lib" /machine:x64
```

> **提示**：项目中已包含自动化脚本 `gen_libs2.ps1`，可直接运行。

### 步骤 3：安装 Qt6

推荐使用 `aqtinstall`（Python 命令行 Qt 安装器）：

```cmd
pip install aqtinstall
aqt install-qt windows desktop 6.3.1 win64_msvc2019_64 --outputdir H:\Qt
```

或使用 Qt 官方在线安装器，选择 Qt 6.3.1 → MSVC 2019 64-bit。

> **重要**：OBS 29.0.2 运行时使用 **Qt 6.3.1**，插件必须使用相同版本编译，否则会出现 "Module not loaded" 错误。

---

## 三、编译步骤

### 方式一：一键编译（推荐）

运行 `do_build.bat`（已自动检测本机环境路径）：

```cmd
do_build.bat
```

该脚本自动执行：
1. 初始化 VC 编译环境（vcvarsall.bat x64）
2. CMake 配置（NMake Makefiles 生成器）
3. Release 编译
4. 自动部署 DLL 到 OBS 插件目录

### 方式二：增量编译

运行 `quick_build.bat`，不删除构建目录，仅重新编译变更的文件。

### 方式三：手动编译

```cmd
:: 1. 初始化环境
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

:: 2. CMake 配置
cd /d H:\ceshiTRAE\obs-auto-switcher
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="H:\Qt\6.3.1\msvc2019_64" ^
    -DOBS_INSTALL_DIR="H:\obs29\obs-studio" ^
    -DOBS_INCLUDE_DIR="H:\obs29\obs-studio\include\obs" ^
    -DOBS_LIB_DIR="H:\obs29\obs-studio\lib" ^
    -DOBS_BIN_DIR="H:\obs29\obs-studio\bin\64bit" .

:: 3. 编译
cmake --build build --config Release
```

### 编译输出

```
build\obs-auto-switcher.dll   (约 133 KB)
```

---

## 四、插件安装

### 自动安装

`do_build.bat` 编译成功后会自动将 DLL 复制到 OBS 插件目录。

### 手动安装

将编译输出的 DLL 复制到 OBS 插件目录：

```
源文件:  build\obs-auto-switcher.dll
目标:    H:\obs29\obs-studio\obs-plugins\64bit\obs-auto-switcher.dll
```

> **注意**：DLL 必须放在 OBS 根目录下的 `obs-plugins\64bit\` 中，**不是** `bin\64bit\obs-plugins\64bit\`。
> **部署前提**：必须先关闭 OBS，否则 DLL 会被锁定无法覆盖。

### 目录结构验证

```
H:\obs29\obs-studio\
├── bin\
│   └── 64bit\
│       ├── obs64.exe
│       ├── obs.dll
│       ├── obs-frontend-api.dll
│       ├── Qt6Core.dll
│       └── Qt6Widgets.dll
├── obs-plugins\
│   └── 64bit\
│       ├── frontend-tools.dll          ← OBS自带插件
│       └── obs-auto-switcher.dll       ← 我们的插件在这里
```

---

## 五、使用指南

### 5.1 打开插件面板

1. 启动 OBS Studio
2. 菜单栏 → **工具** → **自动场景切换面板**
3. 面板将以 Dock 窗口形式嵌入 OBS 界面
4. 关闭面板后，随时可通过工具菜单重新打开

### 5.2 模式 A：场景循环

1. 选择 **场景循环** 单选框
2. 点击 **添加**，从下拉框选择要循环的场景
3. 设置每个场景的停留秒数
4. 使用 **上移/下移** 调整切换顺序
5. 勾选 **启用自动轮换** 开始循环

### 5.3 模式 B：来源轮播

1. 选择 **来源轮播** 单选框
2. 在 **目标场景** 下拉框中选择要操作的场景
3. 点击 **添加**，从下拉框选择参与轮播的来源
4. 设置每个来源的停留秒数
5. 勾选 **启用自动轮换** 开始轮播

> **轮播逻辑**：所有来源初始全部显示（用户预先叠好图层）→ 按列表顺序逐层隐藏 → 全部隐藏后重置显示全部 → 循环。
> 只有添加到轮播列表的来源参与切换，未添加的来源完全不受影响。

### 5.4 热键配置

1. OBS 菜单 → **文件** → **设置** → **热键**
2. 找到并绑定快捷键：
   - **自动切换 - 暂停/恢复轮换**
   - **自动切换 - 重置轮换序列**
3. 热键配置自动保存，重启 OBS 不丢失

### 5.5 冷却时间

- 在面板中设置「切换冷却」秒数
- 每次切换后等待冷却结束才开始下一轮倒计时
- 防止频繁切换造成画面闪烁

---

## 六、故障排查

### 插件未加载

| 现象 | 原因 | 解决 |
|------|------|------|
| 工具菜单无面板选项 | DLL 未放在正确目录 | 确认 DLL 在 `obs-plugins\64bit\` 下（非 `bin\64bit\`） |
| OBS 启动后插件未加载 | Qt 版本不匹配 | OBS 29.0.2 使用 Qt **6.3.1**，必须用相同版本编译 |
| 提示缺少 DLL | 编译配置不正确 | 确认使用 Release x64 编译 |

### 编译错误

| 错误 | 原因 | 解决 |
|------|------|------|
| `Cannot open include file: obs.h` | SDK 头文件缺失 | 按步骤 1 安装头文件 |
| `Could not find package Qt6` | Qt6 路径错误 | 设置 `CMAKE_PREFIX_PATH` 指向 Qt6 |
| `LNK1107: 文件无效或损坏` | obs.lib 生成失败 | 重新运行 `gen_libs2.ps1` |
| 路径含中文导致 LNK1201 | 编译器不支持中文路径 | 将项目复制到纯英文路径编译 |

### 运行时问题

| 现象 | 解决 |
|------|------|
| 倒计时不更新 | 确认主开关已勾选且有有效场景/来源 |
| 来源轮播不工作 | 确认目标场景存在且包含所添加的来源 |
| 来源下拉框为空 | 点击「刷新场景/来源列表」按钮，查看 OBS 日志搜索 `[自动切换]` |
| 日志报「场景不存在」 | 场景被删除或重命名，插件自动跳过，可在面板刷新列表 |
| 关闭面板后无法打开 | 通过工具菜单「自动场景切换面板」重新打开 |

### 查看日志

OBS 菜单 → **帮助** → **日志文件** → **查看当前日志**，搜索 `[自动切换]` 关键字。

---

## 七、文件清单

```
obs自动切换插件/
├── CMakeLists.txt           # CMake 构建配置
├── do_build.bat             # 一键编译脚本（完整编译）
├── quick_build.bat          # 增量编译脚本（快速）
├── gen_libs2.ps1            # OBS 导入库(.lib)生成脚本
├── INSTALL.md               # 本安装手册
├── README.md                # 功能介绍与使用说明
└── src/
    ├── auto-switcher.h      # 核心切换器头文件
    ├── auto-switcher.cpp    # 核心切换逻辑
    ├── ui-panel.h           # UI 面板头文件
    ├── ui-panel.cpp         # UI 面板实现
    └── plugin-main.cpp      # 插件入口与热键注册
```
