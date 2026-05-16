# YACReader 缩略图网格导航 & 幻灯片自动播放 — 概要设计文档

> 本文档基于 `docs/proposal.md` 需求，结合 YACReader 现有代码架构进行模块划分与接口设计。

---

## 1. 模块总览

新增功能涉及以下模块，按职责分为三层：

```
┌─────────────────────────────────────────────────────────┐
│                    应用层 (Application)                  │
│  MainWindowViewer  —  Action 注册、菜单/工具栏、快捷键    │
│  OptionsDialog     —  配置 UI（新增网格/幻灯片选项卡）     │
├─────────────────────────────────────────────────────────┤
│                    核心层 (Core)                          │
│  Viewer            —  叠加层调度、幻灯片状态机              │
│  ThumbnailGridWidget  —  缩略图网格叠加层控件              │
│  ThumbnailGridToolBar —  网格视图工具栏（页码输入+滑块）    │
├─────────────────────────────────────────────────────────┤
│                    基础层 (Infrastructure)                │
│  Configuration     —  配置持久化（新增配置项）              │
│  ShortcutsManager  —  快捷键注册与持久化                   │
│  Theme / Themable  —  主题系统扩展                         │
│  Render            —  已有页面数据管线（只读复用）           │
└─────────────────────────────────────────────────────────┘
```

**模块依赖关系图：**

```
MainWindowViewer
    ├── ThumbnailGridWidget ──→ Render (imageLoaded/bufferedImage)
    │       └── ThumbnailGridToolBar
    ├── Viewer
    │       ├── ThumbnailGridWidget (叠加层生命周期)
    │       ├── GoToFlowWidget (互斥调度)
    │       └── QTimer (slideshowTimer)
    ├── OptionsDialog → Configuration
    └── ShortcutsManager → shortcuts_manager.h/.cpp
```

---

## 2. 模块详细设计

### 2.1 ThumbnailGridWidget — 缩略图网格叠加层

**职责**：以网格形式显示所有页面缩略图，支持点击跳转、键盘导航、当前页高亮。

#### 2.1.1 类定义

```cpp
// YACReader/thumbnail_grid_widget.h

class ThumbnailGridToolBar;

class ThumbnailGridWidget : public QWidget, protected Themable
{
    Q_OBJECT

public:
    explicit ThumbnailGridWidget(QWidget *parent = nullptr);
    ~ThumbnailGridWidget() override;

    void setFlowRightToLeft(bool b);

public slots:
    void reset();
    void centerSlide(int slide);   // 滚动到指定页（0-based）
    void setPageNumber(int page);  // 更新工具栏页码显示
    void setNumSlides(unsigned int slides);
    void setImageReady(int index, const QByteArray &image);
    void updateSize();
    void updateConfig(QSettings *settings);
    void highlightPage(int pageIndex);

signals:
    void goToPage(unsigned int page);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void applyTheme(const Theme &theme) override;

private:
    // ---- 内部子控件 ----
    QScrollArea *scrollArea;
    QWidget *gridContainer;
    QGridLayout *gridLayout;
    ThumbnailGridToolBar *toolBar;

    // ---- 缩略图项存储 ----
    struct ThumbnailItem {
        QLabel *imageLabel;      // 缩略图显示
        QLabel *pageLabel;       // 页码文字
        int pageIndex;           // 0-based 页码
        bool imageLoaded;        // 数据是否已到达
    };
    QVector<ThumbnailItem> items;

    // ---- 状态 ----
    int currentHighlightIndex = -1;
    int currentFocusIndex = -1;  // 键盘导航焦点（0-based）
    int totalPages = 0;
    QSize thumbnailSize;
    int columnCount = 0;         // 0 = 自适应

    // ---- 内部方法 ----
    void buildGrid();
    void clearGrid();
    void scrollToPage(int index);
    void updateItemHighlight(int oldIndex, int newIndex);
    int computeColumnCount() const;
    QLabel *createPlaceholder() const;
};

// YACReader/thumbnail_grid_toolbar.h

class ThumbnailGridToolBar : public QWidget, protected Themable
{
    Q_OBJECT

public:
    explicit ThumbnailGridToolBar(QWidget *parent = nullptr);

public slots:
    void setPage(int pageNumber);  // 0-based internal, 显示为 1-based
    void setTop(int numPages);
    void goTo();
    void centerSlide();
    void updateOptions();

signals:
    void setCenter(unsigned int);
    void goToPage(unsigned int);

protected:
    void applyTheme(const Theme &theme) override;

private:
    QLineEdit *edit;
    QSlider *slider;
    QIntValidator *v;
    QPushButton *centerButton;
    QPushButton *goToButton;
};
```

#### 2.1.2 布局结构

