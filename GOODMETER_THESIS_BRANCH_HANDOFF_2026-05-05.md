# GOODMETER Thesis Branch Handoff - 2026-05-05

给主电脑 Codex / Claude Code 的交班说明。当前分支是论文临时备份支线，目标是让论文组这两天先用 Audio Doctor 产图、跑 manifest，不影响后续主线迭代。

## 最紧急任务：给老师一份安全可下载版

这份交班的第一优先级不是继续打磨 UI，也不是继续改 `academic_light`，而是：

**在主电脑上把当前 Audio Doctor 论文分支构建成一份老师可以下载、普通双击打开的安全版 GOODMETER。**

原因：

- 论文指导/检查这两天需要看到工具能跑、图能导出、数据能复核。
- 这台电脑能构建和推分支，但没有 Developer ID 证书私钥，也没有可用 notarytool profile。
- 老师电脑如果下载未公证 app，会被 Gatekeeper 拦截，不适合作为正式演示包。
- 主电脑预计有开发者证书或更接近发布环境，所以请优先在主电脑完成签名、公证、staple 和下载包上传。

请主电脑接手后优先做：

1. 拉取本分支：`codex/thesis-audio-doctor-20260505`
2. 构建 standalone。
3. 修正并运行 `sign_and_notarize.sh`。
4. 产出 `.pkg` 或 `.zip`，确认 `spctl` / notarization 通过。
5. 把下载包上传到 GitHub 论文支线或 release/附件位置，给老师使用。

## 为什么要做 Audio Doctor

用户当前论文方向是动画电影中“能量碰撞音效”的分层构建与效果链应用。论文现在最需要的不是再做一个普通音频软件功能，而是需要一个可复核的图例与数据工具，帮助把“听感判断”落到可展示、可引用、可复验的证据上。

Audio Doctor 的定位：

- 它是 GOODMETER 内部服务论文的分析/出图/manifest 工具。
- 它不是为了替代 DAW，也不是为了做完整 PluginDoctor 克隆。
- 它的重点是把 Dry/Wet、插件链、效果器参数、频谱/包络/群延时/混响/动态等信息固定成论文可用的图和数据。
- 它服务第二、三、四章，尤其是第四章“理论落地成效果链策略”的图例论证。

论文中的核心使用场景：

- 第三章：解释分层不是简单低中高堆叠，而是通过频谱占位、瞬态边界、掩蔽关系、泛音关系让不同素材合成一个事件。
- 第四章：解释效果链为什么这样排：饱和/失真生成可感知泛音，EQ/共鸣整理材质身份，all-pass / disperser 改变瞬态展开，动态处理维持撞击边界，混响/空间塑造规模与距离但不能吞掉清晰度。
- 附录/方法论：每张图要能追溯素材、hash、采样率、截取区间、插件、参数、渲染延迟、FFT/window/hop、指标单位。

因此 Audio Doctor 的核心价值是：

```text
导入/生成素材 -> 加载插件 -> 设置参数 -> 离线渲染 -> 分析曲线 -> 导出论文图 -> 导出 CSV/manifest -> 支撑论文文字
```

## 迭代历史简述

这条线不是一天做出来的，接手时请按这个历史理解，不要误以为只是一个新增页面：

1. 起点是 GOODMETER 原有表资产和 Audio Lab 表资产。
   - 用户明确要求复用已有表的绘制/算法，不要另起一套草稿美术。
   - Spectrum 和 Spectrogram 后续都围绕“和原 GOODMETER / Audio Lab 算法一致”反复校验。

2. 早期 Audio Doctor 先做手动 UI。
   - 支持导入 Dry/Wet、生成测试信号、加载 AU/VST3 插件、渲染、导出图。
   - 后来发现直接把插件 UI 塞主窗口不合适，改成独立插件编辑窗口思路。

3. 插件路线从单 Plugin 扩展到 Plugin A/B/C。
   - 用户需要比较不同插件或同插件不同参数。
   - Wet A/B/C 都应该来自明确的 Dry 源和插件渲染，而不是含糊地拿手动 Wet 文件替代。

