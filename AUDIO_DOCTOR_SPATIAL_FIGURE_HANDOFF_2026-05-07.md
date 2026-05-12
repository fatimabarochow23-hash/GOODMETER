# Audio Doctor 空间与立体图交工说明（2026-05-07）

面向：论文组 / 后续 GOODMETER 协作 AI  
项目：GOODMETER / Audio Doctor  
核心目的：为论文第四章提供更有“展示肌肉”价值的图例，尤其是时频谱、混响空间、空间印象相关图。

---

## 1. 这轮新做了什么

本轮重点不是普通 UI 美化，而是把 Audio Doctor 的论文图例能力向前推了一截：

1. **2.5D Spatial Terrain 立体图**
   - 将原本平面瀑布/时频图反哺成 2.5D 山峦式体积图。
   - 支持多个视角：`Front High`、`Front Low`、`Diagonal`、`Side Low`、`Side High`。
   - 支持时间方向翻转，用来避免瞬态素材开头一堵“高墙”挡住后续尾音。
   - 当前可用于 Spectrogram A/B/C、Reverb / Space 等有时频信息的图例。

2. **Spatial Image / L-C-R 空间印象图**
   - 新图不再是普通时频瀑布，而是以 **L-C-R 为横轴、频率为纵轴、能量密度为高度** 的空间印象图。
   - 颜色在这张图里对应 **L-C-R 位置**：蓝绿色偏左，黄色接近中心，红/粉色偏右。宽度、Side/Mid 与相关性主要影响山体的横向扩散和能量形态，不再直接作为颜色含义。
   - 支持时间位置选择，图中显示类似 `Time 1.88s | window 1.74-2.02s` 的时间窗。也就是说它不是只给一个全局平均，而是可以挑声音事件的关键时刻看空间形态。

3. **图面布局修复**
   - Spatial Image 之前 20k 端的山峰会被裁头，现在加了投影 headroom。
   - Spectrogram A/B/C 底部文字区过大、压缩图像的问题已经收紧。
   - Reverb / Space 尤其是三素材对比时，EDC 和 tail spectrogram 被压扁的问题已经重新分配空间。
   - 底部 metrics 字体已放大到更适合论文截图缩放的级别。

4. **UI 与 JobRunner 同步**
   - 这轮改动覆盖主界面与共享导出路径。
   - JobRunner 通过 `--audio-doctor-job` 导出的图也会使用同一套核心绘图布局，不是只修了界面截图。

---

## 2. 论文组不要偷懒：这几类图值得重点出

这轮新增的图不是“可有可无的装饰”，而是论文里最容易体现工具价值的部分。建议论文组至少跑下面几组：

### A. Spectrogram A/B/C + 2.5D

用途：展示瞬态、尾音、频段能量随时间展开的结构。

建议场景：
- Dry 原始碰撞素材
- Wet A 加 Disperser / all-pass / transient spreading 类插件
- Wet B 加 reverb / delay / spatial 类插件

论文写法重点：
- 不只说“声音变厚/变宽”，而是指出高频瞬态、低频尾音、衰减层在时间轴上的展开方式。
- 对比普通 2D spectrogram 和 2.5D terrain，2.5D 更适合展示“能量山体”的形态变化。

### B. Reverb / Space + Tail Spectrogram

用途：说明混响不是“加空间感”这么简单，而是改变 EDC 衰减、DRR、early/late、尾部频谱保留情况。

建议场景：
- Dry 碰撞素材
- Wet A 短空间 / early reflection 类处理
- Wet B 长尾混响 / 大空间处理

论文写法重点：
- EDC 曲线看整体衰减速度。
- Tail spectrogram 看尾部频率是否拖泥带水、是否吞掉清晰度。
- 指标区看 RT20 / RT30 / RT60 est. / DRR / Early-Late / Corr / S-M。

### C. Spatial Image / L-C-R

用途：展示某个关键时刻的空间能量分布，而不是只给一个抽象的 stereo width 数字。

建议场景：
- 选择攻击瞬间、主体爆发点、尾音展开点分别导图。
- 同一个素材在 Dry / Wet A / Wet B 下对比。
- 重点看：能量是否从 C 扩到 L/R，某些频段是否更偏 side，混响或宽化是否集中在中高频。

论文写法重点：
- 这是“空间印象图”，适合解释听感中的体积、宽度、中心稳定性。
- 用时间窗切换展示“空间变化过程”：攻击期可能集中，中后段可能扩散，尾音可能更偏 side。

