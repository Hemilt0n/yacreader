# YACReader 缩略图网格导航 & 幻灯片自动播放 — 详细设计文档

> 本文档基于 `docs/proposal.md`（需求）和 `docs/high-level-design.md`（概要设计），给出每个模块的完整类定义、方法签名、数据流、状态转换和修改点，确保模块间可独立开发与测试。

---

## 1. 模块划分与依赖关系

```
                    ┌─────────────────────┐
                    │  Configuration (1)  │  ← 纯数据，无依赖
                    ├─────────────────────┤
                    │  ShortcutsManager (2)│  ← 纯数据，无依赖
                    ├─────────────────────┤
                    │  Theme 扩展 (3)      │  ← 纯数据，无依赖
                    └──────────┬──────────┘
                               │ 读取配置/主题
                    ┌──────────▼──────────┐
                    │ ThumbnailGridToolBar │  ← 依赖 Qt Widgets + Themable
                    │        (4)           │
                    ├──────────────────────┤
                    │ ThumbnailGridWidget  │  ← 依赖 (4) + Themable
                    │        (5)           │
                    ├──────────────────────┤
                    │ SlideshowController  │  ← 纯逻辑，无 UI 依赖
                    │        (6)           │
                    └──────────┬───────────┘
                               │ 组装
                    ┌──────────▼───────────┐
                    │ Viewer 扩展 (7)      │  ← 依赖 (5)(6) + 已有代码
                    ├──────────────────────┤
                    │ MainWindowViewer 扩展│  ← 依赖 (2)(7) + 已有代码
                    │        (8)           │
                    ├──────────────────────┤
                    │ OptionsDialog 扩展   │  ← 依赖 (1)(3)
                    │        (9)           │
                    └──────────────────────┘
```

**编号越小，依赖越少，可越早开始实现和独立测试。**

---

## 2. 模块 1：Configuration 扩展

### 2.1 修改文件

- `common/yacreader_global_gui.h`
- `YACReader/configuration.h` (仅 .h，getter/setter 全部内联)

### 2.2 新增常量

`common/yacreader_global_gui.h` 尾部（`namespace YACReader` 闭合花括号之前）新增：

```cpp
#define THUMBNAIL_GRID_COLUMNS    "THUMBNAIL_GRID_COLUMNS"
#define THUMBNAIL_GRID_SIZE       "THUMBNAIL_GRID_SIZE"
#define SLIDESHOW_INTERVAL        "SLIDESHOW_INTERVAL"
#define SLIDESHOW_LOOP            "SLIDESHOW_LOOP"
```

### 2.3 Configuration 类扩展

在 `YACReader/configuration.h` 的 `Configuration` 类 `public:` 段（`setShowTimeInInformation` 之后）新增：

```cpp
int getThumbnailGridColumns() { return settings->value(THUMBNAIL_GRID_COLUMNS, 0).toInt(); }
void setThumbnailGridColumns(int c) { settings->setValue(THUMBNAIL_GRID_COLUMNS, c); }
QSize getThumbnailGridSize() { return settings->value(THUMBNAIL_GRID_SIZE, QSize(150, 200)).toSize(); }
void setThumbnailGridSize(const QSize &s) { settings->setValue(THUMBNAIL_GRID_SIZE, s); }
qreal getSlideshowInterval() { return settings->value(SLIDESHOW_INTERVAL, 3.0).toReal(); }
void setSlideshowInterval(qreal i) { settings->setValue(SLIDESHOW_INTERVAL, i); }
bool getSlideshowLoop() { return settings->value(SLIDESHOW_LOOP, false).toBool(); }
void setSlideshowLoop(bool l) { settings->setValue(SLIDESHOW_LOOP, l); }
```

### 2.4 独立测试方式

- 单元测试：直接 `Configuration::getConfiguration()` 读写，验证 INI 持久化。
- 无需 GUI 事件循环即可测试。

---

## 3. 模块 2：ShortcutsManager 扩展

### 3.1 修改文件

- `shortcuts_management/shortcuts_manager.h`
- `shortcuts_management/shortcuts_manager.cpp`

### 3.2 新增 Action ID

`shortcuts_manager.h` 尾部（`#endif` 之前）新增：

```cpp
#define SHOW_THUMBNAIL_GRID_ACTION_Y  "SHOW_THUMBNAIL_GRID_ACTION_Y"
#define SLIDESHOW_TOGGLE_ACTION_Y     "SLIDESHOW_TOGGLE_ACTION_Y"
#define SLIDESHOW_FASTER_ACTION_Y      "SLIDESHOW_FASTER_ACTION_Y"
#define SLIDESHOW_SLOWER_ACTION_Y      "SLIDESHOW_SLOWER_ACTION_Y"
```

### 3.3 默认快捷键

`shortcuts_manager.cpp` 的 `initDefaultShorcuts()` 中 `#else` 段（YACReader 区块，`GO_TO_PAGE_ACTION_Y` 之后）新增：

```cpp
defaultShorcuts.insert(SHOW_THUMBNAIL_GRID_ACTION_Y, Qt::Key_G);
defaultShorcuts.insert(SLIDESHOW_TOGGLE_ACTION_Y, Qt::SHIFT | Qt::Key_S);
defaultShorcuts.insert(SLIDESHOW_FASTER_ACTION_Y, Qt::SHIFT | Qt::Key_Up);
defaultShorcuts.insert(SLIDESHOW_SLOWER_ACTION_Y, Qt::SHIFT | Qt::Key_Down);
```

**同时修改** `GO_TO_PAGE_ACTION_Y` 的默认快捷键：

```cpp
// 修改前:
defaultShorcuts.insert(GO_TO_PAGE_ACTION_Y, Qt::Key_G);
// 修改后:
defaultShorcuts.insert(GO_TO_PAGE_ACTION_Y, Qt::CTRL | Qt::Key_G);
```

### 3.4 独立测试方式

- 调用 `ShortcutsManager::getShortcut(SHOW_THUMBNAIL_GRID_ACTION_Y)` 验证返回默认 `QKeySequence("G")`。
- 验证已有 `GO_TO_PAGE` 快捷键用户自定义不受影响（INI 覆盖机制）。

---

## 4. 模块 3：Theme 扩展

### 4.1 修改文件

- `YACReader/themes/theme.h`
- `YACReader/themes/theme_factory.h`
- `YACReader/themes/theme_factory.cpp`

### 4.2 Theme 结构体扩展

`theme.h` 新增：

```cpp
struct ThumbnailGridThemeTemplates {
    QString labelQSS = "QLabel { color: %1; font-size: %2px; }";
    QString borderQSS = "QLabel { border: 2px solid %1; border-radius: 3px; }";
};

struct ThumbnailGridTheme {
    QColor backgroundColor;
    QColor textColor;
    QColor highlightColor;
    QColor selectionColor;
    QColor borderColor;
    int thumbnailSpacing = 8;
    QString labelQSS;
    QString borderQSS;
    QIcon centerIcon;
    QIcon goToIcon;
};

struct SlideshowTheme {
    QColor overlayBackgroundColor;
    QString statusQSS;
};
```

