# GOODMETER 项目架构答辩说明（Codex 版）

生成日期：2026-05-10  
用途：帮助蔡艺扬在论文答辩或老师追问时，用自己的话说明 GOODMETER 与 Audio Doctor 的项目结构、代码规模和功能边界。

> 说明：以下代码行数按当前本机 `/Users/caiyiyang/Desktop/GOODMETER/Source` 统计，是施工队继续修改中的阶段性快照，不是最终发布版固定数字。

## 1. 一句话说明

GOODMETER 是笔者从零开发的跨形态音频测量与分析项目，主体使用 JUCE / C++ 实现，包含 DAW 插件版、macOS standalone 桌面端、iOS 端以及面向论文图例分析的 Audio Doctor 模块。

在论文语境里，最重要的是 macOS standalone 中扩展出的 Audio Doctor：它可以导入或生成音频素材，加载第三方音频插件，离线渲染处理结果，并把处理前后的频谱、时频谱、动态、群延时、尾音衰减、声像分布和声层融合关系导出为论文图表与数据。

## 2. 项目总分层

可以把 GOODMETER 理解为四层：

```text
音频输入层
  ├─ DAW 插件输入
  ├─ macOS 系统音频捕获
  └─ Audio Doctor 文件/生成信号输入

音频处理与分析层
  ├─ PluginProcessor 实时测量
  ├─ Audio Doctor 离线分析
  ├─ PluginHost 插件渲染
  └─ Analysis 指标计算

图形与交互层
  ├─ 八张实时表
  ├─ Nono 桌面端交互
  ├─ Audio Lab 等技能页面
  └─ Audio Doctor 论文图例界面

导出与复现层
  ├─ UI Export
  ├─ Job Runner
  ├─ PNG / CSV / manifest
  └─ 插件参数快照与素材基准信息
```

## 3. 一级模块代码规模

| 一级模块 | 代码量 | 文件数 | 主要作用 |
|---|---:|---:|---|
| Audio Doctor 论文分析模块 | 17,562 行 | 5 | 论文图例、插件渲染、数据分析、Job Runner、图表导出 |
| iOS 端 | 14,851 行 | 16 | iOS 版 GOODMETER 的页面、历史记录、视频页、表页面和 Nono 页面 |
| macOS standalone / Nono / 技能壳 | 10,245 行 | 4 | 桌面宠物、窗口、技能入口、Nono 动画与交互 |
| 八张基础表与通用 UI 组件 | 6,844 行 | 10 | LUFS、VU、三频、频谱、相位、声像、瀑布、PSR 等实时表 |
| 附加技能 / Audio Lab / 视频音频 / AI 降噪 | 4,062 行 | 5 | Audio Lab、从视频提取音频、Room Tone、DeepFilter 降噪 |
| 核心音频引擎 / DSP / 采集录音 | 2,126 行 | 6 | 实时音频处理、LUFS/RMS/Peak/FFT、录音和系统音频捕获 |
| 插件版宿主 UI | 820 行 | 2 | DAW 插件版界面，显示基础表 |
| **合计** | **56,505 行** | **48** | GOODMETER 当前主要源码体量 |

## 4. 一级模块展开

### 4.1 核心音频引擎 / DSP / 采集录音

代码量：2,126 行  
核心文件：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `Source/PluginProcessor.cpp` | 830 | JUCE AudioProcessor 主处理逻辑，实时读取音频并计算指标 |
| `Source/PluginProcessor.h` | 325 | 暴露 Peak、RMS、LUFS、相位、三频、M/S 等原子数据 |
| `Source/SystemAudioCapture.mm` | 429 | macOS CoreAudio Process Tap 系统音频捕获 |
| `Source/SystemAudioCapture.h` | 51 | 系统音频捕获接口 |
| `Source/AudioRecorder.h` | 288 | WAV 录音与后台写入 |
| `Source/AudioHistoryBuffer.h` | 203 | 倒录/回溯录音缓冲 |

答辩口径：

这层负责把实时音频变成可读的数据流。比如 Peak、RMS、LUFS、相位相关、低中高频能量、FFT 数据都在这里产生。基础表看到的是实时指标；Audio Doctor 看到的是离线素材分析，两者共用一部分音频分析思路。

### 4.2 八张基础表与通用 UI 组件

