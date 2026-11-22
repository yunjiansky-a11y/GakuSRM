# GakuSMM

学园偶像大师（Gakuen Idolmaster）场景模型管理插件，运行在 GakuLoader 宿主内。

A scene-model manager plugin for *Gakuen Idolmaster*, running inside the GakuLoader host.

## 功能 / Features

- 扫描当前场景的模型对象，按资源命名规则分组
  - 场景公共部件 / 场景组 / 道具 / 人物 / 光影
  - 可选“显示全部对象”，额外归类列表外对象（骨骼、特效、观众、场景几何等）
- 搜索、隐藏、恢复、批量操作、整类选择
- 按住右键在游戏画面滑动，连续选择光标附近的模型
- 保护节点（场景根、角色、灯光、相机）默认禁止隐藏，可勾选“无视风险”强制隐藏
- 中文优先显示，可切换回英文原名
- 可选命名词典 `GakuSMM_naming.tsv`，为对象提供中文标注

## 依赖 / Requirements

- Windows 10/11
- 学园偶像大师（PC 版）
- GakuLoader（插件宿主）
- 构建：MinGW-w64 g++（C++20）、Windows SDK 库

## 构建 / Build

```powershell
# 在仓库根目录
powershell -ExecutionPolicy Bypass -File build.ps1
# 输出：dist\GakuSMM.dll
```

或手动：

```bash
g++ -std=c++20 -O2 -shared -static -fpermissive \
  -Iinclude -Ideps/imgui -o dist/GakuSMM.dll \
  src/GakuSMM.cpp deps/imgui/imgui.cpp deps/imgui/imgui_draw.cpp \
  deps/imgui/imgui_widgets.cpp deps/imgui/imgui_tables.cpp \
  -lkernel32 -luser32 -lgdi32 -lcomctl32 -lole32 -loleaut32
```

## 安装 / Install

1. 退出游戏
2. 把 `GakuSMM.dll` 放入 `GakuLoader\plugins\`
3. 启动游戏，打开 GakuLoader 面板（默认 `Ctrl+M`），选择 **GakuSMM**

Release 包附带 `install.ps1`：

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -GameRoot "D:\gakumas"
```

## 使用 / Usage

- 顶部搜索框过滤名称
- 每行显示 `[显示]` / `[隐藏]`、中文标注、隐藏/恢复按钮
- 分类标题前的复选框可整类选择
- `全选当前` / `清空选择` / `隐藏选中` / `恢复选中` / `恢复全部`
- 在游戏画面按住右键滑动可连续选择
- 日志：`GakuLoader\plugins\GakuSMM.log`

## 命名词典 / Naming dictionary

`GakuSMM_naming.tsv` 是**可选**文件，格式：

```
name<TAB>category<TAB>label
```

- `name`：GameObject 名
- `category`：分类（如 道具 / 场景 / 人物 / 特效）
- `label`：显示用中文标注

仓库**不附带**该文件。你可以根据自己的游戏环境自行生成，放在 `GakuLoader\plugins\GakuSMM_naming.tsv`；没有它时插件会退回内置的命名规则翻译。

格式说明见 [docs/naming-format.md](docs/naming-format.md)。

## 安全与风险 / Safety

- 隐藏场景根节点、舞台组、角色、灯光可能导致游戏崩溃；插件默认保护这些节点
- 勾选“无视风险”后强制隐藏仍可能崩溃
- 本插件修改客户端内存/对象状态，在线游戏可能违反用户协议，请自行评估风险
- 仅建议在离线/单人场景使用

详见 [docs/safety.md](docs/safety.md)。

## 目录 / Layout

```
src/            插件源码
include/        GakuLoader 插件 ABI 头
deps/imgui/     Dear ImGui（MIT）
docs/           命名格式、安全说明
build.ps1       构建脚本
install.ps1     安装脚本
uninstall.ps1   卸载脚本
```

## 许可证 / License

MIT，见 [LICENSE](LICENSE)。

## 致谢 / Credits

- GakuLoader 插件 ABI（`include/GakuPlugin.h`）
- [Dear ImGui](https://github.com/ocornut/imgui)（MIT）

本项目与 Bandai Namco、QualiArts 无关，不包含游戏资源。
