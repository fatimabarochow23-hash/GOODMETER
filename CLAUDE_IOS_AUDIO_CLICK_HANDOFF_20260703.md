# GOODMETER iOS 暂停电流声问题交接给 Claude

日期：2026-07-03
当前负责人：Codex
目标读者：Claude / Claude Code
状态：Claude 上一轮补丁已构建并安装到真机，但用户实测暂停时仍有明显 click/crackle，描述为“放屁一样的电流”。

---

## 1. 一屏结论

这次不要先怀疑“新版没装上”。已经确认：

- 当前工作树：`/Users/MediaStorm/Desktop/GOODMETER`
- 当前分支：`codex/integrate-audio-doctor-20260515`
- iOS 工程：`/Users/MediaStorm/Desktop/GOODMETER/Builds/iOS/GOODMETER.xcodeproj`
- scheme：`GOODMETER - App`
- bundle id：`com.solaris.GOODMETER`
- 真机 UDID：`00008150-00041D300A88401C`
- 真机名：`AEAEAEEOO`
- 构建产物：`/Users/MediaStorm/Desktop/GOODMETER/Builds/iOS/build/Release/GOODMETER.app`
- 已安装到真机并启动。

用户最新反馈：

> 暂停还是有放屁一样的电流。

因此上一轮“禁用 AVPlayer audio track，而不只是 mute”的假设不能单独解决问题。下一轮应优先检查 `iOSAudioEngine` 的 stop / pause / seek / fade 真实执行顺序，以及 `VideoPageComponent` 在 timer 中是否重复触发 `pauseAndSeek()`。

---

## 2. 不要混错工作区

本次 iOS 真机测试线使用 Desktop 仓库：

```text
/Users/MediaStorm/Desktop/GOODMETER
```

不要用外置盘论文/Audio Doctor 主线：

```text
/Volumes/solari/Codex_Work/GOODMETER
```

两套仓库不保证同分支、同 commit、同工作树状态。

当前 Desktop 工作树是脏的，且有其他长期任务文件。不要执行：

```bash
git reset --hard
git clean
git checkout -- .
git add .
```

尤其不要覆盖这些已有改动：

```text
Source/iOS/VideoPageComponent.mm
Source/AudioDoctorComponent.h
build.sh
AudioDoctorApp/
memory_palace/
```

---

## 3. 关键 iOS 路径

主要入口：

```text
/Users/MediaStorm/Desktop/GOODMETER/Source/iOS/VideoPageComponent.mm
/Users/MediaStorm/Desktop/GOODMETER/Source/iOS/iOSAudioEngine.h
/Users/MediaStorm/Desktop/GOODMETER/Source/iOS/iOSMainApp.cpp
/Users/MediaStorm/Desktop/GOODMETER/Builds/iOS/GOODMETER.xcodeproj/project.pbxproj
```

相关共享音频/表头：

```text
/Users/MediaStorm/Desktop/GOODMETER/Source/PluginProcessor.h
/Users/MediaStorm/Desktop/GOODMETER/Source/PluginProcessor.cpp
/Users/MediaStorm/Desktop/GOODMETER/Source/AudioHistoryBuffer.h
```

构建日志：

```text
/tmp/goodmeter_ios_device_build.log
```

旧交接文档：

```text
/Users/MediaStorm/Desktop/GOODMETER/CODEX_HANDOFF_DA_DIANNAO_20260623.md
/Users/MediaStorm/Desktop/GOODMETER/GOODMETER_iOS_DECLICK_DEBUG_HANDOFF_2026-05-07.md
```

---

## 4. Claude 上一轮补丁内容

文件：

```text
/Users/MediaStorm/Desktop/GOODMETER/Source/iOS/VideoPageComponent.mm
```

核心改动：

- 新增 `GOODMETERItemStatusObserver`，监听 `AVPlayerItem.status` 和 `tracks`。
- 新增 `NativeVideoPlayer::setAudioTracksEnabled(bool)`。
- 当 `syncedAudioLoaded == true` 时：
  - `nativePlayer->setMuted(true)`
  - `nativePlayer->setAudioTracksEnabled(false)`
  - `nativePlayer->setVolume(0.0f)`
- 目的：同步音频由 `iOSAudioEngine` 输出时，让 AVPlayer 只负责画面，彻底禁用 AVPlayer 自己的 audio track，避免 mute 后 audio render node 仍挂在共享输出上。

关键位置：