代码量：6,844 行  
核心文件：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `Source/GoodMeterLookAndFeel.h` | 1,238 | 项目统一视觉风格、颜色、字体、控件绘制 |
| `Source/MeterCardComponent.h` | 1,067 | 表卡片外壳，负责折叠、标题、阴影和卡片布局 |
| `Source/LevelsMeterComponent.h` | 879 | LUFS / Peak / RMS / LRA 电平表 |
| `Source/StereoImageComponent.h` | 784 | 声像、相位、M/S 相关显示 |
| `Source/PsrMeterComponent.h` | 542 | Peak-to-Short-Term Ratio 表 |
| `Source/VUMeterComponent.h` | 514 | 传统 VU 表 |
| `Source/SpectrogramComponent.h` | 505 | 实时时频瀑布图 |
| `Source/PhaseCorrelationComponent.h` | 483 | 相位相关表 |
| `Source/Band3Component.h` | 455 | 低/中/高三频能量表 |
| `Source/SpectrumAnalyzerComponent.h` | 377 | 频谱分析表 |

答辩口径：

GOODMETER 的基础功能是桌面音频测量助手，核心是八张实时表。它们负责观察播放系统或 DAW 中的声音状态，例如响度、频谱、声像、相位和动态。论文中的 Audio Doctor 是在这个基础测量系统上进一步扩展出的离线分析功能。

### 4.3 macOS standalone / Nono / 技能壳

代码量：10,245 行  
核心文件：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `Source/StandaloneNonoEditor.h` | 4,628 | macOS 桌面端主界面，桌面宠物、卡片展开、技能入口 |
| `Source/HoloNonoComponent.h` | 4,068 | Nono 角色绘制与动画 |
| `Source/StandaloneApp.cpp` | 808 | 自定义 standalone 应用入口、透明窗口、系统音频捕获和命令行入口 |
| `Source/SkillTreeComponent.h` | 741 | 技能树/技能入口 UI |

答辩口径：

GOODMETER 不只是插件，也有桌面端软件。桌面端用 Nono 作为交互入口，展开后可以看到基础表和额外技能。Audio Doctor 就是其中一个面向论文分析的技能页面。

### 4.4 插件版宿主 UI

代码量：820 行  
核心文件：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `Source/PluginEditor.cpp` | 703 | DAW 插件版界面布局与实时刷新 |
| `Source/PluginEditor.h` | 117 | 插件 UI 类声明 |

答辩口径：

插件版主要放在 DAW 中使用，用来实时观察音频链路中的响度、频谱、相位等指标。它和 standalone 共享核心音频引擎，但交互方式更简单。

### 4.5 附加技能 / Audio Lab / 视频音频 / AI 降噪

代码量：4,062 行  
核心文件：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `Source/AudioLabComponent.h` | 1,783 | Audio Lab 技能页面，素材实验和可视化 |
| `Source/VideoAudioExtractor.mm` | 1,022 | 从视频文件提取音频 |
| `Source/DeepFilterProcessor.h` | 781 | DeepFilterNet / ONNX 降噪处理 |
| `Source/RoomToneExtractor.h` | 401 | Room Tone 提取 |
| `Source/VideoAudioExtractor.h` | 75 | 视频音频提取接口 |

答辩口径：

这些是桌面端的扩展技能，说明 GOODMETER 不是单一表头，而是围绕声音后期工作流做的一组工具。论文主要用 Audio Doctor，但 Audio Lab、视频提取、降噪等模块体现了软件面向声音后期的综合定位。

### 4.6 iOS 端

代码量：14,851 行  
核心文件：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `Source/iOS/NonoPageComponent.h` | 3,852 | iOS 版 Nono 页面 |
| `Source/iOS/HistoryPageComponent.h` | 3,132 | 历史记录页面 |
| `Source/iOS/VideoPageComponent.mm` | 2,177 | iOS 视频页面实现 |
| `Source/iOS/MarathonNonoComponent.h` | 1,515 | iOS Nono 长时间运行/马拉松组件 |
| `Source/iOS/MetersPageComponent.h` | 1,449 | iOS 表页面 |
| `Source/iOS/SettingsPageComponent.h` | 702 | 设置页面 |
| `Source/iOS/iOSMainComponent.h` | 575 | iOS 主界面 |
| `Source/iOS/iOSAudioEngine.h` | 284 | iOS 音频引擎 |

答辩口径：