```
┌─────────────────────────────────────────────────┐
│  ThumbnailGridWidget (QWidget, 作为 Viewer 子控件) │
│ ┌───────────────────────────────────────────────┐│
│ │ QScrollArea (占据主区域)                      ││
│ │ ┌───────────────────────────────────────────┐││
│ │ │ gridContainer (QWidget)                    │││
│ │ │ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐│││
│ │ │ │ [0] │ │ [1] │ │ [2] │ │ [3] │ │ [4] ││││
│ │ │ │  p1 │ │  p2 │ │  p3 │ │  p4 │ │  p5 ││││
│ │ │ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘│││
│ │ │ ┌─────┐ ┌─────┐ ...                       │││
│ │ │ │ [5] │ │ [6] │                           │││
│ │ │ │  p6 │ │  p7 │                           │││
│ │ │ └─────┘ └─────┘                           │││
│ │ └───────────────────────────────────────────┘││
│ └───────────────────────────────────────────────┘│
│ ┌───────────────────────────────────────────────┐│
│ │ ThumbnailGridToolBar (底部固定)               ││
│ │  [页码输入] [滑块 ────────○─────] [跳转] [居中]││
│ └───────────────────────────────────────────────┘│
└─────────────────────────────────────────────────┘
```

#### 2.1.3 缩略图渲染策略

采用**两级缓存**策略，与 `ContinuousPageWidget` 的 `ScaledPageCache` 思路一致：

```
Render::imageLoaded(int, QByteArray)
       │
       ▼
ThumbnailGridWidget::setImageReady()
       │
       ├── 1. QByteArray → QImage (QImageReader)
       │      缩放到 thumbnailSize
       │
       ├── 2. 存原始 QByteArray 到 rawImages[index] + 标记 imagesReady[index]
       │
       └── 3. 显示缩放后的 QPixmap 到对应 ThumbnailItem.imageLabel

替代方案（已加载的页面）：
Render::bufferedImage(int) → const QImage*
       │
       ▼
ThumbnailGridWidget::buildGrid() / highlightPage()
       │
       └── 直接取已 buffer 的页面 → 缩放 → 显示
```

**占位符策略**：加载漫画后立即 `setNumSlides(n)` 创建 n 个占位项，显示统一灰色背景 + 页码。数据到达后逐个替换为真实缩略图。

#### 2.1.4 键盘导航

```
┌──────────────┬────────────────────────────────────┐
│ 按键          │ 行为                                 │
├──────────────┼────────────────────────────────────┤
│ Left/Right   │ 移动焦点到相邻缩略图                  │
│ Up/Down      │ 移动焦点到上/下行缩略图                │
│ Enter/Space  │ 跳转到焦点页 → emit goToPage()        │
│ Escape       │ 关闭网格视图                          │
│ S            │ 关闭网格视图（与打开快捷键一致）         │
└──────────────┴────────────────────────────────────┘
```

焦点项用高亮边框标识，与 `currentHighlightIndex`（当前阅读页的底色标识）共存。

#### 2.1.5 与 GoToFlowWidget 的接口对齐

`ThumbnailGridWidget` 的公有接口刻意与 `GoToFlowWidget` **保持一致**：

| GoToFlowWidget 方法      | ThumbnailGridWidget 方法   | 语义       |
|-------------------------|---------------------------|-----------|
| `reset()`               | `reset()`                 | 重置       |
| `centerSlide(int)`      | `centerSlide(int)`        | 定位到某页  |
| `setPageNumber(int)`    | `setPageNumber(int)`      | 更新页码    |
| `setNumSlides(uint)`    | `setNumSlides(uint)`      | 设置总页数  |
| `setImageReady(int, QByteArray&)` | `setImageReady(int, QByteArray&)` | 数据就绪 |
| `updateSize()`          | `updateSize()`            | 响应尺寸    |
| `updateConfig(QSettings*)` | `updateConfig(QSettings*)` | 配置变更 |
| `setFlowRightToLeft(bool)` | `setFlowRightToLeft(bool)` | 漫画模式  |
| 信号 `goToPage(uint)`    | 信号 `goToPage(uint)`      | 用户选择页  |

**原因**：Viewer 中的叠加层切换逻辑可以统一用相同接口管理，降低耦合。如果后续需要更多类型的导航叠加层，可考虑提取 `INavigationOverlay` 接口。

---

### 2.2 Viewer — 叠加层调度与幻灯片状态机

**职责**：管理多种叠加层的互斥调度，以及幻灯片定时器。

#### 2.2.1 新增成员

