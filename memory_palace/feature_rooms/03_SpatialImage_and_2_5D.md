# 功能房 03｜Spatial Image 与 2.5D 图

## 2.5D Spatial Terrain

用途：

- 将 Spectrogram A/B/C、Reverb / Space 的 tail spectrogram 等时频信息转为山峦式体积图。
- 观察瞬态墙、尾音坡、频段残留、能量展开。

推荐论文视角：

- `front_high`
- `side_high`

低视角和 diagonal 可以保留，但正文优先用高视角，因为缩小到论文版面后更可读。

重要边界：

- 2.5D 是可视化投影，不是物理三维声场。
- Z 轴是分析能量密度，不是真实空间高度。
- `flip time` 只改变观察方向，避免瞬态墙遮挡尾音，不改变底层数据。

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

## 推荐出图策略

1. 先用普通 Spectrogram A/B/C 确认素材和插件确实有差异。
2. 再导 2.5D front_high / side_high。
3. 若瞬态墙遮挡，导 flip_time 版本。
4. Spatial Image 至少导早期窗和尾音窗。
5. Dry/Wet 对比默认使用 shared scale，避免弱尾音被自动归一化拉高。

状态：有效。

标签：#SpatialImage #SpatialTerrain #2_5D #LCR

