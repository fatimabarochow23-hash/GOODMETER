# GOODMETER 如何协作论文：交班文档

更新时间：2026-05-01  
工作目录：`/Users/caiyiyang/Desktop/GOODMETER`  
目标读者：下一棒 Claude Code / Codex / Gemini  
用途：让新对话快速理解 GOODMETER 如何服务论文，而不是只把它当普通音频软件项目。

## 0. 一句话定位

GOODMETER 当前最重要的论文协作角色，是用 macOS standalone 里的 Audio Doctor 为论文第三、第四、第五章生产“可解释、可复现、可追溯”的音频图例和底层数据。

它不是为了替论文堆软件功能，也不是为了展示插件医生式工程测试界面。它要服务的写作目标是：

- 第三章：证明能量碰撞音效为什么需要频率、时间、功能分层。
- 第四章：证明效果器链如何改变频谱、瞬态、空间和动态关系。
- 第五章：证明不同动画场景为什么选择不同的分层和信号流策略。

论文端的核心要求是：图像必须能支撑论证，不能只是“看起来很专业”；每张图最好能同时关联素材、插件、参数、分析设置、曲线数据和 manifest。

## 1. 先读哪些文件

接手后建议按这个顺序读，不要一上来全仓库乱扫：

1. `GOODMETER_AudioDoctor_Handoff_for_CC.md`
   - Claude Desktop 写的工程恢复文档。
   - 很适合了解 Audio Doctor 文件地图、构建规则和旧缺口。
   - 注意：里面“JobRunner 缺 Spectrogram A/B/C”“Dynamics Sidechain 命名”这两点已经被后续 Codex 修过，见本文第 5 节。

2. `/Users/caiyiyang/Desktop/论文工作文件夹/HANDOVER_CLAUDE_CODE_论文危急交班_2026-05-01.md`
   - 论文战斗对话写的论文主线交班。
   - 它解释第三、第四、第五章为什么要从教程/技巧集合压回学术论证主线。

3. `docs/audio-doctor-requirements.md`
   - Audio Doctor 最初需求说明。
   - 它讲的是“声音证据实验室”的目标，不是当前已实现状态。

4. 关键代码：
   - `Source/AudioDoctorComponent.h`
   - `Source/AudioDoctorAnalysis.h`
   - `Source/AudioDoctorPluginHost.h`
   - `Source/AudioDoctorJobRunner.h`
   - `Source/StandaloneApp.cpp`
   - `Source/AudioLabComponent.h`

## 2. 当前工程状态

重要事实：

- Audio Doctor 相关核心文件目前仍是未追踪文件：
  - `Source/AudioDoctorAnalysis.h`
  - `Source/AudioDoctorComponent.h`
  - `Source/AudioDoctorJobRunner.h`
  - `Source/AudioDoctorPluginHost.h`
- 不要执行 `git reset --hard`、`git clean`、`git checkout -- .`、`git add .`。
- 这台机器的本地 GOODMETER 可以实验，但多 AI 正在同一工作树协作，必须小步改动。
- macOS 构建和 AU/VST3 插件真实加载，只有能访问本机 Xcode/插件目录的对话才能验证。Claude Desktop/Cowork 之类如果显示 Linux 沙箱，就只能做代码审查，不能声称验证了 standalone。

最近确认过的构建：

```bash
./build.sh standalone
```

已通过，standalone 产物在：

```text
Builds/MacOSX/build/Release/GOODMETER.app
```

## 3. Audio Doctor 当前能力

Audio Doctor 是 GOODMETER standalone 里的论文分析窗口，核心能力包括：

- 导入 Dry 音频。
- 导入 Wet 音频。
- 生成测试信号。
- 加载 AU/VST3 Plugin A。
- 加载 AU/VST3 Plugin B。
- 显示插件编辑器窗口。
- Render A / Render B，把插件处理结果写成 Wet A / Wet B。
- 导出图像、JSON、JobRunner 的 CSV/manifest。

当前主要视图：

