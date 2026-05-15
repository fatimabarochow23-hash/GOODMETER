# GOODMETER · Audio Doctor 上工文档（给 Claude Code CLI）

> 写作日期：2026-05-01
> 写作者：Claude（在 Cowork 模式里做了一轮只读复查）
> 重要前置说明：写这份文档的我跑在 Linux ARM64 沙箱里，跑不了 xcodebuild、改文件需要用户授权——**这些是我的限制，不是你（CC CLI）的限制**。你跑在用户 macOS 本地，有完整 shell + 文件系统 + git 权限，可以真正 build、Edit、commit。下面凡是出现"先列清单等用户拍板"的地方，是协作礼貌不是技术限制。
> 目标读者：Claude Code CLI（接下来用户会让你接手 GOODMETER 的 Audio Doctor 模块）
> 这份文档的搭档：另一份由 Codex 输出的「论文项目情况」文档（用户会一起拖进工作目录）
>
> ⚠️ 在你回答下面的核验题之前，**不要写任何代码、不要 git add/commit、不要 Edit 任何文件**。先读，再答题，答完通过再说动手的事。

---

## 0. 你是谁、用户是谁、这个项目是什么

用户是 cyy（艺扬），同时承担两个角色：

1. **论文作者**：正在写《动画电影中能量碰撞音效的分层构建与效果链应用》。Audio Doctor 是论文图表/数据的唯一工具来源——也就是说论文里所有声学分析图、频谱、混响时间、动态对比，都靠这个工具产。
2. **工具开发统筹**：管理多个 AI（Codex、Cowork 端的 Claude、你 CC CLI）并发对同一工作树做改动。

工作风格关键词：
- 喜欢"先只读复查→明确列出能力 vs 缺口→等用户拍板→再动手"四步走
- 接受技术细节（FFT 参数、配色十六进制、文件结构）
- **强烈讨厌 AI 假装跑了某个命令**——能跑就跑、不能跑就直说限制
- 中英文混用，技术词汇用英文
- 用自我核验题确认 AI 是否真读懂代码，回答必须给具体证据（文件名 + 行号 + 代码片段）

---

## 1. GOODMETER 项目身份

- **类型**：JUCE 8 / C++17 专业音频计量套件
- **工作目录**：`/Users/caiyiyang/Desktop/GOODMETER`
- **GitHub**：https://github.com/fatimabarochow23-hash/GOODMETER.git，分支 main
- **三平台**：
  - Plugin（AU/VST3）—— 给 DAW 用
  - Standalone（macOS Desktop Pet "Nono"）—— 透明、悬浮、桌面宠物
  - iOS（Universal）—— 双页 swipe 布局
- **当前主线**：**macOS Standalone 里的 Audio Doctor 模块**（不是 iOS、不是 Plugin、不是 Nono 宠物）

详细的项目根 `CLAUDE.md` 在 `/Users/caiyiyang/Desktop/GOODMETER/CLAUDE.md`，里面有三平台架构、文件清单、构建命令、Neo-Brutalism UI 规范等。**你接管后第一件事是读那个 CLAUDE.md**。

---

## 2. Audio Doctor 模块文件地图

```
Source/AudioDoctorComponent.h     2251 行   主 UI 容器、Export、所有 draw*Plot
Source/AudioDoctorAnalysis.h       931 行   DSP 分析（spectrogram/EDC/dynamics/group delay）
Source/AudioDoctorJobRunner.h      700 行   JSON job 协议、独立绘图路径、CSV/manifest 导出
Source/AudioDoctorPluginHost.h     573 行   AU/VST3 加载、参数序列化、render
Source/StandaloneApp.cpp                    入口、菜单栏、把 Audio Doctor 接进 standalone
Source/AudioLabComponent.h                  Spectrogram 算法的"参考实现"（Audio Doctor 必须对齐）
Source/SpectrogramComponent.h               主程序 live spectrogram（meter card 用，跟 Audio Doctor 不是同一套）
```

**重要**：这 4 个 AudioDoctor*.h 文件在 git 里**还是未追踪状态**（`??`），整套约 4455 行尚未入库。任何 reset/checkout/clean 操作都会丢光，**永远不要做**。

## 3. 当前能力（已验证存在）

按 `viewMode` 下拉框列举的 6 个视图：