iOS 端是 GOODMETER 的移动扩展，目前和论文关系较弱。答辩时如果老师问，可以说论文分析主要在 macOS desktop 的 Audio Doctor 中完成，iOS 端属于同一软件生态的移动端探索。

## 5. Audio Doctor 模块详细拆解

Audio Doctor 是论文最相关的模块，代码量 17,562 行。

| 二级模块 | 文件 | 行数 | 职责 |
|---|---|---:|---|
| UI 编排与交互入口 | `Source/AudioDoctorComponent.h` | 5,559 | 导入素材、生成信号、加载插件、编辑插件、渲染 Wet、选择视图、导出图表 |
| 论文图渲染器 | `Source/AudioDoctorFigureRenderer.h` | 4,739 | 把分析结果画成论文用 PNG 图，如频谱、时频谱、空间、融合、临界带图 |
| 分析算法层 | `Source/AudioDoctorAnalysis.h` | 3,335 | 计算频谱、RMS、Crest Factor、包络、群延时、EDC、DRR、空间与融合指标 |
| Job Runner / manifest | `Source/AudioDoctorJobRunner.h` | 3,143 | 通过 JSON 自动跑图，导出 PNG、CSV、manifest、插件状态和参数快照 |
| 插件宿主与离线渲染 | `Source/AudioDoctorPluginHost.h` | 786 | 加载 AU/VST3 插件，捕获参数/state，离线渲染 Dry 到 Wet |

### 5.1 Audio Doctor 的工作流

```text
导入/生成素材
  ↓
Dry A / Dry B / Dry C
  ↓
选择 Plugin A / B / C
  ↓
设置插件参数或读取插件 state
  ↓
Render A / B / C
  ↓
Wet A / Wet B / Wet C
  ↓
计算分析指标
  ↓
选择图例视图
  ↓
导出 PNG / CSV / manifest
```

### 5.2 Dry / Wet / Plugin 的关系

- Dry：处理前素材，可以是导入音效，也可以是软件生成的测试信号。
- Plugin：第三方音频效果器，例如失真、混响、空间、压缩、滤波器等。
- Wet：Dry 经过插件处理后的结果。

当前设计支持三组：

- `Dry A/B/C`
- `Wet A/B/C`
- `Plugin A/B/C`

这样既能比较同一个素材经过不同效果器后的变化，也能比较多层素材被分别处理后再融合的结果。

### 5.3 Audio Doctor 主要视图

| 视图 | 论文用途 |
|---|---|
| Spectrum | 看频谱结构、谐波密度、低中高频占位 |
| Envelope | 看起音、衰减、尾音长度和动态包络 |
| Group Delay | 看全通滤波、Disperser 等造成的频率相关时间展开 |
| Spectrogram A/B/C | 看素材层次、Charge-Shot-Tail、尾音频段残留 |
| Reverb / Space | 看 EDC、RT60 估算、DRR、早期/后期能量关系 |
| Dynamics | 看 RMS 动态变化和表观衰减 |
| Spatial Image | 看立体声声像、侧向扩散和不同时间点的空间变化 |
| Layer Fit / Fusion | 看多 stem 的频带贴合、融合、遮蔽风险和 bounce 后变化 |

### 5.4 Job Runner 是什么

Job Runner 是给论文组和 AI 使用的自动化接口。它不是模拟鼠标操作，而是用 JSON 描述：

- 使用哪条素材；
- 加载哪个插件；
- 使用什么插件参数或 state；
- 用什么路由；
- 输出哪类图；
- 导出到哪个目录。

它的命令形式大概是：

```bash
GOODMETER --audio-doctor-job /path/to/job.json
```

输出包括：

- PNG 图；
- CSV 数据；
- manifest 说明文件；
- 素材基准信息；
- 插件名称、插件路径、参数快照、state hash。

答辩口径：

Job Runner 的意义是让图表可复现。不是每次靠手动截图，而是把素材、插件和参数记录下来，方便重新生成同一批图表。

### 5.5 插件参数快照和 state 是什么

插件参数快照记录的是插件界面里每个旋钮/参数的可读信息，例如：

- 参数名；
- 参数编号；
- normalized value；
- display value；
- 是否缺少参数名。

plugin state 记录的是插件完整状态，适合复杂插件。比如 BEAM、Enrage 这类插件有很多内部模块，只记录几个旋钮不够，需要 state 才能复现当时的处理状态。

答辩口径：

