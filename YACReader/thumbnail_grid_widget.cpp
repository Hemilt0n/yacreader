#include "thumbnail_grid_widget.h"

#include "QsLog.h"
#include "configuration.h"
#include "theme_manager.h"
#include "thumbnail_grid_toolbar.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

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
    gridContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    gridLayout = new QGridLayout(gridContainer);
    thumbnailSize = Configuration::getConfiguration().getThumbnailGridSize();
    gridLayout->setSpacing(theme.thumbnailGrid.thumbnailSpacing);
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
    connect(scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ThumbnailGridWidget::ensureVisibleThumbnails);

    setFocusPolicy(Qt::StrongFocus);
    hide();

    initTheme(this);
}

ThumbnailGridWidget::~ThumbnailGridWidget() = default;

void ThumbnailGridWidget::setNumSlides(unsigned int slides)
{
    clearGrid();
    totalPages = static_cast<int>(slides);
    imagesReady.resize(slides);
    imagesReady.fill(false);
    QLOG_DEBUG() << "Thumbnail grid page count" << slides;
    buildGrid();
    toolBar->setTop(slides);
    ensureVisibleThumbnails();
}

void ThumbnailGridWidget::buildGrid()
{
    if (totalPages == 0)
        return;

    columnCount = computeColumnCount();
    gridLayout->setOriginCorner(flowRightToLeft ? Qt::TopRightCorner : Qt::TopLeftCorner);
    items.resize(totalPages);

    for (int i = 0; i < totalPages; ++i) {
        QWidget *cell = createThumbnailCell(i);
        int row = i / columnCount;
        int col = i % columnCount;
        gridLayout->addWidget(cell, row, col);
        items[i].cellWidget = cell;
    }

    refreshItemStyles();
    QTimer::singleShot(0, this, &ThumbnailGridWidget::ensureVisibleThumbnails);
}

int ThumbnailGridWidget::computeColumnCount() const
{
    int configuredCols = Configuration::getConfiguration().getThumbnailGridColumns();
    if (configuredCols > 0)
        return configuredCols;
    int viewportWidth = scrollArea->viewport()->width();
    if (viewportWidth <= 0 && parentWidget())
        viewportWidth = parentWidget()->width();
    int availableWidth = viewportWidth - gridLayout->contentsMargins().left() - gridLayout->contentsMargins().right();
    if (availableWidth <= 0)
        return 1;
    int cols = availableWidth / (thumbnailSize.width() + gridLayout->spacing());
    return qMax(1, cols);
}

void ThumbnailGridWidget::clearGrid()
{
    QLayoutItem *child;
    while ((child = gridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->removeEventFilter(this);
            delete child->widget();
        }
        delete child;
    }
    items.clear();
}

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
    imageLabel->setScaledContents(false);

    auto *pageLabel = new QLabel(QString::number(pageIndex + 1));
    pageLabel->setAlignment(Qt::AlignCenter);
    pageLabel->setStyleSheet(theme.thumbnailGrid.labelQSS);

    cellLayout->addWidget(imageLabel);
    cellLayout->addWidget(pageLabel);
    cellLayout->setAlignment(imageLabel, Qt::AlignCenter);
    cellLayout->setAlignment(pageLabel, Qt::AlignCenter);

    if (pageIndex < items.size()) {
        items[pageIndex].imageLabel = imageLabel;
        items[pageIndex].pageLabel = pageLabel;
        items[pageIndex].pageIndex = pageIndex;
    }

    cell->setCursor(Qt::PointingHandCursor);
    imageLabel->setCursor(Qt::PointingHandCursor);
    pageLabel->setCursor(Qt::PointingHandCursor);
    cell->installEventFilter(this);
    imageLabel->installEventFilter(this);
    pageLabel->installEventFilter(this);

    return cell;
}

void ThumbnailGridWidget::setImageReady(int index, const QByteArray &image)
{
    if (index < 0 || index >= totalPages || items.isEmpty())
        return;

    if (index < imagesReady.size())
        imagesReady[index] = true;
    if (isVisible() && isThumbnailVisible(index))
        displayThumbnail(index, image);
}

void ThumbnailGridWidget::displayThumbnail(int index)
{
    if (!imageProvider)
        return;
    displayThumbnail(index, imageProvider(index));
}

void ThumbnailGridWidget::displayThumbnail(int index, const QByteArray &imageData)
{
    if (index < 0 || index >= items.size())
        return;
    if (!items[index].imageLabel)
        return;

    QImage img;
    if (!imageData.isEmpty()) {
        img.loadFromData(imageData);
    }
    if (img.isNull()) {
        items[index].imageLabel->setFixedSize(thumbnailSize);
        items[index].imageLabel->setText(QObject::tr("Page %1").arg(index + 1));
        return;
    }

    const QSize displaySize = scaledThumbnailSize(img);
    img = img.scaled(displaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    items[index].imageLabel->setFixedSize(displaySize);
    items[index].imageLabel->setText(QString());
    items[index].imageLabel->setPixmap(QPixmap::fromImage(img));
    items[index].imageLoaded = true;
}

void ThumbnailGridWidget::highlightPage(int pageIndex)
{
    int oldIndex = currentHighlightIndex;
    currentHighlightIndex = pageIndex;
    updateItemHighlight(oldIndex, pageIndex);
}

void ThumbnailGridWidget::updateItemHighlight(int oldIndex, int newIndex)
{
    applyItemStyle(oldIndex);
    applyItemStyle(newIndex);
}

void ThumbnailGridWidget::centerSlide(int slide)
{
    if (slide < 0 || slide >= totalPages || items.isEmpty())
        return;

    QWidget *cell = items[slide].cellWidget;
    if (cell)
        scrollArea->ensureWidgetVisible(cell, 0, 0);

    setPageNumber(slide);
    ensureVisibleThumbnails();
}

void ThumbnailGridWidget::setPageNumber(int page)
{
    toolBar->setPage(page);
}

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
    applyItemStyle(oldIndex);
    applyItemStyle(newIndex);

    if (newIndex >= 0 && newIndex < totalPages && newIndex < items.size() && items[newIndex].cellWidget)
        scrollArea->ensureWidgetVisible(items[newIndex].cellWidget, 0, 0);
}

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
        ensureVisibleThumbnails();
    }

    toolBar->setFixedWidth(width());
    ensureVisibleThumbnails();
}