```text
Source/iOS/VideoPageComponent.mm
GOODMETERItemStatusObserver
NativeVideoPlayer::setAudioTracksEnabled()
NativeVideoPlayer::applyAudioTrackEnabledState()
VideoPageComponent::attachSyncedAudioIfAvailable()
VideoPageComponent::playTransport()
VideoPageComponent::pauseTransport()
VideoPageComponent::refreshVideoPlaybackSurface()
```

这次实测说明：AVPlayer audio track 不是唯一问题，或者 click/crackle 主要来自 JUCE/iOSAudioEngine 路径。

---

## 5. 现在最可疑的代码路径

### 5.1 `VideoPageComponent::pauseTransport()`

文件：

```text
/Users/MediaStorm/Desktop/GOODMETER/Source/iOS/VideoPageComponent.mm
```

当前逻辑：

```cpp
forcedPausePosition = nativePlayer->getPosition();
userRequestedPlayingState = false;
setPlayButtonVisualState(false);

if (syncedAudioLoaded)
    nativePlayer->pauseClockOnly();
else
    nativePlayer->rampDownAndPause(72);

if (syncedAudioLoaded)
    audioEngine.pauseAndSeek(forcedPausePosition);
```

注意：同步音频路径里，native video 只暂停画面时钟，真正声音停靠 `audioEngine.pauseAndSeek()`。用户现在的暂停 click 很可能在这条线。

### 5.2 timer 里可能重复触发 pause

文件：

```text
/Users/MediaStorm/Desktop/GOODMETER/Source/iOS/VideoPageComponent.mm
```

当前 timer 中有这段：

```cpp
else if (syncedAudioLoaded && !userRequestedPlayingState && audioEngine.isPlaying())
{
    audioEngine.pauseAndSeek(juce::jlimit(0.0, duration, forcedPausePosition > 0.0 ? forcedPausePosition : position));
}
```

需要确认：

- 用户点击暂停后，`pauseTransport()` 已经调用 `audioEngine.pauseAndSeek()`。
- timer 是否在 fade 尚未完成时又重复调用 `pauseAndSeek()`。
- `forcedPausePosition`、`nativePlayer->isPlaying()`、`audioEngine.isPlaying()` 三者是否在若干帧内不同步，导致多次 stop/seek/fade 命令叠加。

### 5.3 `iOSAudioEngine::pauseAndSeek()`

文件：

```text
/Users/MediaStorm/Desktop/GOODMETER/Source/iOS/iOSAudioEngine.h
```

当前逻辑摘要：

```cpp
if (!transportSource.isPlaying())
{
    transportSource.stop();
    if (positionSeconds >= 0.0)
        transportSource.setPosition(positionSeconds);
    transportStopPending.store(false);
    silenceOutputEnvelope();
    return;
}

const auto serial = ++transportCommandSerial;
beginDeferredTransportStop(serial, FadeCompletionAction::pause, positionSeconds);
```

重点检查：

- 当 `transportSource.isPlaying()` 已经 false 但音频 callback buffer 仍有尾巴时，立即 `transportSource.stop()` + `setPosition()` + `silenceOutputEnvelope()` 是否会造成突变。
- `silenceOutputEnvelope()` 直接把 envelope 设为 0 并 `forceOutputMute=true`，是否会在某些时序里造成非渐变静音。
- `beginDeferredTransportStop()` 先 `beginOutputFade(0)`，但 completion action 被设为 `none`，真正 stop/seek 由 Timer 延迟做；需要确认 timer 和 audio callback 在 iOS 上是否稳定执行。

### 5.4 end-of-file 自动归零路径

文件：

```text
/Users/MediaStorm/Desktop/GOODMETER/Source/iOS/VideoPageComponent.mm
```

当前 reachedEnd 路径：

```cpp
if (syncedAudioLoaded)
{
    nativePlayer->pauseClockOnly();
    audioEngine.stop();
}
...
Timer 140ms later:
nativePlayer->pauseClockOnly();
nativePlayer->setPosition(0.0, true);
```

用户之前提过暂停、停止、结尾自动归零都有电流声。`audioEngine.stop()` 和 `pauseAndSeek()` 应一起检查，不能只看手动暂停。

---

## 6. 今天的构建与安装记录

Xcode 状态：

```text
xcode-select -p -> /Library/Developer/CommandLineTools
实际可用 Xcode -> /Applications/Xcode-26.4.app
```

设备：

```text
devicectl: AEAEAEEOO / C39165FC-94DB-52AC-90E2-446AF1A771AB / available paired
xcodebuild destination: platform=iOS,id=00008150-00041D300A88401C
```

