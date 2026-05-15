# 测试输出索引

本索引只记录关键 smoke / 论文图例输出。临时路径可能过期，正式论文用图应复制到论文资产目录。

| 日期 | 测试 | Job JSON | 输出路径 | Manifest | Figure | 状态 |
|---|---|---|---|---|---|---|
| 2026-05-01 | JobRunner spectrogram smoke | `/tmp/goodmeter_audio_doctor_spectrogram_job_test.json` | `/tmp/goodmeter_audio_doctor_exports/codex_job_runner_spectrogram_smoke_test/` | 同输出目录内 manifest | `figures/codex_job_runner_spectrogram_smoke_test_spectrogram_abc_ui_dark.png` | 历史记录显示通过 |
| 2026-05-07 | Spatial figure smoke | 未记录 | 未记录 | 未记录 | 覆盖 `spatial_impression`、`spectrogram_abc`、`reverb_space_combo` | 交工文档记录通过 |
| 2026-05-08 | Masking / band highlight smoke | `/tmp/goodmeter_masking_band_smoke.json` | `/tmp/goodmeter_audio_doctor_smoke/masking_band_smoke_20260508/` | 输出目录内 manifest | masking fusion + spatial impression band highlight | 最近交班记录显示通过 |
| 2026-05-08 | Material alias smoke | `/tmp/goodmeter_material_alias_smoke.json` | `/tmp/goodmeter_audio_doctor_smoke/` | 输出目录内 manifest | material_fire token figure | 最近交班记录显示通过 |

## 新输出追加格式

```md
| YYYY-MM-DD | 测试名 | `/path/to/job.json` | `/path/to/outDir` | `/path/to/manifest.json` | `/path/to/figure.png` | 通过/失败/待复查 |
```

如果输出进入论文，另在论文侧记忆宫殿记录图号、章节和最终 caption。

