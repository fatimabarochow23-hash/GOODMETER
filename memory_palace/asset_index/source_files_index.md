# 关键源码文件索引

| 文件 | 负责功能 | 高危点 |
|---|---|---|
| `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorComponent.h` | Audio Doctor UI、按钮、路由、导出入口、插件编辑窗口 | 文件大，改动易冲突；UI 与 JobRunner 语义要保持一致 |
| `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorAnalysis.h` | 音频分析、曲线、指标、spectrogram、masking/fusion 数据 | 数据可信度核心；算法口径必须能解释 |
| `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorFigureRenderer.h` | 论文图渲染、主题、2.5D、Spatial Image、图例布局 | 视觉一致性核心；不要让 UI 和 JobRunner 分叉 |
| `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorJobRunner.h` | JSON job、manifest、CSV、批处理图例导出 | 复现链路核心；路径、state、参数快照必须稳定 |
| `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorPluginHost.h` | AU/VST3 加载、编辑器、state、离线渲染 | 插件兼容高危；BEAM/Enrage 等复杂插件要分 UI path 与 CLI path |
| `/Users/caiyiyang/Desktop/GOODMETER/Source/StandaloneApp.cpp` | standalone 启动、命令行参数、JobRunner 实例行为 | `--audio-doctor-job` 不能被旧 GUI 实例吞掉 |
| `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioLabComponent.h` | Audio Lab 波形/瀑布图历史资产 | Spectrogram 对齐参考，不要随意改坏已有视觉 |

## 修改提醒

- 改图例视觉优先看 `AudioDoctorFigureRenderer.h`。
- 改 UI 操作优先看 `AudioDoctorComponent.h`。
- 改可复现任务优先看 `AudioDoctorJobRunner.h`。
- 改插件卡死/状态优先看 `AudioDoctorPluginHost.h`。
- 改算法指标优先看 `AudioDoctorAnalysis.h`。

