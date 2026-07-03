# 大电脑施工队交接说明书

日期：2026-06-23 17:43 CST
作者：大电脑施工队 Codex
状态：当前对话建议归档。不要继续加载本长线程做重活。

## 0. 一屏结论

这份交接给下一位 Codex / Claude / Gemini 使用。当前最重要的判断是：

1. Codex app 反复闪退不是 GOODMETER 源码崩溃，而是 Codex 自己的 Node/V8 OOM。
2. 本机存在多条超大 Codex thread 记录，已经对最危险的两条做了归档止血。
3. GOODMETER 当前至少有两套本地仓库：Desktop 近期 iOS 实机测试线，外置盘论文/Audio Doctor 主线。接手前必须确认自己在哪一套。
4. Claude 刚修的 iOS 视频暂停电流声补丁方向合理，但还没有完成真机构建/听感验证。
5. VST3 插件版健康检查刚开始，已有初步结论：插件工程没有被 iOS/Audio Doctor 资源大改直接改坏，但尚未完成一次干净 VST3 build 和 DAW 扫描验证。

## 1. 当前关键路径

### 1.1 Desktop 工作树

近期用户让我测试 iOS 真机、暂停电流声、Claude 修复，以及这次 VST3 健康检查时，实际操作主要在：

```text
/Users/MediaStorm/Desktop/GOODMETER
```

当前分支：

```text
codex/integrate-audio-doctor-20260515
```

最后确认到的 HEAD：

```text
cf638be 大电脑施工队: update Audio Doctor preview assets and memory
```

当前有已修改文件：

```text
Source/iOS/VideoPageComponent.mm
Builds/MacOSX/GOODMETER.xcodeproj/xcuserdata/MediaStorm.xcuserdatad/xcschemes/xcschememanagement.plist
```

`VideoPageComponent.mm` 是 Claude 对 iOS 视频暂停 click/crackle 问题的修复。plist 是 Xcode 用户态噪声，除非明确需要，不要提交。

此外有大量历史 untracked 文件。不要 `git add .`，不要 `git clean`。

### 1.2 外置盘工作树

记忆宫殿认为当前主 GOODMETER 工作区在：

```text
/Volumes/solari/Codex_Work/GOODMETER
```

当前分支最后确认：

```text
codex/thesis-audio-doctor-20260505
d1c269a Update Audio Doctor preview solo and figure exports
```

这套更像论文/Audio Doctor 主线，包含 memory palace、release dmg/pkg、论文图表导出、需求单等。

注意：Desktop 和外置盘不是同一 commit，也不是同一 branch。下一位不要混着改。

## 2. Codex 闪退诊断

已查崩溃报告：

```text
/Users/MediaStorm/Library/Logs/DiagnosticReports/Codex-2026-06-22-234449.ips
/Users/MediaStorm/Library/Logs/DiagnosticReports/Codex-2026-06-23-011854.ips
/Users/MediaStorm/Library/Logs/DiagnosticReports/Codex-2026-06-23-035849.ips
/Users/MediaStorm/Library/Logs/DiagnosticReports/Codex-2026-06-23-042247.ips
/Users/MediaStorm/Library/Logs/DiagnosticReports/Codex-2026-06-23-051154.ips
```

共同特征：

```text
EXC_CRASH / SIGABRT
Abort trap: 6
triggered thread name: git
stack: abort -> node::OOMErrorHandler(char const*, v8::OOMDetails const&)
```

判断：这是 Codex app 内部 Node/V8 OOM，可能和超大 thread、超长标题/preview、git/thread 索引搬运大文本有关。不是 GOODMETER 源码导致 macOS app 崩溃。

已做止血：

1. 归档线程 `019d29e0-ed04-7d93-a29e-4cceff1f5b42`，`tokens_used=874643161`。
2. 归档线程 `019e34a3-1034-75d0-8865-dae892200131`，`tokens_used=369474159`。
3. 从 `/Users/MediaStorm/.codex/session_index.jsonl` 移除了上述两条轻量索引。
4. 把四条旧线程的超长 `title/preview` 缩短，原始 jsonl 未删除。

备份文件：

```text
/Users/MediaStorm/.codex/state_5.sqlite.backup_before_archive_20260623
/Users/MediaStorm/.codex/session_index.jsonl.backup_before_archive_20260623
/Users/MediaStorm/.codex/state_5.sqlite.backup_before_archive_acoustics_20260623
/Users/MediaStorm/.codex/session_index.jsonl.backup_before_archive_acoustics_20260623
/Users/MediaStorm/.codex/state_5.sqlite.backup_before_title_trim_20260623
```

仍然最大未归档线程是当前 GOODMETER 长线：