| ID | 视图 | 来源数据 | 备注 |
|---|---|---|---|
| 1 | Spectrum Overlay | `Asset::spectrum` | dry/wetA/wetB 同图叠加 |
| 2 | Transient Envelope | `Asset::envelope` | 时域包络 |
| 3 | Group Delay | `Asset::groupDelay` | dry vs wet 传输函数 |
| 4 | Spectrogram A/B/C | `spectrogramBlue/Yellow/Pink` | 蓝/黄/粉三色调色板，刚和 Audio Lab 对齐 |
| 5 | Reverb / Space | `spaceMetrics + energyDecay` | EDC/RT/DRR/EarlyLate/Stereo |
| 6 | Dynamics | `dynamicsMetrics + dynamicsRms` | 名字叫 "DynamicsSidechain" 但只是 RMS 叠图 |

**辅助能力**：
- Dry / Wet 音频导入
- Plugin A / Plugin B 加载（AU/VST3）+ render 到 wet 缓冲
- Generate 菜单（合成测试信号）
- freqZoom 6 档（20-20k / 20-200 / 100-1k / 250-4k / 1k-8k / 5k-20k）
- themeMode（Dark / Light）
- Export：一次产 dark+light 双 PNG + 一个 JSON
- JSON Job Runner：支持程序化提交可复现分析任务，独立的 figures/ + data/ + manifest.json 输出结构

---

## 4. 构建命令（macOS only）

**默认始终 Release。**

```bash
./build.sh standalone   # 你 80% 的时间用这条
./build.sh plugin       # AU/VST3 only
./build.sh ios          # 当前不优先
./build.sh ios-sim      # 当前不优先
./build.sh resave standalone   # 只 resave .jucer，不编译
```

构建产物（standalone）：`Builds/MacOSX/build/Release/GOODMETER.app`

⚠️ **不要 codesign / notarize**，testing 阶段不需要。
⚠️ **不要修改 PluginProcessor::processBlock 的 DSP 主路径**，除非用户明确要求。
⚠️ **不要去掉 standalone 的输出 muting**（防反馈保护）。

---

## 5. 最近一次重要修改：Spectrogram 对齐 Audio Lab（2026-04-30）

**问题**：同一份素材 `DISPERSER_001.wav` 在 Audio Doctor 的 Spectrogram A/B/C 里比 Audio Lab 的 Spectro "曝光更多"。

**根因**：不是素材问题。Audio Doctor 之前用了对数频率纵轴 + 720 列；Audio Lab 用线性 FFT-bin 纵轴 + 1024 FFT + 256 hop（75% overlap）+ 最多 2048 列。对数轴在像素上把低频区域拉开，看上去更亮。

**修复点（两个文件、两处）**：

1. `Source/AudioDoctorAnalysis.h` 的 `computeSpectrogramImage`（约 643-735 行）：
   - `imageWidth` 默认改为 `2048`
   - `hopSize = fftSize / 4` （= 256）
   - `renderWidth = jmin(imageWidth, numFrames)`
   - **取消对数频率重采样**，直接按 FFT bin 出图
   - `y = halfFFT - 1 - bin`（线性 FFT-bin 纵轴）
   - 归一化保留 `(logMag - (logMax - 4.0f)) / 4.0f`

2. `Source/AudioDoctorComponent.h` 的 `drawSpectrogramPanel`（约 1680-1715 行）：
   - 右侧频率刻度从对数位置改为线性 `norm = freq / nyquist`
   - `y = contentArea.getBottom() - norm * height`

**验证方法**：拿同一 wav 在 Audio Doctor 和 Audio Lab 各导一次截图，肉眼比对应该接近。

---

## 6. 已知缺口（2026-05-01 复查结论）

按优先级。这部分**最重要**，因为接下来 Codex 或者你被指派去填这些坑时，要拿这份清单做验收基准。

### 高优（论文阶段必修）

1. **JobRunner 缺 Spectrogram A/B/C 视图**
   - `AudioDoctorJobRunner.h` 的 `writeFigure` 没有 spectrogram case
   - 提交 `views: ["spectrogram_abc"]` 会 fallback 成 spectrum overlay
   - **影响**：你刚费力对齐 Audio Lab 的视图，程序化路径拿不到

