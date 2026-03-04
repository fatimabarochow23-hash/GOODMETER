# GOODMETER 避险指南 — Bug 复盘与经验总结

> 本文档记录开发过程中踩过的坑、犯过的错、以及对应的修复方案。
> 目的：避免同类问题再次发生，供后续开发参考。

---

## 1. JUCE 父子组件渲染层级陷阱

### 问题
`MeterCardComponent`（父）在 `paint()` 中绘制了标题栏下方的分界线，但多个子组件（`Band3Component`、`VUMeterComponent`、`SpectrumAnalyzerComponent`、`SpectrogramComponent`）在各自的 `paint()` 中调用了 `g.fillAll(juce::Colours::white)`，导致分界线被白色背景完全覆盖。

JUCE 渲染顺序：**父组件 paint() 先执行，子组件 paint() 后执行**。子组件的不透明填充会覆盖父组件画的任何内容。

### 表现
- 分界线"随机消失"（实际是被子组件白色背景盖住）
- 左右两端偶尔有残留（子组件 bounds 有 padding 时露出的边缘）

### 修复
删除所有子组件中的 `g.fillAll()` 调用，让子组件保持透明，背景绘制权交给父组件统一管理。

### 规则
```
子组件（contentComponent）禁止调用 g.fillAll()。
唯一例外：离屏 Image 的初始化填充（如 StereoImageComponent 的 goniometerImage）。
```

---

## 2. 画布切割 vs 悬浮叠加 (Spectrogram 刻度尺)

### 问题
Spectrogram 瀑布图最初用 `removeFromRight(38)` 切出右侧 38px 作为刻度区域，导致：
- 瀑布图无法铺满全宽，右侧出现空白断层
- 刻度尺与瀑布图之间有硬边界，视觉割裂

### 修复
改用 Overlay（悬浮叠加）方案：
1. 瀑布图使用完整 bounds 绘制
2. 离屏 Image 宽度 = 组件全宽（不再减 38）
3. 刻度尺在 `paint()` 最后绘制，用 `black.withAlpha(0.85f)` 直接压在瀑布图之上

### 规则
```
需要标尺/刻度的图表类组件，优先使用 Overlay 悬浮绘制，
不要用 removeFromLeft/Right 切割画布。
```

---

## 3. 刻度文字出界被裁切 (Spectrogram 20k 标签)

### 问题
Spectrogram 顶部 "20k" 文字因为紧贴组件边缘，上半截被裁切。

### 错误修复（第一次）
加了 `if (textDrawY < plotBounds.getY() + 1.0f)` 强制下推文字 → 导致文字和刻度线错位。

### 正确修复（第二次）
不要在文字侧打补丁，而是**整体下移绘图区域**：
```cpp
auto plotBounds = bounds.withTrimmedTop(15).withTrimmedBottom(0);
```
顶部留 15px 自然容纳 20k 文字，底部 0px 极致压榨。所有刻度线和文字的相对位置保持一致。

### 规则
```
文字出界问题，优先通过调整 plotBounds 的 margin 解决，
不要在绘制循环里对个别元素做位置 hack。
```

---

## 4. 修改错目标 (LEVELS 峰值游标 vs 目标浮标)

### 问题
用户要求修改 LEVELS 卡片中"固定在 -23 LUFS 的目标参考虚线"为荧光橙色。但第一次误将效果应用到了"跟随电平移动的峰值游标 (peak hold marker)"上。

### 根因
用户描述为"绿色游标"，有两个匹配对象：
- **峰值游标**：跟随当前电平移动的竖线（原色为黑色 `border`）
- **目标浮标**：固定在 -23 LUFS 位置的虚线（原色为 `accentCyan`）

### 修复
1. 峰值游标恢复为 `GoodMeterLookAndFeel::border`（黑色）
2. 目标浮标改为 `#FF6600` 荧光橙 + 过载脉冲发光
3. **水平模式 `drawPeakBar` 和垂直模式 `drawVerticalBar` 两套渲染路径都要同步修改**

### 规则
```
LEVELS 组件有两套独立的渲染路径（水平/垂直），修改任何视觉元素时必须两套同步。
修改前先确认目标元素的行为特征（是跟随电平移动还是固定位置）。
```

---

## 5. Viewport 滚动跳顶

### 问题
在单列/双列布局下，点击展开或折叠面板时，Viewport 滚动条自动跳回 (0, 0) 顶部。

