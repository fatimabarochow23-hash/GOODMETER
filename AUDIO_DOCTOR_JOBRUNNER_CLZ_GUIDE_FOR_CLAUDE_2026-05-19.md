# Audio Doctor JobRunner 与 .clz 工程说明

写给 Claude / Claude Code / 其他接手 agent。

更新时间：2026-05-19  
GOODMETER 根目录：`/Volumes/solari 1/Codex_Work/GOODMETER`

## 0. 先记住三句话

1. **JobRunner 是可复现实验入口**：给 GOODMETER 一个 JSON job，它离线导入素材、生成信号、加载插件、渲染、分析、导出图和数据。它不是让 AI 去点 UI。
2. **`.clz` 是 Audio Doctor 工程包**：像 `.ptx` / `.rpp` 那样保存当前 Audio Doctor 页面状态、音频槽位、显示槽、插件状态、Layer Fit 设置等。它不是一张图，也不是 JobRunner 的输出 manifest。
3. **论文图要分四层看**：UI 看到的图、JobRunner 导出的 PNG、manifest/CSV 数据、论文正文引用。不要把这四层混在一起解释。

## 1. 当前路径

权威工作根目录：

```bash
/Volumes/solari 1/Codex_Work
```

GOODMETER 项目：

```bash
/Volumes/solari 1/Codex_Work/GOODMETER
```

macOS standalone app 常见构建产物：

```bash
/Volumes/solari 1/Codex_Work/GOODMETER/Builds/MacOSX/build/Release/GOODMETER.app
```

Audio Doctor 工程默认保存目录通常是：

```bash
/Volumes/solari 1/Codex_Work/AudioDoctor_Exports/AudioDoctor_Projects
```

如果用户改过导出目录，以 UI 里当前 Export / Save Project 选择的位置为准。

## 2. JobRunner 是什么

JobRunner 的入口在代码里：

```text
Source/AudioDoctorJobRunner.h
Source/StandaloneApp.cpp
```

命令行参数：

```bash
--audio-doctor-job "/absolute/path/to/job.json"
```

也支持：

```bash
--doctor-job "/absolute/path/to/job.json"
```

运行方式：

```bash
cd "/Volumes/solari 1/Codex_Work/GOODMETER"
"Builds/MacOSX/build/Release/GOODMETER.app/Contents/MacOS/GOODMETER" \
  --audio-doctor-job "/absolute/path/to/job.json"
```

如果 app 没构建，先跑：

```bash
cd "/Volumes/solari 1/Codex_Work/GOODMETER"
./build.sh standalone
```

路径里有空格或中文时，命令行里一定要加引号。JSON 里也要写绝对路径。

## 3. JobRunner 最小 job 示例：导入素材分析

把下面保存成 `job.json`，把 `path` 换成真实 wav/aiff 文件：

```json
{
  "schemaVersion": 2,
  "sessionName": "claude_smoke_imported_audio",
  "sources": {
    "dryA": {
      "path": "/absolute/path/to/source.wav",
      "selection": { "start": 0.0, "end": 2.5 }
    }
  },
  "displaySlots": ["dryA"],
  "views": ["spectrum", "spectrogram_abc", "reverb_space"],
  "export": {
    "preset": "ui_dark",
    "outDir": "/Volumes/solari 1/Codex_Work/AudioDoctor_Job_Exports",
    "width": 1800,
    "height": 1050
  }
}
```

输出会生成在：

```text
/Volumes/solari 1/Codex_Work/AudioDoctor_Job_Exports/claude_smoke_imported_audio/
```

核心输出：

```text
figures/                 PNG 图
data/                    CSV 曲线和结构化数据
manifest.json            论文证据链主文件
response.json            JobRunner 运行结果
job_summary.md           人读摘要
appendix_table.csv       附录表
```

## 4. JobRunner 最小 job 示例：生成测试信号