```cpp
// viewer.h 新增

class ThumbnailGridWidget;

// ---- 叠加层 ----
ThumbnailGridWidget *thumbnailGrid;
QPropertyAnimation *showThumbnailGridAnimation;

// ---- 幻灯片 ----
QTimer *slideshowTimer;
bool slideshowActive = false;
bool slideshowPaused = false;

// ---- 叠加层调度 ----
public slots:
    void thumbnailGridSwitch();       // toggle 网格视图
    void showThumbnailGrid();
    void animateShowThumbnailGrid();
    void animateHideThumbnailGrid();

// ---- 幻灯片 ----
public slots:
    void slideshowToggle();           // 启动/暂停
    void slideshowStop();             // 停止（手动翻页触发）
    void slideshowFaster();
    void slideshowSlower();
private slots:
    void slideshowTick();             // QTimer timeout → next()

private:
    void startSlideshowTimer();
    void updateSlideshowStatus();     // 更新通知栏状态
```

#### 2.2.2 叠加层互斥调度

```
showThumbnailGrid() / animateShowThumbnailGrid()
    │
    ├── 如果 goToFlow 可见 → animateHideGoToFlow()
    ├── thumbnailGrid->show()
    ├── thumbnailGrid->setPageNumber()
    ├── thumbnailGrid->centerSlide()
    └── thumbnailGrid->setFocus()

animateHideThumbnailGrid()
    │
    ├── thumbnailGrid->hide()
    └── this->setFocus()  // 焦点还给 Viewer
```

**Viewer 中叠加层的显示优先级**：
1. `thumbnailGrid`（最高） — 全屏遮挡
2. `goToFlow`（中） — 底部悬浮
3. 主阅读控件（content / continuousWidget）（低）

同一时刻只允许一个叠加层可见。

#### 2.2.3 幻灯片状态机

```
                    ┌──────────────────┐
                    │     Stopped       │
                    │ slideshowActive=false │
                    └────────┬─────────┘
                             │ slideshowToggle()
                             ▼
                    ┌──────────────────┐
                    │     Playing       │◄───────┐
                    │ timer running     │        │ slideshowTick()
                    └────────┬─────────┘        │ (next page, not last)
                             │                  │
                 ┌───────────┼──────────┐       │
                 │           │          │       │
            slideshowToggle()  last page   slideshowToggle()
                 │           │  & loop    │
                 ▼           ▼           ▼
          ┌──────────┐  ┌────────┐  ┌──────────┐
          │  Paused  │  │Stopped │  │ Playing  │
          │ timer stopped │same   │  │ (loop   │
          └────┬─────┘  └────────┘  │ restart)│
               │                     └─────────┘
        slideshowToggle()
               │
               ▼
          ┌──────────┐
          │ Playing  │ (resume timer)
          └──────────┘

任何手动翻页 (prev/next/goTo) → slideshowStop() → Stopped
slideshowFaster/Slower → 调整 interval，若 Playing 则 restart timer
```

**与现有 Viewer 方法的集成点**：

| Viewer 现有方法 | 需添加的幻灯片逻辑 |
|----------------|------------------|
| `next()` | 若 `slideshowActive && !slideshowPaused`：不做额外处理（定时器驱动 next 本身）|
| `prev()` | 调用 `slideshowStop()` |
| `goTo()` | 调用 `slideshowStop()` |
| `goToFirstPage()` / `goToLastPage()` | 调用 `slideshowStop()` |

> **设计决策**：幻灯片的 `slideshowTick()` 直接调用 `next()`，不绕过双页模式等逻辑。到达最后一页时检查 `Configuration::getSlideshowLoop()`，循环则 `goTo(0)`，否则 `slideshowStop()`。

#### 2.2.4 状态指示

利用已有的 `NotificationsLabelWidget` 做瞬时通知，以及 `PageLabelWidget` 做持续指示：

| 事件 | 组件 | 格式 |
|------|------|------|
| 幻灯片启动 | `NotificationsLabelWidget::flash()` | `▶ Auto 3.0s` |
| 幻灯片暂停 | `NotificationsLabelWidget::flash()` | `⏸ Auto paused` |
| 幻灯片停止 | `NotificationsLabelWidget::flash()` | `⏹ Auto stopped` |
| 调速 | `NotificationsLabelWidget::flash()` | `Auto: 2.5s` |
| 翻页中（持续状态） | `PageLabelWidget::setText()` | 追加 ` ▶` 标记 |

`slideshowTick()` 中调用 `informationSwitch()` 更新 PageLabelWidget 内容。

---

### 2.3 ThumbnailGridToolBar — 网格视图工具栏

职责：页码输入框 + 滑块 + 跳转/居中按钮，与 `GoToFlowToolBar` 功能对等。

