# Audio Doctor 第二章图例补强需求：分频高亮与掩蔽/融合图

日期：2026-05-08  
提出方：论文组  
目标：为论文第二章“概念与感知基础”补充更直观的声学/心理声学证据图，避免只用概念文字讨论频率感知、素材构成和掩蔽融合。

## 1. 为什么需要这次补强

当前论文第二章已经写到频率、响度、掩蔽、谐波、时间包络和空间线索，但图例主要集中在第四章效果器链。第二章如果只有等响曲线示意和文字概念，读者仍需要自行想象“火、电、水等素材为什么听起来不同”“多层素材为什么会互相遮蔽或融合”。这会削弱论文“为什么这样做有效”的根基。

本次新增图例不追求复杂数量，而是补三类基础证据：

1. 低/中/高频在立体声像或频谱地形中的可视化分布。
2. 火焰、电流、水流等典型素材的频谱/时频谱差异。
3. 两个素材叠加时的遮蔽风险与融合区域。

## 1.1 已核对理论依据与论文转化边界

本节已依据论文组本地提供的两本书进行工作性整理，供施工队确定图表算法方向；论文正文最终引用仍由论文组按学校格式补齐页码和参考文献条目。

1. Tim Ziemer, *Psychoacoustic Music Sound Field Synthesis: Creating Spaciousness for Composition, Performance, Acoustics and Perception*。这本书对本次需求最关键：听觉系统不是逐点读取完整 FFT 频谱，而是在时间、频率和空间上以有限分辨率组织声音。书中关于 critical bands、masking、auditory integration time、interaural coherence、precedence effect、auditory scene analysis 的讨论，可以转化为 Audio Doctor 的分频高亮、遮蔽风险、空间声像和多时间点图例。
2. J. H. Ginsberg, *Acoustics: A Textbook for Engineers and Physicists, Volume 1*。这本书提供物理声学底座：声压扰动、RMS/均方声压、频带能量、分贝计算、波动方程、平面波/球面波和声传播等。它适合支撑 Audio Doctor 输出 RMS、Crest Factor、频带能量和时频图时的“信号代理指标”口径。
3. 施工队不需要在软件界面里写书名，也不要把本需求单里的理论句子直接进入 UI。软件只需要导出足够清楚的图、数据、算法口径和 manifest；论文组会在正文中把这些图转换为学术表述。

论文正文的转化方式建议：

- 不把音乐研究直接等同于动画音效研究。
- 只借用底层听觉和声学机制：频率、响度、谐波、共振、掩蔽、双耳线索、听觉场景组织。
- 正文可写成“这些机制同样作用于能量碰撞音效的分层构建”，不要写成“音乐理论证明了动画音效一定如此”。
- GOODMETER 图例只承担“可视化解释”和“辅助分析”功能，不宣称替代听感实验。
- 施工队不要把本节书目内容写进软件界面或 manifest；这些是论文组的理论线索，不是软件实测结果。

## 2. 术语边界

掩蔽不要写成简单的“低频一定掩蔽高频”。更准确的表述是：

- 同时掩蔽通常在频率接近、时间重叠、响度差距明显时更强。
- 低频对较高频率的遮蔽更容易向上扩展，但不是唯一方向。
- 在声音设计中，掩蔽既可能造成细节丢失，也可能帮助多层素材融合成一个事件。

因此图表命名建议使用：

- `Masking / Fusion View`
- 中文界面可写“掩蔽与融合”
- 正文图注可写“频段重叠与遮蔽风险示意”

避免使用“精确心理声学阈值”“人耳真实遮蔽量”等过强表述，除非后续真的实现 ISO/ERB/Bark 级别的模型。

## 3. Spatial Image / Spatial Terrain 分频高亮

### 3.1 需求

现有 `Spatial Image` 和 `Spatial Terrain` 已经能很好地显示声像/空间扩散，但第二章需要看“低频、中频、高频分别在哪里”。建议新增分频高亮功能：

- 支持低频/中频/高频三段可配置高亮。
- 默认频段：
  - Low：20-200 Hz
  - Mid：200-2000 Hz
  - High：2000-20000 Hz
- 支持只高亮一个频段，其他频段降亮度但仍保留轮廓。
- 支持同时高亮多个频段。
- 图中需要有简洁 legend：Low / Mid / High。
- JobRunner 需要支持 JSON 参数控制。

