# 示例记忆条目

## 2026-05-08 下午｜Masking / Fusion v1 与分频高亮

触发问题：
论文第二章需要基础声学/心理声学图例，说明低中高频分布、素材构成，以及多层素材之间的遮蔽风险和融合趋势。

判断：
v1 不做真实 masked threshold，只做基于临界带近似、时间/频率重叠和局部电平差的遮蔽风险代理。论文只能写“遮蔽风险”“融合趋势”，不能写“人耳已经听不见”。

改动文件：
- `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorAnalysis.h`
- `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorFigureRenderer.h`
- `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorJobRunner.h`
- `/Users/caiyiyang/Desktop/GOODMETER/Source/AudioDoctorComponent.h`

验证：
- `./build.sh standalone`
- JobRunner smoke：`/tmp/goodmeter_masking_band_smoke.json`
- JobRunner smoke：`/tmp/goodmeter_material_alias_smoke.json`

输出产物：
- `/tmp/goodmeter_audio_doctor_smoke/masking_band_smoke_20260508/`
- `/tmp/goodmeter_audio_doctor_smoke/`

论文影响：
用于第二章感知基础、第三章分层逻辑，以及第四章效果链对层间可辨性和融合趋势的说明。

风险与未完成：
当前是 proxy，不是严格听感实验；后续如果要写 estimated masked threshold，必须继续补算法口径和 manifest 字段。

状态：
有效。

标签：
#AudioDoctor #MaskingFusion #BandHighlight #SecondChapter #JobRunner

