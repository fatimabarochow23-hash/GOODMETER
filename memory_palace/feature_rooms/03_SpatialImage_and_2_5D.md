# 功能房 03｜Spatial Image 与 2.5D 图

## 2.5D Spatial Terrain

用途：

- 将 Spectrogram A/B/C、Reverb / Space 的 tail spectrogram 等时频信息转为山峦式体积图。
- 观察瞬态墙、尾音坡、频段残留、能量展开。
- 也可反哺 Layer Fit / Fusion 的 Time-Frequency Terrain，但语义仍是时间-频率-能量，不应画成临界带散点。

推荐论文视角：

- `front_high`
- `side_high`

低视角和 diagonal 可以保留，但正文优先用高视角，因为缩小到论文版面后更可读。

重要边界：

- 2.5D 是可视化投影，不是物理三维声场。
- Z 轴是分析能量密度，不是真实空间高度。
- `flip time` 只改变观察方向，避免瞬态墙遮挡尾音，不改变底层数据。
- `front` 语义：横轴是时间，纵向/深度轴是频率，Z 是能量。
- `side` 语义：横轴是频率，左侧纵向/深度轴是时间，Z 是能量。
- 鼠标拖拽视角是自由观察；视角下拉框是五种起始视角的复位点。
- 鼠标滚轮缩放应以鼠标停靠位置为中心，不再依赖早期 `20-20k` 频段按钮。
- 如果 0s 瞬态墙遮挡尾音，优先用高机位或 `flip time`，不要改底层时间数据。

## Spatial Image / L-C-R

轴定义：

- X：L-C-R 空间位置
- Y：频率
- Z：时间窗内能量密度

颜色含义：

- 颜色表示 L-C-R 位置倾向。
- 蓝绿偏左。
- 黄色接近中心。
- 红/粉偏右。
- 宽度、Side/Mid 与相关性主要影响山体横向扩散和能量形态，不应解释为“颜色直接表示宽度”。

论文用途：

- 攻击期看主体是否集中。
- 主体期看空间扩散是否影响中心稳定性。
- 尾音期看宽化、混响、delay、stereo effect 是否留下侧向能量。

边界：

- 这是 stereo-derived spatial impression。
- 不是 5.1 / Atmos 多声道 bus meter。
- 如果要判断真实绝对高频能量，必须对照 Spectrum / Spectrogram 的 dB 数据。
- 时间进度条表示当前分析窗中心；拖动它只是换时间窗，不是播放音频。
- 播放/倒放按钮可以自动扫时间窗；空格应 toggle 暂停/继续。

## 推荐出图策略

1. 先用普通 Spectrogram A/B/C 确认素材和插件确实有差异。
2. 再导 2.5D front_high / side_high。
3. 若瞬态墙遮挡，导 flip_time 版本。
4. Spatial Image 至少导早期窗和尾音窗。
5. Dry/Wet 对比默认使用 shared scale，避免弱尾音被自动归一化拉高。

状态：有效。

标签：#SpatialImage #SpatialTerrain #2_5D #LCR