**设计决策**：不复用 `GoToFlowToolBar`，而是新建平行类。原因：
1. `GoToFlowToolBar` 继承 `QStackedWidget`，包含 normal/quickNavi 两种模式的切换逻辑，是专为 3D Flow 设计的
2. 网格视图的工具栏布局可能不同（例如横向紧凑排列），需要独立适配主题
3. 两者共享相同的信号契约（`goToPage`、`setCenter`），便于 Viewer 统一接入

接口与 `GoToFlowToolBar` 一致：`setPage()`、`setTop()`、`goTo()`、`centerSlide()`、`updateOptions()`、`goToPage(uint)` 信号。

---

### 2.4 MainWindowViewer — Action 注册与 UI 集成

#### 2.4.1 新增 Action

在 `MainWindowViewer::createActions()` 中新增：

```cpp
// 缩略图网格
showThumbnailGridAction = new QAction(tr("Thumbnail grid"), this);
showThumbnailGridAction->setData(SHOW_THUMBNAIL_GRID_ACTION_Y);
showThumbnailGridAction->setShortcut(
    ShortcutsManager::getShortcutsManager().getShortcut(SHOW_THUMBNAIL_GRID_ACTION_Y));
connect(showThumbnailGridAction, &QAction::triggered,
        viewer, &Viewer::thumbnailGridSwitch);

// 幻灯片
slideshowToggleAction = new QAction(tr("Toggle slideshow"), this);
slideshowToggleAction->setData(SLIDESHOW_TOGGLE_ACTION_Y);
slideshowToggleAction->setShortcut(
    ShortcutsManager::getShortcutsManager().getShortcut(SLIDESHOW_TOGGLE_ACTION_Y));
connect(slideshowToggleAction, &QAction::triggered,
        viewer, &Viewer::slideshowToggle);

// 幻灯片调速
slideshowFasterAction = addActionWithShortcut(tr("Slideshow faster"), SLIDESHOW_FASTER_ACTION_Y);
connect(slideshowFasterAction, &QAction::triggered, viewer, &Viewer::slideshowFaster);

slideshowSlowerAction = addActionWithShortcut(tr("Slideshow slower"), SLIDESHOW_SLOWER_ACTION_Y);
connect(slideshowSlowerAction, &QAction::triggered, viewer, &Viewer::slideshowSlower);
```

#### 2.4.2 工具栏

在 `comicToolBar` 的 Flow 按钮旁添加 Grid 按钮：

```cpp
comicToolBar->addAction(wrappedToolbarAction(showThumbnailGridAction));
comicToolBar->addAction(wrappedToolbarAction(slideshowToggleAction));
```

#### 2.4.3 快捷键分组

在 `setUpShortcutsManagement()` 中新增两个分组：

```cpp
editShortcutsDialog->addActionsGroup(
    tr("Thumbnail Grid"), theme.shortcutsIcons.thumbnailGridIcon,
    tmpList = QList<QAction *>()
        << showThumbnailGridAction);

editShortcutsDialog->addActionsGroup(
    tr("Slideshow"), theme.shortcutsIcons.slideshowIcon,
    tmpList = QList<QAction *>()
        << slideshowToggleAction
        << slideshowFasterAction
        << slideshowSlowerAction);
```

---

### 2.5 Configuration — 配置持久化

#### 2.5.1 新增配置常量

在 `common/yacreader_global_gui.h` 中：

```cpp
#define THUMBNAIL_GRID_COLUMNS   "THUMBNAIL_GRID_COLUMNS"
#define THUMBNAIL_GRID_SIZE      "THUMBNAIL_GRID_SIZE"
#define SLIDESHOW_INTERVAL       "SLIDESHOW_INTERVAL"
#define SLIDESHOW_LOOP            "SLIDESHOW_LOOP"
```

#### 2.5.2 Configuration 类扩展

在 `YACReader/configuration.h` 的 `Configuration` 单例中新增：

```cpp
// Thumbnail Grid
int getThumbnailGridColumns() { return settings->value(THUMBNAIL_GRID_COLUMNS, 0).toInt(); }
void setThumbnailGridColumns(int cols) { settings->setValue(THUMBNAIL_GRID_COLUMNS, cols); }
QSize getThumbnailGridSize() { return settings->value(THUMBNAIL_GRID_SIZE, QSize(150, 200)).toSize(); }
void setThumbnailGridSize(const QSize &size) { settings->value(THUMBNAIL_GRID_SIZE, size); }

// Slideshow
qreal getSlideshowInterval() { return settings->value(SLIDESHOW_INTERVAL, 3.0).toReal(); }
void setSlideshowInterval(qreal interval) { settings->setValue(SLIDESHOW_INTERVAL, interval); }
bool getSlideshowLoop() { return settings->value(SLIDESHOW_LOOP, false).toBool(); }
void setSlideshowLoop(bool loop) { settings->setValue(SLIDESHOW_LOOP, loop); }
```