在 `Theme` 结构体中新增字段（在 `goToFlowWidget` 之后、`helpAboutDialog` 之前）：

```cpp
ThumbnailGridTheme thumbnailGrid;
SlideshowTheme slideshow;
```

在 `ShortcutsIconsTheme` 中新增：

```cpp
QIcon thumbnailGridIcon;
QIcon slideshowIcon;
```

在 `ToolbarTheme` 中新增（在 `showFlowAction18x18` 之后）：

```cpp
QIcon showThumbnailGridAction;
QIcon showThumbnailGridAction18x18;
QIcon slideshowToggleAction;
QIcon slideshowToggleAction18x18;
```

### 4.3 ThemeParams 扩展

`theme_factory.cpp` 新增参数结构体：

```cpp
struct ThumbnailGridParams {
    ThumbnailGridThemeTemplates t;
    QColor backgroundColor { 0x1a, 0x1a, 0x1a };
    QColor textColor { Qt::white };
    QColor highlightColor { 0x4c, 0xaf, 0x50 };   // 绿色 高亮当前阅读页
    QColor selectionColor { 0x42, 0xa5, 0xf5 };     // 蓝色 键盘焦点
    QColor borderColor { 0x66, 0x66, 0x66 };
    QColor iconColor { 0xcc, 0xcc, 0xcc };
};

struct SlideshowParams {
    QColor overlayBackgroundColor { 0x22, 0x22, 0x22 };
    QColor statusTextColor { Qt::white };
};
```

在 `ThemeParams` 中新增：

```cpp
ThumbnailGridParams thumbnailGridParams;
SlideshowParams slideshowParams;
```

### 4.4 makeTheme 函数扩展

在 `makeTheme(const ThemeParams &params)` 中新增（GoToFlowWidget 段之后）：

```cpp
auto &gridP = params.thumbnailGridParams;
theme.thumbnailGrid.backgroundColor = gridP.backgroundColor;
theme.thumbnailGrid.textColor = gridP.textColor;
theme.thumbnailGrid.highlightColor = gridP.highlightColor;
theme.thumbnailGrid.selectionColor = gridP.selectionColor;
theme.thumbnailGrid.borderColor = gridP.borderColor;
theme.thumbnailGrid.labelQSS = gridP.t.labelQSS.arg(gridP.textColor.name());
theme.thumbnailGrid.borderQSS = gridP.t.borderQSS.arg(gridP.borderColor.name());

const QString gridCenterIconPath = recoloredSvgToThemeFile(":/images/centerFlow.svg", gridP.iconColor, params.meta.id);
const QString gridGoToIconPath = recoloredSvgToThemeFile(":/images/gotoFlow.svg", gridP.iconColor, params.meta.id);
theme.thumbnailGrid.centerIcon = QIcon(gridCenterIconPath);
theme.thumbnailGrid.goToIcon = QIcon(gridGoToIconPath);

auto &slideP = params.slideshowParams;
theme.slideshow.overlayBackgroundColor = slideP.overlayBackgroundColor;
theme.slideshow.statusQSS = QString("QLabel { color: %1; font-size: 14px; font-weight: bold; }")
                                     .arg(slideP.statusTextColor.name());

setToolbarIconPairT(theme.toolbar.showThumbnailGridAction, theme.toolbar.showThumbnailGridAction18x18,
                    ":/images/viewer_toolbar/thumbnailGrid.svg");
setToolbarIconPairT(theme.toolbar.slideshowToggleAction, theme.toolbar.slideshowToggleAction18x18,
                    ":/images/viewer_toolbar/slideshow.svg");

theme.shortcutsIcons.thumbnailGridIcon = makeShortcutsIcon(":/images/shortcuts/shortcuts_group_grid.svg");
theme.shortcutsIcons.slideshowIcon = makeShortcutsIcon(":/images/shortcuts/shortcuts_group_slideshow.svg");
```

### 4.5 JSON 解析扩展

在 `makeTheme(const QJsonObject &json)` 中新增（`goToFlowWidget` 段之后）：

```cpp
if (json.contains("thumbnailGrid")) {
    const auto g = json["thumbnailGrid"].toObject();
    auto &gp = p.thumbnailGridParams;
    gp.backgroundColor = colorFromJson(g, "backgroundColor", gp.backgroundColor);
    gp.textColor = colorFromJson(g, "textColor", gp.textColor);
    gp.highlightColor = colorFromJson(g, "highlightColor", gp.highlightColor);
    gp.selectionColor = colorFromJson(g, "selectionColor", gp.selectionColor);
    gp.borderColor = colorFromJson(g, "borderColor", gp.borderColor);
    gp.iconColor = colorFromJson(g, "iconColor", gp.iconColor);
}

if (json.contains("slideshow")) {
    const auto s = json["slideshow"].toObject();
    auto &sp = p.slideshowParams;
    sp.overlayBackgroundColor = colorFromJson(s, "overlayBackgroundColor", sp.overlayBackgroundColor);
    sp.statusTextColor = colorFromJson(s, "statusTextColor", sp.statusTextColor);
}
```

### 4.6 独立测试方式

- 创建 `Theme` 结构体，设置 `thumbnailGrid` 字段后，传给 `ThumbnailGridWidget::applyTheme()` 验证无崩溃。
- 加载现有主题 JSON 文件，确保不含 `thumbnailGrid`/`slideshow` 键时走默认值路径。

---

## 5. 模块 4：ThumbnailGridToolBar

### 5.1 新增文件

- `YACReader/thumbnail_grid_toolbar.h`
- `YACReader/thumbnail_grid_toolbar.cpp`

### 5.2 完整类定义

```cpp
// thumbnail_grid_toolbar.h
#ifndef THUMBNAIL_GRID_TOOLBAR_H
#define THUMBNAIL_GRID_TOOLBAR_H

#include "themable.h"

#include <QWidget>

class QLineEdit;
class QSlider;
class QIntValidator;
class QPushButton;
class QLabel;

class ThumbnailGridToolBar : public QWidget, protected Themable
{
    Q_OBJECT

public:
    explicit ThumbnailGridToolBar(QWidget *parent = nullptr);

public slots:
    void setPage(int pageNumber);
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
    QLabel *pageLabel;
};

#endif
```

### 5.3 实现要点