适合论文中不想依赖真实素材的图，例如泛音列、扫频、冲激、瞬态 burst。

```json
{
  "schemaVersion": 2,
  "sessionName": "claude_smoke_harmonic_series",
  "sources": {
    "dryA": {
      "generateSignal": {
        "type": "harmonic_series",
        "sampleRate": 48000,
        "channels": 2,
        "seconds": 2.0,
        "levelDb": -6.0,
        "frequencyHz": 220.0,
        "harmonicCount": 6,
        "harmonicRolloffDb": 6.0,
        "seed": 4432
      }
    }
  },
  "displaySlots": ["dryA"],
  "views": ["spectrum", "spectrogram_abc"],
  "export": {
    "preset": "ui_dark",
    "outDir": "/Volumes/solari 1/Codex_Work/AudioDoctor_Job_Exports",
    "width": 1800,
    "height": 1050
  }
}
```

常用 `generateSignal.type`：

```text
sine
harmonic_series
white_noise
pink_noise
sweep
click
impulse
transient_burst
charge_riser
shot_impact
tail_decay
band_limited_noise
harmonic_fusion
cst_event
```

常用字段：

```text
sampleRate, channels, seconds, levelDb, frequencyHz,
startHz, endHz, phaseDegrees,
harmonicCount, harmonicRolloffDb,
seed, invert,
noiseAmount, modRateHz, modDepth,
bodyHz, bodyDecayMs, crackHz, crackAmount,
noiseBandLowHz, noiseBandHighHz,
transientMs, fundamentalHz, decayMs,
tailBandLowHz, tailBandHighHz, damping,
earlyReflectionAmount, stereoWidth,
chargeStartSec, shotTimeSec, tailStartSec,
stageBalance, fundamentalAHz, fundamentalBHz,
detuneCents, overlapAmount
```

## 5. JobRunner 插件渲染示例

插件 A/B/C 会渲染出 WET A/B/C。默认输入通常来自 dryA，也可以用 `renderRouting` 指定。

```json
{
  "schemaVersion": 2,
  "sessionName": "claude_plugin_render_example",
  "sources": {
    "dryA": {
      "path": "/absolute/path/to/dry.wav",
      "selection": { "start": 0.0, "end": 3.0 }
    }
  },
  "pluginA": {
    "path": "/Library/Audio/Plug-Ins/VST3/Some Plugin.vst3",
    "params": [
      { "index": 0, "value": 0.35 },
      { "name": "Mix", "value": 0.25 }
    ]
  },
  "render": {
    "slot": "A",
    "tailSeconds": "auto",
    "latencyCompensation": true
  },
  "renderRouting": {
    "wetA": { "input": "dryA" }
  },
  "displaySlots": ["dryA", "wetA"],
  "views": ["spectrum", "envelope", "group_delay", "spectrogram_abc", "reverb_space"],
  "export": {
    "preset": "ui_dark",
    "outDir": "/Volumes/solari 1/Codex_Work/AudioDoctor_Job_Exports",
    "width": 1800,
    "height": 1050
  }
}
```

插件预设 / 状态优先级：

```text
statePath
presetPath
preset        如果它是一个真实存在的文件路径
pluginStateBase64
params
```

注意：有些第三方插件的 `.preset` / `.nodepreset` 不是 AU/VST3 原生状态。JobRunner 可能拒绝直接吃这种文件，这是正常边界，不要写成 GOODMETER 算法错误。

## 6. JobRunner 支持的视图

常用视图名：

```text
spectrum
envelope
group_delay
spectrogram_abc
reverb_space
spatial_image
spatial_terrain
spatial_terrain_2_5d
layer_fit_fusion
critical_band_terrain
time_frequency_terrain
critical_band_crystal
dodecahedron_crystal
dynamics
```

`views` 里的连字符会被归一化成下划线。例如 `reverb-space` 和 `reverb_space` 应按同一类理解。

