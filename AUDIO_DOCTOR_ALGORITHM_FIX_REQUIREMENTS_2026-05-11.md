# Audio Doctor 算法层修复需求单（2026-05-11）

本文档给施工队使用，目标是把 Audio Doctor 当前已经做出来的图形能力往“算法口径可靠、论文图例可复现、答辩时说得清楚”的方向收口。优先级按论文风险和后续重跑图成本排序。

## 0. 先解释两个概念

### 0.1 现在的“临界带”图为什么还不算真正 Bark 公式

当前代码里 `makeApproximateCriticalBands(...)` 的默认分带方法是 ERB-like spacing。它也是一种听觉频带近似，但不是用户现在想写进论文里的 Bark 尺度公式。

用户希望使用的是 Bark / Zwicker 语境下的临界带公式，例如：

```text
z(f) = 13 * atan(0.76 * f_kHz) + 3.5 * atan((f_kHz / 7.5)^2)
```

以及基于 24 个临界带的频带划分。也就是说，图形已经做出来了，但算法口径还需要从“ERB-like 近似听觉频带”升级到“Bark 临界带”。如果短期内不改算法，就不能在 UI、manifest、论文图注里直接写成 Bark 临界带。

### 0.2 什么叫“跨临界带遮蔽风险模型”

现在的 Layer Fit / Fusion 风险基本是“同一频带内比较多个 stem 是否同时有能量”。它能看出几个 stem 在同一个频段、同一个时间窗里是否重叠，但还没有真正表达“一个频带里的强能量，会不会抬高相邻频带中另一个声音的听觉门槛”。

跨临界带模型要做的是：

1. 先把频率轴转换成 Bark 轴，得到 24 个临界带。
2. 对每个时间窗、每个 stem、每个临界带计算能量。
3. 对目标 stem 的某个目标临界带，不只看同带 stem，还要看其他 stem 在相邻 Bark 带里的能量是否可能对它形成遮蔽压力。
4. 用一个非对称扩散函数表示“向高频的遮蔽压力”和“向低频的遮蔽压力”不同。
5. 输出每个 band/time 的：
   - `bandEnergyDb`：本 stem 在该临界带的能量。
   - `maskingThresholdProxyDb`：由其他 stem 估算出的遮蔽压力。
   - `maskingMarginDb`：本 stem 能量减去遮蔽压力。
   - `riskIndex`：0-1 归一化风险。
   - `dominantMasker`：主要遮蔽来源是哪一个 stem。

一句话：旧模型只问“两个声音是不是挤在同一格里”，新模型要问“旁边几格里更强的声音，会不会把这格里的声音盖住”。

## 1. 必修：Bark 临界带公式落地

### 问题

当前 `makeApproximateCriticalBands(...)` 的 `bark` 分支并没有真正使用 Bark 公式，只是走了 ERB-like 逻辑。这样会导致 UI、manifest、论文图注的“临界带/Bark”说法和实际计算不一致。

### 需求

1. 新增 Bark 公式函数：

```cpp
double hzToBark(double frequencyHz);
double barkToHz(double bark);
```

`barkToHz` 可以用二分搜索反解，不必硬写解析反函数。

2. 当 `settings.bandScale` 为 `bark`、`bark_24`、`critical_bands` 或 crystal/dodecahedron 相关模式时，使用 24 个 Bark 临界带。

3. 生成每个 band 的：

```json
{
  "index": 1,
  "barkLow": 0.0,
  "barkHigh": 1.0,
  "lowHz": 20.0,
  "highHz": 100.0,
  "centerHz": 63.0,
  "scale": "bark_24"
}
```

4. manifest 中明确写：

```json
"criticalBandScale": "bark_24",
"formula": "z = 13 atan(0.76 f_kHz) + 3.5 atan((f_kHz/7.5)^2)"
```

5. 如果用户选择 ERB-like，则 UI 和 manifest 必须叫 `ERB-like`，不能叫 Bark。

### 验收

- 导出 manifest 中能看到 24 个 Bark band。
- 同一份素材用 `bandScale=bark_24` 和 `bandScale=erb_like` 跑出的 band 边界不同。
- UI 中不再把 ERB-like 图误称为 Bark 临界带。

## 2. 必修：Auto Bounce / 外部 Bounce 的采样率与长度策略

### 问题

