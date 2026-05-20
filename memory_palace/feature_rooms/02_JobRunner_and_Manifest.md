# 功能房 02｜JobRunner 与 Manifest

## 当前定位

JobRunner 是 Audio Doctor 给论文组和 AI 使用的可复现任务接口。它的目标不是“模拟鼠标点 UI”，而是通过 JSON job 明确描述素材、插件、参数、路由、视图和导出设置。

CLI 入口：

```bash
Builds/MacOSX/build/Release/GOODMETER.app/Contents/MacOS/GOODMETER --audio-doctor-job /path/to/job.json
```

如果路径里有空格，需要持续确认解析是否稳定；历史上论文组曾用 `/tmp/solari_codex_field` 软链接绕过空格路径。

## Manifest 必须保留的信息

素材基准：

- source file name
- source path
- source hash
- selection start/end
- analysis duration
- sample rate
- channels
- peak dBFS
- RMS dBFS
- crest factor

插件基准：

- plugin name / format / path
- statePath / stateHash / stateLoaded
- pluginStateBase64 或 state 文件
- changedParameters
- full parameter snapshot：index、id、name、label、normalizedValue、displayValue、nameUnavailable

图例基准：

- preset / palette
- view / figure mode
- frequency range
- FFT size / window / hop
- terrainCamera / terrainTimeReversed
- spatialTimePositionSeconds / spatialWindow
- sharedScale / scaleMinDb / scaleMaxDb

## 当前已知重要能力

- JobRunner 支持 Spectrogram A/B/C，不应 fallback 成 Spectrum。
- JobRunner 与 UI 应共享核心图例渲染路径。
- thesis preset / thesisFigures 可用于批量论文图。
- 输出应包含 PNG、CSV、manifest、必要时 appendix table。

## 2026-05-16 续接验收清单

新增或改动任何 Audio Doctor 视图时，JobRunner 至少要能回答这些问题：

- 这个 view token 能不能从 JSON job 调到。
- PNG 是否使用与 UI/export 同一套语义配色和字体尺度。
- CSV/JSON 是否包含图中关键数据，而不是只有截图。
- manifest 是否记录素材 hash、工程相对路径、插件 state、参数快照、路由、render gain、tail、latency compensation。
- 中文路径、空格路径、外接硬盘路径是否安全。

当前应重点覆盖的 view 家族：

- `spectrum`
- `envelope`
- `group_delay`
- `spectrogram_abc`
- `reverb_space`
- `spatial_image`
- `spatial_terrain` / `*_2_5d`
- `layer_fit_fusion`
- `critical_band_terrain`
- `time_frequency_terrain`
- `critical_band_crystal`
- `dodecahedron_crystal`

如果代码中某个 token 名称与上述不一致，更新 handoff 时必须写清楚真实 token，不能让论文组猜。

## 边界

- JobRunner 能跑某个插件默认状态，不代表 UI Render 路径就一定安全。
- 厂商 `.preset` / `.nodepreset` 不一定是 JUCE native state。
- 对 BEAM / Enrage 等复杂插件，优先依赖从 Audio Doctor UI 捕获的 `pluginStateBase64` 或 host state。
- 不要把 `.clz` 工程保存和 JobRunner job 混为一谈：`.clz` 是交互工程，JobRunner 是可复现实验任务。两者可以共享 manifest 语义，但入口不同。
- 如果新增 Layer Fit / Crystal 算法，只做 UI 不算完成；JobRunner 也要能导出完整风险/融合/增益 band 列表和当前时间窗数据。

状态：有效。

标签：#JobRunner #Manifest #CSV #AI接口
