# GOODMETER 热启动卡

日期：2026-05-26

用途：给 GOODMETER / Audio Doctor 的新对话或压缩上下文后使用。先读这一页，再决定是否读主题房间。不要递归扫全仓库。

---

## 1. 当前一屏结论

- 仓库路径：`/Volumes/solari 1/Codex_Work/GOODMETER`
- 当前分支：`codex/thesis-audio-doctor-20260505`
- 远端关系：本地领先 `origin/codex/thesis-audio-doctor-20260505` 5 个提交
- 当前源码危险区：`Source/AudioDoctorComponent.h`
- 当前源码改动规模：`240` 行 diff，约 `226` 行新增、`14` 行删除
- 当前需求单：`AUDIO_DOCTOR_PREVIEW_SOLO_GROUP_DELAY_REQUIREMENTS_2026-05-21.md`
- 当前记忆结构优化单：`CODEX_MEMORY_STRUCTURE_OPTIMIZATION_2026-05-21.md`
- 当前复活入口：`CONSTRUCTION_CODEX_LIGHT_REVIVAL_2026-05-26.md`
- 最近两周复活任务卡：`memory_palace/current_tasks/2026-05-26_recent_two_weeks_revival.md`
- 当前新增任务单：`AUDIO_DOCTOR_CHAIN_FIGURE_PARAMS_TICKET_2026-05-26.md`

当前任务不是重新设计 Audio Doctor，而是接住已经开始的 Preview Solo / Group Delay 修改，把半成品收口。

补充判断：施工队旧对话打不开时，不要继续点旧对话。`memory_palace` 本身只有数 MB，真正高风险是 `.codex/sessions` 下多 GB 的 `rollout*.jsonl`。

---

## 2. 开工前只跑这些命令

```bash
git -C "/Volumes/solari 1/Codex_Work/GOODMETER" status --short --branch
git -C "/Volumes/solari 1/Codex_Work/GOODMETER" diff --stat
git -C "/Volumes/solari 1/Codex_Work/GOODMETER" diff -- Source/AudioDoctorComponent.h
```

用途：

- 确认半成品 diff 还在。
- 确认没有被其他施工队继续改动。
- 确认当前任务是否仍然只集中在 `AudioDoctorComponent.h`。

---

## 3. 当前不要做什么

- 不要 `git reset --hard`
- 不要 `git checkout -- Source/AudioDoctorComponent.h`
- 不要 `git stash`
- 不要 `git clean`
- 不要 `git add .`
- 不要把 `._*` 当成源码或记忆正文
- 不要全量读取 `dist/`、`releases/`、`tmp/`、`Signing/`
- 不要全量读取视频、音频、导出 PNG、全量 CSV 或 rollout/jsonl 日志

如果必须处理上述内容，先向用户说明原因和范围。

---

## 4. 当前任务入口

先读：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/CONSTRUCTION_CODEX_LIGHT_REVIVAL_2026-05-26.md
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/current_tasks/2026-05-26_recent_two_weeks_revival.md
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/current_tasks/2026-05-21_preview_solo_group_delay.md
```

再读需求单：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/AUDIO_DOCTOR_PREVIEW_SOLO_GROUP_DELAY_REQUIREMENTS_2026-05-21.md
```

如果要改源码，再读：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/asset_index/source_files_index.md
```

---

## 5. 构建与验证

只有完成一组小而清晰的源码改动后，再运行：

```bash
cd "/Volumes/solari 1/Codex_Work/GOODMETER"
./build.sh standalone
```

验证重点：

- Standalone 能构建。
- 六个 Preview Solo 方块显示正确。
- 师姐按钮和进度条预听服从 solo 选择。
- 插件 insert 内部预听不受 solo 影响。
- Group Delay 在加载两条及以上 DRY 时仍显示。
- `.clz` 保存/加载后 solo 状态一致。
- Job Runner 能覆盖新字段并输出 manifest。

---

## 6. 压缩上下文后恢复规则

压缩后不要靠感觉续写。先读本文件，再读当前任务卡，然后只看当前 diff。

如果发现本文件与磁盘状态冲突，以用户最新消息和磁盘当前文件为准，并在回复里指出冲突。
