# Audio Doctor 掩蔽/融合图补完需求单

日期：2026-05-08  
需求方：论文组  
目标模块：GOODMETER / Audio Doctor / UI + Figure Renderer + Job Runner

## 1. 这张单子的目的

前一版施工单已经提出 `Critical Band Masking Risk / 临界带遮蔽风险`，并且当前代码里已经出现了相关数据结构、CSV 输出和 `masking_fusion` 图像模板。但是目前实现还没有达到论文组真正需要的形态：

1. 桌面 UI 的视图下拉菜单里没有 `Masking / Fusion`，用户无法在 Audio Doctor 界面中直接查看或导出掩蔽图。
2. 图像渲染当前把 `data.dry / data.wetA / data.wetB` 硬当作 A / B / Mix 使用，和“两个素材 A/B 叠加后形成 Mix”的真实语义不一致。
3. Job Runner 的 CSV/manifest 已经部分支持 `sourceA/sourceB/mixSource`，但图像渲染路径没有完全使用同一套源选择，可能造成图、数据、manifest 不同源。
4. 论文组想要的不是“DRY/WET 对比图”，而是一个更自由的多 stem 聚合/拟合图：用户可以选择 1-3 个输入 stem，观察叠加前的互相遮蔽、能量峰、声像分布，以及经过各自效果链处理后形成的新状态和最终 bounce 状态。

本单目标：把掩蔽/融合图升级为 UI 可用、Job Runner 可复现、图表与 CSV 同源的 `Layer Fit / Fusion View`。临界带遮蔽风险只是该视图下辖的一个分析功能，不是这张图的全部。

## 2. 正确的 Stem / Bus / Bounce 语义

该视图不应硬性绑定 `DRY B -> WET B` 或 `DRY C -> WET C`。用户应该能像小型总线分配器一样自由选择 1-3 个 stem：

- `Stem 1 Source`
- `Stem 2 Source`
- `Stem 3 Source`
- `Bounce Source`

每个 Stem Source 可以从以下对象选择：

- `DRY A`
- `DRY B`
- `DRY C`
- `WET A`
- `WET B`
- `WET C`

`Bounce Source` 默认是：

```text
Auto Bounce Selected Stems
```

也允许用户手动选择 `DRY A/B/C` 或 `WET A/B/C` 中的任意一个已有文件作为 bounce 结果。

核心原则：

- 最多 3 个 stem。
- 只导入 1 个 stem 也能看单层能量/声像。
- 只导入 2 个 stem 就自然变成双源遮蔽/融合分析。
- 导入 3 个 stem 就变成完整多层聚合/拟合分析。
- 不需要单独搞“高级模式”；默认就是最多 3 stem 的自由 bus 模式。

## 2.1 三 Stem / 多 Stem 视角：更贴合真实能量音效制作

论文组进一步明确：真实能量碰撞音效制作中，经常会把七八轨素材先合并或组织成少数功能 stem。Audio Doctor 当前的 `DRY A/B/C` 可以理解为三个 stem，而不一定是三条单独素材。

推荐功能语义：

- `DRY A`：高频/边界/逸散 stem，例如电弧、粒子、空气撕裂、能量边缘。
- `DRY B`：中频/身份/材质 stem，例如金属、咒力纹理、声像主体、共振层。
- `DRY C`：低频/体积/压力 stem，例如低频冲击、爆炸底盘、重心层。

每个 stem 可以单独进入自己的处理链：

- `DRY A -> WET A`：例如 BEAM、空间逸散、调制、延迟或扩散处理。
- `DRY B -> WET B`：例如压缩、饱和、共振滤波、材质强化处理。
- `DRY C -> WET C`：例如低通、低频增强、动态整形或体积层处理。

因此本视图需要支持 1-3 stem 的自由组合。一个典型论文用法是：

```text
Stem 1 = WET A
Stem 2 = WET B
Stem 3 = WET C
Bounce = Auto Bounce WET A+B+C
```

这个模式服务论文第三章“分层构建”和第四章“效果链应用”的连接：它不只是看两个声音有没有遮蔽，而是看三个功能层经过各自效果器处理后，最终是否形成一个层级清楚、又能融合成整体的能量碰撞事件。

### 2.1.2 三 Stem 图形语义

该图不建议叫普通 `Masking`，建议 UI 和 manifest 使用：

```text
Layer Fit / Fusion
```

或：

```text
Stem Fit View
```

