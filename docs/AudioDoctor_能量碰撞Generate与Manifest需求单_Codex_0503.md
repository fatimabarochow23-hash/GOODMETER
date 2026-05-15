# Audio Doctor 能量碰撞 Generate 与 Manifest 可复现需求

写给 GOODMETER 施工队。本文只描述论文侧需求，不限定具体实现方式。

## 目标

Audio Doctor 的 `generate` 已经能生成基础测试信号，例如 sine、harmonic series、sweep、transient burst。下一步需要把它从“通用 DSP 测试信号生成器”升级为“服务动画能量碰撞音效论文图例的可控素材生成器”。

核心目标有两个：

1. 提供少量能量碰撞专用 preset，用来稳定生成蓄力、撞击、尾音、CST 组合等论文机制图素材。
2. manifest 完整记录生成参数、随机种子、色带、dB 映射、素材编辑、路由和插件链，保证图例可复现、可解释、可被 AI 自动读取。

这不是要做 Phase Plant 或完整合成器。它只需要生成“足够可控、足够清楚、足够适合论文分析”的测试信号。

## 一、为什么需要专用 preset

论文图例有两类素材来源：

- 真实音效素材：更接近制作语境，适合第五章和部分第三章；
- 生成测试信号：变量更干净，适合解释机制，尤其适合第二、三、四章的原理图和处理前后对比。

真实素材会包含很多不可控因素，例如原素材已经经过混响、失真、限幅、层叠或剪辑。生成信号的价值是把变量收窄，让图例能够明确说明“某个处理改变了什么”。

因此，`generate` 不需要替代真实素材，而是补上真实素材很难做到的可控实验条件。

## 二、最急需的能量碰撞 preset

### 1. `charge_riser`

用途：蓄力阶段，说明能量聚集、上升、趋向撞击点。

听觉目标：

- 能量逐渐积累；
- 频率或亮度上行；
- 包络逐渐变密或变强；
- 可用于解释“预期感”和“运动方向”。

建议组成：

- 上行 sine 或 harmonic partial；
- filtered noise 或 band-limited noise；
- 可选 AM/LFO 轻微脉动；
- 包络从弱到强；
- 可选 stereo width 从窄到宽。

关键参数：

- `seconds`
- `levelDb`
- `startHz`
- `endHz`
- `noiseAmount`
- `harmonicCount`
- `modRateHz`
- `modDepth`
- `curve`
- `seed`

### 2. `shot_impact`

用途：撞击阶段，说明瞬态、低频体积、高频边缘如何共同形成碰撞感。

听觉目标：

- 起点清楚；
- 低频有重量；
- 高频有边缘；
- 中频或谐波提供材质识别；
- 可用于解释瞬态优先级、crest、transient/sustain。

建议组成：

- low thump：短低频 sine/drop；
- high crack：短噪声或 click；
- mid body：短谐波团或 band-pass noise；
- very short envelope；
- 可选轻微随机相位。

关键参数：

- `seconds`
- `levelDb`
- `bodyHz`
- `bodyDecayMs`
- `crackHz`
- `crackAmount`
- `noiseBandLowHz`
- `noiseBandHighHz`
- `transientMs`
- `seed`

### 3. `tail_decay`

用途：尾音阶段，说明能量释放、频段残留、空间尾部、混响拖尾。

听觉目标：

- 撞击后能量逐渐释放；
- 能看出不同频段残留；
- 可用于 Reverb / Space、Tail Spectrogram、EDC 图。

建议组成：

- decaying resonant tone；
- band-limited noise tail；
- 可选 early reflection 模拟；
- 可选 stereo decorrelation。

关键参数：

- `seconds`
- `levelDb`
- `fundamentalHz`
- `decayMs`
- `tailBandLowHz`
- `tailBandHighHz`
- `damping`
- `earlyReflectionAmount`
- `stereoWidth`
- `seed`

### 4. `cst_event`

用途：完整 Charge / Shot / Tail 三阶段组合。最适合第三章时间结构和第五章案例策略。

听觉目标：

- 一个可控的能量碰撞事件；
- 能明确标出蓄力、撞击、尾音三个阶段；
- 可用于 Spectrogram A/B/C、Envelope、Dynamics、Reverb / Space。

建议组成：

- `charge_riser`
- `shot_impact`
- `tail_decay`

关键参数：

- `seconds`
- `chargeStartSec`
- `shotTimeSec`
- `tailStartSec`
- `chargePreset`
- `shotPreset`
- `tailPreset`
- `stageBalance`
- `levelDb`
- `seed`

manifest 必须写出 stage markers：

```json
"stageMarkers": [
  { "label": "Charge", "startSec": 0.00, "endSec": 0.82 },
  { "label": "Shot", "startSec": 0.82, "endSec": 0.92 },
  { "label": "Tail", "startSec": 0.92, "endSec": 2.40 }
]
```