为了保证论文图表不是偶然截图，Audio Doctor 会记录素材信息、插件信息和参数状态。这样图表背后的处理链路可以复核。

## 6. 你需要能背下来的答辩版解释

### 如果老师问：GOODMETER 是什么？

GOODMETER 是我从零开发的音频测量与分析软件，最初核心是桌面端音频助手和多张实时表，用来观察响度、频谱、相位、声像等信息。后来为了论文需要，我在桌面端扩展了 Audio Doctor 模块，让它可以导入素材、加载插件、离线渲染处理结果，并导出论文图和数据。

### 如果老师问：Audio Doctor 和 Plugin Doctor 是什么关系？

Audio Doctor 的观察思路参考了通用插件测试软件，例如对比处理前后频谱、动态和相位变化。但它不是复制 Plugin Doctor，而是为了论文重新做的分析模块。它更重视素材对比、多层素材、空间尾音、临界带融合和论文图表导出。

### 如果老师问：你论文里的图是不是软件自动生成的？

图是由我开发的软件根据素材和插件处理结果导出的，但不是随便截图。每张图都有对应的分析指标，比如频谱、时频谱、EDC、DRR、群延时、RMS 或声像分布。附录里记录了素材基准、插件参数和图表口径，方便复核。

### 如果老师问：你自己做了什么？

可以按这个顺序说：

1. 先做了 GOODMETER 的基础音频测量系统，包括实时电平、频谱、相位和声像等表。
2. 又做了 macOS standalone 桌面端，把这些表和额外技能整合进一个桌面应用。
3. 为论文扩展了 Audio Doctor，让它能加载素材、渲染插件效果、计算数据、输出论文图。
4. 后期根据论文需要继续补了空间图、2.5D 时频图、Layer Fit / Fusion 和临界带相关图。

### 如果老师问：这个软件和论文有什么关系？

论文研究的是动画电影中能量碰撞音效的分层构建与效果链应用。这个对象很难只靠文字解释，所以我做了 Audio Doctor，把声音设计中的频率分层、瞬态、尾音、空间扩散和层间融合变成可观察的图和数据。软件不是论文的研究对象本身，而是论文的分析工具。

## 7. 最容易被问到的边界

### 7.1 Audio Doctor 不等于主观听感实验

Audio Doctor 测的是信号指标，比如频谱、RMS、DRR、声像分布。它不能直接证明观众一定怎么听，只能为听感分析提供可观察的信号依据。

### 7.2 Spatial Image 不是真实三维声场

它显示的是立体声信号里的 L-C-R 或 M/S 能量分布线索，不是实际空间坐标测量。

### 7.3 Layer Fit / Fusion 不是完整心理声学模型

它用于观察多层素材在时间、频率和临界带附近的重叠、融合与风险。它服务论文分析，不替代严格听感实验。

### 7.4 插件 state 比厂商 preset 更适合复现

厂商 preset 文件不一定能被独立宿主可靠读取，所以 Audio Doctor 更重视插件自己的 native state 和参数快照。

## 8. 当前施工状态提示

当前 GOODMETER 仓库中有未提交修改，主要集中在：

- `Source/AudioDoctorAnalysis.h`
- `Source/AudioDoctorComponent.h`
- `Source/AudioDoctorFigureRenderer.h`
- `Source/AudioDoctorJobRunner.h`
- `Source/AudioDoctorPluginHost.h`
- `Source/StandaloneApp.cpp`

这些修改多半与 Audio Doctor、Layer Fit / Fusion、Spatial Image、Job Runner 和插件渲染有关。后续如果要给老师看或上传 GitHub，需要先让施工队完成收尾、构建验证和提交。

## 9. 项目内部的最短理解路线

如果只想快速理解项目，不需要从所有 5.6 万行开始看。推荐顺序：

1. 读 `Source/PluginProcessor.h/.cpp`：理解实时音频数据从哪里来。
2. 读 `Source/AudioDoctorComponent.h` 的前 300 行：理解 Audio Doctor UI 有哪些按钮和视图。
3. 读 `Source/AudioDoctorAnalysis.h`：理解每张图背后的数据怎么计算。
4. 读 `Source/AudioDoctorFigureRenderer.h` 的图表入口：理解数据怎么变成论文图。
5. 读 `Source/AudioDoctorJobRunner.h`：理解如何自动生成图和 manifest。
6. 读 `Source/AudioDoctorPluginHost.h`：理解插件如何加载和离线渲染。

