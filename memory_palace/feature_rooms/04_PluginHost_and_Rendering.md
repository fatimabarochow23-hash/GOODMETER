# 功能房 04｜插件宿主与渲染

## 当前定位

Audio Doctor 的插件宿主负责加载 AU/VST3、打开插件编辑器、捕获参数与 state、离线渲染 Wet A/B/C，并把渲染结果交给分析与导出链路。

关键源码：

- `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorPluginHost.h`
- `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorComponent.h`
- `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorJobRunner.h`

## 复现原则

论文证据需要能复跑。插件状态优先级：

1. `pluginStateBase64` 或 native state 文件。
2. 真实插件参数快照和 changed parameters。
3. Job JSON 的 normalized param overrides。
4. 厂商 preset 文件仅作为辅助，不能假设一定可直接 `setStateInformation()`。

## 参数导出要求

manifest 中参数应尽量人可读：

```json
{
  "index": 0,
  "id": "param_00",
  "name": "Size",
  "label": "%",
  "normalizedValue": 0.68,
  "displayValue": "68%",
  "nameUnavailable": false
}
```

如果插件不给可读名，fallback 到 `param_00`，并写 `nameUnavailable: true`。

## 高危点

- UI Render 和 JobRunner Render 可能走不同生命周期。
- 复杂插件可能在 prepare/release/state 阶段访问 message thread。
- 关闭编辑器、抓 state、suspend plugin、后台渲染之间不能互相踩。
- 插件私有 preset 未必等于 native state。

状态：有效。

标签：#PluginHost #Render #State #ParameterSnapshot