### 5. `harmonic_fusion_test`

用途：解释谐波匹配、频谱融合、掩蔽与辨识度之间的关系。

听觉目标：

- A/B 两个素材可以有相近或错开的泛音列；
- 适合比较“融合”和“分离”；
- 可用于第二章感知基础、第三章频率分层、第四章饱和/谐波粘合。

建议组成：

- 两组 harmonic series；
- 可设置 fundamental ratio；
- 可设置 partial detune；
- 可设置 spectral overlap。

关键参数：

- `fundamentalAHz`
- `fundamentalBHz`
- `harmonicCount`
- `detuneCents`
- `overlapAmount`
- `rolloffDb`
- `levelDb`
- `seed`

## 三、通用 generate 参数要求

所有生成信号都应支持或记录以下字段：

```json
{
  "type": "cst_event",
  "preset": "thesis_default",
  "sampleRate": 48000,
  "channels": 2,
  "seconds": 2.4,
  "levelDb": -9.0,
  "seed": 4432,
  "params": {}
}
```

要求：

- `sampleRate` 必须记录；
- `channels` 必须记录；
- `seconds` 必须记录；
- `levelDb` 必须记录；
- `seed` 必须记录，即使默认生成也要写出实际 seed；
- 所有默认参数必须展开写入 manifest，不能只写 preset 名；
- 若 UI 没暴露 seed，也必须在 manifest 中记录实际 seed；
- 同一组参数重复运行，输出应保持一致。

## 四、Manifest 可复现要求

当前验收发现，部分 generated source 只记录了简略 `sourcePath`，例如：

```text
generated:transient burst -6.0 dBFS
```

这不够。不同 baseHz、brightHz、seed、duration 可能被写成同一个来源描述，导致论文图无法复现。

建议 manifest 中每个资产至少包含：

```json
{
  "slot": "dryC",
  "name": "DRY C cst_event thesis_default",
  "sourceType": "generated",
  "sourcePath": "generated:cst_event",
  "sourceHash": "generated-sha256:...",
  "generatedSignalSpec": {
    "type": "cst_event",
    "preset": "thesis_default",
    "sampleRate": 48000,
    "channels": 2,
    "seconds": 2.4,
    "levelDb": -9.0,
    "seed": 4432,
    "params": {
      "chargeStartSec": 0.0,
      "shotTimeSec": 0.82,
      "tailStartSec": 0.92,
      "startHz": 90,
      "endHz": 4200
    }
  },
  "analysisSummary": {
    "peakDb": -7.4,
    "rmsDb": -25.6,
    "crestDb": 18.2,
    "durationSeconds": 2.4
  }
}
```

### Hash 规则

生成素材的 hash 不应只 hash `sourcePath` 或显示名称。

建议：

- 将 `generatedSignalSpec` 展开成稳定排序 JSON；
- 用该 JSON 计算 `sourceHash`；
- hash 前统一数字精度，例如小数保留 6 位；
- preset 默认值展开后一起参与 hash；
- manifest 记录 `hashInputVersion`。

示例：

```json
"sourceHash": "generated-sha256:7a2f...",
"hashInputVersion": "generatedSignalSpec.v1.normalizedJson"
```

## 五、Job Runner 协议建议

Job Runner 应允许这样写：

```json
{
  "schemaVersion": 2,
  "sessionId": "thesis_cst_generate_test",
  "sources": {
    "dryA": {
      "generateSignal": {
        "type": "charge_riser",
        "preset": "thesis_default",
        "seconds": 2.0,
        "levelDb": -12,
        "seed": 1001,
        "params": {
          "startHz": 120,
          "endHz": 3200,
          "noiseAmount": 0.35
        }
      }
    },
    "dryB": {
      "generateSignal": {
        "type": "shot_impact",
        "preset": "thesis_default",
        "seconds": 1.2,
        "levelDb": -9,
        "seed": 1002
      }
    },
    "dryC": {
      "generateSignal": {
        "type": "tail_decay",
        "preset": "thesis_default",
        "seconds": 2.4,
        "levelDb": -15,
        "seed": 1003
      }
    }
  },
  "displaySlots": [
    { "slot": 1, "source": "dryA" },
    { "slot": 2, "source": "dryB" },
    { "slot": 3, "source": "dryC" }
  ],
  "views": ["spectrum", "spectrogram_abc", "dynamics"],
  "export": {
    "theme": "thesis_light",
    "colormap": "blue_amber_academic",
    "fixedDbScale": true,
    "floorDb": -80,
    "ceilingDb": 0,
    "width": 1800,
    "height": 900
  }
}
```

## 六、导出数据要求

除了 PNG，Job Runner 至少需要继续导出 CSV，并在 manifest 里能追踪每个 CSV 属于哪个源、哪个视图。

优先数据：