签名：

```text
Apple Development: Yiyang Cai (ZZFZZ362GM)
Team ID used for provisioning: 33NJKA4738
```

本机一开始没有 Xcode Accounts 登录，`-allowProvisioningUpdates` 失败：

```text
No Accounts: Add a new account in Accounts settings.
No profiles for 'com.solaris.GOODMETER' were found.
```

解决方式：

从旧 Debug app 中恢复了有效 profile：

```text
/Users/MediaStorm/Desktop/GOODMETER/Builds/iOS/build/Debug/GOODMETER.app/embedded.mobileprovision
```

复制到：

```text
/Users/MediaStorm/Library/MobileDevice/Provisioning Profiles/4fc7d5b8-8f45-47b8-8b66-7ad64ae2eab8.mobileprovision
```

该 profile 信息：

```text
Name: iOS Team Provisioning Profile: *
UUID: 4fc7d5b8-8f45-47b8-8b66-7ad64ae2eab8
TeamIdentifier: 33NJKA4738
application-identifier: 33NJKA4738.*
ExpirationDate: 2027-03-27 03:06:39 CST
ProvisionedDevices includes: 00008150-00041D300A88401C
Certificate: Apple Development: Yiyang Cai (ZZFZZ362GM)
```

另一个构建阻碍：

```text
/Users/MediaStorm/Desktop/GOODMETER/Builds/iOS/build/Release/GOODMETER.app
```

原来是旧 archive symlink，导致 Xcode 报：

```text
unable to create directory '.../Builds/iOS/build/Release/GOODMETER.app'
```

处理方式：没有删除，只改名留档：

```text
/Users/MediaStorm/Desktop/GOODMETER/Builds/iOS/build/Release/GOODMETER.app.stale_symlink_20260703_1202
```

成功构建命令：

```bash
cd /Users/MediaStorm/Desktop/GOODMETER

DEVELOPER_DIR=/Applications/Xcode-26.4.app/Contents/Developer \
/Applications/Xcode-26.4.app/Contents/Developer/usr/bin/xcodebuild \
  -project /Users/MediaStorm/Desktop/GOODMETER/Builds/iOS/GOODMETER.xcodeproj \
  -scheme 'GOODMETER - App' \
  -configuration Release \
  -destination 'platform=iOS,id=00008150-00041D300A88401C' \
  -derivedDataPath /Users/MediaStorm/Library/Caches/GOODMETERBuild/ios-device \
  DEVELOPMENT_TEAM=33NJKA4738 \
  CODE_SIGN_STYLE=Automatic \
  CODE_SIGN_IDENTITY='Apple Development' \
  build
```

成功安装命令：

```bash
DEVELOPER_DIR=/Applications/Xcode-26.4.app/Contents/Developer \
/usr/bin/xcrun devicectl device install app \
  --device 00008150-00041D300A88401C \
  /Users/MediaStorm/Desktop/GOODMETER/Builds/iOS/build/Release/GOODMETER.app
```

成功启动命令：

```bash
DEVELOPER_DIR=/Applications/Xcode-26.4.app/Contents/Developer \
/usr/bin/xcrun devicectl device process launch \
  --device 00008150-00041D300A88401C \
  --terminate-existing \
  com.solaris.GOODMETER
```

验证：

```text
codesign --verify --deep --strict --verbose=2 /Users/MediaStorm/Desktop/GOODMETER/Builds/iOS/build/Release/GOODMETER.app
=> valid on disk; satisfies its Designated Requirement
```

安装后设备列表显示：

```text
GOODMETER / com.solaris.GOODMETER / Version 1.0.0 / Bundle Version 4
```

---

## 7. 给 Claude 的下一步建议

建议不要先做大范围 UI 或架构重构。先做一个最小诊断补丁，目标是判断 click 来自 AVPlayer 还是 `iOSAudioEngine`。

推荐顺序：

