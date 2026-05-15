# Audio Doctor 空间热力图与 Enrage 证据图需求单

写给 GOODMETER 施工队。本文只从论文侧提出需求，不限定具体实现方式。  
日期：2026-05-07  
需求方：论文组 / Audio Doctor 论文图例工作流

## 0. 背景

论文第四章已经能用 Spectrum、Group Delay、Dynamics、Reverb / Space 等图解释频谱、动态、群延时和混响尾音，但“空间塑形”目前还不够直观。现有 `Reverb / Space` 能给出 EDC、DRR、相关系数、Side/Mid 等指标，但这些多为数字或曲线；读者需要先理解指标，再反推声音空间变化。

论文真正需要的是一张能一眼看懂的图：处理前的素材空间很窄，处理后声音向两侧膨胀，尾音或高频残留进入更宽的空间区域。  

因此，空间分析不应优先做成“左/右 RMS 包络 + Mid/Side RMS 包络 + 相关系数曲线”的组合图。这类曲线适合工程排查和实时监测，但在论文正文中可读性不如 2.5D 热力图。论文侧更需要类似 Insight 一类空间表的直观表达：用一张图把空间宽度、能量强度和时间变化同时显示出来。

## 1. 核心需求：Spatial Energy Heatmap

新增一种论文图模板：

`Spatial Energy Heatmap / 空间能量热力图`

建议接入位置：

- Audio Doctor 视图按钮：可作为 `Spectrogram A/B/C` 的子模式或相邻模式；
- Thesis Figure / Job Runner 模板：新增 `spatial_heatmap`；
- `Reverb / Space` 或 `reverb_space_combo` 可调用同一套热力图渲染能力。

### 图像含义

空间热力图不是普通频谱图，也不是单纯 M/S 曲线。它需要把以下信息合成到同一张图中：

- 横轴：时间；
- 纵轴：频率；
- 明暗或高度感：该时间-频率单元的能量强度；
- 色相、扩散宽度或空间形态：该时间-频率单元的空间宽度，建议基于 Side/Mid、L/R 差异或局部相关性计算。

论文读者应该能直接从图上看出：

- DRY 素材是否集中在中间；
- WET 素材是否向两侧扩散；
- 扩散发生在撞击瞬间、尾音阶段，还是某些频段；
- 处理后是否只是“变响”，还是空间结构真的改变。

### 显示数量

热力图区域最多显示两张图：

- 0 个有效素材：显示空态提示；
- 1 个有效素材：单图铺满；
- 2 个有效素材：左右并排对比；
- 不建议显示 3 张图，论文版面过挤，读者难以比较。

默认论文用法：

- 左图：DRY 基准素材；
- 右图：WET 处理素材。

注意：这里的“左图/右图”是排版位置，不是 Enrage 的左声道/右声道。

## 2. Enrage 证据图工作流

BOOM Library Enrage 的价值在于它不是简单的宽度插件，而是模块化效果链工具。它的 `St Split / St Merge` 可以把左右声道拆成两条处理路径，`M/S Split / M/S Merge` 可以把 Mid 和 Side 拆成两条处理路径。这非常适合论文第四章讨论“空间不是最后加宽，而是信号流中的结构分配”。

Enrage 不应被浪费成“左声道当 DRY、右声道当 WET”。DRY 应始终是未处理素材；Enrage 应作为 WET 链完整使用。

### 图组 A：DRY vs Enrage L/R Split

目的：证明左右声道可以被当作两条空间处理路径分别塑形。

建议流程：

- 输入：一条真实能量冲击或魔法冲击素材，作为 DRY；
- WET：载入 Enrage；
- Enrage 内部使用 `St Split / St Merge`；
- L 链和 R 链分别加入不同处理，例如短延迟、滤波、调制、失真残留、混响尾音等；
- 导出一张 `Spatial Energy Heatmap`：左图 DRY，右图 Enrage L/R Split WET；
- 同时导出指标和 manifest。