- `spectrum_hz_db.csv`
- `spectrum_peaks.csv`
- `envelope_seconds_dbfs.csv`
- `dynamics_rms_seconds_dbfs.csv`
- `energy_decay_seconds_db.csv`
- `spectrogram_summary.csv`
- `stage_markers.csv`

如果是 `cst_event`，额外导出：

- 每个阶段的 peak/rms/crest；
- 每个阶段的频段能量比例；
- shot 时间点；
- tail 衰减时间；
- stage markers。

## 七、与插件链和路由的关系

生成信号应与真实素材一样进入 DRY/WET 路由系统。

需求：

- `DRY A/B/C` 可由 generate 产生；
- `WET A/B/C` 可选择任意 DRY 或多个 DRY 作为输入；
- 生成信号经过插件渲染后，manifest 同时记录 generated spec 和 plugin chain；
- 如果 `DRY` 被编辑窗口裁剪，manifest 应记录裁剪信息，并说明该素材是 generated 派生资产；
- 如果 WET 由多个 DRY 混合渲染，manifest 应记录输入列表和混合方式。

示例：

```json
"renderRouting": {
  "wetA": {
    "inputs": ["dryA", "dryC"],
    "chain": "pluginChainA",
    "mixMode": "sum_then_render",
    "rendered": true
  }
}
```

## 八、论文侧最常用的 preset 组合

### 第二章：感知基础

- `harmonic_fusion_test`
- `sweep`
- `band_limited_noise`

用于解释：

- 频率感知；
- 谐波融合；
- 掩蔽和辨识度；
- 对数频率感知。

### 第三章：分层构建

- `charge_riser`
- `shot_impact`
- `tail_decay`
- `cst_event`

用于解释：

- 频率分层；
- 时间错峰；
- Charge / Shot / Tail；
- 瞬态、共鸣体、尾音的优先级。

### 第四章：效果链应用

- `harmonic_fusion_test` 送入饱和器；
- `shot_impact` 送入瞬态、压缩、限制；
- `charge_riser` 送入滤波、调制、群延时；
- `tail_decay` 送入混响和空间分析。

用于解释：

- 谐波生成；
- 瞬态重塑；
- 频段重排；
- 群延时；
- 空间尾音；
- 动态控制。

## 九、验收标准

施工队完成后，请至少提供 4 个验收 job：

1. `generate_charge_shot_tail_abc`
   - DRY A = charge_riser
   - DRY B = shot_impact
   - DRY C = tail_decay
   - 导出 Spectrogram A/B/C、Dynamics、manifest

2. `generate_cst_event`
   - DRY A = cst_event
   - 导出 Spectrogram、Envelope、Dynamics
   - manifest 中必须有 stage markers

3. `generate_harmonic_fusion`
   - DRY A/B = 两组不同 harmonic series
   - DRY C = fusion / overlap 版本
   - 导出 Spectrum、Spectrogram A/B/C

4. `generate_plugin_chain_repro`
   - DRY A = shot_impact
   - WET A = 同一素材经过插件链
   - 导出 Spectrum、Envelope、Dynamics、manifest
   - manifest 能追溯 generated spec + plugin parameters

每个 job 的验收条件：

- 图像正常，非空，非截断；
- A/B/C 显示槽顺序正确；
- PNG、CSV、manifest 路径完整；
- manifest 能复原每个素材的生成参数；
- 同一 job 重跑后 sourceHash 一致；
- 改变任意关键参数后 sourceHash 改变；
- AI 读取 manifest 后不用猜素材来源。

## 十、当前优先级

最高优先级：

- `generatedSignalSpec` 完整写入 manifest；
- generated hash 改为基于完整参数；
- 新增 `charge_riser`、`shot_impact`、`tail_decay`、`cst_event`；
- `cst_event` 支持 stage markers；
- Job Runner 支持论文导出主题参数。

次优先级：

- `harmonic_fusion_test`；
- `band_limited_noise`；
- UI 中暴露 seed；
- CSV 导出每阶段统计；
- AI 可读的 `job_summary.md`。

暂时不急：

- 完整合成器；
- 自由调制矩阵；
- 大量风格化音色库；
- 复杂 MIDI 或钢琴卷帘窗；
- 所有参数都做 UI 控件。

## 十一、给论文侧的使用原则

生成素材只用于说明机制，不用于假装还原某部影片的真实制作。论文语言应写成：

- “为了观察某类处理对频谱和动态结构的影响，本文使用可控测试信号进行对比。”
- “图中信号用于模拟能量碰撞音效中的蓄力、撞击与尾音结构。”
- “该图说明处理前后在频段分布、瞬态和尾部能量上的变化。”

避免写成：

- “某影片就是这样制作的。”
- “该插件就是工业标准做法。”
- “该生成信号代表所有能量碰撞音效。”

最终目标是让 GOODMETER 成为论文论证工具，而不是让论文变成软件说明书。
