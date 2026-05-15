# Audio Doctor Layer Fit / Fusion 收口修正单：Auto Bounce 与分频 Solo

日期：2026-05-09  
需求方：论文组  
目标模块：GOODMETER / Audio Doctor / Layer Fit / Fusion  
优先级：高  
关联文件：`Source/AudioDoctorFigureRenderer.h`、`Source/AudioDoctorComponent.h`、`Source/AudioDoctorAnalysis.h`、`Source/AudioDoctorJobRunner.h`

## 1. 本单目的

`Layer Fit / Fusion` 当前已经能显示两栏结构：

- 左栏：所选 stem 在同一坐标空间中的关系。
- 右栏：Auto Bounce / Fitted Result。

但验收时发现两个必须收口的问题：

1. `Auto Bounce` 疑似按最短 stem 截断，导致左栏仍有能量时，右栏 bounce 结果已经消失。
2. `Band = Low / Mid / High` 时，非选中频段没有像现有 `Spatial Image` 那样变成半透明黑色/灰色轮廓，而是继续保留 stem 身份色，导致 solo 频段的视觉重点不清楚。

这两个问题会直接影响论文图例可信度：第一个是数据/图像逻辑错误，第二个是视觉语义不一致。

## 2. 问题一：Auto Bounce 被最短 stem 截断

### 2.1 现象

在 `Layer Fit / Fusion -> Spatial Image` 中选择三个 stem 后，某一时间点左栏仍能看到 stem 在部分频段有能量，但右栏 `Auto Bounce` 已经没有对应能量。

这不符合 bounce 语义。三个 stem 自动叠加时，结果长度应至少覆盖最长 stem；较短 stem 结束后应按 0 填充，而不是把整体结果裁到最短 stem。

### 2.2 疑似代码原因

请检查 `Source/AudioDoctorFigureRenderer.h` 中 `makeLayerFitAutoBounceAsset()` 一类逻辑。当前实现疑似类似：

```cpp
int samples = std::numeric_limits<int>::max();
samples = juce::jmin(samples, track.asset->buffer.getNumSamples());
```

这会让 `Auto Bounce` 取所有 stem 的最短长度。

### 2.3 正确行为

`Auto Bounce Selected Stems` 应使用最长 stem 长度：

```text
bounceLength = max(selectedStem.length)
```

叠加时：

```text
for each sample i:
    sum = 0
    for each selected stem:
        if i < stem.length:
            sum += stem[i]
        else:
            sum += 0
```

也就是说，短 stem 结束后不再贡献能量，但不能截断其它 stem。

### 2.4 声道与采样率

最低要求：

- 声道数按当前 Audio Doctor 内部通道策略处理，通常保持 stereo。
- 如果三个 stem 采样率一致，直接叠加。
- 如果采样率不一致，本单不要求大改重采样系统，但必须在 manifest / debug 字段里明确 warning，避免静默错误。

建议 warning 文案：

```text
Auto Bounce warning: selected stems have different sample rates; no resampling was applied.
```

### 2.5 验收标准

准备三个长度不同的 stem：

```text
Stem 1: 0.86 s
Stem 2: 1.77 s
Stem 3: 2.86 s
```

期望：

- `Auto Bounce` 长度接近 2.86 s。
- 在 1.5 s 左右，Stem 2/3 仍有能量时，右栏 bounce 也仍有对应能量。
- 在 2.5 s 左右，如果只有 Stem 3 仍有能量，右栏 bounce 仍显示 Stem 3 对结果的贡献。
- manifest / CSV 中的 bounce duration 与图中时间轴一致。

## 3. 问题二：Band Solo 的非选中频段需要黑灰化

### 3.1 现象

现有普通 `Spatial Image` 中，当用户选择 `Band = Mid` 时：

- 中频保持正常彩色能量山。
- 低频 / 高频会变成半透明黑色或灰色轮廓。
- 读者能一眼看到当前 solo 的是哪一段频带。

但 `Layer Fit / Fusion` 中选择 `Low / Mid / High` 时，非选中频段仍保留 stem 身份色，尤其在多 stem 叠加时会误导读者，以为那些频段仍是重点观察对象。

### 3.2 正确视觉规则

在 `Layer Fit / Fusion` 里，`Band` 选择必须与普通 `Spatial Image` / `Terrain` 的语义一致。

#### 选择 `All Bands`

正常显示：

- Stem 1 / 2 / 3 保留身份色。
- Bounce 保留 bounce 色。
- 可以显示融合 / 遮蔽 / 风险等 Layer Fit 专用 overlay。

