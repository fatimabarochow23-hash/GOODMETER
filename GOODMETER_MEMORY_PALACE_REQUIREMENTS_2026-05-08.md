# GOODMETER 记忆宫殿建设需求单

日期：2026-05-08  
提出方：论文组 / GOODMETER 协作线  
目标：在 GOODMETER 项目内建立一套外置长期记忆，让施工队、论文组、WinCodex、Claude Code、DeepSeek 或后续任意 AI 在换对话、压缩上下文、换电脑后，能快速恢复项目状态。

## 1. 为什么要建

GOODMETER 现在不只是一个普通代码仓库，它同时承担三条线：

1. 桌面端音频测量软件本体。
2. Audio Doctor 论文专用分析模块。
3. 论文图例、数据、素材、插件测试和导出链路。

仅靠聊天摘要会丢失很多关键上下文，例如：

- 哪个 bug 是 GUI 线程问题，哪个 bug 是 JobRunner 问题。
- 哪个图例需求来自论文正文，哪个只是附录补强。
- 哪个功能已经验证过，哪个只是施工单。
- 哪个插件能在 UI 渲染，哪个只能命令行跑，哪个需要 state。
- 哪些需求是老师反馈触发的，哪些是用户审美或论文策略触发的。

所以 GOODMETER 也需要一座“记忆宫殿”，并和论文工作文件夹里的记忆宫殿互相链接。

论文侧记忆宫殿位置：

`/Users/caiyiyang/Desktop/论文工作文件夹/记忆宫殿`

## 2. 建议目录结构

建议在 GOODMETER 根目录建立：

```text
/Users/caiyiyang/Desktop/GOODMETER/记忆宫殿/
├── README.md
├── 00_总索引.md
├── 时间线/
│   └── 2026-05-01_至_2026-05-08_关键时间线.md
├── 功能房/
│   ├── 01_AudioDoctor总览.md
│   ├── 02_JobRunner与Manifest.md
│   ├── 03_SpatialImage与2_5D图.md
│   ├── 04_插件宿主与渲染.md
│   ├── 05_素材编辑与路由.md
│   └── 06_掩蔽与分频高亮.md
├── 故障房/
│   ├── BEAM_JUCE消息线程死锁.md
│   ├── Word_docx打开警告与论文输出.md
│   └── 插件兼容性记录.md
├── 论文协作房/
│   ├── 论文图例需求总表.md
│   ├── 正文与附录图例映射.md
│   └── 老师反馈关联.md
└── 资产索引/
    ├── 关键需求单索引.md
    ├── 关键源码文件索引.md
    ├── 插件与预设状态索引.md
    └── 测试输出索引.md
```

如果施工队觉得中文路径对构建工具有风险，也可以使用英文目录名：

```text
memory_palace/
├── README.md
├── 00_index.md
├── timeline/
├── feature_rooms/
├── incident_rooms/
├── thesis_collab/
└── asset_index/
```

二选一即可，不要两套并存。

## 3. README 必须说明的规则

`README.md` 至少包含：

1. 新 AI 接手 GOODMETER 时的读取顺序。
2. 哪些内容属于“当前事实”，哪些只是历史方案。
3. 每次施工后如何追加记忆。
4. 哪些信息禁止记录。
5. 如何和论文侧记忆宫殿互相引用。

建议读取顺序：

1. `README.md`
2. `00_总索引.md`
3. `时间线/最近关键时间线.md`
4. 对应功能房或故障房
5. 资产索引

## 4. 每条记忆的标准格式

每次完成一个功能、修一个 bug、跑一轮图、或生成一份需求单，都追加一条：

```md
## YYYY-MM-DD 上午/下午/晚上｜事件标题

触发问题：
用户、论文组或老师反馈提出了什么问题。

判断：
施工队当时如何判断问题边界。

改动文件：
- /Users/caiyiyang/Desktop/GOODMETER/Source/...

验证：
- ./build.sh standalone
- GOODMETER --audio-doctor-job ...
- 其他手动 UI 验证

输出产物：
- /tmp/...
- /Users/caiyiyang/Desktop/AudioDoctor_Exports/...

论文影响：
这个功能对应论文哪一章、哪张图或哪个附录。

风险与未完成：
还有什么没验证、什么不能过度宣称。

状态：
有效 / 部分过时 / 已废弃

标签：
#AudioDoctor #JobRunner #SpatialImage #论文图例
```

## 5. 第一批应沉淀的关键节点

### 5.1 GOODMETER 论文协作起点

相关文件：

- `/Users/caiyiyang/Desktop/GOODMETER/GOODMETER_THESIS_COLLAB_HANDOFF_2026-05-01.md`
- `/Users/caiyiyang/Desktop/GOODMETER/GOODMETER_THESIS_BRANCH_HANDOFF_2026-05-05.md`

要记录：

- Audio Doctor 被定位为桌面端软件的新技能，而不是独立新 App。
- 论文组负责提出图例证明需求，施工队负责实现方式。
- 正文不应暴露太多内部 JobRunner 工作流。

### 5.2 Audio Doctor 编辑与路由

相关文件：

- `/Users/caiyiyang/Desktop/GOODMETER/AUDIO_DOCTOR_EDIT_ROUTING_REQUIREMENTS_2026-05-02.md`

要记录：

- DRY A/B/C、WET A/B/C 的路由设计。
- 编辑窗口、裁剪、send 回各表。
- AI/JobRunner 也需要能操作素材编辑与总线分配。

### 5.3 论文图例升级

相关文件：

- `/Users/caiyiyang/Desktop/GOODMETER/AUDIO_DOCTOR_THESIS_FIGURE_UPGRADE_TICKET_2026-05-05.md`
- `/Users/caiyiyang/Desktop/GOODMETER/AUDIO_DOCTOR_THESIS_DARK_SPATIAL_EXPORT_REQUIREMENTS_2026-05-07.md`

