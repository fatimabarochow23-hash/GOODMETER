# 功能房 01｜Audio Doctor 总览

## 当前定位

Audio Doctor 是 GOODMETER standalone 里的论文分析模块。它不是 DAW，也不是完整 Plugin Doctor 克隆；核心价值是：

```text
导入/生成素材 -> 加载插件 -> 设置参数 -> 离线渲染 -> 分析曲线 -> 导出论文图 -> 导出 CSV/manifest
```

它主要服务论文第二、三、四章：

- 第二章：频率感知、素材构成、掩蔽与融合的基础图例。
- 第三章：能量碰撞音效的分层构建依据。
- 第四章：效果链如何改变频谱、瞬态、空间和动态关系。

## 当前核心能力

- Dry A/B/C 与 Wet A/B/C 多素材/多渲染结果体系。
- Plugin A/B/C 加载、编辑、渲染。
- Display Slots 与 Render Routing。
- Generate 可指定输出到 Dry A/B/C；不能再只服务 Dry A。
- 插件槽位采用 insert-style UI：空槽点击加载，已加载槽点击打开插件 UI；render / output gain 属于插件 UI 底部工作流。
- `.clz` 工程保存当前 Audio Doctor 状态与当前仍使用的音频文件，目标是 Mac/Windows 同版本跨平台打开。
- 视图包括 Spectrum、Envelope、Group Delay、Spectrogram A/B/C、Reverb / Space、Dynamics Response、Spatial Image、Masking / Fusion 等。
- UI Export 和 JobRunner 都需要导出 PNG、CSV、JSON/manifest。

## 论文图例口径

不要把图写成“软件截图”。每张图都要回答论文问题：

- Spectrum：谐波、频谱重心、频谱占位。
- Envelope：瞬态边界、尾部长度、能量包络。
- Group Delay：all-pass / Disperser 类时间展开。
- Spectrogram A/B/C：层次、尾音、频段残留。
- Reverb / Space：EDC、RT20/RT30/RT60 est.、DRR、Early/Late、Stereo / M/S。
- Dynamics Response：RMS 动态响应或表观让位。
- Spatial Image：选定时间窗内 L-C-R 与频段空间印象。
- Masking / Fusion：遮蔽风险、融合趋势、mix delta。
- Critical Band Crystal / Dodecahedron Crystal：临界带关系、遮蔽风险、融合倾向、合成增益，适合论文展示“层间关系”。

## 2026-05-16 当前恢复点

- 当前真实工作区：`/Volumes/solari/Codex_Work/GOODMETER`。
- 当前 GitHub 论文分支：`codex/thesis-audio-doctor-20260505`。
- 已核验远端论文分支 HEAD：`3442e2d1aa526c7c916562a9a1ecbce33554a4c2`。
- 已核验远端 `main` 当时仍在：`d023690c393a17a9b4db56715fedd006afbdab25`。
- `main` 不含最新 Audio Doctor thesis workflow；主线大电脑或 Windows Codex 必须显式 fetch/merge/cherry-pick 论文分支。
- 本地可能有 `MAIN_COMPUTER_CODEX_AUDIO_DOCTOR_HANDOFF_2026-05-15.md` 删除状态，但远端分支已有该文档；恢复前先问用户。

## 风险边界

- Dynamics Response 不是插件内部 GR meter。
- Spatial Image 不是真实多声道定位测量。
- Masking / Fusion v1 不是严格心理声学 masked threshold。
- RT60 当前写 `RT60 est.` 更稳。
- `.clz` 跨平台是硬验收，不能只验证 Mac 自己能打开。
- JobRunner 覆盖新增图例时必须导出同等数据，不要只加 UI 画面。

状态：有效。

标签：#AudioDoctor #论文图例 #总览

## 2026-06-23 晚上｜Audio Doctor 提取式独立 App 首版

触发问题：

用户希望 Audio Doctor 不按“拆卸 GOODMETER”的方式处理，而是在 GitHub 未来可独立成夹子或库的前提下，先提取成一个有自己名字和海鸥图标的独立应用。

判断：

最稳路线是新增 `AudioDoctorApp/` 子工程，复用现有 Audio Doctor 核心源码，不移动 `Source/AudioDoctor*.h`，不改插件工程和 iOS 代码。这样后续可以整夹迁移到独立 GitHub repo，也能继续吃到 GOODMETER 主仓库里的 Audio Doctor 修复。

改动文件：

- `/Users/MediaStorm/Desktop/GOODMETER/AudioDoctorApp/AudioDoctor.jucer`
- `/Users/MediaStorm/Desktop/GOODMETER/AudioDoctorApp/Source/AudioDoctorStandaloneApp.cpp`
- `/Users/MediaStorm/Desktop/GOODMETER/AudioDoctorApp/README.md`
- `/Users/MediaStorm/Desktop/GOODMETER/build.sh`

验证：

