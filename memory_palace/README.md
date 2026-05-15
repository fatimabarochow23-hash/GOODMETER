# GOODMETER Memory Palace

本目录是 GOODMETER 工程侧的长期记忆库。它服务施工队、论文组、WinCodex、Claude Code、DeepSeek 和后续任意 AI，在换对话、压缩上下文、换电脑后快速恢复项目现场。

这里记录工程事实，不替代论文工作文件夹里的论文决策记忆。

论文侧记忆宫殿：

```text
/Users/caiyiyang/Desktop/论文工作文件夹/记忆宫殿
```

## 新 AI 读取顺序

1. `README.md`
2. `00_index.md`
3. `timeline/2026-05-01_to_2026-05-08_key_timeline.md`
4. 当前任务相关主题房间
5. `asset_index/` 下的需求单、源码、插件、测试输出索引

## 当前事实与历史方案

- **当前事实**：已经实现、已验证、或代码中能直接定位到的状态。
- **历史方案**：需求单、讨论、曾经尝试过但未必落地的方案。
- **风险边界**：算法口径、插件兼容、尚未验证的行为，必须明确写出。

如果一个条目没有构建或 JobRunner 验证，只能写“待验证”，不能写“已完成”。

## 每次施工后如何追加

完成一个功能、修一个 bug、跑一轮关键图或新增需求单后：

1. 在对应主题房间追加一条事件。
2. 必要时更新 `timeline/`。
3. 更新 `asset_index/requirements_index.md` 或 `asset_index/test_outputs_index.md`。
4. 如果涉及插件兼容，更新 `asset_index/plugin_state_index.md`。
5. 如果涉及源码职责变化，更新 `asset_index/source_files_index.md`。

条目格式参考：

```md
## YYYY-MM-DD 上午/下午/晚上｜事件标题

触发问题：

判断：

改动文件：

验证：

输出产物：

论文影响：

风险与未完成：

状态：

标签：
```

示例见 `examples/example_memory_entry.md`。

## 禁止记录

不要写入：

- API key
- 公司服务器 URL 的敏感 token
- 私人账号凭证
- 插件授权码
- 未公开片源下载链接
- 任何可能导致版权或账号风险的具体来源信息

可以记录：

- 插件是否可用
- 技术问题和修复方式
- 本地路径
- 论文图例素材文件名和拷贝路径
- GitHub 项目公开链接

## 与论文记忆宫殿互链

GOODMETER 侧记录：

- 源码位置
- 构建与验证
- JobRunner / manifest / CSV
- 插件兼容与 state
- 图例算法口径

论文侧记录：

- 老师反馈
- 正文措辞
- 章节结构
- 图例进入正文还是附录

当一个 GOODMETER 功能进入论文时，GOODMETER 侧写：

```text
论文影响：用于第 X 章图 X-X / 附录 X。
```

论文侧写：

```text
图例来源：GOODMETER Audio Doctor，技术实现详见 GOODMETER 记忆宫殿对应条目。
```

