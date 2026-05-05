# GOODMETER Thesis Branch Handoff - 2026-05-05

给主电脑 Codex / Claude Code 的交班说明。当前分支是论文临时备份支线，目标是让论文组这两天先用 Audio Doctor 产图、跑 manifest，不影响后续主线迭代。

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

### 论文图 preset

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