```cpp
// thumbnail_grid_toolbar.cpp 关键实现

ThumbnailGridToolBar::ThumbnailGridToolBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    pageLabel = new QLabel(tr("Page:"), this);
    edit = new QLineEdit(this);
    edit->setValidator(v = new QIntValidator(1, 1, this));
    edit->setMaximumWidth(50);
    edit->setAlignment(Qt::AlignCenter);

    slider = new QSlider(Qt::Horizontal, this);
    slider->setMinimum(0);

    centerButton = new QPushButton(this);
    goToButton = new QPushButton(this);

    layout->addWidget(pageLabel);
    layout->addWidget(edit);
    layout->addWidget(slider, 1);
    layout->addWidget(centerButton);
    layout->addWidget(goToButton);

    connect(slider, &QSlider::valueChanged, this, [this](int value) {
        emit setCenter(static_cast<unsigned int>(value));
        setPage(value);
    });
    connect(edit, &QLineEdit::returnPressed, this, &ThumbnailGridToolBar::goTo);
    connect(centerButton, &QPushButton::clicked, this, &ThumbnailGridToolBar::centerSlide);
    connect(goToButton, &QPushButton::clicked, this, &ThumbnailGridToolBar::goTo);

    setFixedHeight(36);
    initTheme(this);
}

void ThumbnailGridToolBar::setPage(int pageNumber)
{
    edit->setText(QString::number(pageNumber + 1));
    slider->setValue(pageNumber);
}

void ThumbnailGridToolBar::setTop(int numPages)
{
    v->setTop(numPages);
    slider->setMaximum(numPages - 1);
}

void ThumbnailGridToolBar::goTo()
{
    unsigned int page = edit->text().toInt();
    if (page >= 1 && page <= static_cast<unsigned int>(v->top()))
        emit goToPage(page - 1);
}

void ThumbnailGridToolBar::centerSlide()
{
    unsigned int page = edit->text().toInt();
    if (page >= 1 && page <= static_cast<unsigned int>(v->top()))
        emit setCenter(page - 1);
}

void ThumbnailGridToolBar::updateOptions()
{
}

void ThumbnailGridToolBar::applyTheme(const Theme &theme)
{
    auto gridTheme = theme.thumbnailGrid;
    QPalette pal = palette();
    pal.setColor(QPalette::Window, gridTheme.backgroundColor);
    pal.setColor(QPalette::WindowText, gridTheme.textColor);
    setPalette(pal);
    setAutoFillBackground(true);
    edit->setStyleSheet(gridTheme.editQSS.isEmpty() ? theme.goToFlowWidget.editQSS : gridTheme.editQSS);
    slider->setStyleSheet(gridTheme.sliderQSS.isEmpty() ? theme.goToFlowWidget.sliderQSS : gridTheme.sliderQSS);
    pageLabel->setStyleSheet(gridTheme.labelQSS.isEmpty() ? theme.goToFlowWidget.labelQSS : gridTheme.labelQSS);
    centerButton->setIcon(gridTheme.centerIcon.isNull() ? theme.goToFlowWidget.centerIcon : gridTheme.centerIcon);
    centerButton->setStyleSheet(theme.goToFlowWidget.buttonQSS);
    goToButton->setIcon(gridTheme.goToIcon.isNull() ? theme.goToFlowWidget.goToIcon : gridTheme.goToIcon);
    goToButton->setStyleSheet(theme.goToFlowWidget.buttonQSS);
}
```

**与 GoToFlowToolBar 的差异**：
- 继承 `QWidget` 而非 `QStackedWidget`（不需要 normal/quickNavi 双模式）
- 水平布局而非竖向堆叠
- `applyTheme` 中优先使用 `thumbnailGrid` 主题字段，回退到 `goToFlowWidget` 字段

### 5.4 独立测试方式

- 创建 `ThumbnailGridToolBar` 实例，设置 `setTop(100)` → `setPage(50)` → 验证 lineEdit 显示 "51"、slider 值为 50。
- 点击 goToButton → 验证 `goToPage(50)` 信号发射。
- 纯控件测试，无需 Render/Viewer 依赖。

---

## 6. 模块 5：ThumbnailGridWidget

### 6.1 新增文件

- `YACReader/thumbnail_grid_widget.h`
- `YACReader/thumbnail_grid_widget.cpp`

### 6.2 完整类定义

```cpp
// thumbnail_grid_widget.h
#ifndef THUMBNAIL_GRID_WIDGET_H
#define THUMBNAIL_GRID_WIDGET_H

#include "themable.h"

#include <QHash>
#include <QSize>
#include <QVector>
#include <QWidget>

class QGridLayout;
class QScrollArea;
class QLabel;
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
    void centerSlide(int slide);
    void setPageNumber(int page);
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
    struct ThumbnailItem {
        QWidget *cellWidget = nullptr;
        QLabel *imageLabel = nullptr;
        QLabel *pageLabel = nullptr;
        int pageIndex = -1;
        bool imageLoaded = false;
    };

    struct ScaledCacheEntry {
        qint64 sourceCacheKey = 0;
        QSize sourceSize;
        QImage scaledImage;
    };

    QScrollArea *scrollArea = nullptr;
    QWidget *gridContainer = nullptr;
    QGridLayout *gridLayout = nullptr;
    ThumbnailGridToolBar *toolBar = nullptr;

    QVector<ThumbnailItem> items;
    QHash<int, QByteArray> rawImages;
    QVector<bool> imagesReady;
    QHash<int, ScaledCacheEntry> scaledCache;

    int currentHighlightIndex = -1;
    int currentFocusIndex = -1;
    int totalPages = 0;
    QSize thumbnailSize{ 150, 200 };
    int columnCount = 0;
    bool flowRightToLeft = false;

    void buildGrid();
    void clearGrid();
    void scrollToPage(int index);
    void updateItemHighlight(int oldIndex, int newIndex);
    void updateItemSelection(int oldIndex, int newIndex);
    int computeColumnCount() const;
    QWidget *createThumbnailCell(int pageIndex);
    void displayThumbnail(int index);
};

#endif
```

### 6.3 关键方法实现

#### 6.3.1 构造函数

```cpp
ThumbnailGridWidget::ThumbnailGridWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);

    gridContainer = new QWidget;
    gridLayout = new QGridLayout(gridContainer);
    gridLayout->setSpacing(8);
    gridLayout->setContentsMargins(8, 8, 8, 8);
    gridContainer->setLayout(gridLayout);

    scrollArea->setWidget(gridContainer);
    mainLayout->addWidget(scrollArea, 1);

    toolBar = new ThumbnailGridToolBar(this);
    mainLayout->addWidget(toolBar);

    setLayout(mainLayout);

    connect(toolBar, &ThumbnailGridToolBar::goToPage, this, &ThumbnailGridWidget::goToPage);
    connect(toolBar, &ThumbnailGridToolBar::setCenter, this, [this](unsigned int page) {
        centerSlide(static_cast<int>(page));
    });

    setFocusPolicy(Qt::StrongFocus);
    hide();

    initTheme(this);
}
```