主题 / preset：

```text
ui_dark
ui_light
academic_light
thesis_dark
thesis_light
```

当前论文组主要用深色图，优先 `ui_dark` 或 `thesis_dark`。如果老师要求白底，再用 light preset。

导出尺寸：

```json
{
  "export": {
    "width": 1800,
    "height": 1050
  }
}
```

宽高会被限制在合理范围内。论文图通常不要太小，否则标签进 Word 会糊。

## 7. Layer Fit / Fusion 的 JobRunner 用法

Layer Fit / Fusion 是给论文解释“声层贴合、频段占位、融合倾向、遮蔽风险”的 proxy model。它不是人耳阈值测量，也不是插件内部 gain reduction。

示例：

```json
{
  "schemaVersion": 2,
  "sessionName": "claude_layer_fit_example",
  "sources": {
    "dryA": { "path": "/absolute/path/stem_a.wav" },
    "dryB": { "path": "/absolute/path/stem_b.wav" },
    "dryC": { "path": "/absolute/path/stem_c.wav" }
  },
  "displaySlots": ["dryA", "dryB", "dryC"],
  "layerFitFusion": {
    "stems": ["dryA", "dryB", "dryC"],
    "bounce": "auto",
    "figure": "dodecahedron_crystal",
    "band": "all",
    "angle": "diagonal",
    "flipTime": false
  },
  "views": ["layer_fit_fusion", "dodecahedron_crystal", "critical_band_terrain"],
  "export": {
    "preset": "ui_dark",
    "outDir": "/Volumes/solari 1/Codex_Work/AudioDoctor_Job_Exports",
    "width": 2000,
    "height": 1100
  }
}
```

Layer Fit 图的重点：

```text
critical_band_terrain       临界带地形，适合看频段占位和风险区
time_frequency_terrain      时间-频率-能量立体图，适合看多个声层如何在时频上叠合
spatial_image               L-C-R 空间印象图，适合看中心/侧向能量分布
critical_band_crystal       晶体图，适合看临界带结构化关系
dodecahedron_crystal        十二面体晶体图，适合展示高维声层关系
```

颜色语义：

```text
红色 / masking risk       遮蔽风险，主导层可能压过较弱层
紫色 / fusion tendency    融合倾向，多层能量接近并同步重叠
黄色 / mix gain           合成增益，bounce 后该频带强于最强单层
```

写论文时建议说“proxy / 指标模型 / 可视化辅助判断”，不要写成“测得真实听觉遮蔽阈值”。

## 8. `.clz` 工程是什么

`.clz` 是 Audio Doctor 工程包。macOS Finder 会把它显示成一个文件，但它本质上是一个目录。

内部结构大致是：

```text
SomeProject.clz/
  project.json
  audio files/
    dryA.wav
    dryB.wav
    wetA.wav
    ...
```

`project.json` 保存：

```text
projectType
schemaVersion
savedAt
view/theme/band/angle/time-reverse 等 UI 状态
displaySlots
renderRouting
layerFitFusion
pluginA/pluginB/pluginC 的路径、参数、状态、输出 gain、render 信息
audioFiles 列表
```

`audio files/` 只保存当前工程里还在使用的音频槽位，不保存已经被替换掉的旧素材。

## 9. `.clz` 怎么保存

在 GOODMETER Audio Doctor UI 里：

```text
Export -> Save Project / 保存工程
```

保存时会让用户选择路径。默认通常在：

```text
/Volumes/solari 1/Codex_Work/AudioDoctor_Exports/AudioDoctor_Projects
```

保存出来的文件名类似：

```text
chapter_4_reverb_test.clz
```

不要把 `.clz` 当成普通 JSON。需要看内部内容时，在 macOS 可以右键显示包内容，或者命令行：

```bash
find "/path/to/Project.clz" -maxdepth 2 -type f
```

## 10. `.clz` 怎么打开

