# Audio Doctor - BEAM Render / Preset 兼容性修复单

日期：2026-05-07  
提出方：论文组  
目标：让 Audio Doctor 可以稳定使用 Lunacy Audio BEAM 及其单模块插件，并能把真实预设/参数状态用于论文图例导出。

## 背景

用户实测：

- REAPER 中 BEAM 与 BEAM 单模块插件可以正常使用。
- Pro Tools AudioSuite 中 BEAM 可以打开并 render 到 clip。
- GOODMETER Audio Doctor UI 中加载 BEAM 后，点击 Render 会卡死或崩溃。
- Audio Doctor Job Runner 可以渲染 BEAM/BEAM 单模块的默认状态，但目前 `.preset` / `.nodepreset` 只是被记录为 stateLoaded，实际声音和数据没有随预设改变。

因此这个问题不应简单判断为 “BEAM 不支持离线渲染”。更像是 Audio Doctor 宿主路径对复杂插件的 UI 生命周期、状态捕获、离线实例创建、预设格式导入处理不够稳。

## 已验证信息

插件路径：

```text
/Library/Audio/Plug-Ins/VST3/BEAM.vst3
/Library/Audio/Plug-Ins/VST3/BEAM Haze.vst3
/Library/Audio/Plug-Ins/VST3/BEAM Taps.vst3
/Library/Audio/Plug-Ins/VST3/BEAM Grains.vst3
/Library/Audio/Plug-Ins/VST3/BEAM Volt.vst3
/Library/Audio/Plug-Ins/VST3/BEAM Space.vst3
```

预设路径：

```text
/Users/caiyiyang/Library/Application Support/Lunacy Audio/BEAM/Presets/
/Users/caiyiyang/Library/Application Support/Lunacy Audio/BEAM/Node Presets/
```

Job Runner 默认状态测试输出目录：

```text
/Volumes/solari 1/给codex的试验田/beam_smoke_20260507/
```

已跑过的 smoke job：

```text
beam_default
beam_lower_districts
beam_haze_default
beam_haze_fuzzy_sides
beam_haze_blurred_space
beam_taps_default
beam_taps_depth_of_field
beam_grains_default
beam_grains_crystal_butterflies
beam_volt_default
beam_volt_cybernetic_shine
```

关键观察：

- BEAM 主插件默认状态可以通过 Job Runner 渲染。
- BEAM Haze / Taps / Grains / Volt 单模块默认状态可以通过 Job Runner 渲染。
- `presetPath` 指向 `.preset` 或 `.nodepreset` 时，manifest 会记录 `stateLoaded: true` 和 `stateLabel`，但输出数据与对应默认状态完全一致。
- 这说明 BEAM 的 `.preset` / `.nodepreset` 不是可直接传给 `AudioProcessor::setStateInformation()` 的 JUCE 原生插件 state。

## 代码层可疑点

### 1. UI Render 路径复用 UI host，且 suspend 时机较激进

位置：

```text
Source/AudioDoctorComponent.h
renderWetWithPlugin()
```

当前逻辑大致是：

1. `closePluginEditorWindow()`
2. `host.captureCurrentState(pluginState, stateError)`
3. `suspendPluginSlot(A/B/C)`
4. 后台线程中调用 `getPluginHost(slot).renderOffline(dryCopy, stateToApply)`

风险：

- active slot 自己也被 suspend。
- render 线程里仍通过 UI 组件持有的 `getPluginHost(slot)` 访问宿主对象。
- 对 BEAM 这类复杂插件，关闭编辑器、抓状态、释放 editable instance、后台创建新实例之间可能触发卡死或崩溃。

建议修复：

- UI 点击 Render 后只在主线程做三件事：
  - 捕获 `PluginDescription`
  - 捕获真实 `pluginState`
  - 复制输入素材
- 后台线程不要再访问 UI 持有的 `AudioDoctorPluginHost`。
- 后台线程创建一个局部的、全新的离线 host / renderer，用捕获到的 `PluginDescription + pluginState + dryCopy` 渲染。
- active plugin slot 不要在捕获状态后立刻强制 suspend；至少不要让后台线程继续依赖已 suspend 的同一个 host。

