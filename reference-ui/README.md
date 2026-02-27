# GOODMETER UI Reference Files

此目录包含 Gemini 设计的 React/TypeScript UI 原型代码，作为 JUCE C++ 实现的参考。

## 需要的文件列表

请将以下 Gemini 生成的文件复制到此目录：

### 核心文件
- [ ] `src/App.tsx` - 主应用组件
- [ ] `src/main.tsx` - 应用入口
- [ ] `index.css` - 全局样式
- [ ] `package.json` - 依赖配置

### 计量模块组件
- [ ] `src/components/meters/Levels.tsx` - LUFS/Peak 电平表
- [ ] `src/components/meters/ClassicVUMeter.tsx` - 经典 VU 表
- [ ] `src/components/meters/ThreeBandMeter.tsx` - 三频段表
- [ ] `src/components/meters/SpectrumAnalyzer.tsx` - 频谱分析器
- [ ] `src/components/meters/PhaseCorrelation.tsx` - 相位相关性
- [ ] `src/components/meters/StereoImage.tsx` - 立体声成像
- [ ] `src/components/meters/StereoFieldAnalyzer.tsx` - 立体声声场分析
- [ ] `src/components/meters/Spectrogram.tsx` - 频谱图

### 通用组件
- [ ] `src/components/MeterCard.tsx` - 表卡片容器（如果有）
- [ ] `src/components/Section.tsx` - 区块组件（如果有）
- [ ] `src/components/ToggleSwitch.tsx` - 切换开关（如果有）

### 音频引擎
- [ ] `src/lib/audio.ts` - 音频处理和分析

### 其他文件
- [ ] `tsconfig.json` - TypeScript 配置
- [ ] `vite.config.ts` - Vite 配置（如果使用 Vite）

---

## 如何使用

1. 将所有文件复制到此目录
2. 阅读代码了解 UI 布局和交互逻辑
3. 参考实现 JUCE C++ 版本
4. 特别关注：
   - 组件布局和尺寸
   - 颜色方案和样式
   - 动画和过渡效果
   - 数据更新频率

---

**注意**: 此目录仅用作参考，不会被编译进最终插件。
