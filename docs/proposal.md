# YACReader 缩略图网格导航 & 幻灯片自动播放 — 需求文档

> 对应原仓库 issue: [YACReader/yacreader#123](https://github.com/YACReader/yacreader/issues/123)

## 1. 背景与动机

YACReader 查看器目前提供三种页面导航方式：

| 导航方式 | 说明 |
|---------|------|
| 单页/双页翻页 | 主阅读模式，逐页查看 |
| 连续滚动 | 所有页面纵向排列，滚动阅读 |
| GoTo Flow（页面流） | 底部悬浮的 3D 封面流导航叠加层 |

**问题**：GoTo Flow 基于 RHI 3D 渲染，同一时刻只能看到当前页及左右少量邻接页的缩略图，无法一览全局。用户在多页漫画（100+ 页）中快速定位特定页面时效率较低。

issue #123 的讨论核心诉求：
- 以**网格/瀑布流**形式同时展示所有页面缩略图，便于全局浏览
- 支持动态切换 GoTo Flow 与 Grid View
- 图形效果是次要的，**布局可用性**是首要目标

此外，当前查看器缺乏**自动翻页（幻灯片）**功能，用户需要手动逐页翻阅。

---

## 2. 功能需求

### 2.1 缩略图网格视图 (Thumbnail Grid View)

#### 2.1.1 核心行为

- 打开网格视图后，显示当前漫画所有页面的缩略图，以等宽网格布局排列
- 每个缩略图下方显示页码标签
- 当前阅读页在网格中高亮标记（边框/底色）
- 点击任意缩略图跳转到对应页面，并自动关闭网格视图、恢复原阅读模式
- 网格视图打开时自动滚动到当前页面所在位置

#### 2.1.2 缩略图加载

- 复用 `Render` 已有的页面数据管线：`Render::imageLoaded(int, const QByteArray&)` 信号
- 采用**渐进式加载**：先显示占位符，页面数据就绪后逐个替换为真实缩略图
- 缩略图尺寸可配置，默认约 150×200px（约 `SLIDE_ASPECT_RATIO` 宽高比），支持在选项对话框中调整

#### 2.1.3 布局与交互

| 特性 | 说明 |
|------|------|
| 列数 | 根据窗口宽度和缩略图尺寸自适应，或在选项中固定列数 |
| 滚动 | 垂直滚动浏览所有页面 |
| 键盘导航 | 方向键移动选中框，Enter/Space 跳转并关闭网格 |
| 搜索定位 | 支持输入页码快速定位（复用 GoToFlowToolBar 的页码输入逻辑） |
| 右键菜单 | 无（保持简洁，与现有 GoTo Flow 一致） |

#### 2.1.4 与 GoTo Flow 的关系

- 网格视图与 GoTo Flow **互斥**：打开网格视图时自动关闭 GoTo Flow（反之亦然）
- 用户可在选项对话框中选择 GoTo Flow 中"Strip"预设所用的叠加层类型（Flow / Grid），或通过独立快捷键直接切换

### 2.2 幻灯片自动播放 (Slideshow / Auto Page Turn)

#### 2.2.1 核心行为

- 启动后按设定间隔（默认 3 秒）自动翻到下一页
- 到达最后一页时停止，或可选循环回第一页（可配置）
- 翻页操作与现有 `Viewer::next()` 一致，兼容双页模式和漫画模式

#### 2.2.2 交互控制

| 操作 | 行为 |
|------|------|
| 启动/暂停 | 快捷键切换，状态可在信息栏/通知栏显示 |
| 停止 | 任何手动翻页（Next/Prev/GoTo）自动停止幻灯片 |
| 调速 | 快捷键增减间隔（±0.5s），或在选项对话框中设置间隔时间 |
| 循环 | 选项对话框中可设置是否循环 |

#### 2.2.3 状态指示

- 幻灯片运行时，在页面信息标签（`PageLabelWidget`）或通知标签（`NotificationsLabelWidget`）显示自动播放图标和倒计时
- 通知格式示例：`▶ Auto 3.0s`，切换暂停时显示 `⏸ Auto paused`

---

## 3. 快捷键规划

### 3.1 新增 Action ID

在 `shortcuts_management/shortcuts_manager.h` 中新增：

```cpp
#define SHOW_THUMBNAIL_GRID_ACTION_Y "SHOW_THUMBNAIL_GRID_ACTION_Y"
#define SLIDESHOW_TOGGLE_ACTION_Y    "SLIDESHOW_TOGGLE_ACTION_Y"
#define SLIDESHOW_FASTER_ACTION_Y     "SLIDESHOW_FASTER_ACTION_Y"
#define SLIDESHOW_SLOWER_ACTION_Y     "SLIDESHOW_SLOWER_ACTION_Y"
```

### 3.2 默认快捷键

| Action | 快捷键 | 说明 |
|--------|-------|------|
| `SHOW_THUMBNAIL_GRID_ACTION_Y` | `G`（替换当前 GoTo Page 的 `G`，GoTo Page 改为 `Ctrl+G`） | 打开/关闭缩略图网格 |
| `SLIDESHOW_TOGGLE_ACTION_Y` | `Shift+S` | 启动/暂停幻灯片 |
| `SLIDESHOW_FASTER_ACTION_Y` | `Shift+PgUp` 或 `Shift+Up` | 减小间隔 |
| `SLIDESHOW_SLOWER_ACTION_Y` | `Shift+PgDown` 或 `Shift+Down` | 增大间隔 |

> **兼容性考量**：原 `G` 键为 GoTo Page（跳转对话框），建议将跳转对话框快捷键改为 `Ctrl+G`，释放 `G` 给更高频的网格视图。

---

## 4. 架构设计

### 4.1 新增文件

| 文件 | 职责 |
|------|------|
| `YACReader/thumbnail_grid_widget.h/.cpp` | 缩略图网格视图控件 |
| `YACReader/thumbnail_grid_model.h/.cpp` | 网格数据模型（页码→缩略图映射） |

### 4.2 需修改的文件

| 文件 | 修改内容 |
|------|---------|
| `YACReader/viewer.h/.cpp` | 添加 `ThumbnailGridWidget*`、Slideshow `QTimer*`、相关槽函数与状态 |
| `YACReader/main_window_viewer.h/.cpp` | 添加 Actions、菜单项、工具栏按钮 |
| `YACReader/configuration.h/.cpp` | 添加配置项：网格列数、缩略图大小、幻灯片间隔、是否循环 |
| `common/yacreader_global_gui.h` | 添加新的配置常量宏 |
| `shortcuts_management/shortcuts_manager.h/.cpp` | 添加新 Action ID 和默认快捷键 |
| `YACReader/options_dialog.h/.cpp` | 添加网格视图和幻灯片选项 UI |

### 4.3 ThumbnailGridWidget 设计

```
ThumbnailGridWidget : public QWidget, protected Themable
├── QScrollArea                    // 垂直滚动容器
│   └── QWidget (grid container)   // 网格布局宿主
│       └── QGridLayout            // 缩略图+页码标签
├── GoToFlowToolBar*              // 复用：页码输入+滑块定位
├── int currentHighlightIndex     // 当前阅读页高亮
├── int columnCount               // 列数（自适应或固定）
├── QSize thumbnailSize            // 缩略图尺寸
│
│ 信号:
│   goToPage(unsigned int)         // 用户选择了某页
│
│ 槽:
│   setNumSlides(unsigned int)     // 设置总页数
│   setImageReady(int, QByteArray) // 页面数据就绪，更新缩略图
│   centerSlide(int)               // 滚动到指定页
│   setPageNumber(int)             // 更新页码显示
│   reset()                        // 重置所有状态
│   updateSize()                   // 响应窗口尺寸变化
│   highlightPage(int)             // 高亮当前页
```

**关键设计决策**：

1. **继承 `Themable`** — 与 `GoToFlowWidget` 保持一致的主题支持
2. **复用 `Render` 数据管线** — 接收 `Render::imageLoaded(int, const QByteArray&)` 信号获取页面原始数据，本地缩放绘制为缩略图
3. **使用 QWidget 方案而非 QML** — 与现有查看器风格一致（`GoToFlowWidget`、`ContinuousPageWidget` 均为 QWidget），避免引入 QtQuick 依赖
4. **叠加层模式** — 与 `GoToFlowWidget` 一致，作为 Viewer 子控件悬浮显示，不干扰主阅读控件

### 4.4 幻灯片 (Slideshow) 设计

在 `Viewer` 中新增：

```cpp
// viewer.h 新增成员
QTimer *slideshowTimer;
bool slideshowActive;
bool slideshowLoop;           // 是否循环
qreal slideshowInterval;      // 间隔秒数

// 新增槽
void slideshowToggle();       // 启动/暂停/恢复
void slideshowStop();         // 完全停止
void slideshowFaster();       // 减小间隔
void slideshowSlower();       // 增大间隔
void slideshowTick();         // 定时器回调 → next()
```

**流程**：
1. 用户按 `Shift+S` → `slideshowToggle()`
2. 首次启动：`slideshowTimer->start(interval_ms)`，`slideshowActive = true`
3. 暂停：`slideshowTimer->stop()`，`slideshowActive` 保留用于恢复
4. `slideshowTick()` 内调用 `next()`，若到最后一页且不循环则 `slideshowStop()`
5. 任何手动操作翻页（`prev()`/`next()`/`goTo()`/`goToFirstPage()`/`goToLastPage()`）→ `slideshowStop()`

### 4.5 Viewer 中的集成模式

```
Viewer (QScrollArea)
  ├── content (QLabel)           — 单页/双页显示
  ├── messageLabel (QLabel)      — 空状态
  ├── continuousWidget           — 连续滚动
  ├── goToFlow (GoToFlowWidget)  — 页面流叠加层 ← 已有
  ├── thumbnailGrid (ThumbnailGridWidget) — 缩略图网格叠加层 ← 新增
  ├── mglass (MagnifyingGlass)   — 放大镜
  ├── slideshowTimer (QTimer)    — 幻灯片定时器  ← 新增
  └── ... (其他已有成员)
```

**互斥逻辑**：
- 打开 `thumbnailGrid` → 关闭 `goToFlow`，反之亦然
- 关闭 `thumbnailGrid` → 恢复到之前的阅读模式
- 快捷键 `G` / `S` 切换各自叠加层的显示/隐藏

---

## 5. 配置项

| 配置键 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `THUMBNAIL_GRID_COLUMNS` | int | 0 (自适应) | 网格列数，0 表示根据窗口宽度自适应 |
| `THUMBNAIL_GRID_SIZE` | QSize | 150×200 | 缩略图尺寸 |
| `SLIDESHOW_INTERVAL` | qreal | 3.0 | 幻灯片间隔（秒） |
| `SLIDESHOW_LOOP` | bool | false | 是否循环播放 |

在 `common/yacreader_global_gui.h` 中新增对应的 `#define` 常量。

---

## 6. 与 Linux 平台兼容性

- `ThumbnailGridWidget` 使用纯 QWidget + `QGridLayout`，不依赖 RHI/OpenGL，确保在无 GPU 加速的环境中也能运行
- `ContinuousPageWidget` 已有类似的纯 CPU 渲染 + 按需缩放的模式，可参考其缓存策略（`ScaledPageCache`）

---

## 7. 参考实现分析

### 7.1 GoToFlowWidget（现有叠加层模式）

- **定位方式**：作为 Viewer 子控件，通过 `move()` 手动定位在底部居中
- **显示/隐藏**：`QPropertyAnimation` 对 `"pos"` 属性做 150ms 滑动动画
- **鼠标交互**：`MouseHandler` 检测鼠标进入底部 15px 区域自动显示，离开时自动隐藏
- **数据流**：`Render` 发射 `imageLoaded(int, const QByteArray&)` → `GoToFlowWidget::setImageReady()`

缩略图网格视图应完全遵循此模式，替换动画可考虑淡入/淡出（`QGraphicsOpacityEffect`）而非底部滑动，因为网格视图占据面积更大。

### 7.2 YACReaderLibrary 的 Grid View（QML）

`YACReaderLibrary` 中有基于 QtQuick 的 `GridComicsView`（`YACReaderLibrary/grid_comics_view.h`），但查看器中不使用 QML。新网格视图应采用 QWidget 实现以保持一致。

### 7.3 ContinuousPageWidget（纯 CPU 渲染参考）

- 实现 `hasHeightForWidth()` / `heightForWidth()` 接口适配滚动区域
- 使用 `ScaledPageCache` 缓存缩放后的页面图像，按需渲染
- 通过 `Render::bufferedImage(int)` 获取已加载的原始图像

缩略图网格可复用此缓存策略，但无需 `heightForWidth`（网格布局而非连续滚动）。

---

## 8. 实现计划

### Phase 1：缩略图网格视图 — 基础功能

1. 创建 `ThumbnailGridWidget`，实现网格布局 + 占位符
2. 接入 `Render::imageLoaded` 信号，渐进式填充缩略图
3. 在 `Viewer` 中集成叠加层逻辑（显示/隐藏/互斥）
4. 在 `MainWindowViewer` 中添加 Action 和菜单项
5. 添加快捷键 `SHOW_THUMBNAIL_GRID_ACTION_Y`
6. 实现点击跳转并关闭网格视图

### Phase 2：网格视图增强

7. 添加页码标签和高亮当前页
8. 键盘导航（方向键 + Enter）
9. 复用 `GoToFlowToolBar` 实现页码输入/滑块
10. 选项对话框集成（缩略图大小、列数）
11. 主题适配（`Themable` 接口）

### Phase 3：幻灯片自动播放

12. 在 `Viewer` 中添加 `QTimer` 和滑动状态逻辑
13. 添加 `SLIDESHOW_TOGGLE_ACTION_Y` 等快捷键
14. 状态指示（通知标签显示播放状态和间隔）
15. 选项对话框集成（间隔、循环）
16. 手动翻页时自动停止

### Phase 4：打磨与测试

17. 配置持久化（`Configuration` 类读写 INI）
18. 翻译更新（`update_translations` 目标）
19. 代码格式化（`clang-format-windows.cmd`）
20. 单元测试（如适用）