# 主线大电脑 Codex 交接说明：合入 Audio Doctor 最新论文工具链

日期：2026-05-15  
交接对象：主线大电脑上的 Codex  
目标：把论文分支里的 Audio Doctor 最新内容准确迭代进主线，不遗漏 `.clz` 工程、Job Runner、Layer Fit / Fusion、空间图、Windows 兼容要求。

## 1. 当前 GitHub 状态

请先注意：最新 Audio Doctor 内容不在 `main` 上，而在论文分支。

- 论文分支：`codex/thesis-audio-doctor-20260505`
- 论文分支最新提交：`1ea4c7e Add Audio Doctor thesis workflow and Windows handoff`
- GitHub 远端已确认存在该提交：`1ea4c7e7e1e42c7b027757bf9003af0a25fedc6a`
- 当前 `main` 最新提交：`d023690c393a17a9b4db56715fedd006afbdab25`

结论：如果你只 `git pull origin main`，拿不到这轮 Audio Doctor 的新增内容。必须显式 fetch / merge / cherry-pick 论文分支。

## 2. 推荐合入方式

如果主线大电脑有自己的主线迭代，请不要直接在 `main` 上莽合。建议先开集成分支：

```bash
git fetch origin
git switch main
git pull --ff-only origin main
git switch -c codex/integrate-audio-doctor-20260515
git merge --no-ff origin/codex/thesis-audio-doctor-20260505
```

如果主线大电脑上已经有未提交工作，先保存或提交，不要 reset 掉用户改动。

## 3. 本轮论文分支包含什么

这轮不是单个小 UI 修复，而是一整套 Audio Doctor 论文工具链：

- Audio Doctor 主界面和窗口风格。
- Dry A/B/C、Wet A/B/C、Plugin A/B/C 插槽。
- Generate 可输出到 Dry A/B/C。
- 插件插入槽 UI，加载/显示/渲染链路。
- `.clz` Audio Doctor 工程保存与打开。
- 工程图标和 Audio Doctor 海鸥资源。
- Export 图、CSV、manifest。
- Job Runner 批量分析接口。
- Spectrum harmonic peak overlay。
- Spectrogram A/B/C 与 Audio Lab 对齐后的绘制逻辑。
- Reverb / Space：EDC、RT20/RT30/RT60 est.、DRR、Early/Late、Tail Spectrogram。
- Dynamics Response。
- Spatial Image：L-C-R / 频率 / 能量的空间印象图。
- 2.5D Time-Frequency Terrain。
- Layer Fit / Fusion。
- Critical Band Terrain。
- Critical Band Crystal。
- Dodecahedron Crystal。
- BEAM 等 GUI 插件离线渲染卡死修复思路。
- memory_palace 记忆宫殿文档。
- Windows Codex 交接文档：`WINDOWS_CODEX_HANDOFF_AUDIO_DOCTOR_2026-05-13.md`。

## 4. 合入后必须检查的关键文件

请重点看这些文件是否完整合入：

```text
Source/AudioDoctorAnalysis.h
Source/AudioDoctorComponent.h
Source/AudioDoctorFigureRenderer.h
Source/AudioDoctorJobRunner.h
Source/AudioDoctorPluginHost.h
Source/GoodMeterLookAndFeel.h
Source/StandaloneApp.cpp
Source/StandaloneNonoEditor.h
GOODMETER.jucer
build.sh
Assets/audio_doctor_plugin_confirm_bg.jpg
Assets/audio_doctor_project.icns
Assets/audio_doctor_project_pigeon.icns
Assets/audio_doctor_project_source_trimmed.png
WINDOWS_CODEX_HANDOFF_AUDIO_DOCTOR_2026-05-13.md
memory_palace/
```

如果这些核心文件缺了，说明你没有合到正确分支。

## 5. 合入后构建验证

Mac 主线大电脑至少先跑：

```bash
./build.sh standalone
```

期望结果：

- `** BUILD SUCCEEDED **`
- 产出 `GOODMETER.app`
- app code signing 验证通过
- Audio Doctor 能正常打开

如果你还要继续主线迭代 iOS 或插件版本，再分别跑对应 target，但不要因为 iOS / plugin 失败就先否定 standalone。三者问题可能不同。

## 6. Audio Doctor GUI 验收清单

合入后请打开 app，至少手测：