#### 6.3.2 setNumSlides / buildGrid

```cpp
void ThumbnailGridWidget::setNumSlides(unsigned int slides)
{
    clearGrid();
    totalPages = static_cast<int>(slides);
    imagesReady.resize(slides);
    imagesReady.fill(false);
    rawImages.clear();
    scaledCache.clear();
    buildGrid();
    toolBar->setTop(slides);
}

void ThumbnailGridWidget::buildGrid()
{
    if (totalPages == 0)
        return;

    columnCount = computeColumnCount();

    for (int i = 0; i < totalPages; ++i) {
        QWidget *cell = createThumbnailCell(i);
        int row = i / columnCount;
        int col = flowRightToLeft ? (columnCount - 1 - i % columnCount) : (i % columnCount);
        gridLayout->addWidget(cell, row, col);
        items[i].cellWidget = cell;
    }

    if (currentHighlightIndex >= 0 && currentHighlightIndex < totalPages)
        updateItemHighlight(-1, currentHighlightIndex);
}

int ThumbnailGridWidget::computeColumnCount() const
{
    int configuredCols = Configuration::getConfiguration().getThumbnailGridColumns();
    if (configuredCols > 0)
        return configuredCols;
    int availableWidth = scrollArea->viewport()->width() - gridLayout->contentsMargins().left() - gridLayout->contentsMargins().right();
    int cols = availableWidth / (thumbnailSize.width() + gridLayout->spacing());
    return qMax(1, cols);
}
```

#### 6.3.3 createThumbnailCell — 单元格创建

```cpp
QWidget *ThumbnailGridWidget::createThumbnailCell(int pageIndex)
{
    auto *cell = new QWidget;
    auto *cellLayout = new QVBoxLayout(cell);
    cellLayout->setContentsMargins(2, 2, 2, 2);
    cellLayout->setSpacing(2);
    cellLayout->setAlignment(Qt::AlignCenter);

    auto *imageLabel = new QLabel;
    imageLabel->setFixedSize(thumbnailSize);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setStyleSheet("QLabel { background-color: #2a2a2a; border: 1px solid #444; border-radius: 3px; }");

    auto *pageLabel = new QLabel(QString::number(pageIndex + 1));
    pageLabel->setAlignment(Qt::AlignCenter);

    cellLayout->addWidget(imageLabel);
    cellLayout->addWidget(pageLabel);

    items[pageIndex].imageLabel = imageLabel;
    items[pageIndex].pageLabel = pageLabel;
    items[pageIndex].pageIndex = pageIndex;

    cell->setCursor(Qt::PointingHandCursor);
    cell->installEventFilter(this); // 点击事件在 eventFilter 中处理

    return cell;
}
```

#### 6.3.4 setImageReady — 渐进式填充

```cpp
void ThumbnailGridWidget::setImageReady(int index, const QByteArray &image)
{
    if (index < 0 || index >= totalPages || items.isEmpty())
        return;

    rawImages[index] = image;
    imagesReady[index] = true;
    displayThumbnail(index);
}

void ThumbnailGridWidget::displayThumbnail(int index)
{
    if (!items[index].imageLabel)
        return;

    QImage img;
    const QByteArray &rawData = rawImages[index];
    if (!rawData.isEmpty()) {
        img.loadFromData(rawData);
    }
    if (img.isNull()) {
        items[index].imageLabel->setText(tr("Page %1").arg(index + 1));
        return;
    }

    img = img.scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    items[index].imageLabel->setPixmap(QPixmap::fromImage(img));
    items[index].imageLoaded = true;
    items[index].imageLabel->setStyleSheet(
            items[index].imageLabel->styleSheet().replace("background-color: #2a2a2a;", ""));
}
```

#### 6.3.5 highlightPage / centerSlide

```cpp
void ThumbnailGridWidget::highlightPage(int pageIndex)
{
    int oldIndex = currentHighlightIndex;
    currentHighlightIndex = pageIndex;
    updateItemHighlight(oldIndex, pageIndex);
}

void ThumbnailGridWidget::updateItemHighlight(int oldIndex, int newIndex)
{
    auto applyStyle = [&](int idx, const QString &style) {
        if (idx >= 0 && idx < totalPages && items[idx].imageLabel)
            items[idx].imageLabel->setStyleSheet(style);
    };

    QString normalStyle = theme.thumbnailGrid.borderQSS.arg(
            theme.thumbnailGrid.borderColor.name());
    QString highlightStyle = theme.thumbnailGrid.borderQSS.arg(
            theme.thumbnailGrid.highlightColor.name());

    applyStyle(oldIndex, normalStyle);
    applyStyle(newIndex, highlightStyle);
}

void ThumbnailGridWidget::centerSlide(int slide)
{
    if (slide < 0 || slide >= totalPages || items.isEmpty())
        return;

    QWidget *cell = items[slide].cellWidget;
    if (cell)
        scrollArea->ensureWidgetVisible(cell, 0, 0);

    setPageNumber(slide);
}
```

#### 6.3.6 键盘导航

```cpp
void ThumbnailGridWidget::keyPressEvent(QKeyEvent *event)
{
    if (totalPages == 0) {
        QWidget::keyPressEvent(event);
        return;
    }

    int oldFocus = currentFocusIndex;

    switch (event->key()) {
    case Qt::Key_Left:
        currentFocusIndex = qMax(0, currentFocusIndex - 1);
        break;
    case Qt::Key_Right:
        currentFocusIndex = qMin(totalPages - 1, currentFocusIndex + 1);
        break;
    case Qt::Key_Up:
        currentFocusIndex = qMax(0, currentFocusIndex - columnCount);
        break;
    case Qt::Key_Down:
        currentFocusIndex = qMin(totalPages - 1, currentFocusIndex + columnCount);
        break;
    case Qt::Key_Return:
    case Qt::Key_Space:
        if (currentFocusIndex >= 0 && currentFocusIndex < totalPages)
            emit goToPage(static_cast<unsigned int>(currentFocusIndex));
        event->accept();
        return;
    case Qt::Key_Escape:
    case Qt::Key_G:
    case Qt::Key_S:
        QCoreApplication::sendEvent(parent(), event);
        return;
    default:
        QWidget::keyPressEvent(event);
        return;
    }

    if (currentFocusIndex < 0)
        currentFocusIndex = 0;

    updateItemSelection(oldFocus, currentFocusIndex);
    centerSlide(currentFocusIndex);
    event->accept();
}

void ThumbnailGridWidget::updateItemSelection(int oldIndex, int newIndex)
{
    QString normalStyle = theme.thumbnailGrid.borderQSS.arg(
            theme.thumbnailGrid.borderColor.name());
    QString selectionStyle = theme.thumbnailGrid.borderQSS.arg(
            theme.thumbnailGrid.selectionColor.name());

    if (oldIndex >= 0 && oldIndex < totalPages && items[oldIndex].imageLabel)
        items[oldIndex].imageLabel->setStyleSheet(normalStyle);
    if (newIndex >= 0 && newIndex < totalPages && items[newIndex].imageLabel)
        items[newIndex].imageLabel->setStyleSheet(selectionStyle);

    if (newIndex >= 0 && newIndex < totalPages && items[newIndex].cellWidget)
        scrollArea->ensureWidgetVisible(items[newIndex].cellWidget, 0, 0);
}
```

