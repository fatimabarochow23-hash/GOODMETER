# Audio Doctor 临界带晶体图需求单

日期：2026-05-09  
需求方：论文组  
目标模块：GOODMETER / Audio Doctor / Layer Fit & Fusion / Critical Band Crystal  
优先级：高  
关联文件：`Source/AudioDoctorComponent.h`、`Source/AudioDoctorFigureRenderer.h`、`Source/AudioDoctorAnalysis.h`、`Source/AudioDoctorJobRunner.h`

## 1. 本单目的

当前 `Layer Fit & Fusion` 的临界带图已经能显示 24/近似临界带上的能量关系，但视觉仍偏格子化，不够直观。论文组希望把临界带图升级为更具结构感的“晶体视图”：每个临界带对应一个晶体面，能量越高，该面向外膨胀越明显。

这张图要服务两件事：

1. 软件内分析：让用户直观看到 1-3 个 stem 在临界带结构中的占位、拥挤、融合与最终 bounce 结果。
2. 论文出图：提供比平面表格和马赛克热力图更清楚的视觉证据，用于说明声层如何在听觉频带中组织成一个整体。

## 2. 核心布局：三联晶体视图

不要做 `Energy / Risk Mode` 切换。论文组希望三个对象同时存在：

```text
[三 stem 重叠晶体]   [析出分析晶体]   [Auto Bounce 结果晶体]
```

### 2.1 左栏：三 stem 重叠晶体

左栏显示所选 1-3 个 stem 的临界带能量状态。

规则：

- 每个 stem 是一颗独立晶体。
- 三颗晶体同轴叠放。
- 每颗晶体都有 24 个面，对应临界带。
- 每个面都有基础单位尺寸；该临界带无明显能量时保持最小形态。
- 某个临界带能量越高，该面越向外膨胀，局部体积越大。
- 三个 stem 保留身份色与透明度。
- 三颗晶体之间不要硬性错位太多，否则同一临界带不好对照。

左栏回答的问题：

```text
当前时间窗里，三个功能声层分别激发了哪些临界带，它们是否集中在相同区域。
```

### 2.2 中栏：析出分析晶体

中栏不是第四个声音，也不是某个 stem。它是从左栏三颗 stem 的关系中计算出来的分析结果。

建议分析维度：

- `dominantStem`：某个临界带中主导能量来自哪个 stem。
- `maskedCandidate`：同一临界带中可能被压住的 stem。
- `fusionTendency`：多个 stem 在该临界带内能量相近、时间相近时的融合倾向。
- `overlapPressure`：多个 stem 同时挤在同一或相邻临界带的强度。
- `mixLoss / mixGain`：合成后相对输入层的削弱或增强趋势。

视觉规则：

- 每个面仍对应同一套临界带。
- 面的膨胀程度表示该临界带的分析强度。
- 颜色不再表示 stem 身份，而表示分析类型。
- 细节信息放到 tooltip、manifest、CSV 或导出数据里，不要全塞到图面上。

建议颜色语义：

```text
红 / 品红：遮蔽风险或局部压制
紫 / 蓝紫：融合倾向
黄 / 白边：主导峰、关键临界带
灰暗：交互较弱
```

中栏回答的问题：

```text
哪些临界带是当前声层关系的关键区域，哪里发生了压制、融合、主导或局部削弱。
```

### 2.3 右栏：Auto Bounce 结果晶体

右栏显示所选 stem 自动合成后的结果。

规则：

- 右栏是一颗晶体，不再显示 stem A/B/C 的身份色。
- 每个面仍对应同一套临界带。
- 面的膨胀程度表示 bounce 后该临界带的能量。
- 可用边缘线或轻微高亮标记相对输入层保留下来的关键临界带。

右栏回答的问题：

```text
三层声源合成后，最终能量集中在哪些临界带，哪些区域被保留、削弱或整合。
```

## 3. 时间进度条与分析窗口

临界带晶体图必须支持时间进度条，交互方式参考现有 `Spatial Image`。

### 3.1 时间条

底部显示：

```text
Time 0.48s
```

图中显示：

```text
Time 0.48s | window 0.44-0.52s
```

时间窗长度建议先复用现有 `Spatial Image` / `Layer Fit` 的分析窗，默认约 80 ms 或当前内部设置。

### 3.2 三栏同步

拖动时间条时，三栏必须同步更新：

- 左栏三 stem 重叠晶体更新。
- 中栏析出分析晶体更新。
- 右栏 Auto Bounce 结果晶体更新。

不要只更新其中一栏。

### 3.3 时间点预设

为论文出图，建议加三个快捷按钮或导出字段：

```text
Impact / Spread / Tail
```

它们不是固定算法命名，只是方便快速定位：

- `Impact`：瞬态或撞击边界附近。
- `Spread`：主体能量展开阶段。
- `Tail`：尾音残留阶段。

如果 UI 暂时不加按钮，至少在 Job Runner / export job 里支持指定多个时间点导出。

## 4. 视角交互：三栏同轴旋转

晶体图需要可旋转视角。