1. Audio Doctor 打开后不清空上次状态，除非退出 app 或主动 Reset。
2. Load Dry 可以加载 Dry A/B/C。
3. Generate 可以选择输出到 Dry A/B/C，多次生成不会把其他 Dry 槽清掉。
4. Plugin A/B/C 插槽：
   - 空槽点击加载插件。
   - 已加载槽点击显示插件 UI。
   - 插件 UI 内 render 可生成 Wet。
   - output gain 不是摆设，应影响渲染输出电平。
5. Export 可输出 PNG + CSV + JSON manifest。
6. `.clz` 工程：
   - Save 后能重新打开。
   - 关闭 Audio Doctor 再打开，状态合理保留。
   - 工程内音频应来自工程 audio files，而不是依赖原始绝对路径。
7. 3D / 2.5D 图：
   - 鼠标拖拽能改变视角。
   - 视角下拉框能复位到 Front High / Front Low / Diagonal / Side Low / Side High。
   - 滚轮缩放不应破坏图形。
8. Layer Fit / Fusion：
   - Critical Band Terrain。
   - Time-Frequency Terrain。
   - Spatial Image。
   - Critical Band Crystal。
   - Dodecahedron Crystal。
   - Time slider / 正放 / 倒放 / 空格暂停继续。
   - strongest band 不应永远是 B0。
   - 分频高亮时，非焦点区域标签应隐藏。

## 7. `.clz` 跨平台要求

这是给 Windows 版和主线后续迭代最重要的要求。

`.clz` 必须像 `.ptx` / `.rpp` 一样尽量跨平台：

- Mac 保存的 `.clz`，Windows 同版本 GOODMETER 应能打开。
- Windows 保存的 `.clz`，Mac 同版本 GOODMETER 应能打开。
- 工程内应保存当前槽位真正使用的音频文件：
  - Dry A/B/C
  - Wet A/B/C
  - generated source
  - rendered output
- 原始绝对路径只能作为 provenance 记录，不应成为打开工程的必需条件。
- 插件缺失时，工程必须打开，不得崩溃，不得清空素材。
- 插件路径不同平台不一致，Windows 主要走 VST3，不要假设 AU 可用。
- 中文路径、空格路径、外置盘路径都要测。

如果 `.clz` 还没有完全做到，请把缺口写清楚，不要假装跨平台已经完成。

## 8. Job Runner 必须跟上 GUI

主线迭代时，不要只修 GUI 忘了 Job Runner。论文组会用 Job Runner 批量跑图和数据。

至少验证这些能力：

- import audio
- generate signal
- load plugin
- set plugin params
- render A/B/C
- export PNG
- export CSV
- export manifest / response
- 路径带空格不失败
- 输出目录带中文不失败

至少验证这些视图或等价 token：

- spectrum
- envelope
- group delay
- spectrogram A/B/C
- reverb space
- spatial image
- layer fit / fusion
- critical band terrain
- time-frequency terrain
- critical band crystal
- dodecahedron crystal

如果 token 名称和这里不同，以代码实际枚举为准，但必须把最终可用清单写回交接说明。

## 9. 不要提交的东西

论文分支本地还会出现一些未跟踪文件，这些不要进主线：

```text
Signing/
dist/
tmp/
releases/*.dmg
*.p8
._*
~$*.docx
GOODMETER_Windows_ThesisPreview_20260513.zip
```

这些是本机产物、签名材料、缓存、AppleDouble 垃圾或临时包，不是源码。

## 10. 如果合并冲突

优先保留主线大电脑最近的业务改动，但 Audio Doctor 相关文件不要随意丢：

- `Source/AudioDoctor*.h` 是核心，不要整文件覆盖成旧版。
- `GOODMETER.jucer` 里新增资源、文件、bundle/document type 配置要保留。
- `build.sh` 的便携构建逻辑要保留，尤其是不要写回某台电脑的绝对路径。
- 图标资源和 `.clz` document type 配置要一起保留，否则工程文件图标/打开方式会退回旧状态。

冲突解决后务必重新跑 `./build.sh standalone`。

## 11. 交回时请说明

主线大电脑 Codex 迭代完请回报：

- 最终分支名。
- 最终 commit hash。
- 是否已经合入 `1ea4c7e`。
- 是否能构建 standalone。
- `.clz` 跨平台测试是否通过。
- Job Runner 跑过哪些视图。
- 有哪些功能仍然只在 GUI 可用，Job Runner 还没覆盖。
- Windows Codex 是否还需要额外说明。

一句话总结：请把 `codex/thesis-audio-doctor-20260505` 的 Audio Doctor 论文工具链完整合入主线，重点保住 `.clz` 跨平台、Job Runner 批处理、Layer Fit / Fusion 和空间图，不要只合 UI 表面。
