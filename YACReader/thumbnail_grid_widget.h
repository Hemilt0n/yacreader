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
    bool eventFilter(QObject *watched, QEvent *event) override;

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
