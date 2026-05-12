# Audio Doctor Layer Fit / Fusion 返工修正需求单

日期：2026-05-09  
需求方：论文组  
目标模块：GOODMETER / Audio Doctor / Layer Fit / Fusion View  
关联文件：`Source/AudioDoctorComponent.h`、`Source/AudioDoctorFigureRenderer.h`、`Source/AudioDoctorAnalysis.h`、`Source/AudioDoctorJobRunner.h`

## 1. 本单目的

当前 `Layer Fit / Fusion` 已经进入 UI，也有 stem / bounce 选择框和 `Critical Bands / Terrain / Spatial Image` 图形类型下拉框。但论文组验收时发现：当前实现和原需求出现明显偏差。

主要问题：

1. `Figure Type` 下拉框切换到 `Terrain` 或 `Spatial Image` 后，画面并没有切换。
2. 当前 `Critical Bands` 画出来的是二维平面热力图，不是论文组要求的临界带立体图。
3. UI 底部的四个 source 下拉框功能本身可以理解，但缺少明确标签，容易被误读为四个莫名其妙的 dry/wet 选择器。
4. 当有效 stem 数不足时，右侧图会空白，但界面没有清楚说明原因。
5. 当前图像没有形成“左边多 stem，右边 bounce 结果”的直观结构。

本单目标：把 `Layer Fit / Fusion` 从“二维遮蔽热力图草稿”修正为可用于论文图例的声层贴合 / 融合分析视图。

## 2. 名称与中文语义

`Layer Fit / Fusion` 建议在中文说明中称为：

```text
声层贴合 / 融合
```

论文侧可写作：

```text
声层贴合与融合分析
```

不要翻译成“层拟合”。“拟合”容易让读者误以为是数学曲线拟合或模型训练；本功能实际表达的是多层素材在时间、频带、能量和空间上的贴合程度，以及最终能否融合为一个整体声音事件。

## 3. 左右两栏的核心图义

请按最直白的语义实现，避免“叠加前 / 叠加后”这种容易产生歧义的描述。

### 3.1 左栏：三座山

左栏表示：

```text
所选 stem 还没有合成成一个声音之前，但放在同一张图里一起观察。
```

也就是：

- Stem 1 是一座山。
- Stem 2 是一座山。
- Stem 3 是一座山。
- 三座山可以出现在同一个坐标空间里。
- 三座山要保留各自身份色。
- 图上要能看出它们哪里互补、哪里拥挤、哪里可能互相遮蔽。

这不是把三条 stem 预先混成一条再显示；左栏必须能看见 stem 之间的关系。

### 3.2 右栏：一座山

右栏表示：

```text
所选 stem bounce 之后形成的结果。
```

也就是：

- 默认使用 `Auto Bounce Selected Stems`。
- 右栏是一座合成后的结果山。
- 右栏不再强行拆成 A/B/C 三座山。
- 右栏用于观察合成后哪些频带被保留、增强、抵消，整体是否更像一个完整声音事件。

## 4. UI 控件修正

当前底部四个 source 下拉框从代码看应为：

```text
Stem 1 / Stem 2 / Stem 3 / Bounce
```

这个思路可以保留，但 UI 必须让用户一眼看懂。

建议底部控件排列为：

```text
Stem 1: [DRY A]   Stem 2: [DRY B]   Stem 3: [Off]   Bounce: [Auto Bounce]   View: [Critical Band Terrain]
```

最低要求：

1. 四个 source 下拉框必须有可见标签，不要只显示 `DRY A / DRY B / DRY C`。
2. `Bounce` 默认值应为 `Auto Bounce Selected Stems`，不要默认落到某个可能为空的 `DRY C`。
3. `Stem 3` 默认建议为 `Off`，除非第三个有效通道真的存在。
4. 当有效 stem 少于 2 个时，遮蔽 / 融合类指标要置灰或提示：

```text
Layer Fit / Fusion needs at least two selected stems for masking/fusion analysis.
```

