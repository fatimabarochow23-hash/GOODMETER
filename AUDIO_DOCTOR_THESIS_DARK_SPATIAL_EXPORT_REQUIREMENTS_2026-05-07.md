# Audio Doctor 论文暗色空间图组追加需求（2026-05-07）

面向：GOODMETER / Audio Doctor 施工队  
提出方：论文组  
优先级：高  
目标：把新完成的 2.5D Spatial Terrain 与 Spatial Image / L-C-R 空间印象图真正变成论文可用的核心图组，而不是只作为演示功能存在。

---

## 1. 背景与论文侧判断

论文第四章现在最需要补强的是“空间处理为什么有效”这一部分。原先的 EDC、DRR、Tail Spectrogram 能说明混响尾音和衰减，但对“主体集中在中心、尾音向侧面扩散”这类空间听感，二维曲线不够直观。

本轮新增的 Spatial Image / L-C-R 空间印象图和 2.5D Spatial Terrain 很适合补这一块：

- Spatial Image 适合正文展示：Dry 比较瘦、Wet 更宽，中心/侧向差别一眼能看懂。
- 2.5D Spatial Terrain 适合作为所有瀑布/时频图的升级图或附录补充：可以看瞬态墙、尾音坡、频段残留和空间宽度变化。
- Enrage 适合做论文案例：它有 L/R、M/S 分路和模块化效果链，能直观解释“空间处理不是整体加宽，而是分路管理中心与侧向能量”。

论文侧口径提醒：正式论文里统一用“笔者”，不要混用“作者”。GOODMETER 脚注可写为：

> GOODMETER 为笔者从零开发的桌面端音频测量与分析软件。Audio Doctor 是笔者在 GOODMETER 中扩展的论文分析模块，功能思路参考 Plugin Doctor，并针对本文图例需求独立实现素材导入、插件渲染与图表导出。项目地址：https://github.com/fatimabarochow23-hash/GOODMETER.git

---

## 2. 全局图例策略：统一暗色主题

论文侧决定后续图例统一使用暗色主题，不再混用 `academic_light` / 白色主题。

要求：

1. 所有正文图、附录图、补充图默认使用 `ui_dark`。
2. JobRunner 文档示例、导出 preset 示例优先写 `ui_dark`。
3. 不要在论文最终批量导出时自动切回 `academic_light`。
4. manifest 里继续记录 `preset` / `palette`，方便复核。

原因：

- 暗色图在电子版里可读性和专业感更强。
- 新空间图依赖颜色和山体层次，白底学术配色会明显损失信息密度。
- 打印可读性让位于电子版可读性，论文侧已接受这一取舍。

---

## 3. 2.5D Spatial Terrain 的论文导出规则

### 3.1 每个带瀑布/时频谱的图，都应能补一组 2.5D

凡是正文或附录里使用以下图类，都建议补对应 2.5D：

- Spectrogram A/B/C
- CST Spectrogram
- Reverb / Space 的 Tail Spectrogram
- 任何用于解释尾音、能量展开、频段残留的时频图

论文侧不一定把全部 2.5D 放正文，但需要导出完整图组，便于选择和放附录。

### 3.2 固定优先视角：Front High + Side High

论文导出时，2.5D 图优先只用高视角：

1. `front_high`
2. `side_high`

不建议默认导：

- `front_low`
- `side_low`
- `diagonal`

原因：

- Low 模式容易被瞬态墙挡住，论文图缩小后可读性差。
- Diagonal 虽然好看，但对正文论证不如 Front High / Side High 清楚。
- Front High 更适合看整体能量山体与时间展开。
- Side High 更适合看尾音坡、瞬态墙和频段残留。

需求：

- JobRunner 支持同一素材/同一模板一次导出 `front_high` 与 `side_high` 两张图。
- 文件名必须包含 camera token，例如：
  - `..._spatial_heatmap_front_high_ui_dark.png`
  - `..._spatial_heatmap_side_high_ui_dark.png`
- manifest 中必须记录：
  - `terrainCamera`
  - `terrainTimeReversed`
  - `preset`
  - `sources`
  - `sharedScale` 状态

### 3.3 Flip Time 是论文必要功能，不是 UI 小玩具

有些素材瞬态在开头，有些素材瞬态在结尾。2.5D 图有时间轴，如果瞬态形成一堵高墙，会遮挡后续尾音和频段残留。