### 根因
`resized()` 中重新计算了 `contentComponent` 的 bounds 和大小，Viewport 内部自动重置了滚动位置。

### 修复
在 `resized()` 开头缓存滚动位置，结尾恢复：
```cpp
auto prevScrollPos = viewport->getViewPosition();   // 开头
// ... 所有布局计算 ...
viewport->setViewPosition(prevScrollPos);            // 结尾
```

### 规则
```
任何会触发 contentComponent->setBounds() 的操作，
都必须在前后包裹 getViewPosition / setViewPosition 防跳顶。
```

---

## 6. 折叠卡片高度不一致 (1-2px 偏差)

### 问题
Mini Mode 下全部折叠的卡片高度出现 1-2 像素的不一致，导致视觉上参差不齐。

### 根因
布局代码中使用 `card->getHeight()` 获取折叠高度，但不同卡片因浮点取整差异返回了略有不同的值。

### 修复
用绝对常量替代动态查询：
```cpp
const int strictCollapsedHeight = isMiniMode ? (24 + 8) : (48 + 8);
// 24/48 = headerHeight, 8 = maxShadowOffset
```
布局计算中统一使用此常量，不再依赖 `card->getHeight()`。

### 规则
```
折叠高度必须用常量定义（headerHeight + maxShadowOffset），
禁止在布局计算中动态查询 card->getHeight()。
```

---

## 7. 锥形瓶液体渲染崩溃 (3-BAND Flask)

### 问题
Erlenmeyer Flask（三角锥瓶）的液体用手动"相似三角形"计算宽度，但没有正确处理 quadraticTo 曲线的肩部弧线，导致液体显示为悬浮的方形色块。

### 修复
删除手写的 `drawFlaskWithLiquid`，改用通用的 `drawVesselWithLiquid`：
```cpp
g.reduceClipRegion(vesselPath);  // 用容器路径裁剪
g.fillRect(fullWidthRect);       // 满宽填充，自动裁剪到容器形状
```

### 规则
```
不规则容器的液体填充，永远使用 g.reduceClipRegion(path) + 满宽 fillRect。
禁止手动计算液面宽度 — 曲线路径的宽度计算极易出错。
```

---

## 8. Jiggle 拖拽坐标系错误

### 问题
拖拽面板交换位置时，`slotTargetBounds` 数组的下标与 `panelOrder` 不匹配，导致拖到错误位置。

### 修复
用身份反查替代直接下标映射：
```cpp
for (int i = 0; i < activePanels; ++i)
{
    if (allCards[i] == card)
    {
        slotTargetBounds[i] = cardBounds;
        break;
    }
}
```

`mouseDrag` 中坐标转换改用 `event.getEventRelativeTo(contentComponent.get())`。

### 规则
```
拖拽系统中的坐标必须统一到 contentComponent 坐标系。
slotTargetBounds 必须通过身份反查写入，不能假设遍历顺序等于 panelOrder 序列。
```

---

## 速查清单

| 类别 | 规则 |
|------|------|
| 子组件背景 | 禁止 `g.fillAll()`，保持透明 |
| 图表刻度 | 用 Overlay 悬浮绘制，不切割画布 |
| 文字出界 | 调整 plotBounds margin，不 hack 个别元素 |
| 双渲染路径 | 水平/垂直模式必须同步修改 |
| Viewport 滚动 | resized() 前后包裹 getViewPosition/setViewPosition |
| 折叠高度 | 用常量，不用 card->getHeight() |
| 液体填充 | reduceClipRegion + fillRect，不手算宽度 |
| 拖拽坐标 | 统一到 contentComponent 坐标系，身份反查写入 |
| **macOS 拖拽冻结** | **见下方 #9 完整复盘（6 轮取样修复）** |

---

## 9. macOS 宿主窗口拖拽时 UI 全局冻结（完整复盘）

> **严重程度**：P0（用户可感知 ~1 秒冻结）
> **平台**：macOS 15.3 / M4 Max / REAPER 7.x / JUCE 8.0.10
> **修复跨度**：6 轮 `sample` 取样 + 5 轮代码修改
> **最终结果**：GOODMETER paint 占比从 65.6% 降至 1.8%，drawText 从 312 采样降至 0

---

### 9.0 问题本质