要记录：

- Reverb/Space、Dynamics、Group Delay、Spectrogram、Spectrum、Spatial Image 等图例能力。
- 用户最终偏好暗色主题。
- 图例必须记录 source、selection、参数快照、state、figure mode、time point/window。

### 5.4 Spatial Image / 2.5D 图

相关文件：

- `/Users/caiyiyang/Desktop/GOODMETER/AUDIO_DOCTOR_SPATIAL_FIGURE_HANDOFF_2026-05-07.md`
- `/Users/caiyiyang/Desktop/GOODMETER/AUDIO_DOCTOR_SPATIAL_HEATMAP_ENRAGE_REQUIREMENTS_2026-05-07.md`

要记录：

- 正文不要写内部俗称“空间印象图”。
- 论文中应写“声场中声像分布示意图”“立体声声像分布”等。
- 正文如果用 Spatial Image，最好至少两到三个时间点。
- Front High / Side High 优先，Low/Diagonal 不优先。
- `flip time` 是为了避开瞬态墙遮挡，方便选取可读角度。

### 5.5 BEAM 渲染与 state

相关文件：

- `/Users/caiyiyang/Desktop/GOODMETER/AUDIO_DOCTOR_BEAM_RENDER_BUG_2026-05-07.md`

要记录：

- BEAM GUI 卡死原因是 BEAM 在后台 `prepareToPlay` 时调用 JUCE message thread，和 UI 渲染路径形成死锁。
- 修复方向：UI render 的插件实例创建、prepareToPlay、releaseResources 放到主线程，processBlock 仍后台跑。
- JobRunner 命令行本身可跑，不等于 GUI 没问题。
- BEAM 预设/状态需要通过 `pluginStateBase64` / state hash 保持复现。

### 5.6 掩蔽与分频高亮

相关文件：

- `/Users/caiyiyang/Desktop/GOODMETER/AUDIO_DOCTOR_MASKING_AND_BAND_HIGHLIGHT_HANDOFF_2026-05-08.md`

要记录：

- 先做 `Critical Band Masking Risk / 临界带遮蔽风险`。
- 不要一上来宣称真实 masked threshold。
- 有余力再做 `Estimated Masked Threshold / 估计遮蔽阈值`。
- 论文里只能写“遮蔽风险”“融合趋势”，不能写“人耳已经听不见”。

## 6. 资产索引要求

### 6.1 关键需求单索引

记录所有需求单：

| 文件 | 日期 | 触发方 | 当前状态 | 是否已实现 |
|---|---|---|---|---|

### 6.2 关键源码文件索引

至少记录：

| 文件 | 负责功能 | 高危点 |
|---|---|---|
| `Source/AudioDoctorComponent.h` | UI、导出、视图绘制 | 文件大，改动易冲突 |
| `Source/AudioDoctorJobRunner.h` | JobRunner、manifest、批量导出 | 论文图例复现核心 |
| `Source/AudioDoctorPluginHost.h` | 插件加载、渲染、state | 插件兼容性高危 |
| `Source/AudioDoctorFigureRenderer.h` | 图例渲染、主题配色 | 论文图视觉一致性 |
| `Source/AudioDoctorAnalysis.h` | 分析指标、曲线、统计 | 数据可信度核心 |

### 6.3 插件与预设状态索引

记录：

- 插件名
- 格式：AU / VST3 / AAX 等
- 在 Audio Doctor UI 是否能渲染
- 在 JobRunner 是否能渲染
- 是否支持 state capture
- 已试过的 preset / state hash
- 论文用途

已知需要记录的插件：

- kHs Distortion
- kHs Reverb
- Disperser
- FabFilter Pro-C 2
- BEAM
- Enrage
- ShaperBox
- Soundtoys 相关插件

### 6.4 测试输出索引

记录每次关键输出：

- 输出路径
- job JSON 路径
- manifest 路径
- figure 路径
- 使用素材
- 使用插件/参数
- 对应论文图号或附录图号

## 7. 安全与边界

不要把以下内容写进记忆宫殿：

- API key
- 公司服务器 URL 的敏感 token
- 私人账号凭证
- 插件授权码
- 未公开片源下载链接
- 任何可能导致版权或账号风险的具体来源信息

可以记录：

- 插件是否可用。
- 技术问题和修复方式。
- 本地路径。
- 论文图例使用的素材文件名和拷贝路径。
- GitHub 项目公开链接。

## 8. 与论文记忆宫殿的互链

GOODMETER 记忆宫殿关注“工程事实”；论文记忆宫殿关注“论文决策”。

两个宫殿要互相引用，但不要混成一个：

- 论文宫殿记录：老师反馈、正文措辞、章节结构、图例使用策略。
- GOODMETER 宫殿记录：实现细节、构建验证、插件兼容、manifest、JobRunner、源码位置。

当一个 GOODMETER 功能进入论文时，GOODMETER 侧记录：

> 论文影响：用于第 X 章图 X-X / 附录 X。

论文侧记录：

> 图例来源：GOODMETER Audio Doctor，技术实现详见 GOODMETER 记忆宫殿对应条目。

## 9. 交付验收

施工队完成后，至少应交付：

1. GOODMETER 记忆宫殿目录。
2. README 和总索引。
3. 5 月 1 日至 5 月 8 日关键时间线。
4. Audio Doctor / JobRunner / Spatial Image / BEAM / Masking 五个主题房间。
5. 关键需求单、源码文件、插件状态、测试输出四个索引。
6. 一条示范记忆，证明后续 AI 知道怎么追加。

这座宫殿不需要一次写成百科全书，第一版先保证能让后续 AI 快速复活 GOODMETER 论文施工现场。