正文可用于说明：

- 处理前能量主要集中在中心；
- 处理后侧向能量、尾音或高频残留向两侧展开；
- 左右路径分别处理使空间扩散不只是整体加宽，而是由信号路径分配产生。

### 图组 B：DRY vs Enrage M/S Split

目的：证明中心命中点和侧向空间残留可以分开管理。

建议流程：

- 输入：同一条 DRY 素材；
- WET：载入 Enrage；
- Enrage 内部使用 `M/S Split / M/S Merge`；
- Mid 链尽量保留主体冲击、低频压力或命中边界；
- Side 链加入扩散、调制、滤波、混响尾音或高频残留；
- 导出一张 `Spatial Energy Heatmap`：左图 DRY，右图 Enrage M/S Split WET；
- 同时导出指标和 manifest。

正文可用于说明：

- Mid 保持撞击点稳定；
- Side 承担空间扩散和尾音延展；
- 这种处理方式比简单 stereo width 更能解释“主体不散、空间变大”的设计策略。

## 3. 可复用现有能力

GOODMETER 现有代码里已经有不少可复用基础，不需要从零发明：

- `StereoImageComponent` 已有 Lissajous / goniometer 思路，适合借鉴空间形态表达；
- `PhaseCorrelationComponent` 已有相关性表的计算和视觉经验；
- `PluginProcessor` 已有 Mid/Side RMS 与 phase correlation 实时指标；
- Audio Doctor 的 `ReverbSpaceMetrics` 已经有 `stereoCorrelation` 和 `sideToMidDb`；
- Audio Doctor 已有 spectrogram 图像计算与渲染管线，可复用 FFT、时间-频率切片、浅色/深色图像渲染逻辑；
- Job Runner 已能导出 manifest、插件参数快照、素材基准和图表 PNG。

施工重点不是重新做一个实时表，而是把这些能力离线化、论文图化：

- 输入来自 Audio Doctor 的 DRY/WET buffer；
- 输出高分辨率 PNG；
- 输出 CSV/manifest；
- 图中文字适合论文，不出现实时 UI 上的装饰信息；
- 能稳定复现。

## 4. 建议输出数据

空间热力图必须配套导出数据。正文不一定全部使用，但附录和复核需要能追溯。

建议每个 asset 导出：

- `durationSeconds`
- `sampleRate`
- `channels`
- `peakDb`
- `rmsDb`
- `crestDb`
- `stereoCorrelationMean`
- `stereoCorrelationMin`
- `sideToMidDbMean`
- `sideToMidDbTail`
- `leftRmsDb`
- `rightRmsDb`
- `lrRmsDiffDb`
- `tailStartSeconds`
- `tailEndSeconds`

建议空间热力图自身导出：

- 每个 time-frequency cell 的时间范围；
- 频率范围；
- energyDb；
- sideToMidDb 或 widthIndex；
- lrBalanceDb；
- correlation，如计算成本允许。

如果 CSV 体量太大，可以至少导出降采样后的 summary CSV，同时 manifest 标注原图计算参数。

## 5. Manifest 要求

manifest 必须能让论文组和其他 AI 只读文件就知道图从哪里来。

至少记录：

- 图表模板：`spatial_heatmap`；
- 显示对象：例如 `DRY A`、`WET A`；
- 源素材路径、文件名、裁剪起止、采样率、声道数；
- 插件链：Enrage 版本、插件格式、插件路径；
- 插件参数快照：可读参数名、display value、normalized value；
- Enrage 路由说明：`LR Split` 或 `M/S Split`，如果无法自动识别，允许 job JSON 手写 `processingNote`；
- 图表计算参数：FFT size、hop size、频率范围、动态范围、空间宽度映射方式；
- 核心指标摘要：Side/Mid、Correlation、L/R 差值、尾音段空间指标。

注意：论文正式附录里不要写“Job Runner 自动跑图”这类容易被误解为偷懒的表述。内部 manifest 可以如实记录，但图表说明文案建议写成“数据由 GOODMETER Audio Doctor 导出”。