1. 在 `VideoPageComponent::pauseTransport()` 和 timer 里的 pause 分支加临时日志，确认一次用户点击是否触发多次 `audioEngine.pauseAndSeek()`。
2. 给 `iOSAudioEngine::pauseAndSeek()`、`stop()`、`beginDeferredTransportStop()`、`scheduleDeferredTransportStop()` 加临时日志，记录 serial、isPlaying、pending、position、fade samples、timer 执行时间。
3. 做一个试验补丁：在用户请求暂停后先只 fade `iOSAudioEngine` 到 0，不立刻 `transportSource.stop()` / `setPosition()`；等更长一点的 drain 后再 stop/seek。
4. 另一个试验补丁：timer 中 `!userRequestedPlayingState && audioEngine.isPlaying()` 分支如果 `transportStopPending == true`，不要再次调用 `pauseAndSeek()`。
5. 如果问题仍在，检查 `AudioDeviceManager` / `AudioProcessorPlayer` / `GOODMETERAudioProcessor` 在 force mute 时是否仍有 meter/processor 状态突变产生非零输出。

判断口径：

- 如果禁用 AVPlayer audio track 后仍 click，说明不要继续只围绕 AVPlayer mute/track 做文章。
- 如果 click 只出现在已提取同步音频的视频，优先看 `iOSAudioEngine`。
- 如果普通视频无同步音频时也 click，再回到 `NativeVideoPlayer::rampDownAndPause()` / AVPlayer 路径。

---

## 8. 交接边界

本文件只是交接，不包含新的修复代码。当前用户已经可以真机复现问题。

Claude 接手时请先：

```bash
cd /Users/MediaStorm/Desktop/GOODMETER
git status --short --branch
git diff -- Source/iOS/VideoPageComponent.mm
```

然后再改：

```text
Source/iOS/VideoPageComponent.mm
Source/iOS/iOSAudioEngine.h
```

不要顺手改 Audio Doctor、macOS standalone、插件工程或记忆宫殿，除非用户明确要求。

---

## 9. Claude 第三轮诊断与修复（2026-07-03，本轮）

### 9.1 根因（与前两轮假设不同）

问题不在 fade、不在 AVPlayer（第 4 节的音轨禁用保留，无害且防了次要机制），而在
`iOSAudioEngine` 的停止流程里三个事实的叠加：

1. JUCE `AudioDeviceManager` 在每次硬件回调期间持有 `audioCallbackLock`
   （即 `getAudioCallbackLock()` 那把锁）。消息线程拿锁 = 渲染线程整个卡住。
2. `juce::AudioTransportSource::stop()` 内部自旋等待 `getNextAudioBlock()`
   置位内部 `stopped` 标志来确认，超时 500 × 2ms = **整整 1 秒**。
3. 旧代码里这个确认永远到不了：延迟停止的第二个 Timer **先拿锁再 stop()**
   （回调被锁挡死）；且第一个 Timer 已设 `forceOutputMute=true`，旧回调在
   `getNextAudioBlock()` 之前就 early-return（`stopped` 永远不被置位）。

结果：**每次暂停/停止，消息线程拿着渲染锁自旋满 1 秒，CoreAudio 连续错过
几十次渲染 deadline** —— 这就是"放屁一样的电流"（连续粗糙的 burst，而非
一两下离散 click）。fade 本身一直是好的，所以历史上怎么调 fade 都无效。

### 9.2 修复内容（文件：`Source/iOS/iOSAudioEngine.h`）

- `audioDeviceIOCallbackWithContext()`：**永远**调用
  `transportSource.getNextAudioBlock()`（stop 握手路径永远畅通）；
  `forceOutputMute` 从 early-return 改为**末尾清零输出**。
  已停止的 transport 只 clear 区域、position 不前进，无额外开销。
- `scheduleDeferredTransportStop()`：两级 Timer 合并为一级
  （延迟 = fade + drain）；`transportSource.stop()` 在**锁外**调用
  （~1-2 个 buffer 内确认），随后短暂持锁做 setPosition + 停机静音。
  serial 检查在消息线程上天然串行，锁外检查安全。
- `loadFile()` / `clearFile()` / 析构：`stop()` 全部移到锁外
  （切换视频时旧音频还在播 → 旧代码同样触发 1 秒饿死）。
- `isPlaying()`：`transportStopPending` 期间即报 false，
  30Hz 页面同步逻辑不再在 fade 窗口内与暂停流程互相打架。

### 9.3 验证口径

- synced 视频暂停/停止/自然播完/切换视频：应完全无电流声。
- 无同步 WAV 的普通视频（AVPlayer 直出 + rampDownAndPause）：确认无退化。
- 页 2 纯音频暂停（无视频）：同样走 pauseAndSeek，应同步痊愈。
- 若仍有残留：用文档第 7 节的日志方案，重点记录 stop() 调用前后的
  时间戳差（修复后应 <25ms；若仍 ~1000ms 说明还有别的锁外因素）。
