# 插件与预设状态索引

| 插件 | 格式 | UI 渲染 | JobRunner 渲染 | State / preset 复现 | 论文用途 | 备注 |
|---|---|---|---|---|---|---|
| kHs Distortion | VST3 / AU | 可用，需按本机验证 | 可用，需按 job 验证 | 参数快照可用 | 饱和、失真、谐波 | 第四章 4.2 候选 |
| kHs Reverb | VST3 / AU | 可用，需按本机验证 | 可用，需按 job 验证 | 参数快照可用 | Reverb / Space、尾音、RT/DRR | 第四章空间案例 |
| Disperser | VST3 | 可用，需按本机验证 | 可用，需按 job 验证 | 参数快照可用 | Group Delay、瞬态展开 | 第四章 all-pass / disperser 案例 |
| FabFilter Pro-C 2 | VST3 / AU | 可用，需按本机验证 | 可用，需按 job 验证 | 参数快照可用 | Dynamics Response / apparent ducking | 不能声称内部 GR，除非读取插件 meter |
| BEAM | VST3 | 曾卡死，已围绕 UI render 路径修复，仍需复杂 preset 回归 | 默认状态可跑 | `.preset` / `.nodepreset` 不能当 native state；优先 `pluginStateBase64` | 空间、粒子、魔法能量尾音 | 见 BEAM 故障房 |
| Enrage | VST3 | 可用性需持续验证 | 需要 state/preset 支撑 | 参数很多，终版只摘要关键参数 + state hash | 空间分路、L/R、M/S、模块链 | 适合第四章空间案例 |
| ShaperBox | VST3 / AU | 需验证 | 需验证 | 需 state | gate、rhythm、dynamic movement | 论文附录候选 |
| Soundtoys | VST3 / AU | 需验证 | 需验证 | 需 state | delay、filter、saturation | 论文附录候选 |

## 记录规则

每新增一个插件实验，追加：

- 插件版本和路径
- UI 是否可打开编辑器
- UI Render 是否完成
- JobRunner 是否完成
- stateHash / pluginStateBase64 是否可复现
- 输出目录和 manifest 路径
- 论文用途

## 2026-06-23｜Desktop 插件版代码健康检查

- 构建环境：`/Applications/Xcode-26.4.app`，旧 `/Applications/Xcode.app` 已不存在；`build.sh` 已改为自动寻找完整 Xcode，避免落到 CommandLineTools 或失效旧路径。
- 构建命令：`./build.sh plugin`
- 构建结果：通过，输出 `GOODMETER.vst3`、`GOODMETER.component`、`juce_vst3_helper`。
- 安装路径：`~/Library/Audio/Plug-Ins/VST3/GOODMETER.vst3`，`~/Library/Audio/Plug-Ins/Components/GOODMETER.component`。
- 签名验证：VST3 与 AU 均通过 `codesign --verify --deep --strict`。
- 代码隔离结论：`GOODMETER_Plugin.jucer` 和 `Builds/MacOSX_Plugin` 未引入 `AudioDoctor*` 或 `Source/iOS/*` 文件，插件工程仍只编译 `PluginProcessor.cpp`、`PluginEditor.cpp` 和插件 UI 相关资源；近期 Audio Doctor / iOS 大改没有污染插件源码列表。
- 补充验证：`auval -v aufx GdMt SLRS` 通过，`AU VALIDATION SUCCEEDED`；本机未安装 `pluginval` / `vst3validator`，但用户本次关注代码健康，宿主打开验证由用户手测即可。