| 视图 | 论文用途 | 适合章节 |
|---|---|---|
| Spectrum Overlay | 饱和、失真、EQ、谐波、频谱粘合 | 4.2 |
| Transient Envelope | 瞬态被展开、冲击边缘变宽、尾部衰减 | 3.3 / 4.3 |
| Group Delay | Disperser / all-pass 类处理让频率成分错开 | 4.2 / 4.3 |
| Spectrogram A/B/C | 分层、Charge-Shot-Tail、Dry/WetA/WetB 对比 | 3.2 / 3.3 / 4.2 |
| Reverb / Space | EDC、RT20/RT30/估算 RT60、DRR、Early/Late、Stereo、M/S | 4.3 |
| Dynamics Response | RMS 动态响应、瞬态/持续关系、处理前后动态变化 | 4.4 |

术语注意：

- 当前第 6 个视图已经改名为 `Dynamics Response`。
- 不要再把它叫 `Dynamics Sidechain`，因为现在没有真实的 sidechain key-input、GR meter、SC HPF 或压缩器参数反演。
- 如果论文需要“侧链让位”，当前图只能支撑“动态响应/表观让位”，不能声称测得真实 Gain Reduction。

## 4. 图例如何服务论文

不要把 Audio Doctor 的图写成“软件截图”。每张图都应该回答一个论文问题。

推荐对应关系：

- 频谱图：
  - 问题：处理是否引入谐波、改变频谱重心或压缩频谱层级？
  - 写法：不要写“插件增加了很多高频”，要写“高频谐波密度上升，使冲击层边缘更清晰，同时中频能量增强让主体层更稳定”。

- 瞬态包络：
  - 问题：冲击是否更尖、更宽，或尾部是否被拉长？
  - 写法：用于解释 Charge、Shot/Impact、Tail 三段时间组织。

- 群延时：
  - 问题：频谱能量变化不大时，为什么听起来瞬态边缘变软或被展开？
  - 写法：强调 all-pass / Disperser 类处理改变的是时间结构，而非简单 EQ。

- 时频谱 A/B/C：
  - 问题：声音层在时间与频率中如何分布？
  - 写法：适合证明低频体量、中频身份、高频边缘，以及处理后尾部如何扩散。

- Reverb / Space：
  - 问题：空间感不是“变大声”，而是尾音衰减、早晚反射比例、直达/混响能量比和声像相关性变化。
  - 写法：RT60 要写成估算值，当前算法主要由 RT30 外推。

- Dynamics Response：
  - 问题：处理后动态范围、RMS 分布、瞬态/持续比例如何变化？
  - 写法：可写“动态响应变化”“表观让位”，不要写“真实侧链增益衰减”，除非后续补了 GR 曲线或插件 meter 读取。

## 5. Codex 最后一轮实际改动

Claude 的 `GOODMETER_AudioDoctor_Handoff_for_CC.md` 是在这些改动前写的，所以读它时要用本节修正。

### 5.1 JobRunner 已补 Spectrogram A/B/C

文件：`Source/AudioDoctorJobRunner.h`

现状：

- `writeFigure()` 现在识别 `spectrogram` / `spectrogram_abc` / `waterfall` 类 view。
- JobRunner 图像会使用：
  - `Asset::spectrogramBlue`
  - `Asset::spectrogramYellow`
  - `Asset::spectrogramPink`
- 不再把 `spectrogram_abc` fallback 成 Spectrum Overlay。
- JobRunner 的 manifest 里，spectrogram 描述已更新为：

```text
STFT, fftSize=1024, hopSize=256, window=hann, linear FFT-bin image, maxWidth=2048, units=time/frequency/magnitude
```

这和 Audio Lab 对齐：1024 FFT、256 hop、线性 FFT-bin 纵轴、最多 2048 列。

### 5.2 Dynamics Sidechain 已改名

文件：`Source/AudioDoctorComponent.h`

现状：

- UI 标题已改成：`动态响应 / Dynamics Response`
- 导出 token 已改成：`DynamicsResponse`
- JSON view 字段已改成：`dynamicsResponse`

原因：当前视图只画 RMS response，不具备真实 sidechain 分析。

### 5.3 JobRunner 独立实例问题已修

文件：`Source/StandaloneApp.cpp`