### 3.2 建议 JSON

```json
{
  "view": "spatial_impression",
  "bandHighlight": {
    "enabled": true,
    "bands": [
      { "id": "low", "label": "Low", "minHz": 20, "maxHz": 200, "color": "#4EA3FF" },
      { "id": "mid", "label": "Mid", "minHz": 200, "maxHz": 2000, "color": "#F3C64E" },
      { "id": "high", "label": "High", "minHz": 2000, "maxHz": 20000, "color": "#F05A7E" }
    ],
    "activeBands": ["low", "mid", "high"],
    "dimInactiveBands": true,
    "inactiveAlpha": 0.22,
    "overlayAlpha": 0.55
  }
}
```

### 3.3 论文用途

- 第二章 2.3.1：说明低频不等于自动“重量”，需要和响度包络、空间规模共同组织。
- 第三章 3.2-3.5：解释体积、材质、运动、边界各自常用的频段线索。
- 第四章：辅助说明效果器链如何改变不同频段的空间残留。

## 4. 火/电/水素材构成图

### 4.1 需求

新增一张组合图，比较三类真实素材：

- 火焰：宽带噪声、持续抖动、低中频厚度和不稳定高频颗粒。
- 电流：短促高频边缘、尖锐瞬态、频谱离散或闪烁。
- 水流：连续滑动、较平滑的时频轨迹、尾音与流动性。

建议图形形式：

- 优先用三联时频谱/2.5D 地形图。
- 每张图标注素材类型、时长、峰值、RMS。
- 如果空间图不如时频谱清楚，则不要强行用空间图。

### 4.2 JobRunner 输出

每个素材至少导出：

- sourceFileName
- sourcePath
- sourceHash
- analysisDurationSeconds
- peakDb
- rmsDb
- crestDb
- spectrum centroid 或可替代的频谱重心指标（如已实现）
- band energy：Low / Mid / High

## 5. Masking / Fusion View

### 5.1 目标

做一张服务第二章 2.3.2 的图，用于说明两层素材叠加时：

1. 哪些时间-频率区域重叠，可能造成遮蔽。
2. 哪些局部重叠反而帮助素材融合。
3. 合并后哪些频段变强、变弱或被重新塑形。

### 5.1.1 理论口径

掩蔽要分清三个层面：

1. 物理层面：两个声波相加后会发生能量叠加、局部抵消或局部增强。这不是心理声学掩蔽本身。
2. 声学层面：声音在频率、时间和空间中的分布会让某些细节更难被区分，例如低频尾音、密集瞬态或混响残留压住其他细节。
3. 心理声学层面：一个声音提高另一个声音的听阈，使后者更难被听见。这才是严格意义上的 masking。

因此 v1 图表不要把“频谱重叠”直接标成“掩蔽”。更稳妥的标签是：

- overlap：频谱/时频重叠
- masking risk：遮蔽风险
- dominant source：主导声源
- fusion tendency：融合倾向
- mix delta：混合后能量变化

论文里应写“存在遮蔽风险”或“具有融合趋势”，不要写“图中证明人耳已经发生掩蔽”。

### 5.1.2 masked threshold 是什么

`masked threshold` 可以译为“被掩蔽听阈”或“掩蔽阈值”。意思是：当一个声音 A 已经存在时，另一个声音 B 不再按安静环境下的听阈被听见，而必须超过由 A 抬高后的局部听阈，才更可能被听众辨认出来。

这个概念对论文有用，但不能被软件过度承诺：

- 严格的掩蔽阈值需要声压级、校准播放条件、听觉模型和听感实验支持。
- Audio Doctor 处理的是数字音频信号，通常只有 dBFS、频谱、时频能量、声像和插件渲染结果。
- 因此本功能应输出 `estimated masked threshold` 或 `masking risk proxy`，不要写成“真实人耳听阈测量”。

论文中推荐写法：

> 图中显示的是基于临界带能量和局部电平差估计的遮蔽风险，而不是严格听感实验中的掩蔽阈值测量。

不推荐写法：

> 图中证明某一层声音已经被人耳掩蔽。

### 5.1.3 临界带为什么要进入算法