macOS 窗口拖拽时，AppKit 将 RunLoop 切换到 `NSEventTrackingRunLoopMode`。此模式下 `CATransaction::commit()` 仍会被 `CFRunLoopObserver` 触发，串行执行所有脏 layer 的 `display`（即 paint）。任何在 `paint()` 路径中的阻塞调用都会直接冻结窗口拖拽动画。

**关键因果链**：
```
用户拖拽窗口
  → RunLoop 进入 Tracking 模式
    → CATransaction::commit() 串行触发
      → 所有脏 CALayer 执行 display
        → JUCE NSView drawRect → handlePaint → paint()
          → 任何阻塞操作 = 全局冻结
```

---

### 9.1 第一轮：MML BlockingMessage（65.6%）

**取样报告 #1 发现**：
```
MessageManagerLock::BlockingMessage::messageCallback  65.6%
```

**根因**：StereoImageComponent 的 Goniometer 使用了 OpenGL 渲染。JUCE 的 OpenGL 在后台线程绘制时需要 `MessageManagerLock` 与主线程同步。拖拽期间主线程被 RunLoop Tracking 模式占用，`BlockingMessage` 无法投递，后台线程阻塞等待 → 主线程的 `CATransaction::commit` 又在等后台线程完成 → **死锁式互等**。

**修复**：移除 OpenGL 渲染，改用 JUCE 默认软件渲染。

**教训**：
```
JUCE 8 插件禁止使用 OpenGL。
OpenGL 的后台线程渲染与 macOS RunLoop Tracking 模式存在结构性死锁。
即使 OpenGL 本身更快，在窗口拖拽场景下会产生灾难性冻结。
```

---

### 9.2 第二轮：StereoImage CoreGraphics 风暴（79.2%）

**取样报告 #2 发现**：
```
StereoImageComponent::timerCallback → CoreGraphics 绘制  79.2%
```

**根因**：移除 OpenGL 后，Goniometer 的 Lissajous 轨迹用 `juce::Graphics::drawLine()` 逐段绘制。每段线调用一次 CoreGraphics `CGContextStrokePath`。512 个采样点 = 512 次 CG 调用，全部在 `paint()` 路径内（即 `CATransaction::commit` 路径内）。

**错误尝试**：Gemini 建议恢复 OpenGL → 导致 MML 回归（见 #3）

**教训**：
```
CoreGraphics 的逐段 drawLine 在高频绘制场景下开销巨大。
512 个点的 Lissajous 轨迹不能用 CG 逐段画。
```

---

### 9.3 第三轮：OpenGL 回退导致 MML 回归（92.9%）

**取样报告 #3 发现**：
```
MessageManagerLock  92.9%（比第一轮更严重）
```

**根因**：听从 Gemini 建议恢复了 OpenGL，MML 问题全面回归。

**修复**：永久移除 OpenGL，改用 **BitmapData + Bresenham** 方案：
```cpp
// 在 timerCallback 中（非 paint 路径）：
juce::Image goniometerImage(ARGB, w, h, true, SoftwareImageType());
juce::Image::BitmapData bmp(goniometerImage, readWrite);

// Bresenham 整数线段光栅化 — 零 CoreGraphics
bresenhamLine(bmp, x0, y0, x1, y1, w, h, r, g, b, alpha);

// paint() 中只做一次 drawImageAt：
g.drawImageAt(goniometerImage, x, y);
```

**教训**：
```
高频逐像素绘制必须用 BitmapData 直接写像素。
SoftwareImageType() 确保离屏 Image 不走 Metal/NativeImage 路径。
paint() 中只做 drawImageAt（纯内存拷贝），绝不做逐点绘制。
```

---

### 9.4 第四轮：Timer 风暴 + 全局降频（69.4% → 54.3%）

**取样报告 #4 发现**：
```
CA::Transaction::commit 69.4%
handlePaint 内部被 9 个组件的 60Hz repaint 轮番触发
```

**根因**：9 个组件各自 `startTimerHz(60)` + `repaint()`，每帧 CATransaction 需要 paint 全部脏 layer。

**修复**：
1. 所有定时器从 60Hz 降到 30Hz
2. Goniometer 采样步长从 step-2 改为 step-4（维持视觉密度）
3. 添加 `juce::Font` 缓存成员避免每帧创建

**效果**：CA::Transaction 从 69.4% 降到 54.3%

**教训**：
```
JUCE 插件的定时器频率直接决定 CATransaction::commit 的工作量。
9 个组件 × 60Hz = 每秒 540 次 repaint → CA 串行 paint 风暴。
降到 30Hz = 每秒 270 次，直接减半。
```