5. 如果用户只选一个 stem，可以显示单 stem 形态，但不要输出空白右图让用户猜。

## 5. Figure Type 必须真实切换

当前实现的问题是：`Figure Type` 的 token 虽然传入了 `settings.figureType`，但渲染阶段仍然固定调用同一个二维 panel。请改成真正分支。

期望结构：

```cpp
if (figureType == "critical_band_terrain")
{
    drawLayerFitCriticalBandTerrain(...);
}
else if (figureType == "terrain")
{
    drawLayerFitTerrain(...);
}
else if (figureType == "spatial_image")
{
    drawLayerFitSpatialImage(...);
}
```

不要只在底部文字里显示 `figure spatial_image`，图形本身也必须切换。

## 6. 三种图形的正确形态

### 6.1 Critical Band Terrain / 临界带立体图

这不是平面热力图。

它应该是立体图：

- 横轴：时间。
- 纵轴：近似临界带或临界带中心频率。
- Z 轴 / 山体高度：能量。
- 左栏：1-3 个 stem 的临界带能量山。
- 右栏：bounce 后的一座临界带能量山。

染色与高亮：

- Stem 身份色：区分 Stem 1 / Stem 2 / Stem 3。
- 分频高亮：Low / Mid / High / All Bands。
- 融合区高亮：多个 stem 在相近时间、相近临界带中能量接近。
- 遮蔽风险高亮：某 stem 在同一时间 / 临界带被另一 stem 明显压住。
- 右栏增强 / 抵消染色：bounce 后相对预期叠加的增强或削弱。

### 6.2 Terrain / 普通 2.5D 山峦图

复用现有 `Terrain` 图的视觉语言。

- 左栏：三座 stem 山，保留身份色。
- 右栏：bounce 后一座结果山。
- 坐标语义：
  - 横轴：时间。
  - 纵轴：频率。
  - 高度：能量。
- 支持 `Front High`、`Side High` 等现有视角。
- 支持 `Flip Time`，方便避开瞬态墙。

该图主要服务：

- 多层素材的频率分工。
- 能量峰位置。
- 共振峰 / 山脊观察。
- bounce 后整体轮廓是否更清楚。

### 6.3 Spatial Image / 声像能量分布图

复用现有空间图逻辑，但进入 Layer Fit / Fusion 的左右结构。

- 左栏：所选 stem 的声像分布关系。
- 右栏：bounce 后的声像分布。
- 坐标语义：
  - 横轴：左 / 中 / 右声像位置。
  - 纵轴：频率或频带。
  - 高度 / 颜色：该频带在对应声像位置上的能量。

重点：

- 声像位置和宽度不能混为一谈。
- 声像中心由 L/R 能量平衡决定。
- 声像宽度由 Side/Mid、相关性和 width index 影响。
- 不要把很小的边缘残留归一化成满宽。

## 7. 新染色机制清单

本节只针对 `Layer Fit / Fusion` 新视图生效。不要把这些染色规则直接套到现有的 `Terrain`、`Spatial Image`、`Spectrogram`、`Spectrum`、`Reverb / Space`、`Dynamics` 等视图上。

如果需要复用现有绘图函数，请通过显式参数或新的配置结构开启 Layer Fit 专用 overlay，不要修改现有函数默认行为。

### 7.1 Stem 身份色

用于左栏三座山：

```text
Stem 1：颜色 A
Stem 2：颜色 B
Stem 3：颜色 C
```

颜色只表示“谁是谁”，不是分析结论。

### 7.2 分频高亮

已有 `Low / Mid / High / All Bands` 选择，应继续支持。

要求：

- 分频高亮使用半透明频段罩、边缘线或局部辉光。
- 不要覆盖 stem 身份色。
- 适用于 `Critical Band Terrain` 和 `Terrain`。
- `Spatial Image` 只有在按频段重算声像能量时才启用；否则可以暂时置灰。

### 7.3 融合区高亮

