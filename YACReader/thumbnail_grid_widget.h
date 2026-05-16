#ifndef THUMBNAIL_GRID_WIDGET_H
#define THUMBNAIL_GRID_WIDGET_H

#include "themable.h"

#include <functional>

#include <QByteArray>
#include <QSize>
#include <QVector>
#include <QWidget>

class QGridLayout;
class QScrollArea;
class QLabel;
class QShowEvent;
class ThumbnailGridToolBar;

class ThumbnailGridWidget : public QWidget, protected Themable
{
    Q_OBJECT

public:
    explicit ThumbnailGridWidget(QWidget *parent = nullptr);
    ~ThumbnailGridWidget() override;

    void setFlowRightToLeft(bool b);
    void setImageProvider(std::function<QByteArray(int)> provider);

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
    void showEvent(QShowEvent *event) override;
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

    QScrollArea *scrollArea = nullptr;
    QWidget *gridContainer = nullptr;
    QGridLayout *gridLayout = nullptr;
    ThumbnailGridToolBar *toolBar = nullptr;

    QVector<ThumbnailItem> items;
    QVector<bool> imagesReady;
    std::function<QByteArray(int)> imageProvider;

    int currentHighlightIndex = -1;
    int currentFocusIndex = -1;
    int totalPages = 0;
    QSize thumbnailSize{ 150, 200 };
    int columnCount = 0;
    bool flowRightToLeft = false;

    void buildGrid();
    void clearGrid();
    void updateItemHighlight(int oldIndex, int newIndex);
    void updateItemSelection(int oldIndex, int newIndex);
    int computeColumnCount() const;
    QWidget *createThumbnailCell(int pageIndex);
    void displayThumbnail(int index);
    void displayThumbnail(int index, const QByteArray &imageData);
    void ensureVisibleThumbnails();
    bool isThumbnailVisible(int index) const;
    void applyItemStyle(int index);
    void refreshItemStyles();
    QString styleForItem(int index) const;
};

#endif