#### 6.3.7 resizeEvent

```cpp
void ThumbnailGridWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateSize();
}

void ThumbnailGridWidget::updateSize()
{
    if (parentWidget() == nullptr)
        return;

    int newCols = computeColumnCount();
    if (newCols != columnCount && totalPages > 0) {
        clearGrid();
        buildGrid();
        if (!rawImages.isEmpty()) {
            for (int i = 0; i < totalPages; ++i) {
                if (imagesReady.value(i, false))
                    displayThumbnail(i);
            }
        }
    }

    toolBar->setFixedWidth(width());
}
```

#### 6.3.8 事件过滤器（处理缩略图点击）

```cpp
// 在类声明中添加:
// bool eventFilter(QObject *watched, QEvent *event) override;

bool ThumbnailGridWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            for (int i = 0; i < items.size(); ++i) {
                if (items[i].cellWidget == watched) {
                    emit goToPage(static_cast<unsigned int>(i));
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
```

### 6.4 独立测试方式

- **widget 独立测试**：创建 `ThumbnailGridWidget` 实例（父窗口为 `QWidget`），调用 `setNumSlides(10)` → 验证 10 个占位单元格出现。逐个调用 `setImageReady(i, data)` → 验证缩略图更新。
- **信号测试**：连接 `goToPage` 信号到 lambda，点击单元格 → 验证信号参数。
- **键盘测试**：模拟按键事件 → 验证 `currentFocusIndex` 变化。
- **不需要 Viewer/Render**。

---

## 7. 模块 6：SlideshowController

**设计变更**：虽然概要设计中将幻灯片逻辑嵌入 Viewer，但为满足"独立可测试"的要求，提取为独立类。该类仅封装状态机和定时器，不持有任何 UI 引用。

### 7.1 新增文件

- `YACReader/slideshow_controller.h`
- `YACReader/slideshow_controller.cpp`

### 7.2 完整类定义

```cpp
// slideshow_controller.h
#ifndef SLIDESHOW_CONTROLLER_H
#define SLIDESHOW_CONTROLLER_H

#include <QElapsedTimer>
#include <QObject>

class QTimer;

class SlideshowController : public QObject
{
    Q_OBJECT

public:
    enum State {
        Stopped,
        Playing,
        Paused
    };
    Q_ENUM(State)

    explicit SlideshowController(QObject *parent = nullptr);
    ~SlideshowController() override;

    State state() const;
    qreal interval() const;
    bool loop() const;

    void setInterval(qreal seconds);
    void setLoop(bool loop);

public slots:
    void toggle();
    void stop();
    void faster();
    void slower();

signals:
    void stateChanged(State newState);
    void advancePage();
    void intervalChanged(qreal newInterval);

private slots:
    void onTimeout();

private:
    QTimer *timer;
    State mState = Stopped;
    qreal mInterval = 3.0;
    bool mLoop = false;

    static constexpr qreal MIN_INTERVAL = 0.5;
    static constexpr qreal MAX_INTERVAL = 30.0;
    static constexpr qreal INTERVAL_STEP = 0.5;

    void startTimer();
    void stopTimer();
};

#endif
```

### 7.3 实现

```cpp
// slideshow_controller.cpp
#include "slideshow_controller.h"
#include "configuration.h"

#include <QTimer>

SlideshowController::SlideshowController(QObject *parent)
    : QObject(parent)
    , timer(new QTimer(this))
{
    mInterval = Configuration::getConfiguration().getSlideshowInterval();
    mLoop = Configuration::getConfiguration().getSlideshowLoop();
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this, &SlideshowController::onTimeout);
}

SlideshowController::~SlideshowController() = default;

SlideshowController::State SlideshowController::state() const { return mState; }
qreal SlideshowController::interval() const { return mInterval; }
bool SlideshowController::loop() const { return mLoop; }

void SlideshowController::setInterval(qreal seconds)
{
    mInterval = qBound(MIN_INTERVAL, seconds, MAX_INTERVAL);
    Configuration::getConfiguration().setSlideshowInterval(mInterval);
    emit intervalChanged(mInterval);

    if (mState == Playing) {
        timer->setInterval(qRound(mInterval * 1000));
    }
}

void SlideshowController::setLoop(bool loop)
{
    mLoop = loop;
    Configuration::getConfiguration().setSlideshowLoop(mLoop);
}

void SlideshowController::toggle()
{
    switch (mState) {
    case Stopped:
    case Paused:
        mState = Playing;
        startTimer();
        break;
    case Playing:
        mState = Paused;
        stopTimer();
        break;
    }
    emit stateChanged(mState);
}

void SlideshowController::stop()
{
    if (mState == Stopped)
        return;
    mState = Stopped;
    stopTimer();
    emit stateChanged(mState);
}

void SlideshowController::faster()
{
    setInterval(mInterval - INTERVAL_STEP);
}

void SlideshowController::slower()
{
{
    setInterval(mInterval + INTERVAL_STEP);
}

void SlideshowController::onTimeout()
{
    emit advancePage();
}

void SlideshowController::startTimer()
{
    timer->start(qRound(mInterval * 1000));
}

void SlideshowController::stopTimer()
{
    timer->stop();
}
```

### 7.4 独立测试方式

- **纯逻辑测试**：创建 `SlideshowController`，验证状态转换：`toggle()` → `Playing`→`Paused`→`Playing`→`Paused`...；`stop()` 从任何状态 → `Stopped`。
- **信号测试**：连接 `advancePage` 信号计数器，验证间隔时间后触发次数。
- **不需要 Viewer/Render/Widget**，可在 Qt Test 框架中独立运行。

---

## 8. 模块 7：Viewer 扩展

### 8.1 修改文件

- `YACReader/viewer.h`
- `YACReader/viewer.cpp`

### 8.2 viewer.h 新增内容

在 `#include` 区新增：

```cpp
#include "slideshow_controller.h"

class ThumbnailGridWidget;
```

在 `private:` 段新增（在 `GoToFlowWidget *goToFlow;` 之后）：

```cpp
ThumbnailGridWidget *thumbnailGrid;
QPropertyAnimation *showThumbnailGridAnimation;
SlideshowController *slideshowController;
```

在 `public slots:` 段新增：

