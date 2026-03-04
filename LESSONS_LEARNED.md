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
