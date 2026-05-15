# Audio Doctor 需求说明

本文档给 GOODMETER 后续施工对话使用，重点说明论文阶段最需要什么。这里不指定最终工程实现方式，具体架构、类名、线程模型、UI 框架细节由掌握 GOODMETER 项目全貌的施工方决定。

## 1. 定位

Audio Doctor 不建议先做成新的独立 app，也不建议第一阶段做成插件宿主。

更合适的定位是：GOODMETER macOS standalone 里的一个新技能页，和现有 Audio Lab、录音、倒录、视频音频提取等能力平级。

现有 GOODMETER 的核心是“桌面音频助手”：

- 八张表负责实时监测系统或插件音频输出。
- Audio Lab 负责离线清理、提取、修复类操作。
- 录音、倒录、视频提取负责素材捕获。
- Audio Doctor 负责离线素材对比、效果链影响分析、论文级图例导出、AI 可调用的数据接口。

一句话目标：

> Audio Doctor 是 GOODMETER 的“声音证据实验室”：导入或生成素材，处理并对比 Dry/Wet 或 A/B，输出漂亮、易懂、可复现的图。

## 2. 当前论文最急需解决的问题

论文第四章需要图例支撑“效果器链如何改变能量、频谱、瞬态和空间感”。当前 Plugin Doctor 类工具的问题是：

- 只能围绕插件测试，不能直接把两份素材作为 A/B 对象比较。
- 不能方便生成标准测试信号，尤其是瞬态信号。
- 图偏工程调试感，难以直接放进论文。
- AI 无法稳定地从软件里拿到素材、分析结果和图像，只能靠人工截图。

因此 Audio Doctor 第一阶段的重点不是做最多的功能，而是快速补齐论文图例生产链路。

## 3. 第一阶段 MVP 优先级

### P0: 双素材 A/B 导入与对齐

必须允许用户加载两份不同音频素材：

- A: 原始素材或处理前素材。
- B: 处理后素材，或另一个可对比素材。

基础能力：

- 自动读取采样率、位深、声道、时长、峰值、LUFS 或 RMS 等元数据。
- 支持统一采样率后的离线分析。
- 支持手动选择时间区间。
- 支持瞬态自动定位，尤其用于碰撞、打击、冲击类素材。
- 支持 A/B 同一视窗显示，而不是两个孤立截图。

论文价值：

- 能直接说明“处理前后发生了什么变化”。
- 能避免论文图例看起来像插件说明书。

### P0: 内置信号发生器

必须提供标准测试信号生成，不依赖用户外部找素材。

最急需的信号：

- 100Hz 正弦波。
- 1kHz 正弦波。
- 白噪声。
- 粉噪声。
- click / impulse。
- 短瞬态 burst。
- 扫频 sweep。

论文价值：

- 饱和章节需要 100Hz 正弦波观察谐波生成。
- EQ/共振章节需要噪声观察频谱塑形。
- Disperser / all-pass / 群延时章节需要瞬态信号观察时间扩散。

### P0: 论文级图例导出

必须支持把分析结果导出为可直接放入论文的图。

导出要求：

- PNG: 300dpi 或 600dpi。
- 可选 PDF/SVG。
- 白底优先，适合论文打印。
- 中文标签。
- 统一字体、统一线宽、统一配色。
- 图中自带必要坐标轴单位，例如 Hz、dB、ms、s。
- 支持隐藏过多工程参数，只保留论文解释所需信息。

建议提供预设：

- `Thesis-Light-A4`
- `Thesis-Compact`
- `Presentation-Dark`
- `Debug-Detailed`

### P0: Disperser / Group Delay 专用图例模板

这是当前第四章最容易被老师看不懂、但又最需要图支撑的一类内容。

不要把原始 phase wrap 相位图作为主要图例。非技术读者很难读。

最舒服、最适合论文的组合图是：

1. 上半部分：处理前后瞬态波形或能量包络对比。
2. 下半部分：Group Delay 曲线。
3. 可选辅助：蓝色能量瀑布图或声谱图。

具体图形要求：