---

### 2.6 ShortcutsManager — 快捷键扩展

在 `shortcuts_management/shortcuts_manager.h` 中新增：

```cpp
#define SHOW_THUMBNAIL_GRID_ACTION_Y  "SHOW_THUMBNAIL_GRID_ACTION_Y"
#define SLIDESHOW_TOGGLE_ACTION_Y     "SLIDESHOW_TOGGLE_ACTION_Y"
#define SLIDESHOW_FASTER_ACTION_Y     "SLIDESHOW_FASTER_ACTION_Y"
#define SLIDESHOW_SLOWER_ACTION_Y      "SLIDESHOW_SLOWER_ACTION_Y"
```

在 `shortcuts_manager.cpp` 的 `initDefaultShorcuts()` 中（`#else` 即 YACReader 段）：

```cpp
defaultShorcuts.insert(SHOW_THUMBNAIL_GRID_ACTION_Y, Qt::Key_G);
defaultShorcuts.insert(SLIDESHOW_TOGGLE_ACTION_Y, Qt::SHIFT | Qt::Key_S);
defaultShorcuts.insert(SLIDESHOW_FASTER_ACTION_Y, Qt::SHIFT | Qt::Key_Up);
defaultShorcuts.insert(SLIDESHOW_SLOWER_ACTION_Y, Qt::SHIFT | Qt::Key_Down);
```

**兼容性处理**：原 `GO_TO_PAGE_ACTION_Y` 默认快捷键从 `G` 改为 `Ctrl+G`：

```cpp
// 修改:
defaultShorcuts.insert(GO_TO_PAGE_ACTION_Y, Qt::CTRL | Qt::Key_G);
```

---

### 2.7 Theme — 主题扩展

#### 2.7.1 新增主题结构体

在 `YACReader/themes/theme.h` 中：

```cpp
struct ThumbnailGridThemeTemplates {
    QString thumbnailLabelQSS = "QLabel { color: %1; font-size: %2px; }";
    QString thumbnailBorderQSS = "...";  // 选中/高亮边框样式
};

struct ThumbnailGridTheme {
    QColor backgroundColor;
    QColor textColor;
    QColor selectionColor;      // 当前选中（键盘焦点）
    QColor highlightColor;      // 当前阅读页底色
    QString labelQSS;
    QString borderQSS;
};

struct SlideshowTheme {
    QString statusQSS;          // 状态文字样式
};
```

在 `Theme` 结构体中新增字段：

```cpp
struct Theme {
    // ... 已有字段 ...
    ThumbnailGridTheme thumbnailGrid;    // 新增
    SlideshowTheme slideshow;            // 新增
};
```

在 `ShortcutsIconsTheme` 中新增：

```cpp
struct ShortcutsIconsTheme {
    // ... 已有字段 ...
    QIcon thumbnailGridIcon;    // 新增
    QIcon slideshowIcon;        // 新增
};
```

在 `ToolbarTheme` 中新增工具栏图标：

```cpp
struct ToolbarTheme {
    // ... 已有字段 ...
    QIcon showThumbnailGridAction;             // 新增
    QIcon showThumbnailGridAction18x18;        // 新增
    QIcon slideshowToggleAction;               // 新增
    QIcon slideshowToggleAction18x18;          // 新增
};
```

---

### 2.8 OptionsDialog — 配置 UI

在现有选项对话框中新增一个 Tab："Grid & Slideshow"。

#### 2.8.1 新增 UI 控件

```cpp
// options_dialog.h 新增

// ---- Grid 选项 ----
QSpinBox *gridColumnsSpin;       // 列数: 0=自适应, 1-20
QSpinBox *gridThumbWidthSpin;    // 缩略图宽度: 80-300
QSpinBox *gridThumbHeightSpin;  // 缩略图高度: 100-400

// ---- Slideshow 选项 ----
QDoubleSpinBox *slideshowIntervalSpin;  // 间隔: 0.5-30.0 秒
QCheckBox *slideshowLoopCheckBox;       // 循环播放
```

#### 2.8.2 Tab 结构

```
┌────────────────────────────────────────────────────────┐
│ General | Page Flow | Image | **Grid & Slideshow**     │
├────────────────────────────────────────────────────────┤
│                                                        │
│  Thumbnail Grid                                        │
│  ┌─────────────────────────────────────────────────┐  │
│  │ Columns:  [0 (Auto) ▼]                          │  │
│  │ Thumbnail size:  Width [150]  Height [200]       │  │
│  └─────────────────────────────────────────────────┘  │
│                                                        │
│  Slideshow                                            │
│  ┌─────────────────────────────────────────────────┐  │
│  │ Interval (seconds): [3.0 ▲▼]                   │  │
│  │ ☑ Loop playback                                  │  │
│  └─────────────────────────────────────────────────┘  │
│                                                        │
└────────────────────────────────────────────────────────┘
```

