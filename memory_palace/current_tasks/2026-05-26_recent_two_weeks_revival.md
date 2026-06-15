# 当前任务卡：最近两周复活记忆

日期：2026-05-26

用途：施工队旧 Codex 对话因超大日志无法打开时，给新施工队快速恢复最近两周状态。只读本卡和相关任务卡，不要打开旧 `rollout*.jsonl`。

---

## 1. 事故判断

`GOODMETER/memory_palace` 本身只有数 MB，体量正常。旧施工队对话打不开，更像是 `.codex/sessions` 下旧 `rollout*.jsonl` 过大导致加载爆内存。

已发现并已搬到外置盘冷归档的高风险日志：

```text
2026-04-28 rollout jsonl: 约 3.8G
2026-05-11 rollout jsonl: 约 1.0G
2026-05-19 rollout jsonl: 约 634M
```

冷归档路径：

```text
/Volumes/solari 1/Codex_Work/Codex旧对话冷归档_20260526/
```

复活方式：开新对话，读轻量入口，不读旧日志。

---

## 2. 只读入口

```text
/Volumes/solari 1/Codex_Work/GOODMETER/CONSTRUCTION_CODEX_LIGHT_REVIVAL_2026-05-26.md
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/00_hot_start_current.md
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/current_tasks/2026-05-21_preview_solo_group_delay.md
```

必要时再读：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/AUDIO_DOCTOR_PREVIEW_SOLO_GROUP_DELAY_REQUIREMENTS_2026-05-21.md
/Volumes/solari 1/Codex_Work/GOODMETER/AUDIO_DOCTOR_CHAIN_FIGURE_PARAMS_TICKET_2026-05-26.md
```

---

## 3. 最近两周主线

1. 论文已进入提交/答辩阶段，GOODMETER 的任务重心从单纯论文出图，转向答辩演示、PPT素材、现场解释和工程可展示性。
2. Audio Doctor 已支持更完整的 Job Runner 论文实验链路，包括多源、多插件、插件状态、图像导出、CSV、manifest 和摘要。
3. 用户非常重视图表与证据链：图中颜色、标签、参数、图注和正文说明要能互相对上。
4. 近期新增效果链能力后，论文新图要展示“链路顺序”和“attack 管理”一类更强案例，但不能把试验参数摘要显示成默认参数。
5. 主界面预听需要更精确的 source solo；这是为答辩演示服务，不只是 UI 装饰。
6. Group Delay 当前有显示逻辑问题：加载多条 DRY 后可能空白，需要绕开旧的 `displaySlots` 依赖。

---

## 4. 当前不要碰的坑

- 不要回滚 `Source/AudioDoctorComponent.h` 的半成品 diff。
- 不要把 `.clz` 写成可选项，它是硬验收。
- 不要只改 UI，不管 Job Runner / manifest / 工程保存。
- 不要把 `._*`、`dist/`、`releases/`、`tmp/`、`Signing/` 放进提交范围。
- 不要重新设计论文实验；当前是收口已确定的功能和图。

---

## 5. 当前可交付优先级

1. 修 Preview Solo / Group Delay，并确认 `.clz` 与 Job Runner 覆盖。
2. 修效果链论文 PNG 的参数摘要显示。
3. 用已有 job 重跑两组效果链图，更新数据登记。
4. 给用户交付清晰状态：改了什么、验证了什么、哪些文件不要动。

---

## 6. 已备份

施工队 memory palace 已备份到：

```text
/Volumes/solari 1/Codex_Work/GOODMETER_memory_palace_backups/memory_palace_backup_20260526_施工队复活前/
```

备份说明：

```text
/Volumes/solari 1/Codex_Work/GOODMETER_memory_palace_backups/memory_palace_backup_20260526_施工队复活前/BACKUP_MANIFEST_20260526.md
```

---

## 7. 恢复后第一句话建议

新施工队读完后先汇报：

1. 已确认旧对话爆内存主要风险来自超大 session jsonl。
2. 已确认 `memory_palace` 本身正常且已有备份。
3. 已确认当前 GOODMETER 分支、未提交文件和两个当前任务。
4. 下一步准备先处理 Preview Solo / Group Delay 或效果链参数摘要，等待用户指定优先级。
