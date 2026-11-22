# 命名词典格式

GakuSMM 可选读取 `GakuSMM_naming.tsv`，用于给“列表外对象”（骨骼、特效、观众等）提供中文标注。

## 文件位置

```
<游戏目录>\GakuLoader\plugins\GakuSMM_naming.tsv
```

## 格式

UTF-8 编码，Tab 分隔，首行为表头：

```
name	category	label
LeftHandThumb2	人物	左拇指2
ps_fireball_smoke	特效	火球·烟
Geo_fence0_LOD2	预制体	围栏 LOD2
```

| 列 | 说明 |
|---|---|
| `name` | GameObject 名（不含 ` Variant` 克隆后缀也可以匹配） |
| `category` | 分类，用于分组，例如 道具/场景/人物/特效/观众/预制体 |
| `label` | 列表里显示的中文标注 |

## 行为

- 词典优先于内置关键词翻译
- 名字完全匹配失败时，会去掉 ` Variant...` 后缀再匹配一次
- 没有词典或没有命中时，退回内置命名规则翻译（`mdl_prp_`、`mdl_env_` 等）

## 生成建议

词典应基于你本地的游戏环境自行生成，不要直接分发从游戏资源提取的数据。

只包含名字/分类/中文标注，不要包含贴图、模型、音频等资源。