要求：

1. JobRunner 继续支持 `timeReversed` / `terrainTimeReversed` / `reverseTime`。
2. 文件名必须体现是否翻转，例如：
   - `..._front_high_normal_ui_dark.png`
   - `..._front_high_flip_time_ui_dark.png`
3. manifest 中必须记录：
   - `terrainTimeReversed: true/false`
4. 图内角落或说明区最好显示：
   - `Time flipped`
   - 或中文导出 caption 可写“时间轴已翻转，仅改变观察方向，不改变底层数据”。

论文侧使用原则：

- 如果开头瞬态挡住尾音，使用 Flip Time。
- 如果结尾瞬态挡住主体，使用正常时间轴或视情况 Flip Time。
- Flip Time 只用于改善观察角度，正文不能写成“处理改变了声音时间方向”。

---

## 4. Spatial Image / L-C-R 空间印象图要求

### 4.1 正文至少需要早期窗 + 尾音窗

空间印象图不应只导一张全局平均图。论文需要证明“空间处理改变了事件的时间后果”，所以至少要有两个时间窗：

1. 早期/命中时间窗  
   用来观察主体是否集中在中心，是否保留明确命中边界。

2. 尾音时间窗  
   用来观察原素材快消失时，Wet 是否仍保留侧向扩散、宽化或混响尾部。

示例论文图组：

- `图4-X Enrage 处理前后早期空间印象`
- `图4-X+1 Enrage 处理前后尾音空间印象`

JobRunner 需要支持：

- `spatialWindow: "attack" | "body" | "tail" | "full"`
- `spatialTimePositionSeconds`
- 或直接指定 `spatialWindowStartSeconds` / `spatialWindowEndSeconds`

如果目前只能用 `spatialWindow` 和 `timePosition`，建议补精确窗口字段，方便论文按具体事件截取。

### 4.2 允许自适应 0/1/2 个面板，但论文默认双面板

空间印象图的论文用法以双面板为主：

- 左：Dry / 原素材
- 右：Wet / 处理后素材

需求：

1. 支持 1 个面板，用于单独说明某个素材。
2. 支持 2 个面板，用于 Dry / Wet 对比。
3. 超过 2 个素材时不要硬塞三图，宁可分两张图导出。
4. 若某个 source 缺失，应自动跳过，不留空白大面板。

### 4.3 共享标尺是硬要求

Dry / Wet 成对比较时，必须使用共享标尺。否则 Dry 已经快消失但被自动归一化拉高，会造成误导。

要求：

- Spatial Image 双面板使用同一能量 scale。
- 2.5D Spatial Terrain 双面板使用同一能量 scale。
- manifest 写明：
  - `sharedScale: true`
  - `scaleMinDb`
  - `scaleMaxDb`
- 如果用户关闭共享标尺，manifest 必须记录 `sharedScale: false`。

论文侧默认要求：所有 Dry / Wet 对比图都用共享标尺。

### 4.4 图注边界语需要稳定输出

Spatial Image 不是真实环绕声定位测量。图注建议固定输出边界说明，避免论文表述过度。

建议 caption 语：

> 空间印象图由立体声信号的 L/R 能量差、Side/Mid 比例与相关性推导得到，用于观察所选时间窗内不同频段的中心与侧向分布，不等同于多声道声场定位测量。

---

## 5. Enrage 复现需求

Enrage 很适合做论文空间处理案例，但默认状态没有明显空间变化。论文真正需要的是一套可复现的 Enrage 处理链。

### 5.1 JobRunner 需要支持插件状态 / 预设加载

当前参数快照能导出 2100+ 个参数，但如果没有加载具体 preset/state，默认状态无法体现 Enrage 的 L/R、M/S 分路能力。

需求：

1. 支持从 job 中加载插件状态文件：
   - `statePath`
   - 或 `presetPath`
   - 或 `pluginStateBase64`
2. 渲染前先加载 state，再应用 `params` 覆盖。
3. manifest 记录：
   - `statePath`
   - `stateHash`
   - `stateLoaded: true/false`
4. 如果 state 加载失败，JobRunner 返回明确错误，不要静默回到默认状态。

### 5.2 参数快照要能服务论文复核

manifest 里保留：

- plugin name
- plugin format
- plugin path
- changedParameters
- displayValue
- normalizedValue
- stateHash

