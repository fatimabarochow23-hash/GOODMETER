# GOODMETER 记忆宫殿总索引

更新时间：2026-05-08

## 一句话定位

GOODMETER 现在同时是桌面端音频测量软件、Audio Doctor 论文分析模块、以及论文图例与数据复核工具。当前最重要的工程线是：让 Audio Doctor 通过 UI 和 JobRunner 产出可解释、可复现、可追溯的论文图、CSV 和 manifest。

## 入口地图

| 区域 | 文件 | 用途 |
|---|---|---|
| 时间线 | `timeline/2026-05-01_to_2026-05-08_key_timeline.md` | 恢复 5 月 1 日至 5 月 8 日的关键进展 |
| 功能房 | `feature_rooms/01_AudioDoctor_overview.md` | Audio Doctor 总览和论文定位 |
| 功能房 | `feature_rooms/02_JobRunner_and_Manifest.md` | JSON job、manifest、CSV、AI 复现链路 |
| 功能房 | `feature_rooms/03_SpatialImage_and_2_5D.md` | Spatial Image、2.5D Terrain、空间图组 |
| 功能房 | `feature_rooms/04_PluginHost_and_Rendering.md` | 插件加载、渲染、state、参数快照 |
| 功能房 | `feature_rooms/06_Masking_and_Band_Highlight.md` | 分频高亮、临界带遮蔽风险、融合趋势 |
| 故障房 | `incident_rooms/BEAM_JUCE_message_thread_deadlock.md` | BEAM 卡死、消息线程死锁、修复边界 |
| 资产索引 | `asset_index/requirements_index.md` | 需求单总表 |
| 资产索引 | `asset_index/source_files_index.md` | 源码职责和高危点 |
| 资产索引 | `asset_index/plugin_state_index.md` | 插件兼容、state、预设记录 |
| 资产索引 | `asset_index/test_outputs_index.md` | 关键 smoke test 和输出路径 |
| 示例 | `examples/example_memory_entry.md` | 后续追加记忆的模板 |

## 当前最该记住的边界

- 不要把 Audio Doctor 当普通 UI 功能，它是论文图例和可复核实验记录工具。
- 不要把 JobRunner 理解成“自动点 UI”，它是可复现 JSON 任务系统。
- `Dynamics Response` 目前不是插件内部真实 GR / sidechain meter，只能写动态响应或表观让位。
- Reverb / Space 的 `RT60 est.` 是估算值，主要由 RT30 或 RT20 外推，不是完整混响室测量。
- Spatial Image 是 stereo-derived spatial impression，不是真实 5.1 / Atmos 定位测量。
- Masking / Fusion v1 是遮蔽风险代理，不是真实人耳 masked threshold。
- BEAM / Enrage 等复杂插件要优先用 `pluginStateBase64` 或 host state 复现，不要把厂商私有 preset 当 native state。

