# Audio Doctor 论文图证据链升级单

写给 GOODMETER 施工队。本文只从论文侧提出需求，不限定具体实现方式。  
日期：2026-05-05  
相关输出包：`/Volumes/solari 1/给codex的试验田/论文输出_0505/figure_batch_missing`

## 0. 背景

论文现在最缺的不是“再多一个表”，而是从素材、处理、图表、CSV、manifest 到正文分析之间形成稳定证据链。  

0505 图包里有两类输出：

- GOODMETER Job Runner 原生图：能导出 Spectrum、Spectrogram A/B/C、Envelope、Reverb/Space、Dynamics 等基础视图。
- Codex 补充组合图：用于把多个视图和关键数据合成论文能直接使用的解释图，例如“群延时 + 瞬态包络”“EDC + Tail Spectrogram + 空间指标”“链路顺序对比”。

这说明 GOODMETER 已经具备核心分析基础，但还缺“论文图证据链模式”。如果这部分补齐，后续就不需要外部脚本拼图，也能减少 AI 在正文里空口解释数据的风险。

## 1. 立即优先级 P0：论文图证据链模式

新增一个 `Thesis Figure / 论文图` 导出模式。它不是替代现有 Audio Doctor 视图，而是在现有分析数据上做组合排版。

最低需求：

- 能选择一个论文图模板；
- 能选择参与图表的来源：`DRY A/B/C`、`WET A/B/C`、Display Slot；
- 能导出 PNG、CSV、manifest、简短说明文本；
- manifest 记录素材路径或 generated spec、裁剪区间、插件链、参数、采样率、图表模板、核心指标；
- 图中不要出现“正文可用”“建议写法”等草稿提示语；
- 图像适合 Word/PDF：白底或浅底、中文不乱码、轴线不挤、曲线不越界、指标框不遮挡图。

建议先做这些模板：

| 模板 | 论文用途 | 需要视图 |
|---|---|---|
| `Layering Spectrum` | 第三章频率分层 | Spectrum Overlay + 频段能量表 |
| `CST Spectrogram` | 第三章 Charge-Shot-Tail | Spectrogram + RMS Envelope + 阶段标记 |
| `Harmonic Fusion` | 第三章融合/掩蔽 | Spectrum Peaks + 泛音匹配指标 |
| `Group Delay Combo` | 第四章全通/相位离散 | Transient Envelope + Group Delay |
| `Reverb Space Combo` | 第四章空间与混响 | Tail Spectrogram + EDC + DRR/Early-Late |
| `Dynamics Apparent Ducking` | 第四章动态让位 | RMS Curves + apparent attenuation |
| `Chain Order Compare` | 第四章信号流顺序 | Spectrum + EDC/Envelope + 指标表 |
| `Resampling Iteration` | 第四章重采样迭代 | Stage 0/1/2 Spectrum + 动态指标 |

验收标准：

- 每个模板至少能输出一张 1800px 宽以上 PNG；
- 每张图都能追溯到 CSV；
- 轴范围自动裁剪，不允许离群点把曲线画出图框；
- 图例文字在 50% 缩放后仍可读；
- 中文标题和图例可正常显示；
- 导出的 manifest 能让另一个 AI 只读文件就知道图从哪里来。

## 2. P0：Reverb / Space 指标改成 onset-aware

0505 补图时发现一个关键问题：如果 DRR、Early/Late 从音频 0 秒开始算，而素材起音有延迟，直达声窗口会落在静音区，DRR 会变成无意义的极小值。

需要改成：

- 自动检测 onset；
- direct window 从 onset 开始，例如 onset 后 0-20 ms；
- early window 从 onset 开始，例如 onset 后 0-80 ms；
- late energy 从 early window 之后计算；
- manifest 记录 `onsetSeconds`、`directWindowMs`、`earlyWindowMs`；
- RT20、RT30、RT60 必须标注估算来源，例如 `RT60 estimated from RT30`。

验收标准：

- 对带 30-50 ms 起音延迟的 transient impact，DRR 不应因为前置静音而失真；
- 图表中指标名称明确区分 `RT20`、`RT30`、`estimated RT60`；
- EDC 曲线从有效起音后开始显示或明确标注 onset。

## 3. P0：Group Delay 组合图与可靠性门限

论文需要解释“频谱幅度变化不大，但瞬态边界被展开”的情况。单独 Group Delay 曲线不够直观，最好同时看到瞬态包络。

需要新增：

- 上半部分：处理前/处理后 RMS 或 peak envelope；
- 下半部分：Group Delay 曲线；
- 右侧指标：mean abs delay、peak delay、peak frequency、reliable bin count；
- 只统计可靠频段，不把低能量 FFT bin 的相位噪声当结论；
- manifest 写明可靠性门限，例如低于频谱峰值 45 dB 的点不纳入统计。

验收标准：

- 曲线不能越出坐标框；
- 坐标范围可自动或手动设置；
- 能从图上看出“瞬态展开”和“频率相关延时”之间的关系。

## 4. P0：Dynamics 命名与真实 sidechain 边界

当前 `Dynamics Response` 能支撑 RMS 动态变化和表观避让，但还不能支撑“插件内部 Gain Reduction”。

有两条路线，二选一即可：

### 路线 A：保守路线

继续叫 `Dynamics Response`，新增：

- `apparent attenuation` 曲线；
- target RMS 与 reference RMS 的差值；
- manifest 明确写：`negative delta means apparent attenuation, not plugin-internal gain reduction`。

