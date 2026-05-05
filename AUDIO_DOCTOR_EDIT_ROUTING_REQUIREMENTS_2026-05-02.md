# Audio Doctor 快捷编辑与 DRY/WET 路由需求

写给 GOODMETER 施工队。本文只描述论文侧和使用侧需求，不限定具体实现方式。

## 目标

Audio Doctor 需要从“加载素材后出图”升级为“能快速整理素材、比较多份素材、比较多条效果链”的论文图例工具。核心目标不是做完整 DAW，而是在分析前完成少量高频编辑动作：裁头、裁尾、淡入淡出、替换当前分析素材，并让表格和导出数据自动刷新。

## 一、顶栏 EDIT 入口

在 Audio Doctor 顶栏增加 `EDIT` 按钮。

点击后弹出当前有内容的通道列表，例如：

- `DRY A`
- `DRY B`
- `DRY C`
- `WET A`
- `WET B`
- `WET C`

列表只显示当前确实有内容或可编辑来源的通道。用户点击某一项后，打开一个独立编辑窗口。

## 二、独立波形编辑窗口

编辑窗口显示所选素材的波形。波形绘制可参考 Audio Lab 现有 waveform 风格，但这里优先服务“快速裁剪”和“回传分析”，不需要复杂多轨编辑。

最低需求：

- 单条大波形视图；
- 播放、暂停、回到开头；
- 鼠标点击定位播放头；
- 可拖拽选择时间范围；
- 可设置入点、出点；
- 可预听编辑结果；
- 右下角有 `Send` 按钮。

快捷键按 Pro Tools 思路：

- `A`：从素材开头裁到播放头；
- `S`：从播放头裁到素材结尾；
- `D`：对选区或素材开头添加短淡入；
- `F`：对选区或素材结尾添加短淡出。

建议同时支持：

- 撤销/重做；
- 零点裁切，避免点击爆音；
- 可选吸附瞬态；
- 不覆盖原文件，保存为临时派生素材；
- 记录来源素材路径、裁剪时间范围、淡入淡出长度，写入 manifest。

## 三、Send 回流逻辑

点击 `Send` 后，编辑结果回到原通道并刷新所有分析视图。

如果编辑的是 `DRY A/B/C`：

- 对应 DRY 替换为新派生素材；
- Spectrum、Envelope、Spectrogram、Group Delay、Reverb/Space、Dynamics 等视图刷新；
- 所有以该 DRY 为输入的 WET 重新渲染；
- manifest 记录本次编辑来源、裁剪范围、生成时间。

如果编辑的是 `WET A/B/C`：

- 优先理解为编辑 WET 渲染结果本身；
- 不反向修改原 DRY；
- 如果当前实现不希望支持 WET 结果编辑，可以先禁用 WET 编辑入口，只显示 DRY A/B/C。

## 四、增加 DRY B 与 DRY C

当前 `DRY` 改名为 `DRY A`，并增加 `DRY B`、`DRY C`。Audio Doctor 最多同时管理三个原始素材。

使用场景：

- 比较三种素材的分层位置；
- 比较 charge、shot、tail 三个阶段；
- 比较处理前后不同素材的频谱/包络/空间残留；
- 给论文第三章和第四章快速生成 A/B/C 图。

## 五、显示槽与实际路由分离

保留现有“三条可见轨道/三条图例线”的上限，但不要把显示顺序写死为 DRY/WET A/WET B。

新增一个 Bus / Display 设置页，分成两块。

### 1. Display Slots

提供三个显示槽：

- `Display 1`
- `Display 2`
- `Display 3`

每个显示槽右侧是下拉框，可选择：

- `DRY A`
- `DRY B`
- `DRY C`
- `WET A`
- `WET B`
- `WET C`

允许多个显示槽选择同一个内容。用户可以自由显示 `DRY B / WET A / DRY A`、`DRY A / DRY B / DRY C`、`WET A / WET B / WET C` 等组合。

所有图表、截图导出、CSV 导出、manifest 里的可见轨道顺序，都应跟 Display Slots 保持一致。

### 2. Render Routing

提供 DRY 到 WET 的连线关系。

左列：

- `DRY A`
- `DRY B`
- `DRY C`

右列：

- `WET A`
- `WET B`
- `WET C`

用户通过连线决定每个 WET 链渲染哪些 DRY。

需要支持的关系：

- 一个 DRY 连接多个 WET：同一素材经过多种效果链，适合做处理策略对比；
- 一个 WET 连接多个 DRY：多个素材共用同一套插件链，适合做素材适配和分层一致性对比；
- 多个 WET 可以引用同一个 DRY；
- 未连接输入的 WET 不参与渲染，也不出现在可选显示项里，除非已有缓存结果。

## 六、多输入 WET 的显示语义

如果一个 WET 连接多个 DRY，需要界面明确显示当前 WET 的输入关系。

建议语义：

- `WET A = Chain A applied to selected DRY inputs`
- 如果多个 DRY 同时连接 `WET A`，`WET A` 可先渲染为一个合成输出，用于观察“多层素材经过同一效果链后的整体结果”；
- manifest 必须记录 `WET A` 的输入列表，例如 `inputs: [DRY A, DRY C]`。

如果施工难度较高，可以 MVP 阶段先限制“一个 WET 只显示一个当前输入”，但数据结构和 UI 文案要预留多输入能力。

## 七、Job Runner / AI 操作接口