```cpp
void thumbnailGridSwitch();
void showThumbnailGrid();
void animateShowThumbnailGrid();
void animateHideThumbnailGrid();
void slideshowToggle();
void slideshowStop();
void slideshowFaster();
void slideshowSlower();
```

在 `private slots:` 段新增：

```cpp
void onSlideshowStateChanged(SlideshowController::State newState);
void onSlideshowAdvancePage();
void onSlideshowIntervalChanged(qreal newInterval);
```

在 `private:` 段新增：

```cpp
bool slideshowWasActiveBeforeGrid = false;
void setupSlideshowController();
void showSlideshowNotification(const QString &text);
```

### 8.3 viewer.cpp 修改

#### 8.3.1 构造函数新增（`createConnections()` 之前）

```cpp
thumbnailGrid = new ThumbnailGridWidget(this);
thumbnailGrid->hide();
showThumbnailGridAnimation = new QPropertyAnimation(thumbnailGrid, "windowOpacity");
showThumbnailGridAnimation->setDuration(200);

slideshowController = new SlideshowController(this);
setupSlideshowController();
```

#### 8.3.2 析构函数新增

```cpp
delete thumbnailGrid;
delete slideshowController;
// 注意：showThumbnailGridAnimation 作为 thumbnailGrid 的子对象会自动删除
```

#### 8.3.3 setupSlideshowController 方法

```cpp
void Viewer::setupSlideshowController()
{
    connect(slideshowController, &SlideshowController::stateChanged,
            this, &Viewer::onSlideshowStateChanged);
    connect(slideshowController, &SlideshowController::advancePage,
            this, &Viewer::onSlideshowAdvancePage);
    connect(slideshowController, &SlideshowController::intervalChanged,
            this, &Viewer::onSlideshowIntervalChanged);
}

void Viewer::onSlideshowStateChanged(SlideshowController::State newState)
{
    switch (newState) {
    case SlideshowController::Playing:
        showSlideshowNotification(tr("\342\226\266 Auto %1s").arg(slideshowController->interval(), 0, 'f', 1));
        break;
    case SlideshowController::Paused:
        showSlideshowNotification(tr("\342\217\270 Auto paused"));
        break;
    case SlideshowController::Stopped:
        showSlideshowNotification(tr("\342\217\271 Auto stopped"));
        break;
    }
}

void Viewer::onSlideshowAdvancePage()
{
    if (!render->hasLoadedComic())
        return;

    if (render->getIndex() >= render->numPages() - 1) {
        if (slideshowController->loop()) {
            goTo(0);
        } else {
            slideshowController->stop();
        }
    } else {
        next();
    }
}

void Viewer::onSlideshowIntervalChanged(qreal newInterval)
{
    if (slideshowController->state() == SlideshowController::Playing) {
        showSlideshowNotification(tr("Auto: %1s").arg(newInterval, 0, 'f', 1));
    }
}

void Viewer::showSlideshowNotification(const QString &text)
{
    if (notificationsLabel) {
        notificationsLabel->setText(text);
        notificationsLabel->flash();
    }
}
```

> **注意**：`"▶"` U+25B6, `"⏸"` U+23F8, `"⏹"` U+23F9 是需求文档中规定的标识符，此处用 UTF-8 八进制转义以避免编码问题。

#### 8.3.4 叠加层调度方法

```cpp
void Viewer::thumbnailGridSwitch()
{
    thumbnailGrid->isVisible() ? animateHideThumbnailGrid() : showThumbnailGrid();
}

void Viewer::showThumbnailGrid()
{
    if (render->hasLoadedComic()) {
        animateShowThumbnailGrid();
    }
}

void Viewer::animateShowThumbnailGrid()
{
    if (thumbnailGrid->isHidden() && showThumbnailGridAnimation->state() != QPropertyAnimation::Running) {
        if (goToFlow->isVisible())
            animateHideGoToFlow();

        slideshowWasActiveBeforeGrid = (slideshowController->state() == SlideshowController::Playing);
        if (slideshowWasActiveBeforeGrid)
            slideshowController->toggle(); // 暂停

        disconnect(showThumbnailGridAnimation, &QAbstractAnimation::finished, thumbnailGrid, &QWidget::hide);
        showThumbnailGridAnimation->setStartValue(0.0);
        showThumbnailGridAnimation->setEndValue(1.0);
        showThumbnailGridAnimation->start();
        thumbnailGrid->show();
        thumbnailGrid->setPageNumber(render->getIndex());
        thumbnailGrid->centerSlide(render->getIndex());
        thumbnailGrid->setFocus(Qt::OtherFocusReason);
    }
}

void Viewer::animateHideThumbnailGrid()
{
    if (thumbnailGrid->isVisible() && showThumbnailGridAnimation->state() != QPropertyAnimation::Running) {
        connect(showThumbnailGridAnimation, &QAbstractAnimation::finished, thumbnailGrid, &QWidget::hide);
        showThumbnailGridAnimation->setStartValue(1.0);
        showThumbnailGridAnimation->setEndValue(0.0);
        showThumbnailGridAnimation->start();
        thumbnailGrid->setFocus(Qt::OtherFocusReason);

        if (slideshowWasActiveBeforeGrid)
            slideshowController->toggle(); // 恢复播放

        this->setFocus(Qt::OtherFocusReason);
    }
}
```

#### 8.3.5 幻灯片槽方法

```cpp
void Viewer::slideshowToggle()
{
    slideshowController->toggle();
}

void Viewer::slideshowStop()
{
    slideshowController->stop();
}

void Viewer::slideshowFaster()
{
    slideshowController->faster();
}

void Viewer::slideshowSlower()
{
    slideshowController->slower();
}
```

#### 8.3.6 createConnections 新增

在 `Viewer::createConnections()` 末尾（`connect(render, &Render::bookmarksUpdated, ...)` 之后）新增：

```cpp
connect(render, QOverload<unsigned int>::of(&Render::numPages),
        thumbnailGrid, &ThumbnailGridWidget::setNumSlides);
connect(render, QOverload<int, const QByteArray &>::of(&Render::imageLoaded),
        thumbnailGrid, &ThumbnailGridWidget::setImageReady);
connect(thumbnailGrid, &ThumbnailGridWidget::goToPage,
        this, qOverload<unsigned int>(&Viewer::goTo));
```

#### 8.3.7 prepareForOpening 新增

在 `Viewer::prepareForOpening()` 的 `goToFlow->reset();` 之后新增：

```cpp
thumbnailGrid->reset();
```

#### 8.3.8 updateInformation 扩展

在 `Viewer::updateInformation()` 的 `informationLabel->setText(...)` 调用处，修改为：