现状：

- 普通 GOODMETER 仍保持单实例。
- 但带以下参数启动时允许新实例独立运行：

```bash
--audio-doctor-job
--doctor-job
```

原因：如果用户正开着旧 GOODMETER，JSON job 不能被转发给旧进程，否则会用旧代码导出旧图。

### 5.4 已做过的验证

构建：

```bash
./build.sh standalone
```

通过。

JobRunner smoke test：

```bash
Builds/MacOSX/build/Release/GOODMETER.app/Contents/MacOS/GOODMETER --audio-doctor-job /tmp/goodmeter_audio_doctor_spectrogram_job_test.json
```

输出中生成了：

```text
/tmp/goodmeter_audio_doctor_exports/codex_job_runner_spectrogram_smoke_test/figures/codex_job_runner_spectrogram_smoke_test_spectrogram_abc_ui_dark.png
```

确认图像不是旧 Spectrum，而是真 Spectrogram A/B/C。旧 GOODMETER 进程仍开着时也能独立跑 job。

## 6. JobRunner 的当前使用方式

CLI：

```bash
Builds/MacOSX/build/Release/GOODMETER.app/Contents/MacOS/GOODMETER --audio-doctor-job /path/to/job.json
```

最小 job 示例：

```json
{
  "schemaVersion": 1,
  "sessionName": "chapter_4_2_disperser_group_delay",
  "dry": {
    "path": "/path/to/dry.wav",
    "selection": { "start": 0.0, "end": 2.5 }
  },
  "pluginA": {
    "path": "/Library/Audio/Plug-Ins/Components/SomePlugin.component",
    "params": [
      { "id": "0", "value": 0.35 }
    ]
  },
  "render": {
    "slot": "A",
    "tailSeconds": "auto"
  },
  "views": ["spectrum", "group_delay", "spectrogram_abc", "reverb_space", "dynamics"],
  "export": {
    "preset": "ui_dark",
    "outDir": "/tmp/goodmeter_audio_doctor_exports"
  }
}
```

输出结构：

```text
<outDir>/<sessionName>/
  response.json
  manifest.json
  figures/
    <session>_<view>_<preset>.png
  data/
    dry_spectrum_hz_db.csv
    dry_envelope_seconds_dbfs.csv
    dry_energy_decay_seconds_db.csv
    dry_dynamics_rms_seconds_dbfs.csv
    dry_group_delay_hz_ms.csv
    wetA_*.csv
    wetB_*.csv
```

重要：

- `sessionName` 要用语义化名称，不要全靠时间戳。
- 每个图要能从 manifest 追溯素材、插件、参数和分析设置。
- 当前 JobRunner 默认偏 `ui_dark`，用户明确说论文输出也更想要 dark 图。

## 7. 仍未完全解决的缺口

这些是下一棒最值得继续做的地方。

### P0: JobRunner 与 UI Export 仍非完全同一绘图路径

已修：

- JobRunner 已支持 Spectrogram A/B/C。
- JobRunner 的 Dry/WetA/WetB 颜色已贴近 UI 语义色。
- JobRunner 写 PNG 前会删除同名旧图，避免复跑旧图残留。

仍未完全修：

- Spectrum / Envelope / Group / Reverb / Dynamics 仍是 JobRunner 自己的离屏绘图函数。
- UI Export 走 `AudioDoctorContent::drawFigure()`。
- 两条路径在背景、字号、metrics 区、画布比例上仍有差异。

下一步方向：

- 最理想：抽出共享 figure renderer，让 UI Export 和 JobRunner 都调用同一套画图逻辑。
- 现实小步：先把 JobRunner 的每个视图补齐和 UI 一样的标题、颜色、metrics 区、图例规则。

### P0: UI Export 还缺 CSV / sourceHash

UI 手动 Export 目前主要是 dark PNG + light PNG + JSON。JobRunner 有 CSV 和 sourceHash。

论文协作更理想的状态：

- UI Export 也输出 CSV。
- UI Export JSON 也写入素材 hash、插件参数、分析设置。
- 同一张图的 PNG、JSON、CSV 文件名能一眼对应。