图形保持两栏：

- 左图：所选 1-3 个 stem 的叠放状态。
- 右图：所选 stem 自动 bounce 后的成品状态。

左图用于观察三条功能层是否在同一时间窗和同一近似临界带中互相抢占；右图用于观察最终合成后哪些频段被保留、增强、抵消或融合。

三 stem 模式的最低输出不要求显示所有 pairwise 图，但 CSV 至少应能提供：

- `A-B masking risk`
- `A-C masking risk`
- `B-C masking risk`
- 每个时间窗/频带的 `dominantSource`
- 每个时间窗/频带的 `maskedCandidate`
- mix 后的 `mixGainDb`
- mix 后的 `mixLossDb`
- mix 后的 `fusionTendency`

如果图上三座山互相遮挡严重，可以在左图中使用半透明叠层、边缘线、或分层错位；不要为了“好看”把三条 stem 归一化到失去真实能量差异。

## 3. UI 入口需求

在 Audio Doctor 顶部视图下拉菜单中新增：

```text
Layer Fit / Fusion
```

建议 ID 紧接在 `Spatial Image` 后面，例如：

```cpp
viewMode.addItem("Layer Fit / Fusion", 8);
```

进入该视图时，需要显示一组最小控制：

- `Stem 1` 下拉框：默认第一个有内容的通道。
- `Stem 2` 下拉框：默认第二个有内容的通道，可为空。
- `Stem 3` 下拉框：默认第三个有内容的通道，可为空。
- `Bounce` 下拉框：默认 `Auto Bounce Selected Stems`。
- `Figure Type`：`2.5D Terrain / Spatial Image / Critical Band Terrain`
- `Band Scale`：`ERB-like / Bark-like / Log`
- `Integration`：默认 `50 ms`
- `Dominance`：默认 `9 dB`，可选 `6 / 9 / 12 dB`
- `Upward Spread`：默认开
- `Estimated Threshold`：默认关，除非 v2 真正完成

如果只选择一个 stem，UI 可以显示单层能量/声像拟合图，但遮蔽风险相关指标应置灰或显示：

```text
Masking risk needs at least two stems.
```

不要静默替换用户的 stem 选择。

## 4. 图形设计需求

论文组需要的是两栏图，不是普通 DRY/WET 对比。两栏图可以用两种视觉形态切换：

1. `2.5D Terrain`：适合看频带、能量峰、临界带遮蔽风险、mix gain/loss。
2. `Spatial Image`：适合看声像宽度、中侧能量、空间扩散、处理后 stem 是否在空间上聚合或分离。
3. `Critical Band Terrain`：适合看近似临界带内的能量拥挤、遮蔽风险和融合趋势，是最贴近心理声学解释的视图。

### 4.1 左图：selected stems before bounce

左图显示所选 1-3 个 stem 在 bounce 前的状态：

- Stem 1、Stem 2、Stem 3 使用不同颜色或线框。
- Stem 之间进入同一或相邻临界带、且同时高于门限的区域，叠加遮蔽风险染色。
- 遮蔽风险区域建议用紫色、品红、亮白或高亮轮廓，避免和普通能量颜色混淆。
- `2.5D Terrain` 坐标含义：
  - 横轴：时间
  - 纵轴：频率或近似临界带
  - 高度：能量
- `Spatial Image` 坐标含义：
  - 横轴：左-中-右声像分布
  - 纵轴：频率或近似频带
  - 高度/颜色：该频带在声像位置上的能量
- `Critical Band Terrain` 坐标含义：
  - 横轴：时间
  - 纵轴：近似临界带序号或临界带中心频率
  - Z 轴/高度：该时间窗和临界带内的聚合能量
  - 颜色/高亮：dominant stem、masking risk、fusion tendency

图中应显示：

```text
Stem 1: selected source
Stem 2: selected source
Stem 3: selected source
Risk: critical-band overlap + dominance
```

### 4.2 右图：Auto Bounce / fitted result

右图显示所选 stem 合成后的结果：

- 默认显示 `Auto Bounce Selected Stems`。
- 对比各 stem 的预期叠加值，标记：
  - 局部增强：mix 比原主导层更突出。
  - 局部抵消/损失：mix 低于预期叠加能量。
  - 融合倾向：stem 间的分离边界在 mix 中变得不明显。
  - 空间聚合/扩散：在 `Spatial Image` 模式下，观察多 stem 是否聚到中心、向两侧扩散，或在不同频段形成不同声像宽度。
  - 临界带聚合：在 `Critical Band Terrain` 模式下，观察 bounce 后能量是否集中到少数听觉频带，或是否把原本分散的 stem 融合成连续的听觉层。