对于 Enrage，参数很多，不建议把所有 2100+ 参数塞进论文附录。论文附录只需要：

- 预设/状态名称或 stateHash
- 关键模块说明
- 关键宏或关键参数
- 完整 manifest 文件路径或项目导出记录

### 5.3 Enrage 论文处理链建议

建议施工队或论文组做一个“空间扩展示例 preset/state”：

- 主体命中保留中心：Mid 或中心链保留较短、较硬的瞬态。
- 侧向尾音扩散：Side 或 L/R 链加入 delay、filter、reverb、modulation。
- 高频边缘适度侧向展开，低频主体避免过宽。

论文解释重点：

> Enrage 的价值不只是“加宽”，而是通过 L/R 与 M/S 分路，把命中主体、侧向拖尾和频段修饰放进不同信号路径中处理。

---

## 6. 文件命名与覆盖 bug

验收时发现：同一个 job 中如果导出两张同模板 thesis figure，例如两个 `spatial_impression`，当前可能写到同一个文件名，后者覆盖前者。

这是正式批量导出前必须修的 bug。

要求：

1. thesis figure 文件名必须包含唯一 token。
2. 优先组合：
   - figure index
   - template token
   - title token
   - camera token
   - spatial window token
   - flip token
   - preset
3. 示例：
   - `chapter4_4_01_spatial_impression_enrage_attack_ui_dark.png`
   - `chapter4_4_02_spatial_impression_enrage_tail_ui_dark.png`
   - `chapter4_4_03_spatial_heatmap_front_high_flip_time_ui_dark.png`

manifest 中 `thesisFigures[].path` 必须与实际文件一一对应，不能出现多条记录指向同一个 PNG。

---

## 7. 批量导出建议结构

建议 JobRunner 支持这种结构，减少论文组反复手写 job：

```json
{
  "export": {
    "preset": "ui_dark",
    "sharedScale": true,
    "outDir": "/path/to/out"
  },
  "thesisFigures": [
    {
      "template": "spatial_heatmap",
      "sources": ["dryA", "wetA"],
      "cameras": ["front_high", "side_high"],
      "timeReversed": "auto"
    },
    {
      "template": "spatial_impression",
      "sources": ["dryA", "wetA"],
      "windows": [
        { "name": "attack", "start": 0.08, "end": 0.28 },
        { "name": "tail", "start": 1.45, "end": 1.85 }
      ]
    }
  ]
}
```

如果短期不做 `auto`，至少先支持显式数组展开：

- `cameras`
- `windows`
- `timeReversedVariants`

---

## 8. 论文侧最终图组建议

### 正文

正文不应变成图展。建议只放最有解释力的空间图：

1. Enrage 处理前后早期空间印象图
2. Enrage 处理前后尾音空间印象图
3. 视情况补一张 Enrage 2.5D Front High 或 Side High 图

### 附录

附录补完整图组：

- 所有正文瀑布图对应的 2.5D Front High
- 所有正文瀑布图对应的 2.5D Side High
- 必要时补 Flip Time 版本
- Enrage early / tail 空间印象图
- Enrage 处理链说明与参数/状态快照

---

## 9. 验收清单

施工完成后，请至少验证：

- `./build.sh standalone` 通过。
- 同一个 job 中导出多张 `spatial_impression` 不再覆盖。
- `front_high` / `side_high` 文件名、manifest、图内说明一致。
- `terrainTimeReversed` 能正确导出，且文件名能区分 normal / flip_time。
- Dry / Wet 双图共享能量标尺，manifest 可查。
- Enrage state/preset 能被加载、渲染、记录 hash。
- `ui_dark` 作为论文导出默认 preset 可稳定使用。
- 输出图在 1800x1050 或更高尺寸下缩进到 Word 后仍可读。

---

## 10. 给论文组的注意事项

论文写作时请使用这些边界表达：

- 可以写：“空间印象图显示，处理后尾音段的侧向成分更明显。”
- 可以写：“2.5D 图显示，处理后尾音在中高频段保留更长的能量坡度。”
- 不要写：“该图证明真实空间定位发生改变。”
- 不要写：“2.5D 图表示真实三维声场。”
- Flip Time 只能写成“为观察方便翻转时间轴”，不能写成“声音时间结构被反转”。