2. **JobRunner 与 UI Export 是两套独立绘图代码**
   - JobRunner 自己实现了 `drawTimePlot / drawFrequencyPlot / drawPlotBackground / drawCurve`
   - 不调 `AudioDoctorContent::drawFigure`
   - 颜色硬编码 `0xFF08CFE8 / 0xFFFFC94F / 0xFFFF2D78`，不走 `GoodMeterLookAndFeel::accentBlue/Yellow/Pink`
   - 画布固定 `1800×1200` vs UI 的 `panel × 2x` 超采样
   - JobRunner 不画 Glass plate 背景、不画 dense metrics 区
   - **影响**：手动 Export 和脚本 Export 出来的不是同一张图，论文里两套图风格

3. **"Dynamics Sidechain" 名实不符**
   - 视图标题写"动态让位 / Dynamics Sidechain"
   - 实际 `drawDynamicsPlot` 只是叠 dry/wetA/wetB 的 RMS envelope
   - `DynamicsMetrics` 只有 `rmsRangeDb / P10/P50/P90 / transientToSustainDb`
   - **没有** GR (Gain Reduction) 包络、SC HPF 频响、key-input 分析
   - **影响**：论文如果谈 sidechain duck，目前只能拿"两条 RMS 叠图"撑场

4. **UI Export 不导出 CSV**
   - 只有 dark/light 两张 PNG + 一个 JSON（曲线 600-800 点抽样）
   - **影响**：审稿人/合作者拿不到机器可读全密度曲线

5. **JobRunner 同次任务只产单一主题**
   - 由 `preset` 字段决定 dark 或 light
   - **影响**：论文 dark/light 双版本无法一次出齐

### 中优（提升说服力）

6. RT60 = RT30 × 2 外推（不是直接拟合到 -60 dB），manifest 没有 `derivedFrom` 标注
7. Mid/Side 只有 `sideToMidDb` 一个标量，缺 M/S 分通道频谱、时间曲线
8. DRR direct 窗口固定 0-20ms，对短瞬态偏小，不可配置
9. onset 检测用 -36 dB below peak 硬阈值，对前回响素材会被反射尾拉偏
10. JobRunner manifest 有 `sourceHash (fnv1a64)`，UI Export JSON **没有**
11. JobRunner sessionName 默认带时间戳，重复跑追不上，需要稳定语义化命名约定

### 低优（锦上添花）

12. 没有插件参数表叠加在图上
13. 没有素材-插件 batch matrix 任务
14. 没有 PDF/SVG 矢量导出
15. 没有实验索引（index.csv / README.md）自动生成
16. 没有 dry vs wet spectrogram diff 模式

---

## 7. 多 AI 协作的硬规则（你必须遵守）

1. **大改前先报范围**。你有完整写权限和 git 权限，但因为是多 AI 协作（用户同时跟 Codex / Cowork Claude / 你协作），动手前先口头列出"打算动哪几个文件、改哪几行、影响什么"，等用户拍板再改。小修小补（typo、注释、明显 bug）可以直接改。**不是不让你写**，是不让你不打招呼就大改。
2. **不要 reset / checkout / clean / git stash drop**。工作树里有大量未提交改动，包括 4 个 AudioDoctor*.h 文件本身就是未追踪状态，任何粗暴的"清理"操作都会丢光别人的活。commit 时只 `git add` 你确认是自己改的文件，不要 `git add .`。
3. **不要 codesign / notarize**。
4. **不要让 in-CLI 的自己跑 update-config skill 改 ~/.claude/settings.json 的 model 字段**——会把字符串翻译成 slug 触发公司网关 401。所有 model 切换由用户手动改 zshrc + settings.json。
5. **CLAUDE.md 是项目宪法**。三平台架构、JuceLibraryCode 隔离、Neo-Brutalism UI 规范、Nono 眼线粗细等约定全在那里，不要违反。
6. **跨平台改动用 build.sh**。绝不直接 `Projucer --resave` 后再编译 plugin/standalone/iOS，必经 build.sh 的 .jlcode_cache 隔离。
7. **JuceLibraryCode 是三个 .jucer 共享的目录**。手贱 resave 会污染另外两个 target。

---

## 8. 自我核验题（你必须答对才算上工合格）

像 Codex 之前考 Cowork 端 Claude 那样，请你读完上面 + 项目根 `CLAUDE.md` + Codex 输出的论文文档之后，回答下面 5 题。回答必须给**文件名 + 行号 + 代码片段**作为证据，光给结论不算分。