```text
019dfaf2-c985-7601-a4d9-0464fc82f67f
约 226M tokens
```

建议：当前线程归档，开新线程接力。不要再打开旧超大线程或全量读取 `.codex/sessions/*.jsonl`。

## 3. iOS 暂停电流声修复状态

用户之前反馈：iOS 真机视频/音频暂停、停止、结尾自动归零会有不自然 click/crackle 电流声。

Claude 最新补丁位于：

```text
/Users/MediaStorm/Desktop/GOODMETER/Source/iOS/VideoPageComponent.mm
```

核心思路：

1. 当视频已提取同步音频并由 `iOSAudioEngine` 出声时，AVPlayer 只负责画面。
2. 以前只是 `player.muted = YES`，AVPlayer 的 audio render node 可能仍挂在系统输出。
3. 新增 `setAudioTracksEnabled(false)`，直接禁用 `AVPlayerItemTrack` 里的音频 track。
4. 新增 `GOODMETERItemStatusObserver`，等 `AVPlayerItem.status` ready 后重新应用音轨开关，因为 `playerItem.tracks` ready 前可能未填充。

我已做的检查：

```text
git diff --check -- Source/iOS/VideoPageComponent.mm
```

结果：无输出，说明补丁空白/patch 层面干净。

静态阅读结论：

1. 同步音频成功路径会禁用 AVPlayer 音轨。
2. 无同步音频路径会重新启用 AVPlayer 音轨，普通视频理论上仍能出声。
3. `pauseTransport()` 同步音频时走 `nativePlayer->pauseClockOnly()`，实际声音停靠 `audioEngine.pauseAndSeek()`。
4. 如果真机仍有 click，下一责任路径是 `Source/iOS/iOSAudioEngine.h` 的 `pauseAndSeek()` / `stop()` / output envelope fade。

未完成：

1. 尚未成功跑一次 iOS build。
2. 尚未装到用户手机听感验证。

环境阻碍：

```text
xcode-select -p -> /Library/Developer/CommandLineTools
机器上只有 /Applications/Xcode-26.4.app
build.sh 仍硬编码 /Applications/Xcode.app/Contents/Developer
```

所以 `./build.sh ios-sim` 失败是环境路径问题，不是源码编译失败。

下一步建议：

1. 不要先改 iOS 代码。
2. 先用 `/Applications/Xcode-26.4.app` 的 `DEVELOPER_DIR` 手动跑一次 simulator 或 device build。
3. 如果要长期修，考虑让 `build.sh` 支持 `DEVELOPER_DIR` 环境变量或自动探测 Xcode-26.4，但改脚本前先告诉用户。

## 4. VST3 插件版健康检查进度

用户刚问：macOS 版、iOS 版这么多大更新之后，VST3 版本 GOODMETER 是否健康。

检查对象优先以 Desktop 近期工作树为准：

```text
/Users/MediaStorm/Desktop/GOODMETER
```

已确认：

1. Desktop 和外置盘的 `build.sh` hash 相同。
2. Desktop 和外置盘的 `GOODMETER_Plugin.jucer` 主要差异是换行格式，实质内容未见功能差异。
3. `GOODMETER_Plugin.jucer` 是插件专用工程，构建 `buildVST3,buildAU`，不构建 standalone。
4. 插件工程 source 列表只包含核心 meter、processor/editor、guoba 资源等，没有把 Audio Doctor 大模块直接列入插件工程。
5. 插件 Xcode project 包含 `GOODMETER - VST3`、`GOODMETER - AU`、`GOODMETER - All`、`VST3 Manifest Helper`。
6. 插件 post-build 会复制到：

```text
~/Library/Audio/Plug-Ins/VST3/GOODMETER.vst3
```

重要初步判断：

```text
VST3 工程配置看起来还在，且没有被 iOS/Audio Doctor 资源功能直接污染。
但尚未完成实际 build、codesign、DAW/pluginval 扫描，所以不能说完全健康。
```

未完成的验证步骤：

1. 用正确 Xcode 路径跑插件构建。
2. 检查产物 `GOODMETER.vst3` 是否存在。
3. `codesign --verify --deep --strict`。
4. 如果本机有 `pluginval`，跑 pluginval；没有则至少用 DAW 扫描。
5. 打开一个宿主验证 UI、输入电平、八个卡片表、LEVELS 下拉不重叠、无 Audio Doctor 相关控件泄漏。

推荐手动 build 命令：

```bash
cd /Users/MediaStorm/Desktop/GOODMETER
DEVELOPER_DIR=/Applications/Xcode-26.4.app/Contents/Developer \
/Applications/Xcode-26.4.app/Contents/Developer/usr/bin/xcodebuild \
  -project Builds/MacOSX_Plugin/GOODMETER.xcodeproj \
  -scheme "GOODMETER - All" \
  -configuration Release \
  build
```

