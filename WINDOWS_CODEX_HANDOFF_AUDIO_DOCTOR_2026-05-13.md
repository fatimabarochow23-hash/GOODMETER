# GOODMETER / Audio Doctor Windows Codex 交班文档

日期：2026-05-13  
交班目标：让 Windows 侧 Codex 接手 GOODMETER 的 Windows 版构建、打包与 `.clz` 工程跨平台验证。

## 1. 当前仓库与分支

- 仓库：GOODMETER
- 当前工作分支：`codex/thesis-audio-doctor-20260505`
- 远端：GitHub `fatimabarochow23-hash/GOODMETER`
- 当前主要工作区来自 Mac 外置盘：`/Volumes/solari 1/Codex_Work/GOODMETER`
- 当前 `Builds` 目录只有 `MacOSX`、`MacOSX_Plugin`、`iOS`，Windows 侧需要先检查 `GOODMETER.jucer` 是否已有 Windows exporter；如果没有，最小化新增 Visual Studio exporter，不要破坏现有 Mac/iOS exporter。

## 2. 为什么要做 Audio Doctor

Audio Doctor 是 GOODMETER 里服务论文的独立分析工具，不是普通音频播放器。它的核心用途是把动画电影能量碰撞音效中的分层、插件处理、空间变化、掩蔽/融合关系变成可复核的图、CSV 和 manifest。

论文当前最需要它支持这些工作：

- 导入或生成 Dry A/B/C 素材。
- 加载 Plugin A/B/C，对 Dry 源进行离线渲染，得到 Wet A/B/C。
- 导出 Spectrum、Envelope、Group Delay、Spectrogram A/B/C、Reverb Space、Spatial Image、Layer Fit / Fusion 等图。
- 导出同一批图背后的 CSV 数据和 JSON manifest，保留素材、hash、采样率、插件、参数、渲染延迟、tail seconds 等实验记录。
- 保存 `.clz` 工程，让论文工作可以在不同电脑上继续，不依赖一次性 UI 状态。

## 3. 这两天 Mac 侧已经做完的重点

请不要把 Audio Doctor 当成刚起步的草稿工具。当前分支已经包含较完整的论文工具链：

- Audio Doctor 主界面：Dry A/B/C、Plugin A/B/C、Generate、Bus、Export、Reset。
- 插件插入槽：类似 Pro Tools 插入槽，空槽点击加载插件，已加载槽点击显示插件 UI；插件 UI 底部带输出 gain，Render 逻辑接入渲染链路。
- Generate：可选择输出到 Dry A/B/C，不再只服务 Dry A。
- `.clz` 工程：保存当前 Audio Doctor 状态、素材快照和工程音频文件，目标是像 `.ptx` / `.rpp` 那样跨机器打开。
- 图表交互：平面图支持鼠标位置滚轮缩放；3D / 2.5D 图支持拖拽视角；视角下拉框保留为五种起始视角复位。
- Layer Fit / Fusion：包含 Critical Band Terrain、Time-Frequency Terrain、Spatial Image、Critical Band Crystal、Dodecahedron Crystal。
- Dodecahedron Crystal：十二面体晶体图已支持鼠标旋转、时间滑条、正放/倒放金字塔播放、空格暂停/继续。
- Export / Job Runner：原则上应输出图、CSV、manifest，并服务论文侧批处理。
- macOS 侧已有签名/公证流程产物，但 Windows 侧不用接触 Mac 的 `Signing/`、notary key 或 dmg 产物。

## 4. Windows 侧最高优先级

### P0：构建 Windows Standalone

目标是先拿到老师能打开的 Windows 版 GOODMETER / Audio Doctor。

建议顺序：

1. 拉取并切到 `codex/thesis-audio-doctor-20260505`。
2. 检查 `GOODMETER.jucer` 是否已有 Windows / Visual Studio exporter。
3. 如果没有，使用 Projucer/JUCE 增加 Windows exporter，尽量只增加 Windows 构建配置，不重写现有 Mac/iOS 配置。
4. 先构建 Standalone。插件格式可以后置，老师演示和论文跑图优先需要 standalone app。
5. 如果遇到 ONNX、VST3 SDK、JUCE 模块路径、资源路径问题，不要硬编码 Mac 路径；要改成跨平台探测或 Windows 条件分支。

### P0：验证 `.clz` 工程跨平台

这是最重要的验收项。新版 Windows 迭代完必须确保 `.clz` 工程可以跨平台打开。

最低验收矩阵：

1. 在 Mac 版保存一个 `.clz`，工程内包含 Dry A、Dry B、Dry C 中至少两个素材。
2. 将 `.clz` 复制到 Windows。
3. Windows 版打开 `.clz` 后，应恢复：
   - 当前视图类型。
   - Dry A/B/C 当前素材。
   - Wet A/B/C 当前渲染素材，如果工程保存时存在。
   - Generate 参数快照。
   - Bus / Stem / Bounce / Figure / Band / Angle / Flip Time 等 Layer Fit 状态。
   - 插件槽名称、参数快照、plugin state hash。
4. 如果 Windows 没有安装同名插件，工程仍然必须打开，插件槽应显示 missing / unavailable 状态，不得崩溃，不得清空音频素材。
5. 如果 Windows 安装了对应 VST3，但路径不同，应允许用户重新定位或重新加载插件，同时保留原 manifest 记录。
6. 路径要支持：
   - 空格。
   - 中文。
   - Windows 反斜杠。
   - Mac 保存时的 `/Volumes/...` 绝对路径失效。

