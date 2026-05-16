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