建议标签：

```text
Bounce: Auto Bounce Selected Stems
Gain / Loss / Fusion tendency / spatial spread
```

不要在图中使用 `inaudible / 听不见` 这类绝对判断。论文侧只会写“遮蔽风险”“融合倾向”“局部细节可能变得不易分辨”。

## 5. 代码接线要求

当前图像渲染中存在类似下面的硬接线：

```cpp
computeMaskingFusionAnalysis(data.dry, data.wetA, data.wetB, data.maskingFusionSettings);
```

这需要改掉。建议方案：

1. 扩展 `FigureData`，增加专用字段：

```cpp
std::array<const Asset*, 3> fitSources { nullptr, nullptr, nullptr };
std::array<juce::String, 3> fitLabels { "Stem 1", "Stem 2", "Stem 3" };
const Asset* fitBounceSource = nullptr;
juce::String fitBounceLabel = "Auto Bounce Selected Stems";
bool fitBounceAuto = true;
juce::String fitFigureType = "terrain"; // terrain / spatial_image / critical_band_terrain
```

2. `drawMaskingFusionFigure` 或新命名的 `drawLayerFitFusionFigure` 只使用这些 fit 专用字段，不再借用 `dry/wetA/wetB`。

3. UI export、preview、Job Runner thesis figure 生成时都走同一套 fit source resolver。

4. 如果 `fitBounceAuto == true`，分析内部使用所选 stem 自动合成；如果用户显式指定 `bounceSource`，则使用指定素材。

5. `manifest`、`CSV`、`PNG` 三者必须记录同一组 stem 和 bounce：

```json
"layerFitFusion": {
  "sources": ["wetA", "wetB", "wetC"],
  "bounceSource": "autoBounce",
  "bounceSourceMode": "linear_sum",
  "figureType": "terrain",
  "algorithmName": "critical_band_masking_risk"
}
```

## 6. Job Runner JSON 建议

最小可用配置：

```json
{
  "view": "layer_fit_fusion",
  "layerFitFusion": {
    "sources": ["wetA", "wetB", "wetC"],
    "bounceSource": "autoBounce",
    "autoBounceSources": ["wetA", "wetB", "wetC"],
    "figureType": "critical_band_terrain",
    "frequencyScale": "erb_like",
    "criticalBandMode": "approximate",
    "integrationTimeMs": 50,
    "gateDbBelowPeak": 48,
    "dominanceThresholdDb": 9,
    "upwardSpread": true,
    "upwardSpreadWeight": 0.35,
    "estimatedMaskedThreshold": false,
    "showPairwiseRisk": true,
    "showDominantSource": true,
    "showFusionTendency": true
  }
}
```

如果不想新增 `layer_fit_fusion` token，也可以先复用 `masking_fusion`，但 manifest 中必须写清楚：

```json
"mode": "layer_fit_fusion"
```

`figureType` 支持：

- `terrain`：输出两个常规时间-频率 2.5D 立体地形图。
- `spatial_image`：输出两个声像分布图。
- `critical_band_terrain`：输出两个时间-临界带-能量 2.5D 立体图，Z 轴为能量，高亮为遮蔽风险或融合趋势。

三种图形使用同一套 stem/bounce 选择和基础分析参数，只改变视觉投影方式。`critical_band_terrain` 需要使用和遮蔽风险计算一致的 ERB-like/Bark-like/log-like 分带，避免图与 CSV 分带不一致。

## 7. CSV / Manifest 输出要求

必须输出：

### `layer_fit_fusion_summary.csv`

至少包括：

- `algorithmName`
- `algorithmVersion`
- `psychoacousticMode`
- `source1FileName`
- `source2FileName`
- `source3FileName`
- `bounceSourceMode`
- `figureType`
- `bandScale`
- `criticalBandMode`
- `integrationTimeMs`
- `dominanceThresholdDb`
- `gateDbBelowPeak`
- `upwardSpread`
- `upwardSpreadWeight`
- `estimatedMaskedThreshold`
- `riskAreaPercent`
- `strongestRiskFrequencyHz`
- `strongestRiskTimeSeconds`
- `strongestRiskBandIndex`
- `meanOverlapDb`
- `meanMixGainDb`
- `meanMixLossDb`