### 4.1 鼠标拖拽

用户按住图形区域上下左右拖拽时：

- 三栏晶体一起旋转。
- 左、中、右三栏保持同一观察角度。
- 不允许左栏转了、中栏没转、右栏角度不同。

这非常重要，因为论文图需要比较同一个临界带面在输入、分析、输出三个状态中的变化。

### 4.2 视角状态

建议维护一个共享视角状态：

```cpp
struct CrystalCamera
{
    float yaw;
    float pitch;
    float zoom;
};
```

三栏共用同一个 `CrystalCamera`。

### 4.3 视角预设

建议提供：

```text
Front / Side / Top / Isometric / Reset
```

论文出图默认推荐 `Isometric`，但用户可以自己转到更清楚的角度后导出。

## 5. 晶体几何规则

### 5.1 面与临界带映射

最低要求：

- 24 个面稳定对应 24 个临界带。
- 每个面的编号和频率范围在 manifest 中可查。
- UI 中可选是否显示 band label，例如 `B1`、`B12`、`B24`。

建议 manifest 中写：

```json
"criticalBandCrystal": {
  "bandCount": 24,
  "bands": [
    { "index": 1, "lowHz": 20, "highHz": 100, "centerHz": 60 },
    { "index": 12, "lowHz": 920, "highHz": 1080, "centerHz": 1000 }
  ]
}
```

频带范围以实际算法为准，不要在 manifest 中写死示例值。

### 5.2 膨胀尺度

每个面的膨胀量来自该临界带在当前时间窗的能量。

建议：

- 先把每个 stem 的 band energy 转成 dB。
- 再映射到 `0.0 - 1.0` 的膨胀量。
- 基础晶体不为 0，最低保持单位形态。
- 避免最大峰把所有其它面压得完全看不见，可加入可调压缩曲线。

建议参数：

```text
baseRadius = 1.0
maxExpansion = 0.8 - 1.6
energyFloorDb = peakDb - 60
curve = sqrt / log / gentle compression
```

### 5.3 三 stem 同面可读性

难点是同一个临界带面上 A/B/C 三个 stem 都有能量时会互相遮挡。

建议可选策略：

1. **透明叠层**：三颗晶体同轴叠放，通过透明度区分。
2. **微小法线偏移**：三个 stem 在同一面法线方向上有极小偏移，但不能错到看不出是同一临界带。
3. **边缘线区分**：填充透明，边缘线更清楚。
4. **选中高亮**：鼠标 hover 某个面时，三颗晶体同一 band 的面同时描边。

默认建议：

```text
透明叠层 + 细边缘线
```

不要把三个 stem 直接画成完全不透明实体。

## 6. 中栏析出晶体的计算建议

中栏的每个面对应一个临界带，值来自当前时间窗内三 stem 的关系。

### 6.1 输入数据

每个 band 至少需要：

```text
stemEnergyDb[3]
stemPresent[3]
dominantStem
secondStem
energyGapDb
overlapCount
fusionTendency
maskingRisk
mixGainDb
mixLossDb
```

### 6.2 风险/融合强度

建议先用工程可解释的指标，不要追求一步到位的复杂模型。

可以这样分：

- `overlapCount`：该 band 中有几个 stem 高于门限。
- `dominanceGapDb`：最强 stem 与次强 stem 的差值。
- `fusionTendency`：多个 stem 能量接近、时间同步时提高。
- `maskingRisk`：多个 stem 同 band 重叠，且能量差较大时提高。
- `mixLossDb`：bounce 能量低于预期叠加时提高。
- `mixGainDb`：bounce 能量高于主导 stem 或预期局部时提高。

### 6.3 中栏颜色优先级

当多个状态同时发生，不要把颜色混成一团。建议优先级：

1. `maskingRisk` 高：红/品红。
2. `fusionTendency` 高：紫/蓝紫。
3. `mixGain` 明显：黄/白边。
4. `mixLoss` 明显：深红边或暗化。
5. 其它：灰暗基础面。

如果某个 band 同时有高风险和高融合，建议：

- 面填充用融合色。
- 边缘用风险色。

这样能同时表达“这一区域既在靠拢，也有遮挡风险”。

## 7. 与现有 Layer Fit / Fusion 的关系

新增视图建议放在现有 `Layer Fit / Fusion` 的 `View` 下拉里：

```text
Critical Band Crystal
```

现有视图仍保留：

- `Spatial Image`
- `Time-Frequency Terrain`
- `Critical Band Terrain`

建议关系：

```text
Critical Band Terrain = 表格/调试友好的离散临界带图
Critical Band Crystal = 论文/演示友好的结构化临界带图
```

不要删除已有图，避免影响已经能跑的 Job Runner 和论文旧图。

## 8. 论文出图需求

这张图未来很可能进入正文或附录，因此导出必须从一开始就考虑排版。

### 8.1 静态导出布局

推荐导出尺寸：

```text
横版 16:9 或接近 2:1
```

三栏标题建议：

```text
Selected Stems
Derived Interaction
Auto Bounce
```