方式 A：双击 `.clz`

```text
双击 Project.clz -> 打开 GOODMETER -> 自动进入 Audio Doctor -> 恢复工程状态
```

方式 B：命令行打开：

```bash
open "/path/to/Project.clz"
```

方式 C：从 GOODMETER UI 里打开工程，如果当前 UI 有入口的话，以 UI 为准。

成功打开后，应该恢复：

```text
Dry/Wet 槽位音频
当前视图
主题
显示槽位
Bus / render routing
Layer Fit / Fusion 的 stem、bounce、figure、angle、flip time、时间位置
插件槽 A/B/C 的名称、路径、参数/状态快照
```

如果另一台电脑没有同一个插件，工程仍应打开音频和图表状态，但对应插件会显示缺失或不可用。不要因为插件缺失就判定 `.clz` 坏了。

## 11. `.clz` 跨平台原则

`.clz` 的目标是 Mac / Windows 同版本 GOODMETER 都能打开。

跨平台必须满足：

```text
1. project.json 使用 UTF-8 JSON。
2. 音频使用包内 relativePath，优先读 .clz/audio files/。
3. 原始绝对路径只作为 sourcePathAtSaveTime 记录，不作为跨平台必需路径。
4. Windows 端打开 Mac 保存的 .clz 时，即使 /Volumes/... 不存在，也应读取包内音频。
5. 插件路径跨平台不保证存在；缺插件时应保留音频和图表状态。
```

给 Windows Codex 的硬验收：

```text
Mac 保存一个包含 dryA/dryB/dryC 或 wetA/wetB 的 .clz。
复制到 Windows。
用同版本 GOODMETER 打开。
确认音频槽位、图表、Layer Fit 状态能恢复。
如果插件不存在，显示缺失但不崩溃。
```

## 12. JobRunner 和 `.clz` 的区别

| 项目 | JobRunner | `.clz` 工程 |
|---|---|---|
| 目的 | 批处理、论文图、可复现实验 | 保存/恢复一次人工调好的 Audio Doctor 会话 |
| 入口 | JSON job + 命令行 | UI 保存/打开，或双击 `.clz` |
| 输出 | PNG、CSV、manifest、summary | project.json + audio files |
| 适合 AI | 非常适合，AI 写 JSON 任务 | 适合接手人工会话，不适合大批量实验 |
| 是否等于论文图 | 不是，但会导出论文图 | 不是，需要再 Export 或 JobRunner |
| 是否包含音频 | 可导出分析数据，不一定复制工程素材 | 会复制当前工程音频到包内 |

简单判断：

```text
要批量跑图、跑数据、给论文组自动分析 -> 用 JobRunner。
要保存当前 Audio Doctor 调参现场，明天继续打开 -> 用 .clz。
要给老师一个可玩的工程状态 -> 给 .clz + GOODMETER app。
要给论文一个图和数据 -> 给 figures/ + data/ + manifest.json。
```

## 13. 常见错误

### 错误 1：Claude 找不到 `.clz`

先查：

```bash
find "/Volumes/solari 1/Codex_Work" -name "*.clz" -maxdepth 5
```

如果用户把工程保存到桌面或 iCloud，也可能在：

```bash
find "$HOME/Desktop" "$HOME/Documents" -name "*.clz" -maxdepth 5
```

### 错误 2：把 `.clz` 当文件读，读不到 project.json

`.clz` 是 package 目录。正确查法：

```bash
find "/path/to/Project.clz" -maxdepth 2 -type f
sed -n '1,120p' "/path/to/Project.clz/project.json"
```

### 错误 3：JobRunner 没输出图

检查：

```text
1. response.json 是否 status ok。
2. manifest.json 是否存在。
3. views 字段是否写对。
4. export.outDir 是否可写。
5. 插件路径是否存在。
6. 输入音频路径是否存在。
```

### 错误 4：路径里有空格导致命令失败

