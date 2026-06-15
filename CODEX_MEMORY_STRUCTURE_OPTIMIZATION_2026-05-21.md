# Codex 记忆与上下文结构优化交接单

日期：2026-05-21  
对象：论文组、后续 Codex / Claude Code / 其他施工队  
目标：降低旧历史回流、上下文频繁压缩、重复检索和路径失忆风险，同时不破坏近一周记忆与当前 GOODMETER 工作。

---

## Codex v2 优化结论（2026-05-22）

这份单子的方向是对的，但原方案里的“在 `/Volumes/solari 1/Codex_Work/` 下新建全局启动面板、热记忆、事实索引、冷归档”容易把当前问题扩大成新一轮搬家。现在更稳的做法是：**不移动旧记忆，不重排全局目录，先在 GOODMETER 自己的 `memory_palace` 里加一个薄启动层和一个当前任务卡。**

也就是说，下面第 3 节提出的四层结构暂时只作为长期参考；当前立即执行的是“就地瘦身版”：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/00_hot_start_current.md
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/current_tasks/2026-05-21_preview_solo_group_delay.md
```

### v2 启动顺序

新对话或压缩上下文后，只读以下文件，不要递归扫全仓库：

1. `/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/00_hot_start_current.md`
2. `/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/current_tasks/2026-05-21_preview_solo_group_delay.md`
3. 只有要改源码时，再读 `/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace/asset_index/source_files_index.md`
4. 只有涉及 Job Runner、`.clz`、插件渲染或图表导出时，再读对应 feature room
5. 只有用户追问旧争议、旧路径、旧构建结果时，再查冷历史或迁移日志

### v2 禁读范围

默认不要读：

- `rollout*.jsonl`、`*.jsonl.gz`
- `AudioDoctor_Exports/data` 全量 CSV
- 视频、音频、导出 PNG 全量目录
- `dist/`、`releases/`、`tmp/`、`Signing/`
- `._*` AppleDouble 元数据
- 旧 DMG / zip / 临时 Word 文件

如果必须读取这些内容，先说明原因、范围和预期结果。

### v2 压缩上下文协议

每次进入容易压缩的长任务时，施工队只需要维护两个小文件：

1. `00_hot_start_current.md`：只写当前仓库状态、当前危险 diff、不要重复做什么、下一步入口。
2. `current_tasks/*.md`：只写当前任务需求、已完成/未完成、涉及文件、验证命令、已知坑。

每完成一个关键动作，追加一条很短的 checkpoint；不要把大段推理、命令全文、长日志搬进热启动卡。构建或烟测结果只记录一次：命令、是否通过、产物路径、失败摘要。

### v2 当前任务切分

当前 `Source/AudioDoctorComponent.h` 已经有半成品改动，后续不要 reset、stash、checkout 或重写整块 UI。接手时按以下顺序收口：

1. 预听 Solo UI：六个 3x2 方块，第一排 DRY A/B/C，第二排 WET A/B/C；无文字标签。
2. 预听音频路由：师姐按钮和进度条预听服从 solo；插件 insert 内部预听不受影响。
3. `.clz` 保存/加载：solo 状态必须进入工程文件，跨机器打开一致。
4. Group Delay：不要依赖 displaySlots；直接从匹配的 DRY/WET 或渲染结果读取群延时。
5. Job Runner：补 `previewSolo` / group delay 导出与 manifest，保证离线任务也能覆盖新能力。
6. UI polish：右侧 Plugin A/B/C 色块加宽，避免论文/PPT截图读不清。

---

## 0. 当前紧急结论

本轮 Codex 已经出现两个危险信号：

1. 上下文压缩频率过高，旧历史和当前任务混在一起。
2. 在实现 `AUDIO_DOCTOR_PREVIEW_SOLO_GROUP_DELAY_REQUIREMENTS_2026-05-21.md` 时，已经开始出现“旧对话重演”和实现线漂移。

因此不要继续让当前对话独自做大改。先做记忆结构优化，再交给状态更干净的对话继续。

当前仓库路径：

```text
/Volumes/solari 1/Codex_Work/GOODMETER
```

当前重要分支：

```text
codex/thesis-audio-doctor-20260505
```

当前未完成源码改动：

```text
Source/AudioDoctorComponent.h
```

后续接手者必须先执行：

```bash
git -C "/Volumes/solari 1/Codex_Work/GOODMETER" status --short --branch
git -C "/Volumes/solari 1/Codex_Work/GOODMETER" diff -- Source/AudioDoctorComponent.h
```

只允许审查，不要立刻 reset、checkout、stash、clean。若要回退或保留这段半成品改动，必须先让用户确认。

---

## 1. 现有问题判断

### 1.1 旧历史回流

症状：

- 当前任务是 2026-05-21 的 Preview Solo / Group Delay 需求，但上下文里反复浮现早期 Audio Doctor、iOS、证书、公证、论文组迁移等旧内容。
- Agent 容易把旧需求当成当前需求，开始重复搜索、重复构建、重复解释。

原因：

- 原始 rollout / jsonl 日志太长。
- 记忆宫殿入口与原始日志没有明确冷热分层。
- 新对话启动时读取材料太多，导致模型把旧状态当成当前状态。

### 1.2 上下文频繁压缩

症状：

- 一次任务内多次压缩后，agent 记不住刚刚确定的路径、产物、build 结果。
- 用户已经明确说“不想重复全盘搜、重复构建”，但 agent 仍可能重新搜。

原因：

- 当前对话负载过重，包含大量历史 UI 修改、论文交接、构建、公证、JobRunner、图表算法等内容。
- 没有把“当前任务焦点”和“禁止重复事项”压成单页热记忆。

### 1.3 记忆宫殿可能太厚

症状：

- 新 agent 读了记忆宫殿后仍不知道 `.clz`、JobRunner、当前分支、当前工作路径怎么用。
- 大量房间文档有价值，但不适合每次全读。

原因：

- 总索引更像目录，不像执行面板。
- “当前一周状态”和“历史背景”没有彻底分离。
- 对 agent 来说，缺少硬性预算：先读哪 3 个文件，不能读哪些大日志。

---

## 2. 绝对保护边界

优化记忆结构时，以下内容不得删除、覆盖或重写：

```text
/Volumes/solari 1/Codex_Work/GOODMETER
/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace
/Volumes/solari 1/Codex_Work/论文工作文件夹/记忆宫殿
/Volumes/solari 1/Codex_Work/迁移复活包_20260516
/Users/caiyiyang/.codex/memories
```

特别注意：

- 原始 rollout / jsonl 日志只允许复制、移动到冷归档，不允许手工编辑内容。
- 不要删除近一周文档，尤其是 2026-05-15 至 2026-05-21 的 GOODMETER / Audio Doctor / JobRunner / `.clz` / 公证 / 答辩相关记录。
- 不要用 `git clean` 清外置盘噪声。
- 不要 `git add .`。
- 不要把 `._*` 当源码或记忆正文。

---

## 3. 建议的新结构

建议把记忆分成四层。

### A. 启动层：只放 1 页

路径建议：

```text
/Volumes/solari 1/Codex_Work/启动面板/00_今日启动卡.md
```

内容不超过 150 行，只写：

- 当前日期
- 当前主任务
- 当前工作路径
- 当前 Git 分支与远端关系
- 最新可用 app 路径
- 今天不要重复做什么
- 今天最危险的未完成 diff
- 新 agent 开工前只读哪些文件

这个文件是“今天开工的驾驶舱”，不是历史档案。

### B. 热记忆层：近 7 天

路径建议：

```text
/Volumes/solari 1/Codex_Work/热记忆_近7天/
```

只收近 7 天真正影响当前工作的事实。建议拆成：

```text
01_GOODMETER_当前代码状态.md
02_AudioDoctor_UI与JobRunner状态.md
03_答辩演示风险清单.md
04_未完成任务与不要动清单.md
05_最近一次成功构建与产物路径.md
```

每个文件不超过 250 行。超过就拆，不要继续堆。

### C. 事实索引层：长期但短

路径建议：

```text
/Volumes/solari 1/Codex_Work/事实索引/
```

只放稳定事实，不放聊天过程。比如：

```text
GOODMETER_FACT.md
AudioDoctor_FACT.md
CLZ_Project_FACT.md
JobRunner_FACT.md
Packaging_Notarization_FACT.md
Thesis_Figure_FACT.md
```

每条事实必须包含：

```text
事实：
来源：
最后确认日期：
是否可能过期：
验证命令或验证路径：
```

### D. 冷归档层：原始日志与旧交接

路径建议：

```text
/Volumes/solari 1/Codex_Work/冷归档_只读/
```

可以收：

- 原始 rollout / jsonl
- 迁移复活包
- 旧版论文交接
- 早期 GOODMETER 需求单

规则：

- 默认新对话不读冷归档。
- 只有查证具体旧事实时才读。
- 读完只摘录结论回热记忆，不把整段历史塞回上下文。

---

## 4. 新 agent 启动读取顺序

后续论文组可以把新 agent 启动规则改成：

```text
第一步：读 00_今日启动卡.md
第二步：读 热记忆_近7天/01_GOODMETER_当前代码状态.md
第三步：按任务读一个事实索引文件
第四步：只在证据不足时读记忆宫殿主题房间
第五步：只有追溯旧争议时读冷归档
```

不要一上来读全部：

```text
记忆宫殿全量
rollout jsonl 全量
所有需求单
所有旧交接
```

这会把新对话直接拖进旧历史。

---

## 5. 当前 GOODMETER 接手保护卡

建议论文组立刻新建或更新：

```text
/Volumes/solari 1/Codex_Work/热记忆_近7天/01_GOODMETER_当前代码状态.md
```

必须写入以下内容：

```text
当前仓库：/Volumes/solari 1/Codex_Work/GOODMETER
当前分支：codex/thesis-audio-doctor-20260505
当前高危未完成改动：Source/AudioDoctorComponent.h
本轮未完成需求：AUDIO_DOCTOR_PREVIEW_SOLO_GROUP_DELAY_REQUIREMENTS_2026-05-21.md
禁止事项：不要 reset、不要 clean、不要 stash、不要重复全盘搜、不要重复构建
接手第一命令：git status --short --branch
接手第二命令：git diff -- Source/AudioDoctorComponent.h
```

如果后续决定继续 Preview Solo 需求，建议不要从压缩摘要继续写，而是先把需求单拆成 3 个小 PR/commit：

1. UI solo 按钮与 `.clz` 状态保存。
2. Group Delay 固定 DRY + WET A/B/C。
3. JobRunner previewSolo mix + manifest。

每个阶段单独 build，避免一次改三层又压缩。

---

## 6. 记忆瘦身操作建议

### 6.1 先备份

在做任何整理前，先复制一份索引级备份：

```bash
mkdir -p "/Volumes/solari 1/Codex_Work/记忆结构备份_20260521"
cp -R "/Volumes/solari 1/Codex_Work/论文工作文件夹/记忆宫殿" \
      "/Volumes/solari 1/Codex_Work/记忆结构备份_20260521/论文记忆宫殿"
cp -R "/Volumes/solari 1/Codex_Work/GOODMETER/memory_palace" \
      "/Volumes/solari 1/Codex_Work/记忆结构备份_20260521/GOODMETER_memory_palace"
```

不要把整个 `.codex/sessions` 复制进热记忆。

### 6.2 生成今日启动卡

今日启动卡只允许写当前状态，不写历史长叙述。

建议包含：

```text
今天最重要任务：
今天禁止重复：
当前仓库：
当前分支：
当前未完成 diff：
最近一次可用构建：
当前答辩演示风险：
下一步建议：
```

### 6.3 把旧日志降级为冷归档

原始日志移动到：

```text
/Volumes/solari 1/Codex_Work/冷归档_只读/rollout_logs/
```

不要让新对话默认读取。

### 6.4 给旧日志做摘要索引

每个旧 rollout 只登记：

```text
时间：
主题：
关键结论：
相关文件：
是否仍有效：
如果需要原文，到哪里查：
```

不要把长日志全文复制到热记忆。

---

## 7. 防止“旧历史重演”的提示词

新对话开头建议加入：

```text
你只能把 00_今日启动卡、热记忆_近7天和当前用户消息视为当前状态。
旧记忆宫殿和 rollout 日志只可用于查证，不可直接覆盖当前状态。
如果旧材料与当前磁盘冲突，以当前用户消息和当前磁盘为准。
不要重复全盘搜，不要重复构建。
已经确认过的路径和产物必须写进本轮回复，避免下一轮失忆。
```

---

## 8. 验收标准

记忆结构优化完成后，开一个全新对话测试。新 agent 只读：

```text
00_今日启动卡.md
热记忆_近7天/01_GOODMETER_当前代码状态.md
事实索引/JobRunner_FACT.md
```

然后问它 5 个问题：

1. 当前 GOODMETER 仓库在哪里？
2. 当前分支是什么？
3. 当前最危险的未完成改动是什么？
4. `.clz` 工程文件用来做什么？
5. JobRunner 和 UI Export 的关系是什么？

合格标准：

- 不全盘搜。
- 不重新推导旧历史。
- 不说“我不知道项目状态”。
- 不把旧 Desktop 路径当主路径。
- 能明确说出当前未完成 diff 和下一步风险。

---

## 9. 给论文组的一句话

现在最需要优化的不是“让记忆更多”，而是“让启动时只加载当前任务真正需要的少量记忆”。近一周状态要热，旧日志要冷，原始证据要保留但不能每次都塞进上下文。这样才能避免 Codex 和论文组都因为历史过载而打不开、乱接、重复构建。