## 6. UI / Job Runner 需求

### Audio Doctor UI

新增或扩展一个图表模式：

- 名称建议：`Spatial Heatmap`；
- 能选择最多两个显示对象；
- 默认使用当前 Display Slot 中前两个非空对象；
- 支持 DRY/WET 对照；
- 支持导出当前图。

### Job Runner

新增 figure view：

```json
{
  "figure": {
    "view": "spatial_heatmap",
    "slots": ["DRY A", "WET A"],
    "title": "Spatial Energy Heatmap - Enrage M/S Split",
    "export": {
      "preset": "ui_light"
    }
  }
}
```

如果 Enrage 路由不能自动识别，允许手写：

```json
{
  "processingNote": "Enrage M/S Split: Mid keeps impact body; Side adds modulation and tail space."
}
```

论文当前不建议使用 `academic_light` 作为默认。为了与正文已用图保持一致，优先沿用当前正文图配色或 `ui_light`。

## 7. 图形设计要求

空间热力图的目标是“直观”，不是把所有指标都塞进图里。

建议版式：

- 图标题在上方；
- 两张热力图并排；
- 每张图左上角标注 `DRY` / `Enrage L/R Split` / `Enrage M/S Split`；
- 图下方只放 3-5 个核心指标；
- 右侧或底部可放一条简洁色标；
- 不要塞入长参数列表，插件参数进 manifest 和附录表。

指标建议：

- `S/M mean`
- `S/M tail`
- `Corr mean`
- `Corr min`
- `L/R diff`

视觉要求：

- 50% 缩放下仍能看清；
- 坐标和标签不压图；
- 色彩能区分“中心集中”和“侧向扩散”；
- 打印成 PDF 后不糊；
- 不要出现实时 UI 里的动态装饰、拟物外壳或花哨控件。

## 8. 论文接入建议

正文只补一小段，不要大改结构：

- 第四章空间处理小节补 Enrage 作为“分路空间塑形”案例；
- 正文放 1 张图，优先使用 `DRY vs Enrage M/S Split`；
- `DRY vs Enrage L/R Split` 可放附录；
- 附录记录参数快照、素材基准和核心指标。

正文想表达的核心不是“Enrage 很强”，而是：

> 空间处理可以发生在信号流内部。通过 L/R 或 M/S 分路，主体冲击、侧向扩散和尾音残留能够被分配到不同处理路径，从而使动画能量碰撞音效在保持命中点清晰的同时获得更大的空间规模。

## 9. 验收标准

P0 验收：

- 能在 Audio Doctor 或 Job Runner 输出 `Spatial Energy Heatmap`；
- 能显示 1 或 2 个素材，2 个素材并排时不挤、不裁切；
- 能跑 `DRY vs Enrage M/S Split`；
- 能跑 `DRY vs Enrage L/R Split`；
- PNG 可直接放进论文；
- manifest 记录素材、插件、参数、图表计算口径；
- CSV 或 summary 数据能支撑附录说明；
- 图中能明显看出 DRY 与 WET 的空间差异。

P1 验收：

- 支持自定义频率范围和动态范围；
- 支持只分析尾音段；
- 支持将空间热力图嵌入 `Reverb Space Combo`；
- 支持将 goniometer density 作为可选附图，而不是默认正文图。

## 10. 当前优先级判断

这项功能比再增加普通曲线图更值。论文最后阶段，空间章节最需要解决的是“读者能不能一眼看懂空间处理为什么有效”。  

曲线组合图适合工程师复核；2.5D 空间热力图适合论文正文证明。  

建议施工顺序：

1. 先做 `spatial_heatmap` 离线图；
2. 接 Job Runner；
3. 跑 Enrage M/S Split 图；
4. 跑 Enrage L/R Split 图；
5. 再考虑是否补 goniometer density 或实时 UI 联动。