如果只用普通 FFT bin 做重叠判断，图会过于“数学”，也容易误导：两个相邻频点是否重叠，并不等于听众能否区分两个声音。Ziemer 对 critical bands 的讨论可以支持一个更稳妥的施工方向：把频率先映射到近似临界带或 ERB/Bark-like 分带，再判断同一时间窗、相邻听觉频带中的能量竞争。

施工口径：

- v1 可以做 `criticalBandMode: "approximate"`，不要写成完整人耳模型。
- 分带不是为了炫算法，而是为了让图更接近“听觉会把哪些频率区域看成一组”的问题。
- 低/中/高三段仍保留，因为论文和声音制作语言需要它；临界带用于遮蔽风险计算，低/中/高用于解释和图注。

### 5.2 图形结构

建议一张图内左右两栏：

#### 左栏：Overlap / Masking Risk

显示素材 A 与素材 B 的时频能量重叠。

- A 用蓝色。
- B 用橙色。
- 重叠区域用紫色或亮白边缘。
- 遮蔽风险区域用红色半透明覆盖。
- 重点不是“谁绝对遮蔽谁”，而是显示“同一时间/相近频段/能量差明显”的区域。
- 支持按低/中/高频段高亮，便于论文讨论“低频压力感”“中频材质”“高频边缘成分”之间的冲突。

#### 右栏：Mix Delta / Fusion Result

显示合并后相对于原始两层的变化。

建议计算两个结果：

- `mixGainDb = Mix - max(A, B)`：合并后相对更强的区域，可理解为融合后的主导能量。
- `mixLossDb = Mix - expectedSum(A, B)`：合并后低于线性叠加预期的区域，可提示相位抵消或频段互相吃掉。

视觉建议：

- 增强区域用黄色/绿色。
- 损失区域用蓝紫/红紫。
- 不要把所有差异都叫“掩蔽”；只有符合时间、频率和响度条件的区域再标为 masking risk。
- 如果混合后局部能量峰变得更连续，可以标注为 fusion tendency，而不是“新共振峰”。只有经过共振滤波、失真或特定处理产生稳定峰值时，才建议称为共振峰。

### 5.2.1 可选 2.5D 版本

用户希望图形保持 Spatial Image / Spatial Terrain 那种直观“山峦”质感。建议 v1 先做二维热力/地形混合，不必一口气做复杂 3D 模型：

- 左图可以是 A/B 两层山峦叠放：A 蓝色、B 橙色，重叠处偏紫或亮白。
- 右图显示 Mix Delta 山峦：高出原始主导层的区域用亮色，低于预期叠加的区域用冷色。
- 如果两层山峦在透视下遮挡严重，允许增加 top-down 模式，确保论文截图可读。
- 图题和 manifest 里不要使用内部俗称；建议用 “Masking / Fusion View” 或 “Overlap and Fusion View”。

### 5.3 算法建议：先做遮蔽风险，再做估计阈值

不要一上来承诺完整 masked threshold。建议拆成两个层级：

#### v1：Critical Band Masking Risk / 临界带遮蔽风险

这是当前最推荐实现的版本，论文也最稳。它回答的问题不是“人耳是否真的听不到 B”，而是“在相近时间窗和相近听觉频带里，A 是否可能压住 B 的局部细节”。

算法步骤：

1. 对 A、B、Mix 做同参数 STFT。
2. 将频率映射到 ERB-like 或 Bark-like 近似临界带；如果时间不够，先用 log-frequency + critical-band-like 聚合。
3. 在每个时间窗与临界带内聚合能量：
   - `energyA`
   - `energyB`
   - `mixEnergy`
   - `deltaAB = energyA - energyB`
   - `overlapEnergy = min(energyA, energyB)`
4. 判断遮蔽风险：
   - A、B 均高于门限，例如相对自身峰值 `gateDbBelowPeak` 以内。
   - 两者位于同一或相邻临界带。
   - 主导层与弱层电平差超过 `dominanceThresholdDb`，例如 6-12 dB。
   - 如果低频层能量明显更强，可加入 `upwardSpreadWeight`，表示向较高频段形成遮蔽压力的倾向，但不要写成单向绝对规则。
5. 输出：
   - `maskingRiskIndex` 0-1
   - `dominantSource` A/B/None
   - `weakerSource` A/B/None
   - `overlapDb`
   - `dominanceDb`
   - `riskBandLowHz/riskBandHighHz`
   - `riskTimeStartSeconds/riskTimeEndSeconds`