用于左栏。

当多个 stem 在同一或相邻时间窗、同一或相邻临界带里都有明显能量，并且强弱差距不大时，高亮为融合区。

建议视觉：

- 紫色 / 青紫色薄雾。
- 或 stem 山脊之间的透明连接带。

含义：

```text
这些区域更容易被听成同一个复合能量事件。
```

### 7.4 遮蔽风险高亮

用于左栏。

当多个 stem 同时占据相近时间和临界带，但某个 stem 明显更强时，高亮弱 stem 被压住的局部区域。

建议视觉：

- 被压住的 stem 局部加玫红 / 红色描边。
- 不要整张图大面积染红。
- 如果能判断 dominant stem 和 masked candidate，tooltip / manifest / CSV 中写清楚。

### 7.5 Bounce 增强 / 抵消染色

用于右栏。

右栏不再显示三条 stem 的身份，而是显示合成结果相对预期叠加的变化：

- 增强：金色 / 黄绿色。
- 抵消或削弱：蓝紫色。
- 变化不明显：保持普通结果色。

### 7.6 能量峰 / 山脊高亮

适用于所有有时间-频率-能量三维关系的图：

- `Critical Band Terrain`
- `Terrain`

不适合直接套到 `Spatial Image`，因为 Spatial Image 的重点是声像位置和宽度。

要求：

- 标出主要能量峰线。
- 可选标出主要共振峰区域。
- 不要为了高亮把所有局部毛刺都标出来。

## 7.7 染色隔离与兼容性要求

为了避免新表影响旧表，请按以下原则实现：

1. 新增 Layer Fit 专用配置，例如：

```cpp
struct LayerFitOverlayConfig
{
    bool stemIdentityColours = true;
    bool bandHighlight = false;
    bool fusionHighlight = true;
    bool maskingRiskHighlight = true;
    bool bounceGainLossHighlight = true;
    bool ridgeHighlight = true;
};
```

2. 现有 `Terrain` / `Spatial Image` 视图默认不读取 `LayerFitOverlayConfig`。
3. 如果复用现有 terrain 或 spatial image 绘图函数，必须增加参数，例如：

```cpp
drawTerrainFigure(..., const LayerFitOverlayConfig* layerFitOverlay = nullptr);
drawSpatialImageFigure(..., const LayerFitOverlayConfig* layerFitOverlay = nullptr);
```

当 `layerFitOverlay == nullptr` 时，旧图行为必须和现在完全一致。

4. `Band Highlight` 现有功能可以继续保留为全局表控件，但在 Layer Fit 中应以 overlay 方式叠加，不要改变其他图的默认调色板、归一化方式和能量映射。
5. `maskingRiskHighlight`、`fusionHighlight`、`bounceGainLossHighlight` 只能在 `Layer Fit / Fusion` 中生效。
6. 不要为了 Layer Fit 修改现有空间图的 pan / width 映射公式；空间图已有的宽度修正逻辑需要保持稳定。
7. 导出 manifest 中应明确记录这些 overlay 是否开启，例如：

```json
"layerFitOverlay": {
  "stemIdentityColours": true,
  "bandHighlight": "mid",
  "fusionHighlight": true,
  "maskingRiskHighlight": true,
  "bounceGainLossHighlight": true,
  "ridgeHighlight": true
}
```

8. 验收时需要同时检查：

- 普通 `Terrain` 视图没有被新染色污染。
- 普通 `Spatial Image` 视图没有被新染色污染。
- 只有进入 `Layer Fit / Fusion` 后，才出现 stem 身份色、融合区、遮蔽风险、bounce 增强/抵消等专用 overlay。

## 8. 数据与导出要求

UI 和 Job Runner 必须使用同一套数据源选择逻辑。

Manifest 需要记录：

```json
{
  "view": "layer_fit_fusion",
  "figureType": "critical_band_terrain",
  "stemSources": ["DRY A", "DRY B", "Off"],
  "bounceSource": "Auto Bounce Selected Stems",
  "bandMode": "Mid",
  "integrationTimeMs": 50,
  "dominanceThresholdDb": 9,
  "sourceCount": 2
}
```