- 横轴频率用 log scale，范围 20Hz-20kHz。
- 纵轴群延时用 ms。
- 曲线做适度平滑，避免工程噪声太多。
- 用颜色区分 Dry 和 Wet，或只显示 Wet 的群延时变化。
- 图上可以标注“低频延迟较明显”“瞬态边缘被展开”等短注释。
- 不要把图做成插件参数截图。

论文要表达的是：

> Disperser / all-pass 类处理不一定显著改变频谱能量，却会让不同频率成分在时间上错开，从而改变瞬态质感和能量冲击的边缘。

### P1: Dry/Wet 效果链快照

第一阶段不强制做完整 AU/VST3 插件宿主，但最好有内部处理模块，能快速生成论文图。

建议先做内部模块：

- Gain。
- Saturation。
- EQ / filter。
- All-pass 或 Disperser-like。
- Compressor 简化版。
- Reverb 简化版。
- Mid/Side gain 或 width。

每次处理后保存快照：

- 原始素材。
- 处理链参数。
- 处理后素材。
- 分析图。
- 导出图。

论文价值：

- 可以展示“链式处理不是一次性效果，而是多阶段能量塑形”。

### P1: AI 可调用接口

后续希望 Claude、Codex、Gemini、DeepSeek 等模型可以通过稳定接口调用 Audio Doctor。

第一阶段可以先不做完整 MCP，但数据结构要为它预留。

理想工具接口：

- `import_audio(path)`
- `generate_signal(type, params)`
- `list_assets()`
- `analyze_asset(asset_id, views)`
- `compare_assets(a_id, b_id, views)`
- `apply_chain(asset_id, chain_spec)`
- `export_figure(view_id, preset)`
- `get_figure_manifest(figure_id)`

注意：

- AI 不应该靠模拟鼠标点击 UI 来完成核心分析。
- AI 应该调用稳定 API，拿到结构化数据和图像路径。
- 每张图都应该有 manifest，记录素材、时间区间、处理参数、分析设置和导出设置。

## 4. 第四章图例需求清单

### 4.2.1 饱和与谐波添加

需要图例：

- 100Hz 正弦波处理前后频谱对比。
- 标出基频、2次谐波、3次谐波等。
- 白噪声或宽频素材经过饱和前后的频谱密度变化。
- 可选：波形削顶前后对比。

图例目的：

- 说明饱和不是简单变大声，而是引入谐波和频谱粘合。

优先实现视图：

- Harmonic spectrum。
- Before/after spectrum overlay。
- Waveform clipping view。

### 4.2.2 Disperser / 群延时 / 瞬态扩散

需要图例：

- 瞬态素材处理前后波形。
- 瞬态能量包络对比。
- Group Delay 曲线。
- 可选蓝色能量瀑布图。

图例目的：

- 说明频谱变化可能不大，但瞬态能量在时间上被铺开。

优先实现视图：

- Transient envelope comparison。
- Group delay plot。
- Spectrogram / waterfall。

最重要的视觉原则：

- 先让读者看见“尖锐瞬态变宽”，再给出群延时曲线解释原因。

### 4.2.3 EQ / 共振 / 频谱塑形

需要图例：

- 噪声通过 EQ 或共振滤波后的频谱峰值。
- 原素材与处理后素材的频谱叠加。
- 可选：滤波器响应曲线。

图例目的：

- 说明效果器链如何给能量碰撞素材制造材料感、金属感、空气感或体积感。

优先实现视图：

- Frequency response。
- Spectrum overlay。
- Peak annotation。

### 4.3 混响、空间与层次

需要图例：

- 直达声与混响尾音的能量时间图。
- 处理前后 stereo image 或 M/S 能量变化。
- 可选：频谱随时间衰减图。

图例目的：

- 说明空间类处理如何改变冲击声的纵深、包围感和尾部能量。

优先实现视图：

- Energy decay curve。
- Mid/Side spectrum。
- Stereo image comparison。

### 4.4 多轮重采样与链式处理

需要图例：