---

### 9.5 第五轮：HarfBuzz 字体创建（83% of paint）

**取样报告 #5 发现**：
```
handlePaint: 387 采样
  其中 drawText → HarfBuzz → CoreText: 312 采样（83%）
    _hb_coretext_shaper_font_data_create: 200
    create_ct_font / CTFontCreateCopyWithAttributes: 233
```

**根因**：JUCE 8 的 `drawText()` 内部链路：
```
drawText()
  → GlyphArrangement::addCurtailedLineOfText()
    → ShapedText::ShapedText()
      → SimpleShapedText::shape()
        → Shaper::getChunksUpToNextSafeBreak()
          → _hb_coretext_shaper_font_data_create()
            → create_ct_font()
              → CTFontCreateCopyWithAttributes()  ← 每次创建新 CTFont！
```

即使使用 `juce::Font` 缓存成员，JUCE 8 的 ShapedText 层每次调用仍会重新创建 CTFont。这是 JUCE 8 引入 HarfBuzz shaper 后的结构性开销。

**各组件 HarfBuzz 采样分布**：
| 组件 | HarfBuzz 采样 |
|------|:---:|
| MeterCard (标题+箭头) | 64 |
| LevelsMeter (LUFS 数值+刻度) | 37 |
| Spectrogram (频率刻度) | 21 |
| VUMeter (刻度+VU 标签) | 18 |
| PsrMeter (网格+读数) | 17 |
| StereoImage (LRMS+相位仪+dB) | 16 |
| SpectrumAnalyzer (频率网格) | 12 |
| Band3 (LOW/MID/HIGH) | 8 |
| PhaseCorrelation (侧标签+数值) | 7 |

**修复 — 离屏文本贴图方案**：

核心思想：**paint() 中永远不调用 drawText()**。

分两类缓存：
- **静态文本**（标签、刻度、标题）：resize 时渲染一次到 `juce::Image`，之后只 blit
- **动态文本**（数值读数）：在 `timerCallback` / `updateMetrics` 中预渲染到 `juce::Image`，paint() 只 blit

```cpp
// === 静态缓存示例（MeterCard 标题）===
// paint() 中：
bool needsRebuild = headerTextCache.isNull() ||
                    lastHeaderCacheW != cacheW ||
                    lastHeaderCacheMini != isMiniMode ||
                    lastHeaderCacheExpanded != isExpanded;
if (needsRebuild)
{
    headerTextCache = juce::Image(ARGB, cacheW, hh, true, SoftwareImageType());
    juce::Graphics tg(headerTextCache);
    tg.setFont(...);
    tg.drawText(cardTitle, ...);  // 只在状态变化时执行
}
g.drawImageAt(headerTextCache, x, y);  // 每帧只做这一行

// === 动态缓存示例（LUFS 数值）===
// updateMetrics() 中（非 paint 路径）：
void prerenderLUFSText(int w, int h)
{
    lufsTextCache = juce::Image(ARGB, w, h, true, SoftwareImageType());
    lufsTextCache.clear(lufsTextCache.getBounds());
    juce::Graphics tg(lufsTextCache);
    renderLUFSText(tg, w, h);  // drawText 在这里执行
}
// paint() 中：
g.drawImageAt(lufsTextCache, bounds.getX(), bounds.getY());
```

**与 Gemini 方案的关键区别**：Gemini 方案将 drawText 放在 `paint()` 内条件执行 → 首帧仍会阻塞 CATransaction。我的方案将**所有** drawText 移出 paint()，确保 paint() 永远只做 `drawImageAt`。

**效果**：handlePaint 从 387 降到 43（↓90%），drawText 从 312 降到 0（↓100%）

**教训**：
```
JUCE 8 的 drawText() 每次调用都会触发 HarfBuzz → CoreText 字体重建。
juce::Font 缓存对此无效 — 瓶颈在 ShapedText 层。
唯一解法：将 drawText 移到离屏 Image，paint() 只做 drawImageAt。
静态文本 = resize 时缓存；动态文本 = timerCallback 中预渲染。
必须使用 SoftwareImageType() 创建 Image，避免 NativeImage 走 Metal。
```

---

### 9.6 第六轮：REAPER HeartCore Metal 等待（31.9%）