CSV 至少包含：

- timeStartSeconds
- timeEndSeconds
- bandIndex
- frequencyLowHz
- frequencyHighHz
- energyStem1Db
- energyStem2Db
- energyStem3Db
- mixDb
- dominantSource
- maskedCandidate
- overlapDb
- dominanceDb
- maskingRiskIndex
- fusionTendency
- mixGainDb
- mixLossDb

如果只选一个 stem，`maskingRiskIndex` 和 `fusionTendency` 可以为空或 0，但 manifest 必须说明 `sourceCount = 1`。

## 9. 当前实现的具体偏差

从当前代码观察：

1. `fitFigureType` 有 `Critical Bands / Terrain / Spatial Image` 三个选项。
2. `layerFitFigureTypeToken()` 会返回 `critical_band_terrain / terrain / spatial_image`。
3. `makeFigureDataForExport()` 已经把 `fitFigureType` 写入 `FigureData`。
4. 但 `drawMaskingFusionFigure()` 中仍固定调用 `drawMaskingHeatPanel()` 画左右两栏。
5. `drawMaskingHeatPanel()` 是二维时间-频率热力图，不是立体图，也不是声像图。

因此本次返工重点不是“补一个下拉框”，而是让下拉框真正控制渲染路径。

## 10. 验收标准

### 10.1 UI 验收

1. 进入 `Layer Fit / Fusion` 后，底部控件能清楚看见：

```text
Stem 1 / Stem 2 / Stem 3 / Bounce / View
```

2. 默认选择不应造成右侧空白。
3. 至少两个有效 stem 时，能正常显示遮蔽 / 融合相关图形。
4. 一个 stem 时有明确提示，不误导用户。

### 10.2 图形验收

同一组 stem 下切换：

- `Critical Band Terrain`
- `Terrain`
- `Spatial Image`

三种视图必须明显不同。

其中：

- `Critical Band Terrain` 必须是立体图，不是平面热力图。
- `Terrain` 必须复用现有山峦图视觉。
- `Spatial Image` 必须复用现有声像分布视觉。

### 10.3 数据验收

1. UI 导出和 Job Runner 导出的 manifest / CSV 口径一致。
2. 图中的 source label 与 manifest 中的 stemSources 一致。
3. 右栏如果是 Auto Bounce，manifest 不能写成某个 DRY/WET 手动源。
4. CSV 中 sourceCount 与图中实际 stem 数一致。

### 10.4 论文验收

论文组需要能用这张图表达：

```text
左边三座山：多层素材在合成前的贴合、拥挤、融合和遮蔽风险。
右边一座山：合成后形成的整体能量结构。
```

如果图上不能一眼看出这件事，就还没有完成。

## 11. 2026-05-09 二次验收补充问题

施工队新版本已经修复了“图形类型切换没有反应”的大问题，也让 `Critical Band Terrain` 初步立体化。但是二次验收仍有几个缺口，需要继续补。

### 11.1 Layer Fit 缺少分频显示按钮

当前新表没有接入分频显示控制。论文组需要在 `Layer Fit / Fusion` 中也能切换：

```text
Bands Off / Low / Mid / High / All Bands
```

但注意：Layer Fit 左栏已有 1-3 个 stem 身份色，不能再用大面积分频染色覆盖 stem 颜色。

Layer Fit 中的分频显示建议改成：

- 频段区域描边。
- 半透明频段罩。
- 频段边界线。
- 频段标签。

不要把山体主色改成低频色、中频色、高频色。  
身份色优先，分频只做区域提示。

### 11.2 Spatial Image 不能拆成多个小表

当前 `Spatial Image` 模式下，左栏仍然是“加一个 stem 就多一个分表”。这不符合 Layer Fit 的目标。

Layer Fit 的 Spatial Image 应该是：

