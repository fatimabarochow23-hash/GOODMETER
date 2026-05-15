# 故障房｜BEAM JUCE 消息线程死锁

## 触发问题

Lunacy Audio BEAM 在 REAPER 和 Pro Tools 中可以正常使用，但 Audio Doctor UI 中点击 Render 曾出现卡死或崩溃。

相关需求单：

- `/Users/caiyiyang/Desktop/GOODMETER/AUDIO_DOCTOR_BEAM_RENDER_BUG_2026-05-07.md`

## 判断

问题不应简单归因于“BEAM 不支持离线渲染”。更准确的边界是：

- BEAM 默认状态在 JobRunner 命令行路径中可渲染。
- UI Render 路径曾在插件生命周期、message thread、prepare/release/state 捕获之间形成卡死风险。
- GUI path 能否跑通，需要和 CLI path 分开验证。

历史死锁特征：

- 插件 prepare 阶段可能调用 JUCE message thread。
- 主线程如果被 `MessageManager::Lock` 或同步等待卡住，后台渲染等待主线程就会形成死锁。

## 修复方向

- UI 点击 Render 后，主线程只捕获 description、state、输入素材。
- 后台渲染不应继续依赖 UI 持有的 editable plugin instance。
- 复杂插件的 prepare/release/state 操作需要谨慎放在 message thread 或专门离线实例中。
- processBlock 仍可后台执行，但生命周期操作要避开死锁。

## 预设与 state

BEAM `.preset` / `.nodepreset` 不是稳定的 JUCE native state。论文复现优先使用：

- Audio Doctor UI 捕获的 `pluginStateBase64`
- host native state file
- manifest 中的 state hash 和参数快照

不要只看 `stateLoaded: true`，必须比较输出 CSV 或图像是否真的变化。

状态：有效。

标签：#BEAM #Deadlock #JUCE #MessageThread #PluginState