期望结构：

```cpp
auto description = host.getCurrentPluginDescriptionCopy();
auto state = capturedState;
auto dry = dryCopy;

renderThread = std::thread([description, state, dry] {
    AudioDoctorPluginHost offlineHost;
    offlineHost.loadFromDescription(description);
    auto result = offlineHost.renderOffline(dry, &state);
    // async 回 UI
});
```

如果现有 API 不方便，可以新增一个 `AudioDoctorOfflineRenderer`，避免把 UI 插件宿主和离线渲染宿主混在一起。

### 2. BEAM 预设文件不能当作原生 state 直接 setStateInformation

位置：

```text
Source/AudioDoctorJobRunner.h
loadPluginState()
```

当前 `presetPath` / `statePath` 都按 raw state 读入，然后走：

```cpp
editableInstance->setStateInformation(...)
```

但 BEAM 的 `.preset` / `.nodepreset` 文件是文本 JSON 结构，不是插件 native binary state。测试证明直接套用不会改变参数。

建议分两步修：

#### A. 短期稳定方案

提供 UI 功能：

- 用户在插件编辑器里手动选择 BEAM 预设或调好参数。
- Audio Doctor 提供 “Export Current Plugin State” 或在 manifest 中输出 `pluginStateBase64`。
- Job Runner 后续使用 `pluginStateBase64` 或 `.state` 文件复现。

这样不用解析 BEAM 私有 preset 格式，最稳。

#### B. 中期增强方案

为 BEAM 系列做专门 preset importer：

- 识别 `.preset` / `.nodepreset` 前面的长度前缀和 JSON。
- 解析参数 JSON。
- 将 BEAM 参数名映射到插件暴露参数，例如：
  - `HazeA Density`
  - `HazeA Time Scale`
  - `HazeA Spread`
  - `HazeA Decay`
  - `HazeA Mix`
  - `VoltA Color`
  - `GrainsA Mix`
  - `TapsA Feedback`
- 通过 `setParameterValue()` 逐项设置，而不是 `setStateInformation()`。

这个方案更漂亮，但要做参数名映射和数值范围校准，工期更长。

## 建议验收标准

### UI Render 稳定性

1. Audio Doctor UI 加载 `BEAM.vst3`，导入任意短素材，点击 Render，不崩溃不卡死。
2. Audio Doctor UI 加载 `BEAM Haze.vst3` / `BEAM Grains.vst3` / `BEAM Space.vst3`，点击 Render，不崩溃不卡死。
3. Render 后能正常更新 WET A，并刷新频谱、时频谱、空间立体图。

### 预设/状态复现

至少实现一种：

1. 用户在 UI 里加载/调整 BEAM 后，导出的 manifest 含可复现 `pluginStateBase64` 或 state 文件路径，Job Runner 使用该状态渲染结果与 UI WET 基本一致。
2. 或者 Job Runner 直接支持 BEAM `.preset` / `.nodepreset`，且默认状态与预设状态的输出数据不同。

验收时不要只看 `stateLoaded: true`，必须比较：

```text
wetA_spectrum_hz_db.csv
wetA_dynamics_rms_seconds_dbfs.csv
wetA_spatial_heatmap_summary.csv
```

同一插件默认状态与预设状态应该有实际差异。

## 论文侧使用建议

修复前：

- 不把 BEAM 预设作为正式论文证据。
- 可以把 BEAM 作为后续扩展工具提及，但不要写具体预设结论。

修复后：

- BEAM Haze / Space：适合补充空间扩散、侧向能量、尾音空间变化。
- BEAM Grains：适合补充碎片化、粒子化、魔法能量尾音。
- BEAM Volt：适合补充激励、亮度、电子能量边缘。
- BEAM Taps / Time：适合补充延迟、时间扰动、重复能量轨迹。

优先进入附录；只有图像和数据特别强时，再少量补正文。

