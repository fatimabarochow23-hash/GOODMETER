# Audio Doctor 链路模式图面参数摘要修复单

日期：2026-05-26  
发起：论文组 / Codex  
优先级：中高  
范围：GOODMETER Audio Doctor / JobRunner thesis figures  

## 1. 问题概述

Audio Doctor 现在可以通过 JobRunner 跑 `pluginChains.A/B/C`，并且 manifest 中已经正确记录每个 insert 的插件名称、顺序和 changed parameters。

但是在论文图导出时，图底部仍显示：

`Plugin A params default/no changed parameter`

这会让读者误以为链路没有参数变化。实际情况是：链路参数存在于 `pluginChainA/pluginChainB` 的 insert 记录中，图面摘要仍在走单插件 `pluginA/pluginB` 的 changed-parameter 显示逻辑。

这不是算法问题，也不是 manifest 缺数据；是 chain render 的图面摘要没有接入链路参数。

## 2. 复现素材与输出

当前可复现 job：

```text
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/jobs/ch46_effect_chain_order_20260526.json
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/jobs/ch54_energy_attack_chain_20260526.json
```

当前输出目录：

```text
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/outputs/ch46_effect_chain_order_20260526
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/outputs/ch54_energy_attack_chain_20260526
```

典型问题图：

```text
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/outputs/ch46_effect_chain_order_20260526/figures/ch46_effect_chain_order_20260526_thesis_01_cst_spectrogram_ui_dark_ch46_chain_order_cst.png
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/outputs/ch54_energy_attack_chain_20260526/figures/ch54_energy_attack_chain_20260526_thesis_01_cst_spectrogram_ui_dark_ch54_energy_attack_cst.png
```

manifest 已经有正确数据：

```text
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/outputs/ch46_effect_chain_order_20260526/manifest.json
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/outputs/ch54_energy_attack_chain_20260526/manifest.json
```

例如 `ch46` 的 `pluginChainA` 实际为：

```text
kHs Transient Shaper -> kHs Distortion -> kHs Reverb -> kHs Compressor
```

关键参数包括：

```text
Transient Shaper: ATTACK 52%, SUSTAIN -16%, SPEED 129%
Distortion: DRIVE +12.19 dB, DYNAMICS 58%, MIX 82%
Reverb: DECAY 3.29 s, WIDTH 124%, MIX 31%, EARLY 34%
Compressor: ATTACK 14.5 ms, RELEASE 47.1 ms, RATIO 2.8:1, THRESHOLD -9.64 dB
```

但图面仍显示 `default/no changed parameter`。

## 3. 期望效果

当 WET A/B/C 是 chain render，图底部不要显示 `default/no changed parameter`，而应显示链路摘要。

建议格式：

```text
Chain A: Transient Shaper -> Distortion -> Reverb -> Compressor
Params: TS ATTACK 52%, SUSTAIN -16% | Dist DRIVE +12.19 dB | Rev DECAY 3.29 s, MIX 31% | Comp RATIO 2.8:1, THRESHOLD -9.64 dB | +...
```

对于 WET B：

```text
Chain B: Reverb -> Distortion -> Compressor
Params: Rev DECAY 5.37 s, MIX 44% | Dist DRIVE +12.19 dB | Comp RATIO 2.8:1, THRESHOLD -9.64 dB | +...
```

显示原则：

1. 先显示链路顺序，再显示关键参数。
2. 图面空间有限时最多显示每个 insert 的前 1-2 个 changed parameters，超出用 `+N`。
3. 插件名可简写，去掉 `kHs` 前缀也可以，但不要丢失 insert 顺序。
4. 单插件 render 仍保留原来的 `Plugin A params ...` 逻辑。
5. 如果 chain 中所有 insert 都没有 changed parameters，再显示 `default/no changed parameter`，但最好写成 `Chain A params default/no changed parameter`，避免和单插件混淆。

## 4. 可能涉及源码位置

已定位到几个入口，施工队可按实际结构调整：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/Source/AudioDoctorJobRunner.h
/Volumes/solari 1/Codex_Work/GOODMETER/Source/AudioDoctorFigureRenderer.h
/Volumes/solari 1/Codex_Work/GOODMETER/Source/AudioDoctorComponent.h
```

当前相关线索：

- `AudioDoctorJobRunner.h`
  - `RenderInfo` 已有 `chainRender`、`chainCount`
  - `pluginChainA/pluginChainB/pluginChainC` 已写入 manifest
  - `changedParameters` 已能进入各 insert 的 manifest
  - `paramsText = "default/no changed parameter"` 出现在 thesis figure 绘制逻辑附近

- `AudioDoctorFigureRenderer.h`
  - `FigurePluginParam` / `changedParameters`
  - 单插件摘要同样存在 `default/no changed parameter`
  - 如果 UI 导出和 JobRunner 导出共用 renderer，需要一起修

- `AudioDoctorComponent.h`
  - UI 侧也有 chain rendered 状态和 `MIXn` 逻辑
  - 如果师姐按钮或手动导出图也会出现同样问题，需要让 UI 图和 JobRunner 图一致

## 5. 实现建议

建议新增一个小 helper，不要在绘图函数里硬拼大段逻辑：

```text
formatPluginParamSummary(...)
formatPluginChainSummary(...)
```

数据来源可以按优先级：

1. 如果当前 slot 是 chain render，读该 slot 对应的 `pluginChainA/B/C` insert 列表。
2. 每个 insert 取插件短名和 changedParameters。
3. 生成 compact text 给图面用；完整信息继续保留在 manifest 和 appendix table。
4. 如果不是 chain render，沿用原单插件 changedParameters 逻辑。

注意：图面摘要用于读者理解，不要把所有参数都挤进去。完整参数仍以 manifest 为准。

## 6. 验收方式

构建：

```bash
cd "/Volumes/solari 1/Codex_Work/GOODMETER"
./build.sh standalone
```

跑同一组 JobRunner：

```bash
APP="/Users/caiyiyang/Library/Caches/GOODMETERBuild/GOODMETER-501852389/standalone/Products/Release/GOODMETER.app/Contents/MacOS/GOODMETER"
JOBROOT="/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/jobs"
"$APP" --audio-doctor-job="$JOBROOT/ch46_effect_chain_order_20260526.json"
"$APP" --audio-doctor-job="$JOBROOT/ch54_energy_attack_chain_20260526.json"
```

验收标准：

1. JobRunner 返回 `status: ok`。
2. 新导出的 thesis PNG 底部不再出现误导性的 `Plugin A params default/no changed parameter`。
3. WET A/WET B 的图面摘要能看出链路顺序和关键参数。
4. `manifest.json` 中 `pluginChainA/pluginChainB` 仍保留完整 insert 参数。
5. 单插件 job 不回退，仍能显示原来的 changed-parameter 摘要。
6. 输出目录清理 `._*`，避免 Word / Claude / DeepSeek 误读影子文件。

## 7. 不做的事

本单不改：

- 音频算法
- group delay / reverb / dynamics / spatial terrain 指标计算
- JobRunner schema 名称
- 论文正文
- 图的整体视觉主题

本单只修链路模式图面参数摘要，让图面信息与 manifest 记录一致。