```text
左栏：所选 1-3 个 stem 生活在同一张声像图里。
右栏：bounce 后的一张结果声像图。
```

不要把 Stem 1、Stem 2、Stem 3 纵向拆成三个小图。那样看不到它们在同一个声像空间里是否互相重叠、错开或融合。

实现建议：

- 左栏共用同一个 L-C-R / 频率坐标。
- Stem 1、Stem 2、Stem 3 使用身份色叠加。
- 相同区域的重叠可以通过透明度叠加、边缘线或 overlay 表达。
- 右栏只显示 bounce 结果，不再拆 stem。

### 11.3 Spatial Image 需要时间进度条

Layer Fit 的 `Spatial Image` 模式也需要时间进度条。

原因：

- 声像分布是随时间变化的。
- 论文图可能需要截取起音、扩散、尾音三个时间点。
- 如果没有时间点控制，图就无法稳定复现论文中的某一帧。

要求：

- 复用普通 Spatial Image 的时间 slider。
- 进入 `Layer Fit / Fusion` 且 `View = Spatial Image` 时显示。
- manifest/export 中记录当前时间点和窗口范围。

### 11.4 Bounce 结果色不能和 Stem 2 撞色

当前右栏 bounce 颜色与第二个 stem 都偏浅黄色，容易混淆。

建议：

- Stem 1：青色系。
- Stem 2：黄色系。
- Stem 3：品红 / 紫色系。
- Bounce：单独的结果色，例如白金色、浅灰金、或绿色系。

Bounce 是“合成结果”，不应和任意一个 stem 看起来像同一个对象。

### 11.5 Critical Band Terrain 不要停留在“马赛克砖块”

临界带本身是分带概念，图上可以有带状边界，但不等于必须画成难读的方片马赛克。

当前 `Critical Band Terrain` 的块状面片可以说明“按临界带采样”，但论文图例需要更直观。建议改成以下任一方案：

1. **带状连续山体**：仍以临界带为纵向采样单位，但相邻时间窗和相邻 band 之间做视觉连接，让它像一座有台阶的山，而不是散开的砖块。
2. **阶梯山体**：保留 band 台阶，但减少黑缝和过度切割，用连续半透明面连接。
3. **带边界线 + 连续填充**：山体连续，临界带边界用细线标出。

也就是说：

```text
临界带可以是离散采样；视觉表达不必是碎砖马赛克。
```

### 11.6 小圆点不适合作为主要遮蔽/融合表达

当前 risk / fusion 用红紫小圆点表示，太抽象，也不够好看。论文组不希望把小圆点作为主要视觉机制。

建议改成：

- 遮蔽风险：对被压住的 stem 局部山体加玫红描边或局部半透明罩。
- 融合区域：在两个 stem 山体交汇处加紫色 / 青紫色薄雾或连接带。
- 高风险点可以保留很小的中心标记，但不能成为主要表达。

不要满图散落红点。  
读者应能看懂“哪条 stem 的哪一段被压住”，而不是看到一堆红点。

### 11.7 临界带计算与显示口径

后续可以把 `ERB-like` 和 `Bark-like` 做成明确模式。  
如果启用 Bark-like，可以参考常见 Bark 尺度近似公式和临界带宽近似公式：

```text
z = 13 * arctan(0.76 * f/kHz) + 3.5 * arctan((f/7.5kHz)^2)
Δf = 25 + 75 * (1 + 1.4 * (f/kHz)^2)^0.69
```

图形层面建议：

- 允许 `Bark 24 bands` 模式。
- 允许 `ERB-like dense bands` 模式。
- 允许 `Log bands` 工程预览模式。

如果选择 `Bark 24 bands`，纵轴可以显示：

```text
B1 / B5 / B10 / B15 / B20 / B24
```

或显示中心频率。  
但无论采用 24 段还是更密的 ERB-like 分带，都需要让图读起来像连续能量形态，而不是只有表格化方块。