---

## 3. 图的上限与边界，论文里要说准

这些图很值得展示，但不能乱吹。请论文组在正文或图注里保持边界清楚。

### 3.1 2.5D Spatial Terrain 的上限

它的优势：
- 把时频谱的能量分布转成更直观的体积山峦。
- 能看出瞬态墙、尾音坡、频段残留、处理前后能量展开差异。
- 比普通 2D spectrogram 更适合展示论文第四章“效果链如何改变能量形态”。

它的边界：
- 2.5D 是可视化投影，不是物理三维声场测量。
- Z 轴表示分析得到的能量密度，不表示真实空间高度。
- 视角切换只改变观察方式，不改变底层数据。

### 3.2 Spatial Image / L-C-R 的上限

它的优势：
- 比单个 stereo width 数字更能说明“哪些频段在左、中、右如何分布”。
- 时间窗可调，适合展示攻击、主体、尾音三个阶段的空间变化。
- 对混响、宽化、stereo delay、chorus、Enrage 类空间处理特别有解释力。

它的边界：
- 当前是 stereo-derived spatial impression，不是真正的 5.1 / Atmos 多声道 bus meter。
- L-C-R 是由 L/R balance、Side/Mid、相关性和局部频段能量推导出的印象分布。
- 图中某频段山峰高，表示该时间窗内归一化后的局部能量显著；判断“真实绝对高频能量”仍应对照 Spectrum / Spectrogram 的 dB 数据。

一句话：  
**Spatial Image 可以写“空间印象与频段分布”，不要写成“真实环绕声定位测量”。**

---

## 4. 推荐出图策略

论文组后续跑图时，建议不要只导一张默认图。至少按以下顺序出：

1. **先跑普通 Spectrogram A/B/C**
   - 确认素材和插件效果链确实有可见差异。

2. **再跑 2.5D 版本**
   - 选择最能展示差异的视角。
   - 如果瞬态在开头挡住后面，打开时间翻转再导一张。

3. **再跑 Reverb / Space**
   - 看 EDC 和 Tail Spectrogram 是否支撑正文判断。
   - 如果三素材太挤，优先导 Dry + 一个最关键 Wet 的双图，再补三图作附录。

4. **最后跑 Spatial Image**
   - 不要只用默认全局。
   - 至少挑 2-3 个时间位置：攻击期、主体期、尾音期。
   - 每个时间点都要记录图中 `Time ... | window ...`，方便论文图注复核。

---

## 5. 给论文组的验收提醒

出图前请检查：

- 图中是否显示了正确的 Dry / Wet A / Wet B。
- 插件参数是否已经 render 后进入对应 Wet。
- Spatial Image 的时间窗是否选在论文要分析的声音事件上。
- 2.5D 图是否有裁头、遮挡、轴标重叠。
- Reverb / Space 的 Tail Spectrogram 是否足够高，三素材对比是否可读。
- manifest / CSV 是否一起保留，不要只拿 PNG 写结论。

如果一张图里三个人太挤，允许改成：
- 正文放 Dry vs 关键 Wet；
- 附录放 Dry / Wet A / Wet B 三者完整对比。

---

## 6. 本轮验证状态

本轮已做过：

- `git diff --check` 通过。
- `./build.sh standalone` 通过。
- JobRunner 烟测通过，覆盖：
  - `spatial_impression`
  - `spectrogram_abc`
  - `reverb_space_combo`

烟测结论：
- Spatial Image 不再裁掉 20k 端山峰。
- Spectrogram A/B/C 图体高度已恢复。
- Reverb / Space 的 EDC 与 tail spectrogram 不再严重互相挤压。

---

## 7. 给后续工程 AI 的注意事项

这轮关键文件：

- `Source/AudioDoctorFigureRenderer.h`
- `Source/AudioDoctorComponent.h`

不要把 Spatial Image 当普通 spectrogram 继续改。它的轴定义是：

- X：L-C-R 空间位置
- Y：频率
- Z：时间窗内的能量密度
- 颜色：L-C-R 位置倾向，蓝绿偏左，黄色偏中心，红/粉偏右

不要把 2.5D terrain 的视角切换理解成改数据。它只是观察方式。

如果继续优化，优先做：

1. 给 Spatial Image 的时间滑块导出 manifest 字段写得更完整。
2. 给 2.5D 导出图增加视角 token 和 time-reversed token。
3. 给论文 preset 加固定 caption 草稿，减少论文组手写图注成本。