4. 路由从简单按钮发展到 Dry A/B/C + Wet A/B/C + display slots / render routing。
   - 这是为了论文“可控制变量”体系。
   - 也就是说，图上显示谁、谁被哪个插件处理、Wet 来自哪个 Dry，都必须可追踪。

5. 发现“让 AI 点 UI”不可靠，改成 Job Runner。
   - 论文组明确要求不是模拟鼠标，而是提交可复现 JSON 任务。
   - Job Runner 现在负责导入/生成素材、加载插件、设置参数、渲染、分析、导图、导 CSV、导 manifest。

6. 后续围绕论文图补了 Reverb/Space、Dynamics、Group Delay、Spectrogram A/B/C、CST 组合图等。
   - Dynamics 已明确改成 Dynamics Response / apparent attenuation，不假装测插件内部 GR。
   - Group Delay 走可解释的 transfer/group-delay 数据，不把不可靠频点当结论。
   - Reverb/Space 输出 EDC、RT20/RT30/估算 RT60、DRR、Early/Late、Stereo/M/S 相关指标。

7. 最近补的是 manifest 信息层。
   - 插件参数不再只写 `index/value`，而是导出人能读懂的 `name/displayValue/label/normalizedValue`。
   - 素材基准加入 `sourceFileName/sourcePath/sourceHash/selection/analysisDuration/sampleRate/channels/peak/rms/crest`。

## 当前分支用途

- 分支内容是 GOODMETER 桌面版 + Audio Doctor 论文工具的工作快照。
- 重点不是发布主线，而是给论文组临时跑图、导出数据、验证第四章图例。
- 不要直接把本分支整体覆盖到主线；主线合并时请按文件和功能挑选。

## 这两天主要完成内容

### Audio Doctor 核心

- 新增/扩展 Audio Doctor 相关文件：
  - `Source/AudioDoctorAnalysis.h`
  - `Source/AudioDoctorComponent.h`
  - `Source/AudioDoctorFigureRenderer.h`
  - `Source/AudioDoctorJobRunner.h`
  - `Source/AudioDoctorPluginHost.h`
- 支持 Dry A/B/C、Wet A/B/C、Plugin A/B/C，以及 display slots 和 render routing。
- Job Runner 支持本地 JSON 分析任务，输出 PNG、CSV、manifest、appendix table、summary。
- 频谱图、瞬态包络、群延时、时频谱 A/B/C、Reverb/Space、Dynamics Response、论文组合图都已接入。

### 论文图 preset（非当前第一优先级）

注意：`academic_light` 只是论文白底导出配色，不是当前最紧急的交接重点。主电脑接手时，优先级应为：

1. 先 build + codesign + notarize 出老师能打开的安全版。
2. 再让论文组跑 Job Runner 图和 manifest。
3. 最后才根据老师意见调整白底/黑底图风格。

- 新增 `academic_light` 出图 preset，供论文白底图使用。
- 论文组只要在 job JSON 里写：

```json
"export": {
  "preset": "academic_light"
}
```

- 当前色彩出口：
  - Dry: `#0077A3`
  - Wet A: `#E76F00`
  - Wet B: `#C2185B`
  - Text primary: `#111827`
  - Text secondary: `#374151`
  - Plot fill: `#EEF2F7`
  - Grid: `#D7DEE8`
  - Paper: `#FFFFFF`
- 普通 `ui_light` 仍保留偏艺术的浅色瀑布图风格；论文建议用 `academic_light`。

### Thesis figures

Job Runner 新增论文组合图入口：

```json
"thesisFigures": [
  "cst_spectrogram",
  "group_delay_combo",
  "dynamics_apparent_ducking",
  "reverb_space_combo"
]
```

用途：

- `cst_spectrogram`：CST 三阶段事件图，RMS stage markers + spectrogram tracks。
- `group_delay_combo`：瞬态包络 + group delay + 可信门限说明。
- `dynamics_apparent_ducking`：RMS 对比 + apparent attenuation。注意这是表观让位，不是插件内部 GR。
- `reverb_space_combo`：EDC + tail spectrogram + RT/DRR/Early/Late 指标。