当前 Auto Bounce 是按样本下标直接相加。若 DRY A/B/C 或 WET A/B/C 有不同 sampleRate，时间轴会错位。外部 bounceSource 存在时，采样率不一致警告还会被跳过。

### 需求

1. 所有参与 Layer Fit / Fusion 的 stem 和 bounce 在进入分析前必须统一到同一 sampleRate。
2. 默认以第一个有效 stem 的 sampleRate 为目标；如果外部 bounce 存在，也要检查并必要时重采样。
3. manifest 写清楚：

```json
"sampleRatePolicy": "resampled_to_analysis_rate",
"analysisSampleRate": 48000,
"sourceSampleRates": {
  "dryA": 48000,
  "dryB": 96000
}
```

4. 长度策略也要写清楚：

```json
"bounceLengthPolicy": "max_selected_stem_length_zero_padded"
```

或：

```json
"bounceLengthPolicy": "external_bounce_source_duration"
```

5. 如果没有实现重采样，必须显式给 warning，不允许静默比较。

### 验收

- 48k 与 96k 素材混用时不会出现右侧 bounce 时间轴异常缩短或错位。
- UI 底部或 manifest 明确记录是否重采样。
- 外部 bounceSource 也会触发采样率检查。

## 3. 必修：短素材 Dynamics 窗口分母修正

### 问题

`windowEnergy(...)` 会把实际计算范围 clamp 到有效样本内，但 `computeDynamicsMetrics(...)` 中 transient/sustain RMS 的分母仍用理论窗口长度。短素材会导致 sustain RMS 被低估，`Transient-to-Sustain` 被抬高。

### 需求

1. transient 与 sustain 窗口都用实际 clamp 后的 `actualStart/actualEnd` 计算分母。
2. manifest 记录：

```json
"transientWindowSeconds": [0.000, 0.050],
"sustainWindowSeconds": [0.050, 0.500],
"actualSustainWindowSeconds": [0.050, 0.270]
```

3. 如果 sustain 实际长度低于 50ms，给 warning：`sustain window too short for stable TSR`。

### 验收

- 用短于 500ms 的素材测试，TSR 不再因为不存在的尾部时间被错误放大。
- 长素材旧结果基本不变。

## 4. 必修：跨临界带遮蔽风险模型

### 问题

当前遮蔽风险主要来自同频带重叠、dominance 和一个粗略 upward bonus。它能做“贴合/重叠”提示，但还不能说明相邻临界带之间的遮蔽压力。

### 需求

新增一个可选分析模式，例如：

```json
"maskingModel": "cross_band_bark_proxy"
```

算法建议：

1. 对每个 time frame、每个 source、每个 Bark band 计算能量 `E[source][band][time]`。
2. 对目标 source `s` 的目标 band `b`，遍历其他 source `m` 的所有 band `k`。
3. 计算 Bark 距离：

```text
dz = z_b - z_k
```

4. 使用非对称扩散衰减：

```text
if dz >= 0:
    spreadLoss = upwardSlopeDbPerBark * dz
else:
    spreadLoss = downwardSlopeDbPerBark * abs(dz)
```

默认可以先用：

```text
upwardSlopeDbPerBark = 10-15
downwardSlopeDbPerBark = 25-35
```

具体数值做成参数，不写死在论文结论里。

5. 其他 source 对目标 band 的遮蔽压力：

```text
maskPressure = E[m][k][time] - spreadLoss
```

6. 对所有其他 source 和 band 取最大值：

```text
maskingThresholdProxyDb[s][b][time] = max(maskPressure)
```

7. 目标 stem 的安全余量：

```text
maskingMarginDb = E[s][b][time] - maskingThresholdProxyDb
```

8. 风险归一化：

```text
riskIndex = clamp((riskMarginDb - maskingMarginDb) / riskRangeDb, 0, 1)
```

例如 `riskMarginDb = 0dB`、`riskRangeDb = 18dB`。如果目标 stem 比遮蔽压力低很多，风险趋近 1；如果明显高于遮蔽压力，风险趋近 0。

### 输出字段

每个 cell 至少导出：

```json
{
  "timeStartSeconds": 0.12,
  "timeEndSeconds": 0.17,
  "bandIndex": 8,
  "barkLow": 7.0,
  "barkHigh": 8.0,
  "targetSource": "DRY B",
  "targetEnergyDb": -21.4,
  "maskingThresholdProxyDb": -18.2,
  "maskingMarginDb": -3.2,
  "riskIndex": 0.62,
  "dominantMasker": "DRY A",
  "dominantMaskerBand": 7
}
```