#### 选择 `Low` / `Mid` / `High`

选中频段：

- 保留 stem 身份色。
- 保留能量高度。
- 可以显示该频段内的融合 / 遮蔽 / 风险 overlay。

非选中频段：

- 不保留 stem 身份色。
- 不显示融合 / 遮蔽 / 风险 overlay。
- 改为半透明黑色/灰色 ghost silhouette。
- 高度可以保留或轻微压低，但视觉上必须退到背景里。

建议伪代码：

```cpp
if (bandMode == allBands || frequencyInActiveBand(freq, bandMode))
{
    colour = stemIdentityColour;
    alpha = normalAlpha;
    allowLayerFitOverlay = true;
}
else
{
    colour = ghostBandColour; // black/grey
    alpha = ghostAlpha;       // e.g. 0.16 - 0.28
    allowLayerFitOverlay = false;
}
```

### 3.3 适用范围

该规则至少应用于：

- `Layer Fit / Fusion -> Spatial Image`
- `Layer Fit / Fusion -> Time-Frequency Terrain`
- `Layer Fit / Fusion -> Critical Band Terrain`

不要影响普通视图的默认配色：

- `Spatial Image`
- `Terrain`
- `Spectrogram`
- `Spectrum`
- `Reverb / Space`
- `Dynamics`

如果复用绘图函数，请通过显式配置项开启：

```text
LayerFitBandSoloGhostMode = true
```

不要把 ghost 行为硬写进所有图。

### 3.4 验收标准

用同一组 stem 测试：

1. `Band = All Bands`  
   多个 stem 都有身份色，完整显示。

2. `Band = Mid`  
   中频彩色，高频/低频为半透明黑灰轮廓，不再显示 stem 身份色。

3. `Band = High`  
   高频彩色，低频/中频退为半透明黑灰轮廓。

4. `Band = Low`  
   低频彩色，中频/高频退为半透明黑灰轮廓。

验收时请对比普通 `Spatial Image` 的 Band Solo 效果，视觉逻辑应一致。

## 4. Critical Band Terrain 暂缓说明

当前 `Critical Band Terrain` 仍然偏“方片/马赛克”形态。这个问题论文组还在讨论视觉方案，不作为本单的阻塞项。

目前可以先保留：

- 临界带离散格子图。
- 风险面积、strongest risk、band index 等指标。

但请不要把它作为最终论文主图默认形态。后续可能单独开单，把它改成：

- Bark smooth terrain
- critical-band ribbons
- 或更连续的 2.5D 临界带山体图

本单只要求：不要因为 Critical Band Terrain 的后续方案影响 Auto Bounce 和 Band Solo 的修复。

## 5. Export / Job Runner 一致性要求

如果本次修复影响数据输出，请同步检查：

1. UI Export 图像。
2. Job Runner 导出图像。
3. manifest 中的 selected stems、bounce source、duration。
4. CSV 中的时间轴长度。

重点是 `Auto Bounce`：

- 图中右栏时间轴要与 bounce duration 一致。
- manifest 不要写成最短 stem 的时长。
- 如果是自动 bounce，manifest 建议明确：

```json
"bounceSource": "auto_bounce_selected_stems",
"bounceLengthPolicy": "max_selected_stem_length_zero_padded"
```

## 6. 建议施工顺序

1. 先修 `Auto Bounce` 长度逻辑。
2. 用三条不同时长 stem 做最小 smoke test。
3. 再修 `Band Solo` ghost 视觉规则。
4. 分别检查 `Spatial Image`、`Time-Frequency Terrain`、`Critical Band Terrain` 三种视图。
5. 跑 `./build.sh standalone`。
6. 导出一组 UI 图和一组 Job Runner 图，确认图、CSV、manifest 同源。

## 7. 论文组验收口径

修完后请给论文组提供：

- 一张 `Band = All Bands` 的 Layer Fit / Fusion 图。
- 一张 `Band = Mid` 的 Layer Fit / Fusion 图。
- 一张 3 个 stem 不同时长的 Auto Bounce 验证图。
- 对应 manifest 片段，能看到 `bounceLengthPolicy` 或等价说明。
- build 结果。

论文组会重点看两件事：

1. 右栏 bounce 是否不再被最短 stem 截断。
2. 非选中频段是否像普通 Spatial Image 一样退成黑灰半透明轮廓。

这两个过了，这轮 Layer Fit / Fusion 才算真正能进入论文图例候选。