#### 2.8.3 信号流

```
OptionsDialog::saveOptions()
    ├── Configuration::setThumbnailGridColumns()
    ├── Configuration::setThumbnailGridSize()
    ├── Configuration::setSlideshowInterval()
    ├── Configuration::setSlideshowLoop()
    └── emit changedOptions()
            │
            ▼
MainWindowViewer::optionsChanged()
    ├── viewer->updateConfig(settings)  // 已有
    └── viewer->updateSlideshowInterval()  // 新增
```

---

## 3. 数据流

### 3.1 缩略图数据流

```
┌─────────┐  imageLoaded(int, QByteArray)  ┌──────────────────────┐
│         │─────────────────────────────────▶│                      │
│  Render │  numPages(unsigned int)          │ ThumbnailGridWidget  │
│         │─────────────────────────────────▶│                      │
│         │                                 │  setImageReady()      │
│         │  pageChanged(int)                │  setNumSlides()      │
│         │──────────────────────────────────▶│  highlightPage()    │
└─────────┘                                  └──────────┬───────────┘
                                                        │
                                           goToPage(unsigned int)
                                                        │
                                                        ▼
                                                ┌──────────────┐
                                                │    Viewer     │
                                                │  goTo()      │
                                                └──────────────┘
```

**连通方式**（在 `Viewer::createConnections()` 中）：

```cpp
// 已有（GoToFlow）:
connect(render, QOverload<unsigned int>::of(&Render::numPages),
        goToFlow, &GoToFlowWidget::setNumSlides);
connect(render, QOverload<int, const QByteArray &>::of(&Render::imageLoaded),
        goToFlow, &GoToFlowWidget::setImageReady);

// 新增（ThumbnailGrid）:
connect(render, QOverload<unsigned int>::of(&Render::numPages),
        thumbnailGrid, &ThumbnailGridWidget::setNumSlides);
connect(render, QOverload<int, const QByteArray &>::of(&Render::imageLoaded),
        thumbnailGrid, &ThumbnailGridWidget::setImageReady);

// 用户选择 → 跳转:
connect(thumbnailGrid, &ThumbnailGridWidget::goToPage,
        this, qOverload<unsigned int>(&Viewer::goTo));
```

**注意**：`Render::imageLoaded` 信号同时连接到 `goToFlow` 和 `thumbnailGrid`，QtCore 信号-槽机制保证了两者都能收到数据。当某个叠加层处于隐藏状态时，slot 内部可以做轻量级缓存或跳过绘制。

### 3.2 幻灯片数据流

```
┌──────────────────┐  timeout()   ┌─────────────┐  next()  ┌────────┐
│  slideshowTimer  │────────────▶│    Viewer    │─────────▶│ Render │
│  (QTimer)        │             │             │          │        │
└──────────────────┘             │ slideshowTick│          └────────┘
                                 │             │
    ▲ slideshowToggle()          │ slideshowActive│
    │ slideshowFaster()          │ slideshowPaused│
    │ slideshowSlower()          └──────┬──────┘
    │                                   │
    │ SlideshowToggle Action            │ 更新通知
    │ SlideshowFaster/Slower Action     │
    │                                   ▼
    │                          ┌──────────────────────┐
    │                          │ NotificationsLabelWidget│
    │                          │ PageLabelWidget         │
    └──────────────────────────┴──────────────────────────┘
```

### 3.3 叠加层调度流

```
用户按下 G 键
    │
    ▼
MainWindowViewer::showThumbnailGridAction::triggered
    │
    ▼
Viewer::thumbnailGridSwitch()
    │
    ├── thumbnailGrid->isVisible() ? animateHideThumbnailGrid() : showThumbnailGrid()
    │
    ▼
Viewer::showThumbnailGrid()
    │
    ├── if (!render->hasLoadedComic()) return;
    ├── if (goToFlow->isVisible()) animateHideGoToFlow();
    ├── animateShowThumbnailGrid();  // 显示动画
    ├── thumbnailGrid->setPageNumber(render->getIndex());
    ├── thumbnailGrid->centerSlide(render->getIndex());
    └── thumbnailGrid->setFocus();
```

---

## 4. 文件变更清单

### 4.1 新增文件