- `xmllint --noout AudioDoctorApp/AudioDoctor.jucer` 通过。
- `bash -n build.sh` 通过。
- `./build.sh resave audio-doctor` 通过，生成 `AudioDoctorApp/Builds/MacOSX/Audio Doctor.xcodeproj`。
- `./build.sh audio-doctor` 通过，Xcode 26.4 构建成功，codesign verify 通过。
- 安装到 `/Applications/Audio Doctor.app` 后再次 codesign verify 通过，并能启动进程。
- `Info.plist` 确认 `CFBundleName=Audio Doctor`、`CFBundleIdentifier=com.solaris.AudioDoctor`，注册 `.clz` 和 `.goodmeterdoctor` 文档类型，包内有 `audio_doctor_project_pigeon.icns`。

输出产物：

- 构建产物：`/Users/MediaStorm/Library/Caches/GOODMETERBuild/GOODMETER-2499123989/audio-doctor/Products/Release/Audio Doctor.app`
- 安装产物：`/Applications/Audio Doctor.app`

论文影响：

不改变 Audio Doctor 图表、JobRunner、manifest 或论文证据链口径。本次只是为 Audio Doctor 增加独立应用入口。

风险与未完成：

- 目前只完成 macOS arm64 本机 GUI 启动和包信息验证；还未跑 `.clz` 跨平台打开、JobRunner 批处理导出、插件加载宿主扫描。
- 新 App 复用 `AudioDoctorContent`，所以核心行为仍由 GOODMETER 主仓库源码控制；未来拆到独立 repo 前要决定是复制共享源码，还是保留 submodule/subtree。
- 工作区仍有其他未提交改动和历史 untracked 文件，不能 `git add .`。

状态：首版可运行，待后续 GitHub 组织方式确认。

标签：#AudioDoctor #StandaloneApp #提取式独立应用

## 2026-06-23 晚上｜独立 Audio Doctor 隐藏原生应用标题栏

触发问题：

独立 `Audio Doctor.app` 首版使用 macOS 原生标题栏，截图中出现一条白色应用程序框，与 Audio Doctor 自己的深色 UI 不一致；用户指出之前内置 Audio Doctor 已经隐藏过一次。

判断：

独立 App 应沿用 GOODMETER 内置 Audio Doctor 窗口的无原生标题栏路线，而不是使用 macOS native title bar。外层窗口设置成透明/半透明，标题区高度为 0，让 Dark/Light 主题都只显示 Audio Doctor 自己的界面边界。

改动文件：

- `/Users/MediaStorm/Desktop/GOODMETER/AudioDoctorApp/Source/AudioDoctorStandaloneApp.cpp`

验证：

- `git diff --check -- AudioDoctorApp/Source/AudioDoctorStandaloneApp.cpp build.sh` 通过。
- `bash -n build.sh` 通过。
- `./build.sh audio-doctor` 通过，codesign verify 通过。
- 覆盖安装 `/Applications/Audio Doctor.app` 后再次 codesign verify 通过。
- 重新启动安装版后截图确认白色 macOS 原生标题栏已消失。

状态：已修复。

标签：#AudioDoctor #StandaloneApp #WindowChrome

## 2026-06-24 凌晨｜独立 App 工程保存改成 Pro Tools 式菜单流程

触发问题：

用户指出独立 `Audio Doctor.app` 的保存选择框/入口不够优雅，希望学习 Pro Tools：不切换 `.clz` 默认打开器，只在 macOS 左上角菜单提供 `Save Project` 和 `Open Recent`；首次保存工程时让用户过一眼工程中使用到的插件；打开缺少插件的工程时，提示缺少多少插件槽，并说明 DRY/WET 音频素材不受影响。

判断：

这次不是改工程文件格式，而是改独立 App 的工程工作流。`AudioDoctorContent` 仍负责实际 `.clz` 包读写；`AudioDoctorStandaloneApp.cpp` 只做 macOS 菜单、最近项目列表和保存/打开回调。GOODMETER 内嵌 Audio Doctor 继续保留 Export 菜单里的旧保存入口。

改动文件：

- `/Users/MediaStorm/Desktop/GOODMETER/Source/AudioDoctorComponent.h`
- `/Users/MediaStorm/Desktop/GOODMETER/AudioDoctorApp/Source/AudioDoctorStandaloneApp.cpp`

行为：

- 独立 App 菜单显示 `File > Save Project` 和 `File > Open Recent`。
- `Cmd+S` 仍触发保存工程。
- 第一次保存新工程且当前有插件槽时，弹出 `Project Plugins` 确认窗，列出 Plugin A/B/C 中使用的插件。
- 工程保存成功或打开成功后，会写入 `Open Recent` 列表。
- 打开工程时如果 manifest 中的插件路径在本机不存在，会先弹出 `Missing Project Plugins`，提示缺少的插件槽数量，并说明 DRY/WET audio files 已在工程包内，不会受影响；用户确认后才继续以更少插件槽打开。
- `.clz` 默认打开器没有切换：当前仍是 `/Applications/GOODMETER.app`；`.goodmeterdoctor` 仍是 `/Applications/Audio Doctor.app`。

