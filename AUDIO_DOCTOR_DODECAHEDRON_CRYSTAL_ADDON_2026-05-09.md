# Audio Doctor Layer Fit / Fusion 补充需求单：十二面体临界带晶体

日期：2026-05-09  
目标模块：GOODMETER / Audio Doctor / Layer Fit & Fusion  
新增视图建议名：`Dodecahedron Crystal` / `十二面体晶体`

## 1. 背景

当前 `Critical Band Crystal` 的扇面形态已经能表达 24 个临界带的展开关系，视觉上也有一定可读性，但它更像一个开放的频带扇面或接收面。论文组希望再新增一种更“物体化”的显示方式：用一个可旋转、闭合、稳定的十二面体晶体来表达临界带能量结构。

这不是替换现有扇面图，而是在 `Layer Fit / Fusion` 的 `View` 下拉中新增一个选项：

```text
Critical Band Fan / 当前扇面图
Critical Band Terrain / 当前临界带立体图
Dodecahedron Crystal / 新增十二面体晶体
```

保留扇面图的原因是：它对 24 个 band 的展开很直观；新增十二面体的原因是：它更适合论文图和演示场景里表达“声层结构被能量撑开”的整体形态。

## 2. 核心视觉目标

十二面体晶体应当是一个闭合的、多面体式的对象，而不是开放式锅状图、雷达盘、柱状极坐标图或马赛克图。

图像需要让用户一眼看到：

1. 当前 1-3 个 stem 分别激发了哪些临界带区域。
2. 多个 stem 是否挤在相近的临界带上。
3. 合成后的 bounce 是否保留、削弱或重新集中这些能量区域。
4. 同一时间点下，输入、析出关系和输出结果能在同一视角比较。

## 3. 几何映射

### 3.1 十二个面如何对应二十四个临界带

使用正十二面体的 12 个五边形面，每个面承载相邻两个临界带：

```text
Face 01 = B01 + B02
Face 02 = B03 + B04
Face 03 = B05 + B06
...
Face 12 = B23 + B24
```

这样仍然保留 24 个临界带的信息，但视觉上压缩成 12 个稳定区域，避免 24 面过密导致读图困难。

### 3.2 五边形面不要硬切成两半

五边形如果强行对半切，会产生不稳定、不美观的几何碎片。建议采用“面整体 + 双脊线”的表达：

- 五边形面的整体鼓起高度：表示这一组两个临界带的合计能量。
- 面上的第一条脊线/条纹：表示该组前一个临界带，例如 B07。
- 面上的第二条脊线/条纹：表示该组后一个临界带，例如 B08。
- 两条脊线的亮度、高度、粗细或局部发光强度，表示两个 band 的相对强弱。

如果不方便做真实脊线，也可以先做面内双条纹：

```text
左/上条纹 = 前一个 band
右/下条纹 = 后一个 band
```

但不要把五边形切成两个大色块，因为三颗 stem 叠加时会变得很乱。

### 3.3 膨胀规则

每个五边形面沿自己的面法线向外膨胀。

建议映射：

```text
faceEnergy = energy(Bn) + energy(Bn+1)
faceExpansion = baseRadius + normalized(faceEnergy) * expansionScale
```

最低能量时也保留基础晶体形态；不要让面坍缩成 0，否则旋转时会丢失临界带的固定位置。

## 4. 三联布局

十二面体晶体沿用当前 Layer Fit / Fusion 的三联逻辑：

```text
[Selected Stems]   [Derived Interaction]   [Auto Bounce]
```

### 4.1 左栏：Selected Stems

左栏显示所选 1-3 个 stem 的十二面体晶体。

要求：

- 每个 stem 一颗半透明十二面体。
- 三颗晶体同轴叠放。
- 三颗晶体使用同一套面索引，Face 01 永远对应 B01-B02。
- Stem 身份色继续使用当前 Layer Fit 色系。
- 可通过透明度、描边和轻微深度偏移处理遮挡，但不能让同一面错到无法比较。

读图目标：

用户能看到三个 stem 在相同临界带组里的占位和拥挤情况。

### 4.2 中栏：Derived Interaction

中栏不是某一条声音，而是分析结果晶体。

它也使用同一套 12 面结构，但面高度和颜色来自分析指标：

- 哪一组临界带发生强重叠。
- 哪一组临界带出现明显主导 stem。
- 哪一组临界带出现融合倾向。
- 哪一组临界带在合成后产生明显增强或削弱。

建议视觉：

```text
红 / 紫：重叠压力较高
蓝 / 青：局部削弱或被压住
金 / 白描边：当前关键峰或主导区域
中性灰：无明显关系
```

这里不要再叠 stem 身份色，否则会和左栏重复。

### 4.3 右栏：Auto Bounce

右栏显示合成后的十二面体晶体。

要求：

- 只显示一颗 bounce 晶体。
- 使用统一的 bounce 颜色，不要和 Stem B 默认黄色混淆。
- Face 01-Face 12 仍然对应同一套 B01-B24 分组。
- 面高度表示 bounce 后该组临界带能量。
- 可用细描边标出相对输入保留下来的关键区域。

读图目标：

用户能比较“输入层占位”和“合成结果保留在哪里”。