### 第 1 题：Spectrogram 对齐题（基础理解）

为什么 Audio Doctor 的 Spectrogram A/B/C 曾经比 Audio Lab 看起来"曝光更多"？最近一次修复改了哪两个文件、哪两个核心点？

**合格答案应包含**：
- 不是素材问题，是 Audio Doctor 曾用对数频率纵轴 + 720 列，低频区域被像素拉开
- Audio Lab 是线性 FFT-bin 纵轴、1024 FFT、256 hop、最多 2048 列
- 修复在 `AudioDoctorAnalysis.h::computeSpectrogramImage` 和 `AudioDoctorComponent.h::drawSpectrogramPanel`
- 给出 hopSize、renderWidth 计算公式、`y = halfFFT - 1 - bin` 的字段证据

### 第 2 题：JobRunner vs UI Export 一致性（缺口理解）

JobRunner 和 UI Export 当前是不是同一套绘图代码？如果不是，请列出至少 4 个差异点，并指出哪个差异最伤论文（即让同一素材两条路出的图最不一致）。

**合格答案应包含**：
- 明确说"不是同一套"
- 列出绘图函数不同、颜色硬编码 vs LookAndFeel 令牌、画布尺寸、背景风格、metrics 区、视图覆盖、主题输出数量等差异
- 指出最伤论文的差异（推荐答案：JobRunner 缺 spectrogram 视图 或 颜色不走令牌）

### 第 3 题：Sidechain 名实不符（细节核查）

`viewMode` 第 6 项命名为 "DynamicsSidechain"，请问当前代码里是否真有 sidechain 分析？如果没有，列出"真 sidechain 分析"应该包含但当前缺失的至少 3 个要素。

**合格答案应包含**：
- 明确说"没有真 sidechain"
- 引用 `drawDynamicsPlot` 只画 RMS envelope 的事实
- 引用 `DynamicsMetrics` struct 字段（只有 RMS 百分位 + transientToSustain）
- 列出缺失要素，例如 GR 包络、SC HPF 频响、key-input 频谱、压缩比反演等

### 第 4 题：构建一致性（流程合规）

如果用户让你"快速跑一下 plugin 看能不能编过"，但工作树里 standalone 的 jucer 是 dirty 的，你应该：
A. 直接 `./build.sh plugin`
B. 先 `git stash`，再 `./build.sh plugin`，再 stash pop
C. 直接 `./build.sh plugin`，因为 build.sh 里的 .jlcode_cache 已经做了 target 间隔离
D. 先 `Projucer --resave GOODMETER_Plugin.jucer` 再 `xcodebuild`

**正确答案：C**。理由：build.sh 的 `.jlcode_cache/` 机制会针对每个 target 维护独立的 JuceLibraryCode 快照，stash 反而会丢失 4 个未追踪的 AudioDoctor*.h 文件。D 完全错，会污染另外两个 target 的 JuceLibraryCode。

### 第 5 题：你打算先动哪个缺口（独立判断）

读完缺口清单后，假设用户拍板让你动手填坑，你建议从哪一个开始？说出理由（约束、风险、对论文的影响）。**这道题没有标准答案**，但回答应该体现：
- 你理解高/中/低优排序的依据
- 你理解 Audio Doctor 服务于论文这个上层目标
- 你不会盲目"全都修一遍"，会承认带宽有限

参考方向（不强制）：JobRunner 复用 UI 绘图路径（一举消灭缺口 1+2）、UI Export 加 CSV（缺口 4）、给 Dynamics 补真 GR 包络（缺口 3）。

---

## 9. 验收题（用户后续会拿这些题查 Codex 改完的成果）

如果之后有人（Codex 或其他 AI）声称修复了某个缺口，用户可能会让你拿这些题验收。这些题是**给你将来用的工具题**，不是现在要你答。

### V1：JobRunner 和 UI Export 真的对齐了吗？

具体核查：
- `AudioDoctorJobRunner::writeFigure` 内部是不是改成调 `AudioDoctorContent::drawFigure(g, area, true)` 了？
- 是否还有硬编码 `0xFF08CFE8 / 0xFFFFC94F / 0xFFFF2D78`？grep 一下应该 0 命中
- JobRunner 现在能不能输出 spectrogram_abc 视图？跑一个 JSON job 验证
- JobRunner 是否也输出 dark + light 两张？