请特别确认 `.clz` 内部引用音频时使用工程内复制的 audio files，而不是原始绝对路径。原始路径可以保留在 manifest 作为 provenance，但打开工程不能依赖它。

## 5. `.clz` 跨平台设计要求

终版应遵守这些规则：

- `.clz` 是 GOODMETER Audio Doctor 工程格式，不是普通导出截图。
- 保存工程时，只保存当前仍在 Audio Doctor 槽位中的音频：Dry A/B/C、Wet A/B/C、必要的 generated source 和 rendered output。
- 保存工程时，应把这些素材复制进工程包/工程目录的 audio files 区域。
- project manifest 可以记录原始路径，但打开工程优先使用工程内相对路径。
- 插件路径不能要求跨平台一致。Windows 没有 AU，必须以 VST3 为主。
- 插件状态应尽量保存：
  - plugin name
  - plugin format
  - plugin identifier
  - original path
  - parameter snapshot
  - native state blob/hash，如已有
- 缺插件时只降级插件槽，不降级整个工程。
- Windows 若要做双击打开 `.clz`，需要注册文件关联；如果暂时不做关联，也至少要支持从 app 内打开 `.clz`。

## 6. Job Runner 必测清单

Windows 版不仅要 GUI 能打开，还要保证 Job Runner 仍然可用。论文组会用它批量跑图。

必须验证：

- job 文件路径带空格时，`--audio-doctor-job` 不解析失败。
- `outDir` 路径带空格、中文时能正常写出。
- 能导入本地 audio。
- 能使用 generated signal。
- 能加载 VST3 插件。
- 能设置 normalized params。
- 能离线 render A/B/C。
- 能导出 PNG。
- 能导出 CSV。
- 能导出 manifest / response。

建议至少跑这些 view：

- `spectrum`
- `envelope`
- `group_delay`
- `spectrogram_abc`
- `reverb_space`
- `spatial_image`
- `layer_fit_fusion`
- `critical_band_terrain`
- `time_frequency_terrain`
- `critical_band_crystal`
- `dodecahedron_crystal`

如果当前 Job Runner 的 view token 名称与上面不同，请以代码实际枚举为准，但要在交回报告里列出最终可用名称。

## 7. Layer Fit / Fusion 的算法与显示边界

这块是论文展示肌肉的重点，不要为了“看起来更满”改坏算法。

当前图的意义：

- Critical Band Terrain：临界带维度下的多层能量占位和重叠风险。
- Time-Frequency Terrain：时间、频率、能量的 2.5D 山峦图，用于展示多层素材在时频域的贴合与错位。
- Spatial Image：L-C-R 横向、频率纵向、能量高度，颜色用于表示空间宽度/侧向成分，不是简单左蓝右红。
- Critical Band Crystal：用晶体形态表示临界带风险、融合和合成结果。
- Dodecahedron Crystal：十二面体晶体，面代表临界带关系，红/紫/黄分别对应遮蔽风险、融合倾向、合成增益。

注意：

- 下方指标里的 strongest band 不能永远是 B0；必须反映当前最强风险/融合/增益所在临界带。
- 分频高亮时，只显示当前 focus band 范围内的标签，非高亮区域不要继续贴满标签。
- 红/紫/黄色 key 下方应显示对应的多个 band，不要只显示一个 band。
- 如果 Windows 侧修改图形，请优先保证可读性、图注可解释性和论文截图清晰度。

## 8. Windows 打包建议

第一版建议先交付：

- 一个 portable zip，包含 `GOODMETER.exe` 和必需资源。
- 一份 README，写明如何打开 `.clz`、如何打开 Audio Doctor、插件路径限制。
- 一个 smoke-test `.clz`，最好只用 generated signal 或仓库里允许分发的小测试素材，避免插件依赖。

之后再考虑：

- MSI / MSIX / Inno Setup 安装包。
- `.clz` 文件关联。
- Windows 图标 `.ico`，使用当前新的海鸥图标资源。

## 9. 不要提交或不要碰的内容

这些内容不要提交到 Git：

- `Signing/`
- `.p8`
- notary / App Store Connect API key
- Apple 证书私钥
- `dist/`
- `tmp/`
- `releases/*.dmg`
- macOS AppleDouble 文件：`._*`
- Word 临时文件：`~$*.docx`

如果 Windows 侧需要自己的签名证书，另走 Windows 代码签名流程，不要复用 Mac 证书。

## 10. Windows Codex 交回时请报告

请在交回说明里明确写：

- 当前 commit hash。
- 使用的 Windows 工具链和版本。
- 是否新增 Windows exporter。
- Standalone exe 路径。
- 是否有 installer/zip。
- `.clz` 跨平台测试结果。
- 缺插件场景是否能打开工程。
- Job Runner 跑过哪些 view。
- 输出图、CSV、manifest 的样例路径。
- 仍然不支持或需要主线继续做的事项。

## 11. 最小验收标准

Windows 侧这轮不用追求所有论文图都终局完美，但必须满足：

- App 能启动。
- Audio Doctor 能进入。
- `.clz` 能打开 Mac 保存的工程。
- 缺插件不崩溃。
- 至少一个 generated-only 工程能保存、关闭、重新打开。
- Job Runner 能跑一张 spectrum、一张 spectrogram_abc、一张 layer_fit_fusion，并输出 PNG + CSV + manifest。

完成这些后，再继续优化 Windows installer 和文件关联。