bool ThumbnailGridWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick || event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            const int pageIndex = itemIndexForObject(watched);
            if (pageIndex >= 0) {
                if (event->type() == QEvent::MouseButtonRelease)
                    emit goToPage(static_cast<unsigned int>(pageIndex));
                event->accept();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ThumbnailGridWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QLOG_DEBUG() << "Showing thumbnail grid" << totalPages << "pages";
    updateSize();
    QTimer::singleShot(0, this, [this]() {
        updateSize();
        if (currentHighlightIndex >= 0)
            centerSlide(currentHighlightIndex);
        else
            ensureVisibleThumbnails();
    });
}

void ThumbnailGridWidget::applyTheme(const Theme &theme)
{
    auto gridTheme = theme.thumbnailGrid;
    QPalette pal = palette();
    pal.setColor(QPalette::Window, gridTheme.backgroundColor);
    pal.setColor(QPalette::WindowText, gridTheme.textColor);
    setPalette(pal);
    setAutoFillBackground(true);
    if (gridLayout)
        gridLayout->setSpacing(gridTheme.thumbnailSpacing);
    refreshItemStyles();
}

void ThumbnailGridWidget::reset()
{
    clearGrid();
    currentHighlightIndex = -1;
    currentFocusIndex = -1;
    totalPages = 0;
    imagesReady.clear();
}

void ThumbnailGridWidget::updateConfig(QSettings *settings)
{
    Q_UNUSED(settings);
    thumbnailSize = Configuration::getConfiguration().getThumbnailGridSize();
    columnCount = Configuration::getConfiguration().getThumbnailGridColumns();
    if (totalPages > 0) {
        clearGrid();
        buildGrid();
        ensureVisibleThumbnails();
    }
}

void ThumbnailGridWidget::setImageProvider(std::function<QByteArray(int)> provider)
{
    imageProvider = std::move(provider);
}

void ThumbnailGridWidget::setFlowRightToLeft(bool b)
{
    if (flowRightToLeft == b)
        return;
    flowRightToLeft = b;
    if (totalPages > 0) {
        clearGrid();
        buildGrid();
        centerSlide(currentHighlightIndex >= 0 ? currentHighlightIndex : 0);
    }
}

bool ThumbnailGridWidget::isThumbnailVisible(int index) const
{
    if (index < 0 || index >= items.size() || !items[index].cellWidget)
        return false;

    const QWidget *cell = items[index].cellWidget;
    const QPoint topLeft = cell->mapTo(scrollArea->viewport(), QPoint(0, 0));
    const QRect cellRect(topLeft, cell->size());
    return scrollArea->viewport()->rect().adjusted(0, -thumbnailSize.height(), 0, thumbnailSize.height()).intersects(cellRect);
}

void ThumbnailGridWidget::ensureVisibleThumbnails()
{
    if (!isVisible() || totalPages == 0 || items.isEmpty())
        return;

    for (int i = 0; i < items.size(); ++i) {
        if (i < imagesReady.size() && imagesReady.value(i, false) && isThumbnailVisible(i) && !items[i].imageLoaded)
            displayThumbnail(i);
    }
}

QString ThumbnailGridWidget::styleForItem(int index) const
{
    QColor border = theme.thumbnailGrid.borderColor;
    if (index == currentHighlightIndex)
        border = theme.thumbnailGrid.highlightColor;
    if (index == currentFocusIndex)
        border = theme.thumbnailGrid.selectionColor;

    return QString("QLabel { background-color: %1; border: 2px solid %2; border-radius: 3px; }")
            .arg(theme.thumbnailGrid.backgroundColor.darker(115).name(QColor::HexArgb),
                 border.name(QColor::HexArgb));
}

void ThumbnailGridWidget::applyItemStyle(int index)
{
    if (index < 0 || index >= items.size() || !items[index].imageLabel)
        return;
    items[index].imageLabel->setStyleSheet(styleForItem(index));
    if (items[index].pageLabel)
        items[index].pageLabel->setStyleSheet(theme.thumbnailGrid.labelQSS);
}

void ThumbnailGridWidget::refreshItemStyles()
{
    for (int i = 0; i < items.size(); ++i)
        applyItemStyle(i);
}

int ThumbnailGridWidget::itemIndexForObject(QObject *watched) const
{
    auto *watchedWidget = qobject_cast<QWidget *>(watched);
    for (int i = 0; i < items.size(); ++i) {
        if (items[i].cellWidget == watched || items[i].imageLabel == watched || items[i].pageLabel == watched)
            return items[i].pageIndex;
        if (watchedWidget && items[i].cellWidget && items[i].cellWidget->isAncestorOf(watchedWidget))
            return items[i].pageIndex;
    }
    return -1;
}

QSize ThumbnailGridWidget::scaledThumbnailSize(const QImage &image) const
{
    if (image.isNull())
        return thumbnailSize;
    const QSize scaledSize = image.size().scaled(thumbnailSize, Qt::KeepAspectRatio);
    return scaledSize.isValid() ? scaledSize : thumbnailSize;
}