### 图形表现建议

新模型只影响 Layer Fit / Fusion 表，不影响原有 Spectrum、Spatial Image、Terrain 等表。

1. 左侧“所选声层”显示三个 stem 的实际能量结构。
2. 中间“析出关系”显示从跨临界带模型里算出的遮蔽/融合压力。
3. 右侧 “Auto Bounce / Fitted Result” 显示合成后的结果。
4. 风险高亮颜色只作用于析出关系图，避免污染原始能量图。

### 验收

- 低频强 stem 与高频弱 stem 同时出现时，高频 stem 的风险会随 Bark 距离变化，而不是只在同一 band 有风险。
- 两个完全错开的频带不应被判为高风险。
- 同一频带强弱差距大时，弱层风险明显上升。
- manifest 能说明 dominant masker 来源。

## 5. 必修：Spatial Image 时间窗字段修正

### 问题

空间声像图使用 FFT 窗口分析，但 cell 的 `timeEndSeconds` 当前更接近 hop 结束，而不是完整 FFT 窗口结束。这会让图注里 “window 0.44-0.52s” 的含义变得不清楚。

### 需求

1. 区分两个字段：

```json
"displayTimeStartSeconds"
"displayTimeEndSeconds"
"analysisWindowStartSeconds"
"analysisWindowEndSeconds"
```

2. 图面上建议显示：

```text
Time 0.48s | analysis window 0.47-0.49s
```

或：

```text
Time 0.48s | FFT window 0.47-0.49s
```

3. 如果为时间滑块取单点图，manifest 记录 `timePositionSeconds`。

### 验收

- 同一张图的图注、manifest、CSV 中时间点含义一致。
- 论文组不用猜这张图到底截的是哪一瞬间。

## 6. 中优：群延时曲线加能量 / 相干性 gate

### 问题

群延时曲线当前对低能量频段容易出现噪声跳动。Disperser 这类全通滤波测试没大问题，但复杂插件或素材尾部会让曲线变脏。

### 需求

1. 计算 cross-spectrum 时同时保留 dry/wet magnitude 或 coherence proxy。
2. 统计峰值、均值、低中高频平均群延时时，只统计能量高于阈值的频段。
3. manifest 记录：

```json
"groupDelayGate": {
  "minDryMagnitudeDb": -60,
  "minWetMagnitudeDb": -60,
  "coherenceProxy": "crossMagnitude"
}
```

### 验收

- 无信号频段不再主导群延时统计。
- 图上可保留曲线，但数据表用 gated stats。

## 7. 中优：频带能量 total 与 density 分开

### 问题

当前 `maskingBandPower(...)` 返回 band 内平均功率。它适合比较不同宽度频带的能量密度，但不等于该频带总能量。

### 需求

同时导出：

```json
"bandPowerDensityDb"
"bandTotalPowerDb"
```

图形默认可继续用 density，避免宽频带天然显得更大；附录数据表可以同时列出 total，方便复核。

### 验收

- 图注不再把 density 误写成 total。
- 附录可以解释“图形使用频带平均能量密度，避免高频宽带被视觉放大”。

## 8. 论文重跑影响

完成上述 1-5 后，以下内容建议重跑：

1. Layer Fit / Fusion 图。
2. 临界带晶体 / 十二面体图。
3. 所有附录中的 masking / fusion 数据表。
4. 涉及 Auto Bounce 的三 stem 图。
5. 涉及短素材 TSR / Crest / RMS 分析的数据表。

不一定需要重跑：

1. 普通 Spectrum 图。
2. 普通 Envelope 图。
3. 与 Layer Fit 无关的 Spatial Image 单素材对照图。
4. EDC/DRR/RT60 图，除非素材或插件参数变了。

## 9. 最小施工顺序

建议按以下顺序做，避免 UI 先行、算法后补造成返工：

1. Bark 公式与 24 band manifest。
2. Auto Bounce / external bounce 重采样与 warning。
3. 短素材 RMS 分母修正。
4. 跨临界带遮蔽风险模型。
5. Spatial Image 时间窗字段。
6. 群延时 gate。
7. total/density 双能量字段。

每一步做完后先跑一个极小 smoke job，再跑论文图 job。
