# Audio Doctor 预听 Solo / Group Delay 修复需求单

日期：2026-05-21  
对象：GOODMETER / Audio Doctor 施工队  
优先级：高  
范围：主界面预听、时间进度条预听、Group Delay 视图、Job Runner / CLZ 同步

## 0. 当前问题

当前 Audio Doctor 右上角师姐预听按钮会把当前 display slots 中可用素材混合后出声。普通视图下，它会混合显示槽里的资产；Layer Fit / Fusion 视图下，它还会把 stem 与 bounce 一并混入。这个逻辑不支持用户指定“只听某一路”或“只听某两路”。

用户现在需要一个更像 6 轨监听矩阵的预听系统：点亮谁，谁能在预听功能中出声；没点亮的轨道不出声。

同时，当前 Group Delay 图在载入两条及以上 DRY 素材后容易不显示。原因是多 DRY 会挤占 display slots，而 Group Delay 绘图目前依赖 display slot 2 / 3 中是否有 groupDelay 数据。

另外，右下参数区的 Plugin A/B/C 色块太小，需要增大。

## 1. 六轨预听 Solo 方块

### 1.1 UI 形态

在主界面增加 6 个无文字方块格子，排列为 3 x 2：

第一排从左到右：

```text
DRY A | DRY B | DRY C
```

第二排从左到右：

```text
WET A | WET B | WET C
```

注意：

- 方块内部不要显示文字标签。
- 方块只用颜色、亮灭、边框、透明度表达状态。
- 鼠标悬停可以有 tooltip，tooltip 可写 `DRY A Preview Solo` 等，但界面上不要直接写标签。
- 无素材的方块置灰或低透明，不可点。
- 有素材的方块可点亮 / 熄灭。
- 支持多选，不是单选。

### 1.2 默认状态

建议默认策略：

- 如果没有任何 solo 方块被点亮，预听继续使用旧逻辑：当前 display slots 混音。
- 用户一旦点亮任意方块，就进入 solo-filter 模式：只混合点亮且素材存在的轨道。
- 这样可以兼容旧工程，也避免新用户打开工程后师姐按钮突然无声。

### 1.3 预听范围

这套 6 轨 solo 只影响以下预听：

- 右上角师姐预听按钮，也就是当前 `figureLoopPreviewBtn` 触发的 loop preview。
- 空间 / Layer Fit 等带时间进度条的 timeline preview。

明确不影响：

- 插件 insert 内部的实时预听按钮。
- 插件格子里的 `R` / live preview 行为。
- 插件实时预听仍按当前 Bus Routing / render input 逻辑走。

原因：插件内预听是为了听某条插件链对 routed DRY input 的实时处理效果；六轨 solo 是为了听图中 DRY/WET 资产组合，两个逻辑不要混在一起。

## 2. 代码改动建议

主要文件：

- `Source/AudioDoctorComponent.h`

现有相关位置：

- `figureLoopPreviewBtn.onTrigger` 绑定在构造函数附近。
- `toggleFigureLoopPreview()` 目前调用 `makeDisplayPreviewAsset()`。
- `startTimelineAudioPreview()` 也调用 `makeDisplayPreviewAsset()`。
- `makeDisplayPreviewAsset()` 当前根据 display slots 或 Layer Fit sources 组装混音。

建议新增：

```cpp
std::array<bool, 6> previewSoloSlots {};
```

映射顺序固定：

```cpp
0 = DRY A
1 = DRY B
2 = DRY C
3 = WET A
4 = WET B
5 = WET C
```

建议新增 helper：

```cpp
bool hasAnyPreviewSoloEnabled() const;
SourceSlot sourceSlotForPreviewSoloIndex(int index) const;
bool canPreviewSoloSlot(int index) const;
std::vector<const Asset*> makePreviewSoloSources() const;
```

然后改 `makeDisplayPreviewAsset()`：

1. 如果有任意 solo 方块点亮：
   - 只收集点亮且存在素材的 DRY/WET source。
   - 若收集结果为空，返回空 asset，并提示用户当前 solo 轨道没有可预听素材。
2. 如果没有任何 solo 方块点亮：
   - 保留旧逻辑，继续根据 display slots / Layer Fit sources 生成预听混音。

## 3. 视觉设计建议

6 个方块要像轨道开关，不像文字按钮。

建议尺寸：