```cpp
if (render->hasLoadedComic()) {
    auto displayTime = Configuration::getConfiguration().getShowTimeInInformation();
    QString baseText;
    if (displayTime) {
        baseText = render->getCurrentPagesInformation() + " - " + QTime::currentTime().toString("HH:mm");
    } else {
        baseText = render->getCurrentPagesInformation();
    }

    if (slideshowController->state() == SlideshowController::Playing) {
        baseText += " \xe2\x96\xb6"; // ▶
    } else if (slideshowController->state() == SlideshowController::Paused) {
        baseText += " \xe2\x8f\xb8"; // ⏸
    }

    informationLabel->setText(baseText);
    informationLabel->adjustSize();
    informationLabel->update();
    informationLabel->updatePosition();
}
```

#### 8.3.9 resizeEvent 新增

在 `Viewer::resizeEvent(QResizeEvent *event)` 的 `goToFlow->move(...)` 之后新增：

```cpp
thumbnailGrid->move(0, 0);
thumbnailGrid->resize(width(), height());
```

#### 8.3.10 手动翻页时停止幻灯片

在 `Viewer::prev()`、`Viewer::goTo()`、`Viewer::goToFirstPage()`、`Viewer::goToLastPage()` 的开头各添加：

```cpp
slideshowController->stop();
```

**注意**：`Viewer::next()` 中**不**添加 stop，因为幻灯片直接调用 `next()`，不应就此停止。但若用户主动按 Next 快捷键，`MainWindowViewer` 中 `nextAction` 的触发已经会走 `Viewer::next()`，此处需要区分。解决方案：在 `MainWindowViewer` 的 `nextAction` 连接中，先调用 `slideshowController->stop()` 再调 `viewer->next()`。

> **实现细节**：`SlideshowController::onTimeout()` 调用的是 `advancePage` 信号，Viewer 连接此信号到 `next()`。而用户按 Next 快捷键走的是 `nextAction → Viewer::next()`。需要在 `MainWindowViewer` 中对 `nextAction` 的连接做特殊处理——先 stop 再 next。

#### 8.3.11 鼠标交互保护

在 `MouseHandler::mouseMoveEvent` 中，`goToFlow` 显示/隐藏判断之前，新增对 `thumbnailGrid` 的保护：

```cpp
if (viewer->thumbnailGrid->isVisible()) {
    // 网格视图可见时，不触发 goToFlow 的鼠标悬停逻辑
    return;
}
```

同样，在 `MouseHandler::mouseReleaseEvent` 中，`LeftRightNavigation` 和 `HotAreas` 模式的翻页逻辑前，新增：

```cpp
if (viewer->thumbnailGrid->isVisible()) {
    // 让网格视图自行处理点击
    return;
}
```

### 8.4 独立测试方式

- SlideshowController 已独立可测（模块 6）。
- ThumbnailGridWidget 已独立可测（模块 5）。
- Viewer 扩展部分的测试需要打开漫画文件后手动验证叠加层切换和幻灯片行为。

---

## 9. 模块 8：MainWindowViewer 扩展

### 9.1 修改文件

- `YACReader/main_window_viewer.h`
- `YACReader/main_window_viewer.cpp`

### 9.2 main_window_viewer.h 新增

```cpp
class QAction;

// 在 private slots: 或 private: 中新增：
QAction *showThumbnailGridAction;
QAction *slideshowToggleAction;
QAction *slideshowFasterAction;
QAction *slideshowSlowerAction;
```

### 9.3 main_window_viewer.cpp 修改

#### 9.3.1 createActions() 新增

在 `showFlowAction` 创建之后：

```cpp
showThumbnailGridAction = new QAction(tr("Thumbnail grid"), this);
showThumbnailGridAction->setData(SHOW_THUMBNAIL_GRID_ACTION_Y);
showThumbnailGridAction->setShortcut(
    ShortcutsManager::getShortcutsManager().getShortcut(SHOW_THUMBNAIL_GRID_ACTION_Y));
connect(showThumbnailGridAction, &QAction::triggered, viewer, &Viewer::thumbnailGridSwitch);

slideshowToggleAction = new QAction(tr("Toggle slideshow"), this);
slideshowToggleAction->setData(SLIDESHOW_TOGGLE_ACTION_Y);
slideshowToggleAction->setShortcut(
    ShortcutsManager::getShortcutsManager().getShortcut(SLIDESHOW_TOGGLE_ACTION_Y));
connect(slideshowToggleAction, &QAction::triggered, viewer, &Viewer::slideshowToggle);

slideshowFasterAction = addActionWithShortcut(tr("Slideshow faster"), SLIDESHOW_FASTER_ACTION_Y);
connect(slideshowFasterAction, &QAction::triggered, viewer, &Viewer::slideshowFaster);

slideshowSlowerAction = addActionWithShortcut(tr("Slideshow slower"), SLIDESHOW_SLOWER_ACTION_Y);
connect(slideshowSlowerAction, &QAction::triggered, viewer, &Viewer::slideshowSlower);
```

#### 9.3.2 createToolBars() 新增

在 `showFlowAction` 对应的 toolbar 按钮之后：

```cpp
comicToolBar->addAction(wrappedToolbarAction(showThumbnailGridAction));
comicToolBar->addAction(wrappedToolbarAction(slideshowToggleAction));
```

#### 9.3.3 setUpShortcutsManagement() 新增

在已有分组之后：

```cpp
editShortcutsDialog->addActionsGroup(tr("Thumbnail Grid"),
    theme.shortcutsIcons.thumbnailGridIcon,
    tmpList = QList<QAction *>() << showThumbnailGridAction);

editShortcutsDialog->addActionsGroup(tr("Slideshow"),
    theme.shortcutsIcons.slideshowIcon,
    tmpList = QList<QAction *>()
        << slideshowToggleAction
        << slideshowFasterAction
        << slideshowSlowerAction);
```

#### 9.3.4 主题图标绑定

在 `setIcons()` 或类似的主题更新方法中，为新增 Action 设置图标：

```cpp
showThumbnailGridAction->setIcon(theme.toolbar.showThumbnailGridAction);
slideshowToggleAction->setIcon(theme.toolbar.slideshowToggleAction);
```

### 9.4 独立测试方式

- 启动 YACReader，打开漫画 → 验证工具栏出现 Grid 和 Slideshow 图标。
- 按 G 键 → 网格视图弹出；按 S → GoTo Flow（两者互斥）。
- 按 Shift+S → 幻灯片开始；再按 → 暂停；手动翻页 → 幻灯片停止。

---

## 10. 模块 9：OptionsDialog 扩展

### 10.1 修改文件

- `YACReader/options_dialog.h`
- `YACReader/options_dialog.cpp`

### 10.2 options_dialog.h 新增

```cpp
class QSpinBox;
class QDoubleSpinBox;

// 在 private: 段新增：
QSpinBox *gridColumnsSpin;
QSpinBox *gridThumbWidthSpin;
QSpinBox *gridThumbHeightSpin;
QDoubleSpinBox *slideshowIntervalSpin;
QCheckBox *slideshowLoopCheckBox;
```