### `layer_fit_fusion_cells.csv`

至少包括：

- `timeStartSeconds`
- `timeEndSeconds`
- `frequencyLowHz`
- `frequencyHighHz`
- `bandIndex`
- `bandScale`
- `energy1Db`
- `energy2Db`
- `energy3Db`
- `mixDb`
- `overlapDb`
- `dominanceDb`
- `maskingRiskIndex`
- `dominantSource`
- `weakerSource`
- `mixGainDb`
- `mixLossDb`
- `estimatedMaskedThresholdDb`
- `maskingMarginDb`

如果 `estimatedMaskedThreshold` 没开启，`estimatedMaskedThresholdDb` 和 `maskingMarginDb` 可以保留字段但填空或默认值，并在 manifest 中写明 v1 模式未使用阈值估计。

额外建议输出：

### `layer_fit_fusion_pairwise.csv`

- `pair`：`1-2 / 1-3 / 2-3`
- `timeStartSeconds`
- `timeEndSeconds`
- `frequencyLowHz`
- `frequencyHighHz`
- `dominantSource`
- `weakerSource`
- `maskingRiskIndex`
- `overlapDb`
- `dominanceDb`

### `layer_fit_fusion_bounce.csv`

- `timeStartSeconds`
- `timeEndSeconds`
- `frequencyLowHz`
- `frequencyHighHz`
- `dominantSourceBeforeMix`
- `mixDb`
- `mixGainDb`
- `mixLossDb`
- `fusionTendency`

## 8. 算法边界说明

这张图的主功能是多 stem 聚合/拟合观察；其中“临界带遮蔽风险/频带重叠风险”只是一个分析层。论文中不能宣称它真实测得人耳遮蔽阈值。

推荐 manifest 继续保留：

```text
Approximate critical-band masking/fusion risk proxy; not a measured psychoacoustic threshold.
```

中文说明建议：

```text
本图基于近似临界带分组、时间窗能量、层间电平差和声像分布计算多 stem 的重叠、融合和遮蔽风险，仅作为信号层面的可视化参考，不等同于主观听阈测量。
```

## 9. 验收标准

施工完成后请至少验证：

1. UI 下拉菜单能看到 `Layer Fit / Fusion`。
2. 只导入 1 个 stem 时能显示单层图，遮蔽风险指标置灰。
3. 导入 2 个 stem 时能自然完成双源遮蔽/融合分析。
4. 导入 3 个 stem 时能完成三 stem 聚合/拟合分析。
5. 用户可以自由选择 Stem 1/2/3 和 Bounce Source，不硬性绑定 DRY/WET 序号。
6. 默认 Bounce 是 `Auto Bounce Selected Stems`，不是 `WET A` 或 `WET B`。
7. 图中明确标注每个 stem 和 bounce 的来源。
8. `Figure Type = terrain` 时，输出两个 2.5D 立体图：左为 selected stems，右为 bounce result。
9. `Figure Type = spatial_image` 时，输出两个声像分布图：左为 selected stems，右为 bounce result。
10. `Figure Type = critical_band_terrain` 时，输出两个时间-临界带-能量立体图：左为 selected stems，右为 bounce result。
11. 左图能看到 1-3 个 stem 的能量峰、重叠状态和遮蔽风险染色。
12. 右图能看到 bounce 结果，并能标记 mix gain/loss、fusion tendency、spatial spread 或 critical-band concentration。
13. 同一 job 导出的 PNG、manifest、summary CSV、cells CSV 使用同一组 stem/bounce。
14. manifest 必须写清 `mode=layer_fit_fusion`、`sources`、`bounceSource`、`figureType`。
15. `./build.sh standalone` 通过。
16. 用一个最小 smoke job 跑 `layer_fit_fusion` 或兼容 token，返回 `status: ok`。
17. 同一组 stem 分别跑 `figureType=terrain`、`figureType=spatial_image` 和 `figureType=critical_band_terrain`，三次均返回 `status: ok`。

## 10. 暂不要求

以下内容不是本次必须项：

- 精确 psychoacoustic masked threshold。
- 主观听音实验。
- 多通道真实空间声场重建。
- 复杂 HRTF 模型。
- 绝对 SPL 校准。

本次优先把 v1 做扎实：1-3 个 stem 在近似临界带、时间窗和声像分布里的重叠、主导、遮蔽风险、合成后增强/抵消/融合倾向。