- Stage 0 / Stage 1 / Stage 2 / Stage 3 的频谱或声谱图对比。
- 每一阶段的峰值、LUFS、crest factor、PSR 简表。
- 可选：处理链流程图。

图例目的：

- 说明动画声音设计中“能量碰撞”不是单插件结果，而是多轮处理、重采样和再塑形的结果。

优先实现视图：

- Processing snapshots。
- Multi-stage spectrum。
- Metrics table。

## 5. UI 需求建议

Audio Doctor 页面可以仿照 Audio Lab 做成一个独立技能页，而不是塞进八张表主界面。

建议布局：

- 左侧：素材库、生成信号、处理链快照。
- 中间：主图区域，支持 A/B、overlay、split view。
- 右侧：图例设置、导出、AI 摘要、manifest。
- 底部：时间线、选区、播放、loop、Dry/Wet。

第一阶段最重要的 UI 不是炫，而是：

- 快速导入。
- 快速截取。
- 快速对比。
- 快速导出漂亮图。

## 6. 图形审美要求

论文图例应尽量避免“工程软件截图感”。

建议：

- 白底优先。
- 网格线浅灰。
- 曲线颜色克制，推荐蓝、橙、灰三色体系。
- 坐标轴和标题中文化。
- 只显示关键参数，隐藏调试细节。
- 图例标题要能直接说明现象，例如“Disperser 处理前后瞬态能量扩散对比”。
- 图中注释要短，不写长段解释。

不建议：

- 大面积黑底霓虹风，除非是展示软件界面而不是论文图。
- 原始相位缠绕图直接入论文。
- 只贴插件参数截图，不给声音结果图。
- 图上堆太多曲线，导致老师看不出重点。

## 7. 可复用 GOODMETER 能力

施工方可优先评估复用以下能力：

- 现有 standalone 外壳和技能页入口。
- Audio Lab 的文件导入、离线处理流程。
- Spectrum analyzer。
- Spectrogram / waterfall 类显示。
- LUFS、True Peak、PSR、crest factor 等指标。
- Phase / stereo image / Mid-Side 相关分析。
- 录音、倒录、视频提取产生的素材入口。
- 现有 LookAndFeel、MeterCardComponent、图形组件风格。

但 Audio Doctor 的核心分析最好不要硬塞进实时 meter 的音频线程。它本质上是离线分析与图例生成工作台。

## 8. 暂不优先事项

以下功能有价值，但不建议第一阶段抢做：

- 完整 AU/VST3 插件宿主。
- iOS 端同步。
- 云端账号和素材库。
- 自动写论文正文。
- 复杂机器学习音色识别。
- 多人协作项目管理。

原因：

- 当前最急的是论文第四章图例产出。
- 插件宿主涉及崩溃隔离、扫描、授权、路径、UI 嵌入等问题，容易拖慢 MVP。
- 先把导入、生成、分析、导出链路跑通，后续再扩展更稳。

## 9. 验收标准

第一阶段可按以下标准验收：

1. 能在 GOODMETER standalone 中打开 Audio Doctor 页面。
2. 能导入两份音频并进行 A/B 对比。
3. 能生成 100Hz sine、white noise、click / transient burst。
4. 能输出至少三类论文图：
   - 饱和前后谐波图。
   - 瞬态处理前后能量/波形对比图。
   - Group Delay 曲线或 Disperser 解释图。
5. 能导出白底中文论文图。
6. 每张图能保存对应 manifest，记录素材、区间、处理参数和导出设置。
7. 不破坏现有 GOODMETER 八张表、Audio Lab、录音、倒录等功能。

## 10. 给施工方的最终重点

请把 Audio Doctor 当作 GOODMETER 桌面端的新技能，而不是独立产品重做。

论文阶段最需要的不是完整插件宿主，而是：

- 双素材 A/B。
- 标准测试信号生成。
- Dry/Wet 对比。
- Disperser / Group Delay 可读图。
- 饱和谐波可读图。
- 漂亮、统一、可复现的论文图导出。
- 为后续 AI/MCP 调用预留稳定数据结构。

只要这条链路打通，Audio Doctor 就已经能明显补上当前论文第四章最缺的证据图。