### Manifest / 附录复核

- manifest 现在包含素材基准：
  - `sourceFileName`
  - `sourcePath`
  - `sourceHash`
  - `selection.startSeconds`
  - `selection.endSeconds`
  - `analysisDurationSeconds`
  - `sampleRate`
  - `channels`
  - `peakDb`
  - `rmsDb`
  - `crestDb`
- Plugin manifest 现在导出完整参数快照，不只是 index/value：

```json
{
  "index": 0,
  "id": "param_00",
  "name": "Low",
  "label": "",
  "normalizedValue": 0.65,
  "displayValue": "+4.80 dB",
  "nameUnavailable": false
}
```

- 如果插件不给可读参数名，fallback 为 `param_00`，并写 `nameUnavailable: true`。
- 为兼容旧脚本，仍保留英式旧字段 `normalisedValue` 和 `valueText`。

### 已验证

在这台机器上通过：

```bash
./build.sh standalone
```

并用 Job Runner 跑过：

```bash
Builds/MacOSX/build/Release/GOODMETER.app/Contents/MacOS/GOODMETER \
  --audio-doctor-job /tmp/goodmeter_academic_light_preview.json
```

输出过 `academic_light` 预览图和 manifest。

## 主电脑需要完成的发布/公证任务

这台电脑没有 Developer ID 证书私钥，也没有 notarytool profile，所以不能生成真正给老师普通双击可打开的公证版。

项目里已有脚本：

```bash
sign_and_notarize.sh
```

脚本里已有：

- Team ID: `33NJKA4738`
- App identity: `Developer ID Application: Yiyang Cai (33NJKA4738)`
- Installer identity: `Developer ID Installer: Yiyang Cai (33NJKA4738)`
- Notary profile name: `GOODMETER-Notarization`

主电脑 Codex 请做：

1. 确认主电脑钥匙串有两个有效身份：

```bash
security find-identity -v -p codesigning
```

应该能看到：

```text
Developer ID Application: Yiyang Cai (33NJKA4738)
Developer ID Installer: Yiyang Cai (33NJKA4738)
```

2. 确认 notarytool profile 可用：

```bash
xcrun notarytool history --keychain-profile GOODMETER-Notarization
```

如果没有，使用 Apple ID app-specific password 配置。不要把密码写进仓库。

3. 修正 `sign_and_notarize.sh` 里的旧绝对路径。

当前脚本顶部仍可能写着：

```bash
PROJECT_DIR="/Users/MediaStorm/Desktop/GOODMETER"
```

建议改为脚本目录自动定位：

```bash
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
```

4. 运行签名、公证、staple。

```bash
./sign_and_notarize.sh
```

5. 把最终 `.pkg` 或 `.zip` 上传到 GitHub 论文分支 release / 分支附件说明里，给老师下载。

## Git 分支建议

- 论文备份分支名：`codex/thesis-audio-doctor-20260505`
- 这个分支可以给论文组和主电脑 Codex 拉取：

```bash
git fetch origin
git switch codex/thesis-audio-doctor-20260505
```

如果主线要吸收 Audio Doctor，不建议整分支 merge。建议主电脑按以下模块挑：

- Audio Doctor 核心文件：`Source/AudioDoctor*.h`
- `Source/StandaloneApp.cpp` / `Source/SkillTreeComponent.h` 中 Audio Doctor 入口相关改动
- `GOODMETER.jucer` 和 `Builds/MacOSX*.xcodeproj` 中新增文件引用
- docs 中 Audio Doctor 需求单

## 给论文组的一句话

这版 Audio Doctor 已经可以用 Job Runner 可复现地产出论文图、CSV 数据、manifest 和 appendix table。论文白底图使用 `academic_light` preset；插件参数和素材基准现在进入 manifest，后续附录可以作为可复核实验记录。