#### v2：Estimated Masked Threshold / 估计遮蔽阈值

如果 v1 做完还有时间，再加 v2。它回答的问题是“弱层相对估计掩蔽阈值还剩多少余量”。这比 v1 更像 masked threshold，但仍只能叫估计值。

推荐计算口径：

1. 以主导层作为 masker，弱层作为 maskee。
2. 对每个时间窗和临界带估计：
   - `estimatedMaskedThresholdDb`
   - `maskeeEnergyDb`
   - `maskingMarginDb = maskeeEnergyDb - estimatedMaskedThresholdDb`
3. 解释：
   - `maskingMarginDb > 0`：弱层仍有突出余量。
   - `maskingMarginDb` 接近 0：弱层接近被覆盖边界，可能形成融合或边缘模糊。
   - `maskingMarginDb < 0`：弱层处于较高遮蔽风险区。
4. 图中不要使用“inaudible / 听不见”这种绝对标签；用 `above threshold proxy`、`near threshold proxy`、`below threshold proxy` 更稳。

### 5.3.1 临界带/心理声学近似配置

v1 至少需要这个配置；v2 可以在同一结构上扩展：

```json
{
  "psychoacousticMode": "approximate",
  "frequencyBands": "erb_like",
  "integrationTimeMs": 50,
  "dominanceThresholdDb": 9,
  "gateDbBelowPeak": 48,
  "upwardSpread": true,
  "upwardSpreadWeight": 0.35,
  "interauralCoherenceWeight": 0.25,
  "estimatedMaskedThreshold": false
}
```

说明：

- `erb_like` 或 `bark_like` 只作为近似分带，不要写成严格标准实现。
- `integrationTimeMs` 用于避免单个采样点造成误判，体现听觉积分时间。
- `dominanceThresholdDb` 决定主导层和弱层相差多少时才标注遮蔽风险，默认建议 9 dB，UI 可给 6/9/12 dB 三档。
- `gateDbBelowPeak` 避免把底噪或极低能量区域误判为重叠。
- `upwardSpread` 用于表达低频对较高频段更容易产生遮蔽影响，但仍保留双向比较。
- `interauralCoherenceWeight` 可以把左右相关性纳入风险判断：高度相关且集中在中心的强声源，更容易压住同一区域弱细节；低相关的宽尾音更可能形成包围感而非单点遮蔽。
- `estimatedMaskedThreshold` 只有在 v2 真的实现后才打开；v1 输出 masking risk，不输出 masked threshold。

### 5.4 建议 JSON

```json
{
  "view": "masking_fusion",
  "maskingFusion": {
    "sourceA": "dryA",
    "sourceB": "dryB",
    "mix": "dryC",
    "frequencyScale": "erb_like",
    "criticalBandMode": "approximate",
    "integrationTimeMs": 50,
    "gateDbBelowPeak": 48,
    "dominanceThresholdDb": 9,
    "upwardSpread": true,
    "upwardSpreadWeight": 0.35,
    "estimatedMaskedThreshold": false,
    "showOverlap": true,
    "showRisk": true,
    "showMixDelta": true,
    "showDominantSource": true,
    "showFusionTendency": true
  }
}
```

### 5.5 Manifest / CSV

必须输出：

- `masking_fusion_cells.csv`
  - timeStartSeconds
  - timeEndSeconds
  - frequencyLowHz
  - frequencyHighHz
  - bandIndex
  - bandScale：log / erb_like / bark_like
  - energyADb
  - energyBDb
  - mixDb
  - overlapDb
  - dominanceDb
  - maskingRiskIndex
  - dominantSource
  - weakerSource
  - mixGainDb
  - mixLossDb
  - estimatedMaskedThresholdDb（仅 v2）
  - maskingMarginDb（仅 v2）
- `masking_fusion_summary.csv`
  - riskAreaPercent
  - strongestRiskFrequencyHz
  - strongestRiskTimeSeconds
  - strongestRiskBandIndex
  - meanOverlapDb
  - meanMixGainDb
  - meanMixLossDb
  - meanMaskingMarginDb（仅 v2）
  - sourceAFileName
  - sourceBFileName
  - mixSourceMode：linear_sum / rendered_mix / external_file

Manifest 需要额外记录：

