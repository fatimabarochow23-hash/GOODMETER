# 功能房 06｜掩蔽与分频高亮

## 触发问题

论文第二章需要比文字概念更直观的基础图例，解释频段组织、素材构成、层间遮蔽风险和融合趋势。

相关文件：

- `/Users/MediaStorm/Desktop/GOODMETER/AUDIO_DOCTOR_MASKING_AND_BAND_HIGHLIGHT_HANDOFF_2026-05-08.md`

## 分频高亮

默认频段：

- Low：20-200 Hz
- Mid：200-2000 Hz
- High：2000-20000 Hz

用途：

- 在 Spatial Image / Spatial Terrain 中突出低、中、高频空间分布。
- 支撑第二章“听觉并非逐点读取 FFT，而是以有限频率分辨率组织声音”的论证。
- 支撑第三章“体积、材质、运动、边界各自占据不同频段线索”的分层解释。

## Masking / Fusion v1

当前口径：

- overlap：频谱/时频重叠
- masking risk：遮蔽风险
- dominant source：主导声源
- fusion tendency：融合倾向
- mix delta：混合后能量变化

颜色口径：

- 红色：`masking risk / 遮蔽风险`，主导层在同时间窗、同临界带附近压过其他层。
- 紫色：`fusion tendency / 融合倾向`，多层能量接近并同步重叠，更容易被感知为同一事件。
- 黄色：`mix gain / 合成增益`，bounce / 合成结果相对输入层明显抬升的临界带。

图例口径：

- 颜色键应显示多个代表 band，而不是只显示一个 `Bxx`。
- 如果选择 Low/Mid/High/指定 band，高亮之外的标签应隐藏或弱化，避免“我选中频还贴满低高频标签”。
- `strongest band` 和风险 band 长期停在 B0 是异常，应检查算法 fallback、索引映射或未写算法的占位值。
- 图中文字若要进论文，字号优先于留白；不要为了精致压到不可读。

## Critical Band Crystal / Dodecahedron Crystal

用途：

- 把 Layer Fit / Fusion 的临界带关系做成立体结构，用于展示层间遮蔽、融合、合成增益。
- 左图通常是 Selected Stems，展示原始层叠关系；中图 Derived Interaction 展示析出关系；右图 Auto Bounce 展示合成结果。

算法边界：

- 不要把相邻两个临界带合成一个加权高度。
- 五边形顶面应拆成五个三角形：强临界带分 3 块，弱临界带分 2 块。
- 各三角块高度由对应临界带能量直接决定；能量由柱体高度表示，不由体积重新归一化。
- 析出关系里的红/紫/黄是代理指标，不是真实人耳阈值或真实物理晶体。
- band label 只在信息量能读清时显示；如果密到读不出，对正文图宁可隐藏，交给图注解释。

交互：

- 晶体图可鼠标拖拽旋转。
- 视角下拉是复位到标准起始视角，不是唯一观察方式。
- 有进度条的 Layer Fit 图应支持播放/倒放扫时间窗，空格暂停/继续。

不可过度宣称：

- 不写“图中证明人耳已经听不见”。
- 不写“真实心理声学 masked threshold”。
- 推荐写“基于临界带能量和局部电平差估计的遮蔽风险代理”。

## 论文用法

- 第二章：解释掩蔽与融合的听觉基础。
- 第三章：解释多层素材为什么需要错开频段、时间和空间。
- 第四章：解释效果器链改变的不只是音色，还包括层间可辨性与融合趋势。

状态：有效。

标签：#MaskingFusion #BandHighlight #CriticalBand #SecondChapter