| 文件路径 | 说明 |
|---------|------|
| `YACReader/thumbnail_grid_widget.h` | 缩略图网格控件声明 |
| `YACReader/thumbnail_grid_widget.cpp` | 缩略图网格控件实现 |
| `YACReader/thumbnail_grid_toolbar.h` | 网格工具栏声明 |
| `YACReader/thumbnail_grid_toolbar.cpp` | 网格工具栏实现 |
| `images/viewer_toolbar/thumbnailGrid.svg` | 网格视图工具栏图标 |
| `images/viewer_toolbar/thumbnailGrid_18x18.svg` | 网格视图工具栏图标（小） |
| `images/viewer_toolbar/slideshow.svg` | 幻灯片工具栏图标 |
| `images/viewer_toolbar/slideshow_18x18.svg` | 幻灯片工具栏图标（小） |
| `images/shortcuts/shortcuts_group_grid.svg` | 快捷键分组图标 |
| `images/shortcuts/shortcuts_group_slideshow.svg` | 快捷键分组图标 |

### 4.2 修改文件

| 文件路径 | 修改范围 |
|---------|---------|
| `YACReader/viewer.h` | 添加 `ThumbnailGridWidget*`、`slideshowTimer` 等成员、声明 slot/signal |
| `YACReader/viewer.cpp` | 实现叠加层调度、幻灯片状态机、信号连接 |
| `YACReader/main_window_viewer.h` | 添加 `showThumbnailGridAction`、`slideshowToggleAction` 等 Action 成员 |
| `YACReader/main_window_viewer.cpp` | `createActions()` 新增 Action、`setUpShortcutsManagement()` 新增分组、`createToolBars()` 添加按钮 |
| `YACReader/configuration.h/.cpp` | 新增 grid/slideshow 配置项的 getter/setter |
| `common/yacreader_global_gui.h` | 新增配置常量 `#define` |
| `shortcuts_management/shortcuts_manager.h` | 新增 Action ID 宏 |
| `shortcuts_management/shortcuts_manager.cpp` | 新增默认快捷键映射 |
| `YACReader/options_dialog.h/.cpp` | 新增 "Grid & Slideshow" 选项卡 |
| `YACReader/themes/theme.h` | 新增 `ThumbnailGridTheme`、`SlideshowTheme` 等结构体 |
| `YACReader/themes/theme_factory.h/.cpp` | 新增主题字段的 JSON 解析和默认值 |
| `YACReader/mouse_handler.cpp` | 网格视图显示/隐藏的鼠标触发逻辑（可选，与 GoToFlow 类似） |
| `YACReader/CMakeLists.txt` | 添加新源文件和资源文件 |

---

## 5. 关键设计决策与权衡

### 5.1 为什么网格视图使用叠加层而非独立阅读模式？

**决策**：网格视图作为叠加层（overlay），类似 `GoToFlowWidget`，而非替代阅读模式（如 singlePage/continuousScroll）。

**理由**：
- 网格视图是**导航工具**而非阅读模式 — 用户目的是"找到页面然后回去阅读"
- 与 GoToFlow 保持架构一致，降低理解成本
- 关闭后无需恢复之前的阅读状态（叠加层天然不干扰底层控件）
- 如果作为阅读模式，需要与 `setActiveWidget()` 竞争，增加复杂度

### 5.2 为什么不复用 GoToFlowToolBar？

**决策**：新建 `ThumbnailGridToolBar`，不复用 `GoToFlowToolBar`。

**理由**：
- `GoToFlowToolBar` 继承 `QStackedWidget`，有 normal/quickNavi 双模式切换逻辑，是 3D Flow 专属的
- 网格工具栏的布局可能不同（更紧凑的横向排列 vs Flow 的紧凑底部条）
- 但两者共享相同的信号契约（`goToPage`/`setCenter`），便于统一接入
- 如果后续抽象 `INavigationToolBar` 接口，两个类可以实现同一接口

### 5.3 幻灯片设计嵌入 Viewer 而非独立类？

**决策**：幻灯片逻辑（Timer + 状态标志）直接作为 Viewer 的成员，不提取为独立类。

**理由**：
- 幻灯片只有 3 个状态标志（`slideshowActive`、`slideshowPaused`、interval）和 1 个 QTimer
- 状态转换逻辑与 Viewer 的页面导航方法（`next()`/`goTo()`）强耦合
- 将其提取为独立类会导致 Viewer 需要大量友元/friend 访问或信号绕行，增加复杂度而非降低
- 如果幻灯片未来需要更多功能（过渡动画、进度条等），再重构提取为 `SlideshowController`

### 5.4 缩略图加载：Render 信号 vs 主动拉取？

**决策**：主要依赖 `Render::imageLoaded` 信号被动接收，辅以 `Render::bufferedImage()` 主动拉取已缓存页面。

