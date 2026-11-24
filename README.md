# GakuSRM

**GakuSRM**（Gakumas Scene Resource Manager）是《学园偶像大师》PC 版的场景资源管理插件，运行在 GakuLoader 宿主内。

- 场景模型：分类、搜索、隐藏/恢复、保护与风险开关
- 音效：317 个游戏音效试听，中文标注
- 时轴：把音效拖到轨道上，跟随 GakuMotion 播放时间触发

> 名称说明：内部 DLL 与插件 id 使用 `GakuSRM` / `gakusrm`，不再兼容旧 `GakuSMM` 安装。

## 功能

### 场景模型
- 按命名规则分组：场景公共部件 / 场景组 / 道具 / 人物 / 光影
- 可选显示全部对象（含骨骼、特效、观众等列表外对象）
- 搜索、隐藏、恢复、批量操作、整类选择
- 保护节点：场景根、角色、灯光、相机默认禁止隐藏；可勾选“无视风险”
- 中文优先显示

### 音效
- 317 个 `sud_se_*` 音效，ACB/HCA 解码为 MP3
- 中文标注（如 `冒险·剑 01`）
- 分类折叠、搜索、单播放器、主音量调节

### 时轴
- 左侧音效面板，按住拖到右侧轨道
- 音效块可拖动改时间、删除、试听
- 播放头跟随 GakuMotion `state.get` 的播放时间，60fps 本地插值
- 时间重叠的音效块自动分层
- 支持缩放、主音量

## 依赖

- Windows 10/11
- 学园偶像大师 PC 版
- GakuLoader v1.1.0 或更新
- GakuMotion（可选，时轴联动需要）
- WebView2 Evergreen Runtime

## 安装

1. 退出游戏
2. 把 `GakuSRM.dll`、`GakuSRM_naming.tsv`、`GakuSRM.UI/` 放入：

```
<游戏目录>\GakuLoader\plugins\
```

3. 启动游戏，按 **F1**，选择 **GakuSRM**

或使用 Release 包内的 `install.ps1`。

## 构建

需要 MinGW-w64 g++（C++20）：

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

输出 `dist\GakuSRM.dll`。

源码依赖：

- `deps/imgui` Dear ImGui（MIT）
- `include/GakuPlugin.h`、`GakuPluginMetadata.h`、`GakuWebPanel.h` GakuLoader 插件 ABI

## 目录

```
src/                插件源码
include/            GakuLoader 插件 ABI 头
deps/imgui/         Dear ImGui
docs/               命名格式、安全说明
build.ps1           构建脚本
install.ps1         安装脚本
uninstall.ps1       卸载脚本
```

## 安全

- 隐藏场景根、角色、灯光可能导致游戏崩溃，默认受保护
- 客户端修改可能违反在线游戏用户协议，建议只在离线/单人场景使用
- 本项目与 Bandai Namco、QualiArts 无关，不包含游戏本体资源

## 许可证

MIT，见 [LICENSE](LICENSE)。

## 致谢

- GakuLoader 插件 ABI
- [Dear ImGui](https://github.com/ocornut/imgui)
- GakuMotion 播放状态服务（时轴联动）