## 5. 时间与视角交互

### 5.1 时间进度条

十二面体晶体必须有时间进度条，行为参考现有 `Spatial Image`：

- 当前时间点显示在图右上角或底部。
- 所有三栏同步更新时间。
- 分析窗口与当前 Layer Fit 内部时间窗一致。
- 可导出当前时间点对应的静态图。

### 5.2 同轴旋转

用户拖拽视图时，三栏晶体必须同步旋转。

要求：

- 左栏、中央、右栏使用同一 camera angle。
- 鼠标左右拖拽控制水平旋转。
- 鼠标上下拖拽控制俯仰角。
- 双击或按钮可恢复默认视角。
- 导出 manifest 记录当前 camera angle。

论文图需要比较三个状态，如果三栏视角不同，会破坏可比性。

## 6. 分频显示与身份色

十二面体晶体需要支持现有 Band 控件：

```text
Bands Off
Low
Mid
High
All Bands
```

在 `Low / Mid / High` 模式下：

- 选中频段对应的面/脊线保持正常颜色和亮度。
- 非选中频段变成半透明暗色 ghost。
- Stem 身份色仍保留在选中区域。
- 不要让非选中区域继续以鲜艳身份色显示。

因为十二面体每个面承载两个 band，所以如果某一面中只有一个 band 属于当前频段：

- 面整体可以变暗一半或降低透明度。
- 对应 band 的脊线保持高亮。
- 另一个 band 的脊线变成暗色。

## 7. UI 建议

在 Layer Fit / Fusion 的 `View` 下拉中新增：

```text
Dodecahedron Crystal
```

进入该视图后显示：

```text
Stem 1 [Auto/DRY A/...]
Stem 2 [Auto/DRY B/...]
Stem 3 [Off/DRY C/WET C/...]
Bounce [Auto/Manual/...]
Band [Bands Off/Low/Mid/High/All Bands]
Time [slider]
Reset View
Export
```

如果当前少于两个 stem，仍可显示左栏和右栏，但中栏提示分析信息不足。

## 8. Export / Job Runner / Manifest

导出时需要记录：

```json
{
  "figureType": "dodecahedron_crystal",
  "criticalBandGrouping": "pairwise_24_to_12",
  "faces": [
    { "face": 1, "bands": [1, 2] },
    { "face": 2, "bands": [3, 4] }
  ],
  "timeSeconds": 0.16,
  "windowSeconds": [0.12, 0.20],
  "camera": {
    "yaw": 0.0,
    "pitch": 0.0,
    "roll": 0.0
  },
  "selectedStems": ["DRY A", "DRY B", "WET C"],
  "bounce": "Auto Bounce"
}
```

CSV/JSON 数据里至少要有：

- 每个 stem 的 24 band energy。
- 每个 face 的 pair energy。
- 每个 face 内两个 band 的相对值。
- 中栏 derived interaction 的指标。
- bounce 后每个 face 的 pair energy。

## 9. 验收标准

### 9.1 基础视觉

1. `View = Dodecahedron Crystal` 时能看到三栏闭合晶体，不是锅状扇面。
2. 12 个五边形面能稳定对应 12 组临界带。
3. 每个五边形面可以看出组内两个 band 的相对强弱。
4. 面会根据能量向外鼓起，低能量保持基础形态。

### 9.2 交互

1. 时间滑杆能改变三栏晶体状态。
2. 鼠标拖拽旋转时，三栏视角同步。
3. `Reset View` 能恢复默认角度。
4. `Band = Low / Mid / High` 时，非选中频段变暗，不抢视觉重点。

### 9.3 数据一致性

1. 左栏 stem 的 face energy 能从 manifest/CSV 查到。
2. 中栏 derived interaction 的高亮区域能从分析指标查到。
3. 右栏 bounce 的 face energy 能从 bounce 音频分析查到。
4. Auto Bounce 长度不再被最短 stem 截断，长尾 stem 在后续时间点仍能显示。

### 9.4 论文可用性

导出的图需要满足：

- 标题、时间点、band 分组清楚。
- 三栏对照关系明确。
- 深色主题下细线、面、文字都能读。
- 导出图片中不出现调试文字、临时变量名或内部开发注释。

## 10. 施工优先级

建议分两步：

### Step 1：静态可用版

- 新增 `Dodecahedron Crystal` 视图。
- 先实现 12 面晶体、pairwise band 映射、三栏布局。
- 支持当前时间点和导出。

### Step 2：交互与论文增强

- 加入同步旋转。
- 加入面内双脊线或双条纹。
- 加入 Band Solo ghost 行为。
- 完善 manifest / CSV。

如果时间紧，先保证 Step 1 的静态导出图好看、可解释、数据一致。

## 11. 给论文组的图注方向

可参考：

```text
图X-X 声层叠加前后在十二组临界带区域中的能量分布
```

或：

```text
图X-X 多声层在临界带结构中的占位、相互作用与合成结果
```

正文解释时重点讲：

- 每个面代表相邻两个临界带。
- 面的外鼓表示该组临界带能量增强。
- 面内双脊线表示两个临界带的相对强弱。
- 左、中、右三栏分别对应输入层、析出关系和合成结果。