- `algorithmName`: `critical_band_masking_risk`
- `algorithmVersion`
- `psychoacousticMode`: `approximate`
- `bandScale`: `erb_like` / `bark_like` / `log`
- `integrationTimeMs`
- `dominanceThresholdDb`
- `gateDbBelowPeak`
- `upwardSpread`
- `upwardSpreadWeight`
- `estimatedMaskedThreshold`: true / false
- 输入素材 A/B/Mix 的 sourceFileName、sourceHash、selection、peakDb、rmsDb、crestDb

## 6. 论文正文建议

第二章只需要放 1 张掩蔽/融合图，不要铺满工具图。建议正文结构：

- 2.3.1 保留等响曲线示意。
- 2.2 或 2.3.3 增加火/电/水素材构成图。由于三类素材是三种不同材质逻辑，建议三张 2.5D 图分别呈现，不要硬塞在一张图里。
- 2.3.2 增加掩蔽/融合图。
- 其他完整数据和更多素材放附录。

建议论文语言：

> 当两个素材在相近时间和相邻频段同时占据较高能量时，弱层的局部细节可能被强层提高听阈而变得不易辨认；但适度的频段重叠也能让多层素材在听觉上合并为同一事件。图 X 显示的不是严格听阈实验，而是对这种遮蔽风险与融合趋势的可视化。

避免写法：

- “低频一定掩蔽高频”
- “图中证明人耳已经听不到某层素材”
- “重叠区域就是掩蔽区域”
- “合并后峰值就是共振峰”

## 7. 交付优先级

优先级从高到低：

1. Spatial Image / Spatial Terrain 分频高亮。
2. 火/电/水三素材时频谱或 2.5D 对比图。
3. Masking / Fusion View v1。
4. 更复杂的心理声学阈值模型。

前两项可以立刻提升第二章可信度。第三项如果实现得太急，不要冒充精确心理声学实验，只作为“频段重叠与融合风险可视化”即可。

## 8. 验收标准

### 8.1 分频高亮

- UI 与 JobRunner 都能选择 Low / Mid / High。
- 深色主题下高亮频段清楚，未选频段仍能看见轮廓。
- manifest 记录 bandHighlight 配置。
- 图中 legend 不遮挡主体山峦。

### 8.2 火/电/水素材图

- 三张图分别输出，图名中包含 material_fire / material_electric / material_water。
- 每张图记录 sourceFileName、sourceHash、duration、peakDb、rmsDb、crestDb。
- 若图形使用 2.5D，必须保留时间和频率标签；若使用时频谱，必须保留频率刻度。

### 8.3 掩蔽与融合图

- 至少支持两个输入素材 A/B 与一个 Mix 结果。
- 导出主图、summary CSV、cell CSV。
- 图上明确区分 overlap、masking risk、mix gain、mix loss。
- manifest 说明算法是 approximate / visualization，不写成 psychoacoustic threshold measurement。
- v1 必须能输出 `critical_band_masking_risk` 相关字段：bandScale、integrationTimeMs、dominanceThresholdDb、maskingRiskIndex、dominantSource、weakerSource。
- 如果实现 v2，图和 manifest 必须明确标为 `estimated masked threshold`，不要简称为 `masked threshold`，避免论文误写成真实听阈实验。
- 同一 job 可复现，不依赖手动截图。

## 9. 给论文组的使用边界

如果本功能赶得上论文最终版，正文最多使用 1 张掩蔽/融合图。它的功能是解释“遮蔽风险与融合趋势”，不是把第二章改成心理声学实验章。

正文建议只写三句话的逻辑：

1. 听觉系统会在有限时间窗和相近频带内组织声音，频谱相邻且能量差明显的素材更容易互相覆盖。
2. Audio Doctor 的图例把这一关系转化为临界带能量、局部电平差和合并后能量变化的可视化。
3. 图中标注的是遮蔽风险和融合趋势，不等同于严格实验条件下的主观听阈测量。

正文不要写：

- “本软件测得人耳真实遮蔽阈值”
- “某层素材已经不可听见”
- “低频天然遮蔽高频”
- “重叠就是掩蔽”

附录可以写：

> 掩蔽/融合图采用近似临界带分组，对两个输入素材在相同时间窗内的频带能量、局部电平差与合并后能量变化进行统计，用于提示可能的遮蔽风险与融合趋势。该图为信号分析图，不作为主观听感实验结果。