中文 UI 可写：

```text
所选声层
析出关系
合成结果
```

论文正文图注可写成类似：

```text
图X-X 三个声层在临界带结构中的能量分布、相互关系与合成结果
```

图面不要堆太多句子。正文和附录数据表负责解释。

### 8.2 图中必须标注的信息

静态图中最少保留：

- 时间点与分析窗口。
- Stem 1/2/3 对应对象。
- Bounce 来源。
- 颜色图例。
- 频带数量。

建议图面文字：

```text
Time 0.48s | window 0.44-0.52s
Stem 1 / Stem 2 / Stem 3
Auto Bounce
Bands: 24
```

### 8.3 导出当前视角

用户手动旋转到好看的角度后，点击 Export 应该导出当前视角，而不是恢复默认视角。

manifest 中记录：

```json
"camera": {
  "yaw": 35.0,
  "pitch": -18.0,
  "zoom": 1.0
}
```

Job Runner 也要支持指定相同 camera，这样同一张图可以复现。

### 8.4 多时间点导出

Job Runner 建议支持：

```json
"criticalBandCrystal": {
  "times": [0.15, 0.48, 1.50],
  "camera": { "yaw": 35, "pitch": -18, "zoom": 1.0 }
}
```

输出方式可以是：

1. 三张独立 PNG。
2. 一张纵向/横向拼接 PNG。

论文正文更适合拼接图；附录适合独立图。

## 9. Manifest / CSV 输出需求

为了论文附录和复查，导出数据不能只给图。

### 9.1 Manifest

manifest 至少记录：

```json
{
  "view": "critical_band_crystal",
  "timeSeconds": 0.48,
  "windowStartSeconds": 0.44,
  "windowEndSeconds": 0.52,
  "stemSources": ["DRY A", "DRY B", "WET C"],
  "bounceSource": "auto_bounce_selected_stems",
  "bandCount": 24,
  "camera": { "yaw": 35, "pitch": -18, "zoom": 1.0 }
}
```

### 9.2 CSV

建议输出 `critical_band_crystal.csv`：

```text
timeSeconds,bandIndex,bandLowHz,bandHighHz,stem1Db,stem2Db,stem3Db,bounceDb,dominantStem,maskedCandidate,fusionTendency,maskingRisk,mixGainDb,mixLossDb
```

如果字段很多，可以拆：

- `critical_band_energy.csv`
- `critical_band_interaction.csv`

不要只把数据写进图里。

## 10. UI 细节

### 10.1 鼠标操作

- 左键拖拽：旋转三栏。
- 滚轮 / 双指：缩放。
- 双击：Reset View。
- 按住 Shift 拖拽：可选，限制水平旋转。

### 10.2 Hover

鼠标悬停某个面时，建议显示：

```text
B12 | 920-1080 Hz
Stem A: -18.2 dB
Stem B: -25.4 dB
Stem C: -41.0 dB
Bounce: -16.9 dB
```

同时三栏同一 band 的面都描边，方便对照。

### 10.3 颜色

建议沿用当前 Layer Fit stem 身份色：

- Stem 1：青色
- Stem 2：黄色
- Stem 3：品红
- Bounce：绿色或浅黄绿

析出晶体使用分析色，不用 stem 身份色。

## 11. 验收标准

### 11.1 UI 验收

1. `Layer Fit / Fusion` 中能选择 `Critical Band Crystal`。
2. 三栏同时出现：所选声层、析出关系、合成结果。
3. 拖动时间条，三栏同步变化。
4. 鼠标拖拽旋转，三栏同步旋转。
5. `Reset View` 能恢复默认视角。
6. 选择不同 stem 后，左栏和中栏重新计算。
7. Auto Bounce 修复后，右栏长度不被最短 stem 截断。

### 11.2 图像验收

1. 无能量 band 保持基础形态。
2. 高能量 band 明显向外膨胀。
3. 三个 stem 的同一 band 能在空间上对齐比较。
4. 析出晶体不是 stem 复制品，而是分析结果。
5. Bounce 晶体能看出最终能量保留在哪里。
6. 导出的 PNG 中文字不重叠，三栏不挤压。

### 11.3 数据验收

1. manifest 有 view、time、window、stem sources、bounce source、camera。
2. CSV 至少能还原每个 band 的 stem energy 和 bounce energy。
3. 中栏析出晶体的颜色/膨胀能在 CSV 中找到对应指标。
4. Job Runner 能复现同一时间点、同一视角的导出图。

## 12. 建议施工顺序

1. 先做静态三联晶体 renderer，不做鼠标交互。
2. 接入现有 Layer Fit stem/bounce 数据。
3. 加时间条同步更新。
4. 加同轴旋转 camera。
5. 加 manifest / CSV。
6. 加 Job Runner 导出。
7. 再调美术：透明度、边缘线、颜色、字体、图例。

这张图的成败关键不是算法堆多复杂，而是让读者一眼看懂三件事：

```text
输入声层如何占据临界带；
声层之间在哪里发生关键交互；
合成结果留下了怎样的临界带结构。
```