**理由**：
- `Render` 在页面加载时按序发射 `imageLoaded`，网格视图被动接收即可
- 对于已加载的页面（`Render::bufferedImage()` 返回非空），在 `setNumSlides()` 时可主动获取
- 避免引入额外的后台渲染线程（`Render` 已有 `PageRender` 线程池），缩略图缩放只需在主线程 QImage → QPixmap 即可

### 5.5 网格视图全屏叠加 vs 部分遮挡？

**决策**：网格视图**全屏叠加**（类似模态），而非底部悬浮条。

**理由**：
- 网格视图的核心价值是同时浏览所有页面缩略图，面积太小无法发挥作用
- 全屏叠加确保缩略图足够大，用户可以快速识别目标页面
- 退出方式明确：点击跳转或按 Escape/G 键关闭

---

## 6. 动画策略

### 6.1 缩略图网格视图

采用**淡入淡出**动画，而非 GoToFlow 的底部滑动：

```cpp
// 显示
auto *showAnim = new QPropertyAnimation(thumbnailGrid, "windowOpacity");
showAnim->setDuration(200);
showAnim->setStartValue(0.0);
showAnim->setEndValue(1.0);
thumbnailGrid->show();

// 隐藏
auto *hideAnim = new QPropertyAnimation(thumbnailGrid, "windowOpacity");
hideAnim->setDuration(150);
hideAnim->setStartValue(1.0);
hideAnim->setEndValue(0.0);
// hideAnim finished → thumbnailGrid->hide()
```

**注意**：`QWidget::windowOpacity` 会影响整个控件（包括子控件），但 `ThumbnailGridWidget` 作为独立叠加层是独立的顶级子控件，不会影响底层阅读区域。

**替代方案**（若 `windowOpacity` 效果不理想）：使用 `QGraphicsOpacityEffect`：

```cpp
auto *effect = new QGraphicsOpacityEffect(thumbnailGrid);
thumbnailGrid->setGraphicsEffect(effect);
// 对 effect->opacity() 做 QPropertyAnimation
```

### 6.2 GoToFlow 与 ThumbnailGrid 共享动画对象？

**决策**：各自维护独立动画对象。GoToFlow 已有 `showGoToFlowAnimation` 操作 `pos` 属性，ThumbnailGrid 使用单独的 `opacity` 属性动画，互不干扰。

---

## 7. 构建系统集成

### 7.1 CMakeLists.txt 变更

`YACReader/CMakeLists.txt` 中 `qt_add_executable(YACReader ...)` 添加：

```cmake
thumbnail_grid_widget.h
thumbnail_grid_widget.cpp
thumbnail_grid_toolbar.h
thumbnail_grid_toolbar.cpp
```

资源文件列表 `yacreader_image_files` 中添加：

```cmake
${PROJECT_SOURCE_DIR}/images/viewer_toolbar/thumbnailGrid.svg
${PROJECT_SOURCE_DIR}/images/viewer_toolbar/thumbnailGrid_18x18.svg
${PROJECT_SOURCE_DIR}/images/viewer_toolbar/slideshow.svg
${PROJECT_SOURCE_DIR}/images/viewer_toolbar/slideshow_18x18.svg
${PROJECT_SOURCE_DIR}/images/shortcuts/shortcuts_group_grid.svg
${PROJECT_SOURCE_DIR}/images/shortcuts/shortcuts_group_slideshow.svg
```

### 7.2 翻译

新增的用户可见字符串（Action 名、选项卡标题、通知文本等）均使用 `tr()` 包裹。实现完成后运行：

```bash
cmake --build build --target update_translations
```

更新 `.ts` 文件。

### 7.3 代码格式化

提交前运行：

```bash
scripts\clang-format-windows.cmd
```

---

## 8. 测试策略

| 测试类型 | 范围 | 方法 |
|---------|------|------|
| 手动功能测试 | 网格打开/关闭/跳转/高亮/键盘导航 | 打开漫画 → G 键 → 交互 → 验证 |
| 手动功能测试 | 幻灯片 启动/暂停/停止/调速/循环 | 打开漫画 → Shift+S → 翻页 → 验证 |
| 互斥测试 | GoToFlow 与 ThumbnailGrid 同时存在 | 打开 Flow → 按 G → 验证 Flow 关闭 |
| 边界测试 | 1 页漫画、空白状态 | 打开单页 CBR → 网格应显示单个缩略图 |
| 配置持久化 | 修改选项 → 重启 → 验证保留 | 修改网格列数和幻灯片间隔 → 重启 |
| 主题切换 | 切换主题 → 验证网格/幻灯片 UI 颜色 | 切换 Dark/Light 主题 |
| 内存泄漏 | 大量页面漫画 (500+ 页) | 打开 → 网格 → 关闭 → 检查内存 |