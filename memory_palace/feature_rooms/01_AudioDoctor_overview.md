# 功能房 01｜Audio Doctor 总览

## 当前定位

Audio Doctor 是 GOODMETER standalone 里的论文分析模块。它不是 DAW，也不是完整 Plugin Doctor 克隆；核心价值是：

```text
导入/生成素材 -> 加载插件 -> 设置参数 -> 离线渲染 -> 分析曲线 -> 导出论文图 -> 导出 CSV/manifest
```

它主要服务论文第二、三、四章：

- 第二章：频率感知、素材构成、掩蔽与融合的基础图例。
- 第三章：能量碰撞音效的分层构建依据。
- 第四章：效果链如何改变频谱、瞬态、空间和动态关系。

## 当前核心能力

- Dry A/B/C 与 Wet A/B/C 多素材/多渲染结果体系。
- Plugin A/B/C 加载、编辑、渲染。
- Display Slots 与 Render Routing。
- 视图包括 Spectrum、Envelope、Group Delay、Spectrogram A/B/C、Reverb / Space、Dynamics Response、Spatial Image、Masking / Fusion 等。
- UI Export 和 JobRunner 都需要导出 PNG、CSV、JSON/manifest。

## 论文图例口径

不要把图写成“软件截图”。每张图都要回答论文问题：

- Spectrum：谐波、频谱重心、频谱占位。
- Envelope：瞬态边界、尾部长度、能量包络。
- Group Delay：all-pass / Disperser 类时间展开。
- Spectrogram A/B/C：层次、尾音、频段残留。
- Reverb / Space：EDC、RT20/RT30/RT60 est.、DRR、Early/Late、Stereo / M/S。
- Dynamics Response：RMS 动态响应或表观让位。
- Spatial Image：选定时间窗内 L-C-R 与频段空间印象。
- Masking / Fusion：遮蔽风险、融合趋势、mix delta。

## 风险边界

- Dynamics Response 不是插件内部 GR meter。
- Spatial Image 不是真实多声道定位测量。
- Masking / Fusion v1 不是严格心理声学 masked threshold。
- RT60 当前写 `RT60 est.` 更稳。

状态：有效。

标签：#AudioDoctor #论文图例 #总览