**取样报告 #6 发现**：
```
GOODMETER handlePaint: 43 采样（1.8%）— 已触底
drawText in paint: 0（完全清零）
HarfBuzz/create_ct_font: 0（完全清零）

新瓶颈：
[CAMetalLayer nextDrawable] → semaphore_timedwait: 776 采样（31.9%）
  ← 这是 REAPER 自身的 HeartCore Metal 渲染管线
```

**根因**：REAPER 使用私有 HeartCore 框架通过 Metal 渲染自身 UI。窗口拖拽时 GPU 背压导致 `nextDrawable` 信号量等待。这是**宿主级别问题**，插件无法控制。

**最终优化**：
1. 恢复 60Hz 满血（常规状态极致流畅）
2. 智能半频点刹（鼠标按下时隔帧跳过 → ~30Hz）
3. `setOpaque(true)` 减少 CA 合成开销

```cpp
// 智能半频节流（所有 timerCallback 入口）：
void timerCallback() override
{
    if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
    {
        static int dragThrottleCounter = 0;
        if (++dragThrottleCounter % 2 != 0) return;
    }
    // ... 正常逻辑 ...
}
```

**教训**：
```
当插件 paint 已优化到 <2% 时，剩余冻结可能是宿主自身的渲染管线。
此时应转向"减少对宿主的间接压力"而非继续优化自身：
- 60Hz 满血 + 鼠标按下时降半频 = 兼顾流畅与抗冻
- setOpaque(true) = 减少 CA 合成开销
- 局部 repaint(rect) = 减少脏区上传（边际收益，可选）
```

---

### 9.7 完整修复进度表

| 轮次 | 取样 | 主要瓶颈 | 修复措施 | 效果 |
|:---:|:---:|------|------|------|
| 1 | #1 | MML BlockingMessage 65.6% | 移除 OpenGL | MML → 0 |
| 2 | #2 | CG drawLine 风暴 79.2% | — (暂未修复) | — |
| 3 | #3 | OpenGL 回退 → MML 92.9% | 永久移除 OGL + Bresenham | MML=0, CG=0 |
| 4 | #4 | Timer 风暴 69.4% | 全局 30Hz + 字体缓存 | 降至 54.3% |
| 5 | #5 | HarfBuzz 字体 83% of paint | 离屏文本贴图 ×9 组件 | paint 387→43 |
| 6 | #6 | REAPER Metal 31.9% (宿主) | 60Hz + 半频点刹 + setOpaque | 插件仅 1.8% |

---

### 9.8 下一个插件的黄金规则

```
=== macOS JUCE 插件 paint() 路径铁律 ===

1. 禁止 OpenGL
   JUCE OpenGL 后台线程的 MessageManagerLock 与 Tracking RunLoop 死锁。

2. 禁止在 paint() 中调用 drawText / drawFittedText
   JUCE 8 HarfBuzz 每次创建 CTFont，不可缓存。
   所有文本必须预渲染到 SoftwareImageType 离屏 Image。

3. 禁止在 paint() 中逐段 drawLine 画密集轨迹
   用 BitmapData + Bresenham 在 timerCallback 中光栅化。

4. 离屏 Image 必须用 SoftwareImageType()
   NativeImage 可能走 Metal，从后台线程触发 MML。

5. 定时器策略：60Hz 满血 + 鼠标按下半频
   if (ModifierKeys::currentModifiers.isAnyMouseButtonDown())
       skip every other frame;

6. 主编辑器 setOpaque(true)
   告知 CA 跳过背后的 alpha 混合。

7. 静态文本 resize 缓存，动态文本 timerCallback 预渲染
   paint() 只做 drawImageAt — 纯内存拷贝，零字体创建。
```

---

### 9.9 诊断方法论

**工具**：`/usr/bin/sample <PID> 2000 1` — 2000 次采样，1ms 间隔

**分析流程**：
1. 找到主线程（`Thread_xxx: reaper`）
2. 统计 `CA::Transaction::commit` 下的采样分布
3. 区分三类开销：
   - **idle**（`mach_msg_trap`）— 正常空闲
   - **paint 路径**（`handlePaint` → 各组件 `paint()`）— 可优化
   - **宿主渲染**（HeartCore / CAMetalLayer）— 不可控
4. paint 路径内进一步细分：drawText / CoreGraphics / drawImageAt
5. 逐组件计数，从高到低修复

**关键指标**：
```
handlePaint < 50 采样 = 健康（2%）
drawText in paint = 0 = 达标
MML = 0 = 必须
```