### 路线 B：真实 sidechain 路线

如果要真的叫 sidechain，需要实现：

- key input；
- compressor 模块或插件侧链输入；
- gain reduction envelope；
- attack/release/ratio/threshold 参数记录；
- GR 曲线导出；
- manifest 明确记录侧链来源。

论文最后阶段建议先走路线 A，不要为了一个名词冒险。

## 5. P1：真实音效素材分析工作流

0505 图包使用了受控生成信号，适合解释机制；但论文如果只用受控信号，会显得离真实制作稍远。真实素材应作为“验证和例证”，不是替代所有机制图。

建议新增 `Material Scout / 素材筛选` 功能，面向大音效库：

- 允许设置素材根目录，例如 `/Volumes/solari 1/sfx file`；
- 按文件名、路径、时长、峰值、RMS、crest、频谱重心、瞬态密度粗筛；
- 跳过乱码或不可读命名素材，除非用户强制开启；
- 批量生成候选列表；
- 一键把候选素材送入 `DRY A/B/C`；
- 支持调用 EDIT 窗口快速裁剪；
- manifest 记录真实文件路径、source hash、裁剪区间。

真实素材最适合对应论文这些位置：

- 第三章频率分层：用真实 charge、impact、tail 或 whoosh 素材验证低频体积、中频材质、高频边界的分工。
- 第三章 CST：用真实武器蓄力、能量发射、尾流素材验证时间阶段，而不是只用生成信号。
- 第三章融合/掩蔽：用两个真实素材叠加，观察频谱重合后是否更像同一材质。
- 第四章饱和/失真：用真实 transient burst 或 energy impact 跑处理前后频谱，而不是只用正弦或合成 burst。
- 第四章混响空间：用真实 impact 或 magical hit 送入空间处理，看尾音频段残留和 EDC。
- 第四章动态避让：用真实背景能量床 + 真实冲击素材做表观 RMS 让位。

不建议把真实素材作为第二章的核心图。第二章更适合文献、等响曲线、掩蔽/响度等基础理论；真实素材放到第三、第四章更自然。

## 6. P1：链路顺序与重采样迭代模板

论文第四章需要解释“为什么效果器顺序重要”，不能只说某个效果器有什么效果。

建议新增两个模板：

### Chain Order Compare

同一素材经过两条链：

- A：Filter / Saturation / Reverb；
- B：Reverb / Saturation / Filter；

输出：

- 两条链的 Spectrum；
- 两条链的 EDC 或 Envelope；
- 谱心、crest、尾音 500 ms 能量、频段能量表；
- manifest 记录每条链的插件顺序与参数。

### Resampling Iteration

同一素材经过：

- Stage 0：原始素材；
- Stage 1：第一次处理渲染；
- Stage 2：重采样后再次处理；

输出：

- Stage 0/1/2 Spectrum；
- Stage 0/1/2 crest、RMS、谱心、中频/高频能量；
- 每一轮派生素材路径和 source hash。

## 7. P1：论文附录导出

GOODMETER 可以直接生成一个 `appendix_table.md` 或 `appendix_table.csv`，供论文附录使用。

字段建议：

- 图号建议；
- 图像路径；
- 素材来源；
- source hash；
- 处理链；
- 关键参数；
- 导出视图；
- 数据 CSV；
- 适用章节；
- 注意边界。

这样正文里不需要写“数据来自 GOODMETER Job Runner 导出的 CSV”这类突兀的话，可以把来源放在脚注或附录。

## 8. P2：视觉与导出质量

当前 GOODMETER 深色图适合软件内查看，但正式论文通常更适合浅底、克制、清晰的图。

建议增加 `academic_light` preset：

- 白底或浅灰底；
- 网格线淡；
- 颜色不花哨；
- 中文字体可读；
- 适合黑白打印时仍能区分线条；
- 图中不要出现过多 UI 风格装饰；
- 自动避开 AppleDouble `._*` 文件，不要把它们写入扫描或索引。

## 9. 为什么不建议重新做一个新 App

论文最后阶段不建议新开独立 Audio Doctor App，原因不是“新 App 没价值”，而是当前目标更适合直接作为 GOODMETER skill 扩展：

- GOODMETER 已经有 Audio Doctor、Audio Lab、Generate Signal、Job Runner、DRY/WET 路由和导出基础；
- 论文需要的是证据链模板和批处理能力，不是一个全新壳；
- 新 App 会重复做素材导入、波形、播放、导出、manifest、插件宿主，最后风险都落在最后 24 小时；
- GOODMETER 的定位本来就是桌面音频助手，多一个 `Thesis Figure / Audio Doctor` 技能更顺；
- 后续如果真的产品化，再从 GOODMETER 内部抽成独立模块也来得及。

所以当前最优路线是：在 GOODMETER 现有 standalone 的 Audio Doctor 里补“论文图证据链模式”，而不是重建一个应用。

## 10. 最后阶段建议施工顺序

1. 先修 Reverb/Space onset-aware 指标。
2. 再做 Group Delay Combo 和 Dynamics Apparent Ducking 两个组合图。
3. 接着做 Thesis Figure 导出模式的基础 manifest 和 appendix table。
4. 再接真实素材 Material Scout。
5. 最后再考虑 Chain Order、Resampling、真实 sidechain。

如果时间只够做两件事，优先做：

- Reverb/Space onset-aware；
- Thesis Figure 导出模式。

这两项最能直接减少论文图例和数据解释里的硬伤。