- 单个方块约 18-24 px。
- 间距 4-6 px。
- 总体放在右上工具区，靠近 Output / DIM / 师姐按钮。

颜色建议复用现有轨道色：

- DRY A：青色系。
- DRY B：黄色系。
- DRY C：粉 / 紫红系。
- WET A：绿色或当前 Wet A 色。
- WET B：蓝紫或当前 Wet B 色。
- WET C：橙 / 绿灰或当前 Wet C 色。

状态：

- 未加载：低透明灰块。
- 已加载未点亮：轨道色低透明，细边框。
- 已加载已点亮：轨道色高透明填充，边框更亮。
- 当前正在预听时，可以让已点亮方块有轻微呼吸或高亮，但不要大幅动画影响主界面稳定。

## 4. Group Delay 修复

### 4.1 当前问题

当前 `drawGroupDelayPlot()` 只看：

```cpp
displayAsset(1)->groupDelay
displayAsset(2)->groupDelay
```

多 DRY 载入时，`refreshDryDisplaySlots()` 会优先把 DRY A/B/C 塞入 display slots。这样 WET A/B/C 即使存在，也可能被挤出显示槽，导致 Group Delay 图显示：

```text
Load matching Dry/Wet or render a plugin to show group delay.
```

### 4.2 目标行为

Group Delay 视图应显示所有已经渲染并具备 groupDelay 数据的 WET：

- WET A / Plugin A
- WET B / Plugin B
- WET C / Plugin C

不应被当前 displaySlots 影响。

### 4.3 建议改法

在 Group Delay 视图中：

- `drawGroupDelayPlot()` 直接检查 `wetAsset`、`wetBAsset`、`wetCAsset` 的 `groupDelay`。
- 只要任一 WET 有 groupDelay，就画出来。
- Dry reference 仍画 0 ms 基准线。
- legend / metrics 中可显示对应的 WET A/B/C 与 Plugin A/B/C。
- 如果没有任何 WET groupDelay，再显示提示。

Metrics 同步：

- `drawMetrics()` 当前按 displayAsset(0/1/2) 展示 group delay metrics。
- Group Delay 视图中建议专门走 WET A/B/C metrics，不再受 display slots 限制。
- Dry reference 文案仍可显示为 `GD reference 0.00 ms`，但参考源应该来自对应 `renderReferenceA/B/C` 或 render input。

导出图同步：

- `makeFigureDataForExport()` 或 FigureRenderer 的 group delay export 也要按同一逻辑处理。
- 不要出现 UI 能显示、导出图不显示，或反过来的不一致。

## 5. Plugin 参数色块放大

主要文件：

- `Source/AudioDoctorComponent.h`
- `Source/AudioDoctorFigureRenderer.h`

当前 UI 参数区色块宽度偏小：

```cpp
row.removeFromLeft(exportMode ? 12.0f : 8.0f)
```

建议：

- UI 模式下色块宽度改到 18-22 px。
- 导出模式下色块宽度改到 20-24 px。
- 用圆角矩形，避免像一条细线。
- `Plugin A params` 标题区适当加宽，避免色块变大后挤压文字。

## 6. CLZ 工程同步

`.clz` 工程需要保存和恢复六轨 solo 状态。

建议字段：

```json
"previewSolo": {
  "enabledSlots": ["dryA", "dryB"],
  "fallbackToDisplaySlotsWhenEmpty": true
}
```

要求：

- 保存工程时写入当前点亮轨道。
- 打开工程时恢复点亮状态。
- 如果工程来自旧版本、没有 `previewSolo` 字段，则默认所有 solo off，继续旧逻辑。
- 如果某个 solo slot 在工程中点亮，但对应素材没有加载，应显示为 disabled/off 或 disabled-highlight，不能导致预听失败崩溃。

## 7. Job Runner 同步要求

Job Runner 不是 UI 点击器，也不需要真的播放声音，但这次功能必须能被 Job Runner 表达和复现。

### 7.1 JSON job 新增可选字段

建议支持：

```json
"previewSolo": {
  "enabledSlots": ["dryA", "dryB", "wetA"],
  "fallbackToDisplaySlotsWhenEmpty": true,
  "exportPreviewMix": true
}
```

字段含义：

- `enabledSlots`：使用与 UI 一致的六轨 source id：`dryA/dryB/dryC/wetA/wetB/wetC`。
- `fallbackToDisplaySlotsWhenEmpty`：如果为空数组，是否回退到 displaySlots。
- `exportPreviewMix`：如果为 true，导出一份与 UI 师姐按钮一致的预听混音音频。