### P1: 真 Sidechain / Ducking 分析还没做

当前 `Dynamics Response` 可以支撑 RMS 动态变化，但不能支撑真实 sidechain 结论。

如果要做，建议不要直接叫 GR，除非有可靠来源：

- 如果插件暴露 gain reduction meter：读取插件参数或 meter。
- 如果没有 meter：只能做“apparent ducking / apparent level change”，例如 Wet 相对 Dry 的 RMS 差值曲线。
- 论文里必须写清“表观动态变化”，不要假装测得插件内部 GR。

### P1: Reverb / Space 指标需要更严谨标注

当前支持 EDC、RT20、RT30、估算 RT60、DRR、Early/Late、Stereo Correlation、Side/Mid。

注意：

- RT60 是由 RT30 或 RT20 外推，不是直接测到 -60 dB。
- DRR direct 窗口固定，碰撞类瞬态可能需要后续可配置。
- Mid/Side 当前主要是标量，不是完整 M/S 频谱或时间曲线。

## 8. 新对话建议怎么接手

建议新 ClaudeCode 上工时先做这些只读核验：

```bash
rg -n "Dynamics Response|DynamicsSidechain|dynamicsResponse|linear FFT-bin|spectrogram_abc|--audio-doctor-job" Source/AudioDoctorComponent.h Source/AudioDoctorJobRunner.h Source/StandaloneApp.cpp
```

应该看到：

- `Dynamics Response`
- `dynamicsResponse`
- `linear FFT-bin`
- `--audio-doctor-job`

不应该再看到：

- `DynamicsSidechain`
- `Dynamics Sidechain`
- `dynamicsSidechain`
- `log-frequency image`

然后跑：

```bash
./build.sh standalone
```

如果用户允许，再跑一个 JobRunner smoke test。

## 9. 多 AI 分工建议

### Codex / 本机可构建对话

适合做：

- macOS standalone 构建。
- AU/VST3 插件真实加载测试。
- 修改 C++ 代码。
- 跑 JobRunner。
- 检查导出的 PNG 是否非空、是否和 UI 接近。

### Claude Desktop / Cowork / 受限沙箱

适合做：

- 只读代码复查。
- 论文需求缺口整理。
- manifest / JSON 协议审查。
- 指出术语风险，比如 Sidechain 名实不符。

不适合声称：

- 已跑 `xcodebuild`。
- 已真实加载 AU/VST3。
- 已验证 macOS GUI 行为。

### 论文战斗对话

适合做：

- 判断图例放在哪一章。
- 把图例数据转成论文语言。
- 检查正式稿是否有 AI 痕迹。
- 控制第三、第四、第五章不偏离老师反馈。

不适合直接决定工程实现细节。

## 10. 图例写进论文前的验收题

每一张 Audio Doctor 图进入论文前，至少问这 6 个问题：

1. 这张图对应哪个章节、哪个论点？
2. 图中 Dry / Wet A / Wet B 分别是什么素材或插件处理结果？
3. 是否有 manifest 能追溯素材路径、hash、插件、参数、采样率、截取区间？
4. 坐标单位是否明确，例如 Hz、dB、ms、s？
5. 图中数据是否支持论文说法，还是只是“看起来像”？
6. 术语是否准确，例如 `Dynamics Response` 不写成真实 `Sidechain GR`？

如果 6 个问题答不清，这张图先不要进正式稿。

## 11. 给下一棒的最短任务建议

如果用户说“继续修 GOODMETER 和论文协作”，建议优先级如下：

1. 让 UI Export 也输出 CSV 和 sourceHash。
2. 继续统一 JobRunner 与 UI Export 的绘图风格，至少补 metrics 区和标题规则。
3. 给 JobRunner 增加 `exportBothThemes: true` 或类似选项，一次输出 Dark + Light。
4. 给 `Dynamics Response` 增加 apparent ducking 曲线，但命名必须保守。
5. 给 Reverb / Space 的 RT60 manifest 加 `derivedFrom: RT30`。

不要一口气重构整个 Audio Doctor。现在论文需要的是可用、可解释、可复现，而不是架构完美。