这部分是硬需求：上述编辑和总线能力不能只存在于 UI。AI 需要能通过 Job Runner 完成同样的操作，从素材选择、裁剪、路由、渲染到导出图表都可以程序化复现。

### 1. UI 与 Job Runner 能力等价

凡是用户能在 Audio Doctor 里手动完成的以下动作，都应有 Job Runner 等价能力：

- 导入 `DRY A/B/C`；
- 对任意 DRY 执行裁头、裁尾、淡入、淡出；
- 将编辑后的派生素材写回对应 DRY；
- 设置三个 Display Slots；
- 设置 `DRY A/B/C` 到 `WET A/B/C` 的路由；
- 触发依赖 WET 自动重新渲染；
- 导出当前显示槽对应的 PNG、CSV、manifest。

AI 不应该只能读取最终数据，还应该能提交编辑和路由决策。这样论文 agent 才能自己从音效库挑素材、截取合适片段、配置对比关系、生成图例，再把图和数据反哺回第三章、第四章文字。

### 2. 建议的 Job JSON 语义

字段名可以由施工队按现有代码风格调整，但语义需要覆盖这些块：

```json
{
  "schemaVersion": 2,
  "sessionName": "thesis_energy_collision_example",
  "sources": {
    "dryA": {
      "path": "/path/to/source_a.wav",
      "edits": {
        "trimStartS": 0.120,
        "trimEndS": 1.850,
        "fadeInMs": 8,
        "fadeOutMs": 35,
        "snapToZeroCrossing": true
      }
    },
    "dryB": {
      "path": "/path/to/source_b.wav"
    },
    "dryC": {
      "path": "/path/to/source_c.wav"
    }
  },
  "displaySlots": [
    { "slot": 1, "source": "dryA" },
    { "slot": 2, "source": "wetA" },
    { "slot": 3, "source": "dryB" }
  ],
  "renderRouting": {
    "wetA": {
      "inputs": ["dryA"],
      "chain": "pluginChainA"
    },
    "wetB": {
      "inputs": ["dryA", "dryB"],
      "chain": "pluginChainB"
    }
  },
  "views": ["spectrum", "spectrogram_abc", "envelope", "reverb_space"],
  "export": {
    "preset": "ui_dark",
    "outDir": "/path/to/output"
  }
}
```

这段不是要求照抄字段名，而是要求能力完整。施工时可以兼容旧版 `dry` / `wetA` / `wetB` 写法，但新版任务需要能表达三份 DRY、三份 WET、三条显示槽和多对多路由。

### 3. Job Runner 的编辑输出

Job Runner 执行编辑后，应生成临时派生素材，而不是覆盖原始音效库文件。

manifest 至少记录：

- 原始素材路径；
- 派生素材路径；
- 裁剪起止时间；
- 淡入淡出长度；
- 是否启用零点裁切或瞬态吸附；
- 执行编辑的通道名，例如 `DRY A`；
- 哪些 WET 因该编辑被重新渲染。

### 4. Job Runner 的路由输出

manifest 还需要记录：

- Display Slots 的最终显示顺序；
- 每个 WET 的输入 DRY 列表；
- 每个 WET 使用的插件链或效果链标识；
- 多输入 WET 是“混合后统一渲染”，还是“分别渲染后汇总显示”；
- 每张 PNG 和 CSV 对应哪个显示槽、哪个素材或渲染结果。

### 5. AI 侧验收流程

最小 AI 验收流程：

1. AI 从 `/Volumes/solari 1/sfx file` 选出两到三份素材；
2. AI 写入一个 Job JSON，其中包含 `DRY A/B/C`、裁剪参数、显示槽和路由；
3. Job Runner 生成派生素材；
4. Job Runner 根据路由重新渲染 `WET A/B/C`；
5. Job Runner 导出图表、CSV 和 manifest；
6. AI 读取 manifest 和 CSV，能准确知道每张图来自哪份素材、哪段时间、哪条效果链、哪个显示槽。

这条流程跑通后，论文 agent 才算真正能自己操作 GOODMETER，而不是只会让用户手动截图。

## 八、论文侧优先级

MVP 必须先完成：

1. `EDIT` 按钮；
2. DRY 波形独立编辑窗；
3. `A/S/D/F` 裁剪和淡入淡出；
4. `Send` 后刷新 DRY 与依赖它的 WET；
5. `DRY A/B/C` 三素材管理；
6. Display Slots 下拉选择显示内容；
7. Job Runner 支持导入 `DRY A/B/C`、基础裁剪、Display Slots 和导出 manifest。

第二优先级：

1. Render Routing 连线页；
2. 一个 DRY 对多个 WET；
3. 一个 WET 对多个 DRY；
4. Job Runner 支持 Render Routing；
5. manifest 完整记录编辑与路由。

第三优先级：

1. 瞬态吸附；
2. WET 结果编辑；
3. 编辑窗口里直接显示小型频谱或响度读数。

## 九、验收标准

最小验收流程：

1. 导入 `DRY A`；
2. 点击 `EDIT`，选择 `DRY A`；
3. 在独立窗口用 `A` 或 `S` 裁剪素材；
4. 点击 `Send`；
5. Audio Doctor 所有当前视图刷新；
6. 如果 `WET A` 连接 `DRY A`，则 `WET A` 自动重新渲染；
7. 导出 manifest 能看到原文件路径、裁剪范围、新派生素材路径、显示槽选择、DRY/WET 路由关系。

论文侧真正需要的是“快速把素材修到适合出图的状态”，所以交互要比完整编辑功能更轻、更快、更稳定。
