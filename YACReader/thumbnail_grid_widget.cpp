#include "thumbnail_grid_widget.h"
#include "thumbnail_grid_toolbar.h"
#include "configuration.h"
#include "theme_manager.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

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

ThumbnailGridWidget::~ThumbnailGridWidget() = default;

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
    items.resize(totalPages);

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
    int availableWidth = scrollArea->viewport()->width()
        - gridLayout->contentsMargins().left()
        - gridLayout->contentsMargins().right();
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
    imageLabel->setStyleSheet("QLabel { background-color: #2a2a2a; border: 1px solid #444; border-radius: 3px; }");

    auto *pageLabel = new QLabel(QString::number(pageIndex + 1));
    pageLabel->setAlignment(Qt::AlignCenter);

    cellLayout->addWidget(imageLabel);
    cellLayout->addWidget(pageLabel);

    if (pageIndex < items.size()) {
        items[pageIndex].imageLabel = imageLabel;
        items[pageIndex].pageLabel = pageLabel;
        items[pageIndex].pageIndex = pageIndex;
    }

    cell->setCursor(Qt::PointingHandCursor);
    cell->installEventFilter(this);

    return cell;
}

void ThumbnailGridWidget::setImageReady(int index, const QByteArray &image)
{
    if (index < 0 || index >= totalPages || items.isEmpty())
        return;

    rawImages[index] = image;
    if (index < imagesReady.size())
        imagesReady[index] = true;
    displayThumbnail(index);
}

void ThumbnailGridWidget::displayThumbnail(int index)
{
    if (index < 0 || index >= items.size())
        return;
    if (!items[index].imageLabel)
        return;

    QImage img;
    const QByteArray &rawData = rawImages[index];
    if (!rawData.isEmpty()) {
        img.loadFromData(rawData);
    }
    if (img.isNull()) {
        items[index].imageLabel->setText(QObject::tr("Page %1").arg(index + 1));
        return;
    }

    img = img.scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
    auto applyStyle = [&](int idx, const QString &style) {
        if (idx >= 0 && idx < totalPages && idx < items.size() && items[idx].imageLabel)
            items[idx].imageLabel->setStyleSheet(style);
    };

    QString normalStyle = "QLabel { border: 2px solid #444; border-radius: 3px; }";
    QString highlightStyle = "QLabel { border: 2px solid #4caf50; border-radius: 3px; }";

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
    QString normalStyle = "QLabel { border: 2px solid #444; border-radius: 3px; }";
    QString selectionStyle = "QLabel { border: 2px solid #42a5f5; border-radius: 3px; }";

    if (oldIndex >= 0 && oldIndex < totalPages && oldIndex < items.size() && items[oldIndex].imageLabel)
        items[oldIndex].imageLabel->setStyleSheet(normalStyle);
    if (newIndex >= 0 && newIndex < totalPages && newIndex < items.size() && items[newIndex].imageLabel)
        items[newIndex].imageLabel->setStyleSheet(selectionStyle);

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
        if (!rawImages.isEmpty()) {
            for (int i = 0; i < totalPages; ++i) {
                if (i < imagesReady.size() && imagesReady.value(i, false))
                    displayThumbnail(i);
            }
        }
    }

    toolBar->setFixedWidth(width());
}

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

void ThumbnailGridWidget::applyTheme(const Theme &theme)
{
    auto gridTheme = theme.thumbnailGrid;
    QPalette pal = palette();
    pal.setColor(QPalette::Window, gridTheme.backgroundColor);
    pal.setColor(QPalette::WindowText, gridTheme.textColor);
    setPalette(pal);
    setAutoFillBackground(true);
}

void ThumbnailGridWidget::reset()
{
    clearGrid();
    currentHighlightIndex = -1;
    currentFocusIndex = -1;
    totalPages = 0;
    rawImages.clear();
    imagesReady.clear();
    scaledCache.clear();
}

void ThumbnailGridWidget::updateConfig(QSettings *settings)
{
    Q_UNUSED(settings);
    thumbnailSize = Configuration::getConfiguration().getThumbnailGridSize();
    columnCount = Configuration::getConfiguration().getThumbnailGridColumns();
    if (totalPages > 0) {
        clearGrid();
        buildGrid();
        for (int i = 0; i < totalPages; ++i) {
            if (i < imagesReady.size() && imagesReady.value(i, false))
                displayThumbnail(i);
        }
    }
}

void ThumbnailGridWidget::setFlowRightToLeft(bool b)
{
    flowRightToLeft = b;
}