### 10.3 options_dialog.cpp 修改

#### 10.3.1 构造函数中新增 Tab

```cpp
auto *gridSlideshowTab = new QWidget;
auto *gridSlideshowLayout = new QVBoxLayout(gridSlideshowTab);

// --- Grid 组 ---
auto *gridGroup = new QGroupBox(tr("Thumbnail Grid"));
auto *gridLayout = new QFormLayout(gridGroup);

gridColumnsSpin = new QSpinBox;
gridColumnsSpin->setRange(0, 20);
gridColumnsSpin->setSpecialValueText(tr("Auto"));
gridColumnsSpin->setToolTip(tr("Number of columns. 0 means auto-adapt to window width."));
gridLayout->addRow(tr("Columns:"), gridColumnsSpin);

gridThumbWidthSpin = new QSpinBox;
gridThumbWidthSpin->setRange(80, 300);
gridLayout->addRow(tr("Thumbnail width:"), gridThumbWidthSpin);

gridThumbHeightSpin = new QSpinBox;
gridThumbHeightSpin->setRange(100, 400);
gridLayout->addRow(tr("Thumbnail height:"), gridThumbHeightSpin);

gridSlideshowLayout->addWidget(gridGroup);

// --- Slideshow 组 ---
auto *slideshowGroup = new QGroupBox(tr("Slideshow"));
auto *slideshowLayout = new QFormLayout(slideshowGroup);

slideshowIntervalSpin = new QDoubleSpinBox;
slideshowIntervalSpin->setRange(0.5, 30.0);
slideshowIntervalSpin->setSingleStep(0.5);
slideshowIntervalSpin->setSuffix(" s");
slideshowLayout->addRow(tr("Interval:"), slideshowIntervalSpin);

slideshowLoopCheckBox = new QCheckBox(tr("Loop playback"));
slideshowLayout->addRow(slideshowLoopCheckBox);

gridSlideshowLayout->addWidget(slideshowGroup);
gridSlideshowLayout->addStretch();

tabWidget->addTab(gridSlideshowTab, tr("Grid && Slideshow"));
```

#### 10.3.2 saveOptions() 新增

```cpp
Configuration::getConfiguration().setThumbnailGridColumns(gridColumnsSpin->value());
Configuration::getConfiguration().setThumbnailGridSize(
    QSize(gridThumbWidthSpin->value(), gridThumbHeightSpin->value()));
Configuration::getConfiguration().setSlideshowInterval(slideshowIntervalSpin->value());
Configuration::getConfiguration().setSlideshowLoop(slideshowLoopCheckBox->isChecked());
```

#### 10.3.3 restoreOptions() 新增

```cpp
gridColumnsSpin->setValue(settings->value(THUMBNAIL_GRID_COLUMNS, 0).toInt());
QSize thumbSize = settings->value(THUMBNAIL_GRID_SIZE, QSize(150, 200)).toSize();
gridThumbWidthSpin->setValue(thumbSize.width());
gridThumbHeightSpin->setValue(thumbSize.height());
slideshowIntervalSpin->setValue(settings->value(SLIDESHOW_INTERVAL, 3.0).toDouble());
slideshowLoopCheckBox->setChecked(settings->value(SLIDESHOW_LOOP, false).toBool());
```

### 10.4 独立测试方式

- 打开选项对话框 → 验证 "Grid & Slideshow" Tab 出现。
- 修改值 → 保存 → 重启 → 验证配置持久化。
- 纯 UI 测试，无需打开漫画。

---

## 11. CMakeLists.txt 变更

在 `YACReader/CMakeLists.txt` 的 `qt_add_executable(YACReader WIN32 ...)` 列表中新增：

```cmake
    thumbnail_grid_widget.h
    thumbnail_grid_widget.cpp
    thumbnail_grid_toolbar.h
    thumbnail_grid_toolbar.cpp
    slideshow_controller.h
    slideshow_controller.cpp
```

在 `yacreader_image_files` 列表中新增：

```cmake
    ${PROJECT_SOURCE_DIR}/images/viewer_toolbar/thumbnailGrid.svg
    ${PROJECT_SOURCE_DIR}/images/viewer_toolbar/thumbnailGrid_18x18.svg
    ${PROJECT_SOURCE_DIR}/images/viewer_toolbar/slideshow.svg
    ${PROJECT_SOURCE_DIR}/images/viewer_toolbar/slideshow_18x18.svg
    ${PROJECT_SOURCE_DIR}/images/shortcuts/shortcuts_group_grid.svg
    ${PROJECT_SOURCE_DIR}/images/shortcuts/shortcuts_group_slideshow.svg
```

---

## 12. 实现顺序与里程碑

按模块依赖关系，推荐实现顺序：

| 阶段 | 模块 | 可独立测试 | 依赖 |
|------|------|-----------|------|
| P1 | Configuration 扩展 | ✅ | 无 |
| P1 | ShortcutsManager 扩展 | ✅ | 无 |
| P1 | Theme 扩展 | ✅ | 无 |
| P2 | SlideshowController | ✅ | P1 (Configuration) |
| P2 | ThumbnailGridToolBar | ✅ | P1 (Theme) |
| P3 | ThumbnailGridWidget | ✅ | P2 (ToolBar) |
| P4 | Viewer 扩展 | ❌ | P2 + P3 |
| P4 | MainWindowViewer 扩展 | ❌ | P4 (Viewer) |
| P4 | OptionsDialog 扩展 | ❌ | P1 |
| P5 | MouseHandler 扩展 | ❌ | P4 (Viewer) |
| P5 | 集成测试 | ❌ | 全部 |

每个阶段完成后可独立提交和测试：

- P1 完成后：编译通过，不影响现有功能
- P2 完成后：SlideshowController 可通过单元测试验证状态机
- P3 完成后：ThumbnailGridWidget 可通过独立窗口测试网格布局
- P4 完成后：端到端功能可用，手动测试

---

## 13. 所需外部资源（图标）

以下 SVG 图标需要设计/提供（可先使用占位符）：

| 文件路径 | 建议 |
|---------|------|
| `images/viewer_toolbar/thumbnailGrid.svg` | 2×2 网格图标 |
| `images/viewer_toolbar/thumbnailGrid_18x18.svg` | 同上 18×18 版本 |
| `images/viewer_toolbar/slideshow.svg` | 播放按钮图标 |
| `images/viewer_toolbar/slideshow_18x18.svg` | 同上 18×18 版本 |
| `images/shortcuts/shortcuts_group_grid.svg` | 快捷键对话框分组图标 |
| `images/shortcuts/shortcuts_group_slideshow.svg` | 快捷键对话框分组图标 |

临时方案：可复用 `images/viewer_toolbar/flow.svg` 作为 grid 图标，复用播放类通用图标作为 slideshow 图标。