### V2：Dynamics 真的成为 Sidechain 了吗？

具体核查：
- `DynamicsMetrics` 是否新增了 GR 包络字段（如 `gainReductionDb` 时间序列、瞬时 max GR）
- `drawDynamicsPlot` 是否新增了一条 GR 曲线（不只是 dry/wet RMS 叠图）
- 至少有一个口径，例如 `instantaneousGrDb = 20 * log10(wet_rms / dry_rms)` 计算并展示
- manifest JSON 里是否同步导出 GR 数据

### V3：UI Export 加了 CSV 吗？

具体核查：
- `AudioDoctorContent::exportFigure` 是否新增 CSV 写入逻辑
- 命名是否符合论文检索（建议格式：`AUDIODOCTOR_<dryName>_<view>_<timestamp>_<curve>.csv`）
- CSV 是否是全密度（不是 600-800 点抽样）
- 单元和 header 是否写在第一行

### V4：sourceHash 是否在 UI Export 加上了？

具体核查：
- UI Export 的 manifest JSON 里是否新增 `sourceHash`（与 JobRunner 用同一个 fnv1a64 算法）
- 同一素材在 UI Export 和 JobRunner 出的 hash 应该完全相等

### V5：Spectrogram 对齐没倒退？

回归核查（重要）：
- `computeSpectrogramImage` 的 `imageWidth = 2048 / hopSize = fftSize/4 / y = halfFFT-1-bin` 三个点是否仍然成立
- `drawSpectrogramPanel` 的 `norm = freq / nyquist` 是否仍然是线性
- 拿同一素材 export 一次和 Audio Lab 比对

### V6：构建是否仍然三平台干净？

回归核查：
- `./build.sh plugin → standalone → plugin` 三连应该全部成功且互不污染
- 检查 `Builds/MacOSX/build/Release/GOODMETER.app` 存在
- 检查 `.jlcode_cache/` 里有 standalone 和 plugin 两个独立快照

---

## 10. 跟 Codex 输出文档的对接

用户会另外让 Codex 输出一份**论文项目情况**文档，重点是：
- 论文章节结构、当前撰写进度
- 哪些章节需要 Audio Doctor 出图、出哪种图
- 论文用到的素材清单、命名规则
- 引用文献状态、待补章节

你接管后**两份文档要一起读**，因为：
- 我这份文档告诉你**工具能做什么、缺什么**
- Codex 那份告诉你**论文需要什么图、什么数据**
- 两份对照才能判断"目前的 Audio Doctor 是否真的能撑起论文"

如果 Codex 文档里写的论文需求超出 Audio Doctor 当前能力，请把超出部分追加进第 6 节的缺口清单，并标注"由论文文档新增"。

---

## 11. 沟通约定

- 你完成上工核验题后，把答案完整发给用户审阅。**通过了再开始动任何代码**（这是为了证明你真懂了，不是技术限制）。
- 涉及"我打算改 X 文件 Y 行"的大改提议，**先列出 diff 草稿**，等用户口头说"动手"。小修小补可以直接改完汇报。
- 跑 build / git / xcodebuild 时把真实输出贴回来，错误信息不要省略也不要美化。你能真跑，所以请真跑。
- 遇到不确定的地方，先 grep 找证据，不要靠记忆瞎猜。
- 中英文混用 OK，技术词用英文。

---

## 12. 上工 checklist（按顺序做）

1. ☐ 读项目根 `CLAUDE.md`
2. ☐ 读这份文档（你正在做）
3. ☐ 读 Codex 输出的论文文档
4. ☐ `git status --short` 看一眼当前工作树状态（**只看，别 reset/checkout**）
5. ☐ `git log --oneline -10` 看最近 10 个 commit
6. ☐ `./build.sh standalone` 跑一次确认现在 main 分支是 build-green 的（你能真跑，请真跑；如果失败把 stderr 完整贴回来）
7. ☐ 答第 1-5 题，把答案发给用户审阅
8. ☐ 等用户验收通过
9. ☐ 然后才进入"接下来要动手填哪个缺口"的讨论

---

End of handoff. 祝你接班顺利，别把 Audio Doctor 干坏了。
