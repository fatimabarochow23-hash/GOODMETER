# 当前任务卡：Preview Solo / Group Delay / Job Runner

日期：2026-05-22

关联需求：

```text
/Volumes/solari 1/Codex_Work/GOODMETER/AUDIO_DOCTOR_PREVIEW_SOLO_GROUP_DELAY_REQUIREMENTS_2026-05-21.md
```

---

## 1. 用户硬需求

1. 主界面增加六个 3x2 方块。
2. 方块不显示文字标签。
3. 第一排从左到右：DRY A、DRY B、DRY C。
4. 第二排从左到右：WET A、WET B、WET C。
5. 点亮谁，主界面预听就让谁出声；可多选，例如只听 DRY A + DRY B。
6. 这套 solo 逻辑只作用于主界面预听，包括师姐按钮和有进度条的预听。
7. 插件 insert 内部的预听不参与这套逻辑。
8. Group Delay 不能因为加载两条或三条 DRY 就消失。
9. 右侧 Plugin A/B/C 参数色块要加宽，论文/PPT截图里必须能看清。
10. 新功能要进入 `.clz` 工程和 Job Runner，不能只停留在 UI。

---

## 2. 当前源码状态

截至 2026-05-22，本地 `Source/AudioDoctorComponent.h` 已有半成品 diff：

- 新增了 `PreviewSoloButton`。
- 构造函数里开始创建 `previewSoloButtons`。
- 布局里已出现 `layoutPreviewSoloButtons(...)`。
- 已开始加入 `previewSoloSourceSlot`、`sourceSlotColour`、`canPreviewSoloSlot`、`hasAnyPreviewSoloEnabled`、`makePreviewSoloSources`、`refreshPreviewSoloButtons` 等 helper。
- `PluginSlot::C` 的显示颜色已开始从 DRY 色改成 WET C 色。

接手者必须先看当前 diff，不要重写整块 UI，也不要回退半成品。

---

## 3. 待收口清单

1. 确认 `PreviewSoloButton` 视觉状态：未启用、可点、点亮、无素材不可点。
2. 确认 3x2 布局在顶部工具栏里不会挤压 Load / Plugin / Output。
3. 把 solo 选择接入主界面预听音频源混合。
4. 确保插件 insert 内部 preview 仍按原逻辑走。
5. 把 solo 状态写入 `.clz` 保存/加载。
6. 修改 Group Delay 数据选择逻辑，不再依赖 `displaySlots` 排序。
7. Job Runner 增加对应字段、manifest 记录和必要的导出行为。
8. 加宽 Plugin 参数色块，UI 与论文图导出都要检查。

---

## 4. Group Delay 修复方向

现象：DRY 素材加载大于等于两条时，Group Delay 图可能显示空白。

核心原因：UI 显示槽优先放 DRY A/B/C 后，WET A/B/C 可能被挤出；旧 Group Delay 读取 `displayAsset(1/2)`，所以找不到匹配的 DRY/WET 群延时数据。

修复原则：

- Group Delay 视图直接读取匹配的 DRY/WET 或渲染资产。
- 不把 `displaySlots` 当作分析数据源。
- 多 DRY 只影响图例和对比对象，不应该让群延时数据消失。

---

## 5. Job Runner 与 `.clz` 要求

`.clz`：

- 保存六个 preview solo 开关。
- 载入工程后恢复 UI 状态。
- 若工程里没有旧字段，默认按当前主预听默认行为处理。

Job Runner：

- 支持传入 preview solo 配置。
- manifest 写明 preview solo 开关状态。
- 如果导出与主预听相关的图或工程，记录这组状态。
- 不要把它写成插件内部 solo。

---

## 6. 验证顺序

1. `git diff -- Source/AudioDoctorComponent.h` 人工审查。
2. `./build.sh standalone`。
3. 打开 Standalone，分别测试单选、双选、全选、全关。
4. 测试师姐按钮预听。
5. 测试进度条预听。
6. 测试插件 insert 内部 preview。
7. 加载两条和三条 DRY，确认 Group Delay 不空白。
8. 保存 `.clz`，重开后确认 solo 状态一致。
9. 跑 Job Runner 最小 smoke，确认 response / manifest 没有崩。

---

## 7. 记录方式

每完成一项，只在本任务卡追加一条短 checkpoint。不要贴大段 build log。

格式：

```text
YYYY-MM-DD HH:mm | action | result | path/commit if any
```

2026-05-26 17:27 | patch/build | Preview Solo 无文字方块、Group Delay WET A/B/C 口径、JobRunner group-delay thesis 数据源补齐；standalone build passed | Source/AudioDoctorComponent.h, Source/AudioDoctorFigureRenderer.h, Source/AudioDoctorJobRunner.h
2026-06-23 18:13 | sync/build | Desktop 本机线从硬盘最新版 d1c269a 同步 Audio Doctor Preview Solo / Group Delay / JobRunner 更新，生成本机提交 e9376e4；修正本机 Xcode/JUCE 路径后 standalone build passed，产物 `/Users/MediaStorm/Library/Caches/GOODMETERBuild/GOODMETER-2499123989/standalone/Products/Release/GOODMETER.app`；iOS 暂停电流声补丁仍保留为未提交改动 | /Users/MediaStorm/Desktop/GOODMETER