如果要用 `./build.sh plugin`，要先处理它硬编码 `/Applications/Xcode.app` 的问题。

## 5. 共享代码风险点

VST3 插件虽没有 Audio Doctor UI，但会受这些共享文件影响：

```text
Source/PluginProcessor.cpp
Source/PluginProcessor.h
Source/PluginEditor.cpp
Source/PluginEditor.h
Source/MeterCardComponent.h
Source/LevelsMeterComponent.h
Source/SpectrogramComponent.h
Source/AudioHistoryBuffer.h
Source/GoodMeterLookAndFeel.h
Source/HoloNonoComponent.h
```

记忆里有明确经验：

```text
MeterCardComponent 是 standalone / plugin / iOS 共享层。
LEVELS 标签、标准下拉菜单、header clipping 这类问题不能只看单端。
```

所以 VST3 健康检查不能只看能不能编译，还要看 UI 是否没有被 shared card layout 改坏。

## 6. 不要动的东西

不要执行：

```text
git reset --hard
git clean
git checkout -- .
git add .
```

不要提交：

```text
Builds/MacOSX/*.xcuserdata
._*
dist/
releases/
tmp/
Signing/
Word 临时文件
巨大 dmg/pkg/zip
```

不要全量读取：

```text
/Users/MediaStorm/.codex/sessions/*.jsonl
/Users/MediaStorm/.codex/session_quarantine_20260623_codex_freeze/*.jsonl
```

这些会让 Codex 再次 OOM。

## 7. 下一位施工队建议顺序

### 第一步：新线程瘦身启动

先读本文件，再读：

```text
/Volumes/solari/Codex_Work/GOODMETER/memory_palace/00_index.md
/Volumes/solari/Codex_Work/GOODMETER/memory_palace/00_hot_start_current.md
/Volumes/solari/Codex_Work/GOODMETER/memory_palace/asset_index/plugin_state_index.md
/Volumes/solari/Codex_Work/GOODMETER/memory_palace/feature_rooms/04_PluginHost_and_Rendering.md
```

不要读旧 rollout jsonl。

### 第二步：确认仓库

运行：

```bash
git -C /Users/MediaStorm/Desktop/GOODMETER status --short --branch
git -C /Volumes/solari/Codex_Work/GOODMETER status --short --branch
```

然后问清楚本轮目标用 Desktop 还是外置盘。若是 iOS 真机与 Claude 暂停修复，优先 Desktop。若是论文 Audio Doctor 主线和 release，优先外置盘。

### 第三步：收尾 VST3 健康检查

在 Desktop 工作树先跑：

```bash
cd /Users/MediaStorm/Desktop/GOODMETER
DEVELOPER_DIR=/Applications/Xcode-26.4.app/Contents/Developer \
/Applications/Xcode-26.4.app/Contents/Developer/usr/bin/xcodebuild \
  -project Builds/MacOSX_Plugin/GOODMETER.xcodeproj \
  -scheme "GOODMETER - All" \
  -configuration Release \
  build
```

然后验证：

```bash
ls -la "$HOME/Library/Audio/Plug-Ins/VST3/GOODMETER.vst3"
codesign --verify --deep --strict --verbose=2 "$HOME/Library/Audio/Plug-Ins/VST3/GOODMETER.vst3"
```

有 pluginval 就跑 pluginval；没有就让用户指定 Pro Tools / Reaper / Logic 其中一个宿主实测。

### 第四步：再处理 iOS

如果用户继续问 iOS click：

1. 先构建安装 Claude 当前补丁。
2. 测视频暂停、stop、播放到尾自动归零、重新播放从头开始。
3. 测无提取音频的视频是否还能由 AVPlayer 原生出声。
4. 如果仍 click，再看 `Source/iOS/iOSAudioEngine.h`。

## 8. 给用户的简短口径

可以这样汇报：

```text
大电脑施工队已经确认 Codex 闪退主因是超大线程导致的 Node/V8 OOM，不是 GOODMETER。已归档两个亿级线程并压缩超长标题。GOODMETER 当前有 Desktop/iOS 实机线和外置盘论文主线两套仓库，接手前必须先确认路径。VST3 工程配置目前看没有被 iOS/Audio Doctor 大改直接破坏，但还缺一次 Xcode-26.4 下的实际插件 build、codesign 和宿主扫描验证。Claude 的 iOS 暂停修复静态看方向正确，仍需真机听感验证。
```

## 9. 当前停止点

本对话应该归档。下一位从本文件继续，不要尝试复活或继续加载这条大线程。