命令行必须加引号：

```bash
"Builds/MacOSX/build/Release/GOODMETER.app/Contents/MacOS/GOODMETER" \
  --audio-doctor-job "/Volumes/solari 1/Codex_Work/jobs/my job.json"
```

### 错误 5：JobRunner 图和 UI 图不一致

先确认是否同一个 GOODMETER 构建版本。历史上 UI Export 和 JobRunner 曾经是两套绘图路径，后来持续补齐。新图表功能必须同时确认：

```text
UI 能显示
UI Export 能导出
JobRunner 能导出同类图
manifest / CSV 有结构化数据
```

不要只看 UI 截图就说 JobRunner 已支持。

## 14. 给 Claude 的接手流程

如果你是 Claude，接到“跑 Audio Doctor 图”任务，请按这个顺序：

1. 确认当前 GOODMETER 路径：

```bash
cd "/Volumes/solari 1/Codex_Work/GOODMETER"
git status --short --branch
```

2. 确认 app 是否存在：

```bash
ls -la "Builds/MacOSX/build/Release/GOODMETER.app/Contents/MacOS/GOODMETER"
```

3. 如果不存在或用户要求最新构建：

```bash
./build.sh standalone
```

4. 写一个 JSON job 到临时目录或论文工作目录。

5. 运行：

```bash
"Builds/MacOSX/build/Release/GOODMETER.app/Contents/MacOS/GOODMETER" \
  --audio-doctor-job "/absolute/path/to/job.json"
```

6. 回报时必须给：

```text
response.json 路径
manifest.json 路径
figures/ 路径
data/ 路径
如果失败，给 error 字段和可复现命令
```

如果任务是“恢复用户之前调好的现场”，不要写 JobRunner。先找 `.clz`，用 `.clz` 打开。

## 15. 一条最短 smoke test

```bash
cd "/Volumes/solari 1/Codex_Work/GOODMETER"
cat > /tmp/audio_doctor_smoke_job.json <<'JSON'
{
  "schemaVersion": 2,
  "sessionName": "audio_doctor_smoke_harmonic_series",
  "sources": {
    "dryA": {
      "generateSignal": {
        "type": "harmonic_series",
        "sampleRate": 48000,
        "channels": 2,
        "seconds": 2.0,
        "levelDb": -6.0,
        "frequencyHz": 220.0,
        "harmonicCount": 6,
        "harmonicRolloffDb": 6.0,
        "seed": 4432
      }
    }
  },
  "displaySlots": ["dryA"],
  "views": ["spectrum", "spectrogram_abc"],
  "export": {
    "preset": "ui_dark",
    "outDir": "/Volumes/solari 1/Codex_Work/AudioDoctor_Job_Exports",
    "width": 1800,
    "height": 1050
  }
}
JSON

"Builds/MacOSX/build/Release/GOODMETER.app/Contents/MacOS/GOODMETER" \
  --audio-doctor-job "/tmp/audio_doctor_smoke_job.json"
```

成功后看：

```bash
find "/Volumes/solari 1/Codex_Work/AudioDoctor_Job_Exports/audio_doctor_smoke_harmonic_series" -maxdepth 3 -type f
```

至少应该看到：

```text
manifest.json
response.json
figures/...spectrum...
figures/...spectrogram...
data/...csv
```

## 16. 论文写作边界

Audio Doctor 图可以支撑论文分析，但不要过度宣称：

```text
可以说：频谱峰值、群延时、能量衰减、尾部时频分布、临界带重叠 proxy、空间能量分布 proxy。
不要说：测得真实人耳遮蔽阈值、测得插件内部 GR、证明听感必然如此。
```

Layer Fit / Fusion、Critical Band Crystal、Dodecahedron Crystal 都是论文辅助可视化模型。它们的价值在于把“声层之间怎样占位、融合、互相遮蔽或形成合成增益”变成可观察图像和结构化数据。

