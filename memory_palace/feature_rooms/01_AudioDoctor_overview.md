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
- Generate 可指定输出到 Dry A/B/C；不能再只服务 Dry A。
- 插件槽位采用 insert-style UI：空槽点击加载，已加载槽点击打开插件 UI；render / output gain 属于插件 UI 底部工作流。
- `.clz` 工程保存当前 Audio Doctor 状态与当前仍使用的音频文件，目标是 Mac/Windows 同版本跨平台打开。
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
- Critical Band Crystal / Dodecahedron Crystal：临界带关系、遮蔽风险、融合倾向、合成增益，适合论文展示“层间关系”。

## 2026-05-16 当前恢复点

- 当前真实工作区：`/Volumes/solari/Codex_Work/GOODMETER`。
- 当前 GitHub 论文分支：`codex/thesis-audio-doctor-20260505`。
- 已核验远端论文分支 HEAD：`3442e2d1aa526c7c916562a9a1ecbce33554a4c2`。
- 已核验远端 `main` 当时仍在：`d023690c393a17a9b4db56715fedd006afbdab25`。
- `main` 不含最新 Audio Doctor thesis workflow；主线大电脑或 Windows Codex 必须显式 fetch/merge/cherry-pick 论文分支。
- 本地可能有 `MAIN_COMPUTER_CODEX_AUDIO_DOCTOR_HANDOFF_2026-05-15.md` 删除状态，但远端分支已有该文档；恢复前先问用户。

## 风险边界

- Dynamics Response 不是插件内部 GR meter。
- Spatial Image 不是真实多声道定位测量。
- Masking / Fusion v1 不是严格心理声学 masked threshold。
- RT60 当前写 `RT60 est.` 更稳。
- `.clz` 跨平台是硬验收，不能只验证 Mac 自己能打开。
- JobRunner 覆盖新增图例时必须导出同等数据，不要只加 UI 画面。

状态：有效。

标签：#AudioDoctor #论文图例 #总览