验证：

- `git diff --check -- Source/AudioDoctorComponent.h AudioDoctorApp/Source/AudioDoctorStandaloneApp.cpp` 通过。
- `bash -n build.sh` 通过。
- `./build.sh audio-doctor` 通过，codesign verify 通过。
- 覆盖安装 `/Applications/Audio Doctor.app` 后再次 codesign verify 通过。
- 安装版二进制含 `Save Project`、`Open Recent`、`Project Plugins`、`Missing Project Plugins`、DRY/WET 提示文案。
- `open -na /Applications/Audio Doctor.app` 后进程可保持运行。
- `NSWorkspace` 验证 `.clz` 默认仍打开 `/Applications/GOODMETER.app`，`.goodmeterdoctor` 默认打开 `/Applications/Audio Doctor.app`。

状态：已实现并安装，待用户在实际工程里手动点击菜单确认弹窗手感。

标签：#AudioDoctor #StandaloneApp #MacMenu #ProjectSave #OpenRecent #CLZ

## 2026-06-23 晚上｜独立 App 将保存工程迁到 macOS File 菜单

触发问题：

用户希望 `Save Audio Doctor project...` 像 Pro Tools 一样放到 macOS 左上角系统菜单，而不是藏在 Audio Doctor 内部 `Export` 下拉里。

判断：

独立 `Audio Doctor.app` 应提供 `File > Save Audio Doctor Project...` 和 `Cmd+S`。但 `AudioDoctorContent` 同时被 GOODMETER 内置窗口复用，不能全局删除内部保存入口，否则 GOODMETER 内置 Audio Doctor 会失去保存按钮。因此使用顶层窗口属性 `audioDoctorUsesAppMenuSave` 做分流：独立 App 隐藏 Export 下拉里的 Save，GOODMETER 内置窗口保留原入口。

改动文件：

- `/Users/MediaStorm/Desktop/GOODMETER/Source/AudioDoctorComponent.h`
- `/Users/MediaStorm/Desktop/GOODMETER/AudioDoctorApp/Source/AudioDoctorStandaloneApp.cpp`

验证：

- `git diff --check -- Source/AudioDoctorComponent.h AudioDoctorApp/Source/AudioDoctorStandaloneApp.cpp` 通过。
- `bash -n build.sh` 通过。
- `./build.sh audio-doctor` 通过，codesign verify 通过。
- 覆盖安装 `/Applications/Audio Doctor.app` 后再次 codesign verify 通过并启动进程。

`.clz` 打开归属：

- 当前 `/Applications/Audio Doctor.app` 注册 `com.solaris.audiodoctor.project`，扩展 `.clz` 和 `.goodmeterdoctor`。
- 当前 `/Applications/GOODMETER.app` 注册 `com.solaris.goodmeter.audiodoctor-project`，扩展 `.clz`。
- 用 `/tmp/AudioDoctorDefaultProbe.clz` 询问 `NSWorkspace.shared.urlForApplication(toOpen:)`，当前系统默认返回 `/Applications/GOODMETER.app`。
- 用 `/tmp/AudioDoctorDefaultProbe.goodmeterdoctor` 询问同一接口，当前系统默认返回 `/Applications/Audio Doctor.app`。

状态：独立 App 菜单迁移完成；`.clz` 默认仍属 GOODMETER，`.goodmeterdoctor` 默认属 Audio Doctor。

标签：#AudioDoctor #StandaloneApp #MacMenu #CLZ

## 2026-06-23 晚上｜恢复独立 Audio Doctor 自绘拖拽栏和关闭按钮

触发问题：

上一轮把独立 `Audio Doctor.app` 的标题区高度设为 0，白色系统框消失了，但窗口也失去了可拖拽区域和右上角关闭按钮。

判断：

不要恢复 macOS 原生标题栏。应保留 `setUsingNativeTitleBar(false)`，但把 `setTitleBarHeight(30)` 作为 Audio Doctor 自绘窗口操作层，让 `GoodMeterLookAndFeel` 继续绘制与 Dark/Light 主题协调的标题栏和关闭按钮。

改动文件：

- `/Users/MediaStorm/Desktop/GOODMETER/AudioDoctorApp/Source/AudioDoctorStandaloneApp.cpp`

验证：

- `git diff --check -- AudioDoctorApp/Source/AudioDoctorStandaloneApp.cpp` 通过。
- `bash -n build.sh` 通过。
- `./build.sh audio-doctor` 通过，codesign verify 通过。
- 覆盖安装 `/Applications/Audio Doctor.app` 后再次 codesign verify 通过。

注意：

首次启动新版时 macOS 可能弹麦克风权限提示；不要用 AppleScript 强行代点权限。用户处理权限弹窗后再检查窗口拖拽和右上角自绘 X。

状态：已修复，待用户手动确认权限弹窗后的实际拖拽手感。

标签：#AudioDoctor #StandaloneApp #WindowChrome
