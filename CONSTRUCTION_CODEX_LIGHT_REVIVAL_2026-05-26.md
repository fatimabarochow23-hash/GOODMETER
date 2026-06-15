# 施工队轻量复活入口

日期：2026-05-26

用途：施工队旧对话点击后会把内存打满时，给新施工队使用。这个文件是轻量入口，不是完整日志。新对话只读这里，再按路径进入 memory palace。

---

## 1. 先不要读这些

不要打开或递归读取旧会话日志。以下三个超大会话已经从本机 `.codex/sessions` 搬到外置盘冷归档：

```text
/Volumes/solari 1/Codex_Work/Codex旧对话冷归档_20260526/sessions/2026/04/28/rollout-2026-04-28T20-33-53-019dd414-e8a5-7a61-973a-12866bd8bc6f.jsonl.disabled_20260526
/Volumes/solari 1/Codex_Work/Codex旧对话冷归档_20260526/sessions/2026/05/11/rollout-2026-05-11T21-29-28-019e173a-76d1-75a3-a174-ca1f06d98f20.jsonl.disabled_20260526
/Volumes/solari 1/Codex_Work/Codex旧对话冷归档_20260526/sessions/2026/05/19/rollout-2026-05-19T06-11-35-019e3d24-fed4-7c91-9c43-8d44b343bddc.jsonl.disabled_20260526
```

这些文件体量分别约为 3.8G、1.0G、634M。旧对话打不开，基本可以按“日志过大导致加载爆内存”处理。搬移记录见：

```text
/Volumes/solari 1/Codex_Work/Codex旧对话冷归档_20260526/MOVE_MANIFEST_20260526_032821.txt
```

也不要全量读取：

```text
rollout*.jsonl
*.jsonl.gz
AudioDoctor_Exports/data 全量 CSV
音频、视频、导出 PNG
dist/
releases/
tmp/
Signing/
._*
```

---

## 2. 只读启动顺序

先读：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/00_hot_start_current.md
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/current_tasks/2026-05-26_recent_two_weeks_revival.md
```

再按任务选择性读：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/current_tasks/2026-05-21_preview_solo_group_delay.md
/Volumes/solari 1/Codex_Work/GOODMETER/AUDIO_DOCTOR_PREVIEW_SOLO_GROUP_DELAY_REQUIREMENTS_2026-05-21.md
/Volumes/solari 1/Codex_Work/GOODMETER/AUDIO_DOCTOR_CHAIN_FIGURE_PARAMS_TICKET_2026-05-26.md
```

如果要改源码，再读：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/asset_index/source_files_index.md
```

---

## 3. 当前仓库状态快照

仓库：

```text
/Volumes/solari 1/Codex_Work/GOODMETER
```

分支：

```text
codex/thesis-audio-doctor-20260505
```

最近确认的 git 状态：

```text
codex/thesis-audio-doctor-20260505...origin/codex/thesis-audio-doctor-20260505 [ahead 5]
HEAD: cf638be 大电脑施工队: update Audio Doctor preview assets and memory
```

已有改动集中在：

```text
Builds/MacOSX/GOODMETER.xcodeproj/project.pbxproj
Source/AudioDoctorComponent.h
Source/AudioDoctorFigureRenderer.h
Source/AudioDoctorJobRunner.h
memory_palace/asset_index/requirements_index.md
```

不要 `git reset --hard`、不要 `git clean`、不要 `git add .`。外置盘会生成大量 `._*`，它们不是源码。

---

## 4. 最近两周核心记忆

1. 论文已经交过一版，后续重点转向答辩 PPT、现场演示、GOODMETER / Audio Doctor 的可展示能力。
2. GOODMETER 是作者从零开发的桌面端音频分析工具；Audio Doctor 是其中为论文扩展的分析模块。不要写成外部现成软件。
3. Audio Doctor 的图表是信号分析和可视化证据，不是主观听感实验。
4. `.clz` 工程跨平台可打开是硬验收，不是建议项。
5. UI、Job Runner、manifest、论文图表四层要分清。
6. 新功能如果进入 UI，也要考虑 Job Runner、manifest、`.clz` 和 structured data export。
7. ExFAT 外置盘会产生 `._*`，签名、打包、提交时要避开。
8. 构建产物优先落到本机 APFS 缓存，避免 ExFAT 产物签名问题。

---

## 5. 当前最重要的两个任务

### A. Preview Solo / Group Delay

需求单：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/AUDIO_DOCTOR_PREVIEW_SOLO_GROUP_DELAY_REQUIREMENTS_2026-05-21.md
```

任务卡：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/current_tasks/2026-05-21_preview_solo_group_delay.md
```

核心：

- 主界面加六个 3x2 无文字方块。
- 第一排 DRY A/B/C，第二排 WET A/B/C。
- 点亮谁，师姐按钮和进度条预听就让谁出声。
- 插件 insert 内部预听不参与这套逻辑。
- Group Delay 不能因为加载两条或三条 DRY 就空白。
- 右侧 Plugin A/B/C 参数色块要更宽，截图能看清。
- 新状态要覆盖 `.clz` 和 Job Runner。

### B. 效果链论文新图参数摘要

任务单：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/AUDIO_DOCTOR_CHAIN_FIGURE_PARAMS_TICKET_2026-05-26.md
```

现象：

- Job Runner 的 manifest 里已经有插件链参数。
- 但导出的论文 PNG 底部仍显示 `Plugin A params default/no changed parameter`。
- 问题在 figure renderer 的参数摘要取值，不是算法和渲染链本身。

修复目标：

- chain 模式下，论文图底部参数摘要应显示链路内各插件的关键 changed params。
- 修完后用原 job 重跑图，不重新设计实验。

相关 job：

```text
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/jobs/ch46_effect_chain_order_20260526.json
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/jobs/ch54_energy_attack_chain_20260526.json
```

相关输出：

```text
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/outputs/ch46_effect_chain_order_20260526
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/outputs/ch54_energy_attack_chain_20260526
```

数据登记：

```text
/Volumes/solari 1/Codex_Work/论文工作文件夹/论文新图组_20260526_effect_chain_cases/notes/效果链新图数据登记_20260526.md
```

---

## 6. 构建与验证

常用构建：

```bash
cd "/Volumes/solari 1/Codex_Work/GOODMETER"
./build.sh standalone
```

常用 app 路径：

```text
/Users/caiyiyang/Library/Caches/GOODMETERBuild/GOODMETER-501852389/standalone/Products/Release/GOODMETER.app
```

验证时按任务选择最小范围，不要重复全量烟测。用户明确反感重复构建、忘路径、重新全盘搜。

---

## 7. 给新施工队的话

先活下来，再干活。不要打开旧超大对话，不要读完整 jsonl，不要清仓库，不要重置源码。

从本文件进入 memory palace，先确认 `git status --short --branch`，再读当前任务卡。遇到文件状态与本文冲突时，以磁盘当前状态和用户最新消息为准。