### 7.2 输出与 manifest

Job Runner 应在 manifest 中记录：

```json
"previewSolo": {
  "enabledSlots": ["dryA", "dryB"],
  "resolvedSources": ["DRY A", "DRY B"],
  "fallbackUsed": false,
  "mixGainMode": "equal_gain_average"
}
```

如果实现 `exportPreviewMix`，建议输出：

```text
derived_audio/preview_solo_mix.wav
```

并在 manifest / response / job_summary 中登记路径。

### 7.3 与 displaySlots 的关系

- `displaySlots` 决定图中显示哪三路。
- `previewSolo` 决定预听混音包含哪几路。
- 两者不要互相覆盖。
- 如果 `previewSolo.enabledSlots` 为空，才按 fallback 设置决定是否使用 displaySlots。

### 7.4 Group Delay Job Runner 同步

Job Runner 的 Group Delay figure / CSV 也要按 WET A/B/C 的 groupDelay 数据导出，不要因为 `displaySlots` 里塞了多个 DRY 就丢失 groupDelay 曲线。

要求：

- UI Group Delay、Job Runner PNG、CSV、manifest 口径一致。
- WET A/B/C 都有 groupDelay 时，三条都能导出。
- 只有 WET A 有 groupDelay 时，只导出 WET A，不报错。

## 8. 验收清单

### 8.1 UI 预听

1. 只加载 DRY A，未点亮任何 solo：师姐按钮能按旧逻辑播放 DRY A。
2. 加载 DRY A、DRY B，点亮 DRY A：只听 DRY A。
3. 加载 DRY A、DRY B，点亮 DRY A + DRY B：听两路平均混音。
4. 加载 DRY A、DRY B、DRY C，点亮 DRY C：只听 DRY C。
5. 渲染 WET A 后，点亮 WET A：只听 WET A。
6. 点亮 DRY A + WET A：听干声与处理后混音。
7. 空间 / Layer Fit 时间进度条播放时，也遵守同一套 solo 选择。
8. 插件 insert 内部实时预听不受六轨 solo 影响，仍按当前 Bus Routing 走。

### 8.2 Group Delay

1. 只加载 DRY A + 渲染 WET A：Group Delay 正常显示 WET A。
2. 加载 DRY A + DRY B 后，已有 WET A：Group Delay 仍正常显示 WET A。
3. 加载 DRY A + DRY B + DRY C，渲染 WET A/B/C：Group Delay 显示 WET A/B/C 三条曲线。
4. 改 displaySlots 为 DRY A/B/C 后，Group Delay 仍显示 WET 曲线。
5. 导出 PNG / CSV / manifest 与 UI 一致。

### 8.3 CLZ

1. 保存工程后重开，六轨 solo 点亮状态保持。
2. 旧工程无 `previewSolo` 字段时正常打开。
3. 缺素材的 solo slot 不崩溃，不导致预听设备卡死。

### 8.4 Job Runner

1. job 中写 `previewSolo.enabledSlots = ["dryA", "dryB"]`，manifest 正确记录。
2. `exportPreviewMix = true` 时导出 `derived_audio/preview_solo_mix.wav`。
3. `previewSolo` 不改变 displaySlots 的图像显示。
4. Group Delay JobRunner 图和 CSV 不因多 DRY displaySlots 消失。

## 9. 建议验证命令

构建：

```bash
cd "/Volumes/solari 1/Codex_Work/GOODMETER"
./build.sh standalone
```

基础检查：

```bash
bash -n build.sh
```

如果新增 Job Runner 示例 job，建议补一个 smoke job，至少覆盖：

- `displaySlots = ["dryA", "dryB", "dryC"]`
- `previewSolo.enabledSlots = ["dryA", "wetA"]`
- 已渲染 Plugin A
- 导出 Group Delay PNG / CSV / manifest

## 10. 注意事项

- 不要把六轨 solo 做成文字按钮；用户明确要无标签 3 x 2 方块。
- 不要让插件内 `R` 预听走这套 solo。
- 不要让 Group Delay 继续依赖 displaySlots。
- 不要只修 UI，不同步 `.clz` 和 Job Runner。
- 不要把 previewSolo 和 displaySlots 混成一个概念。
