#include "thumbnail_grid_toolbar.h"
#include "theme_manager.h"

#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>

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
    const QSignalBlocker sliderBlocker(slider);
    edit->setText(QString::number(pageNumber + 1));
    slider->setValue(pageNumber);
}

void ThumbnailGridToolBar::setTop(int numPages)
{
    const bool hasPages = numPages > 0;
    v->setTop(qMax(1, numPages));
    slider->setMaximum(qMax(0, numPages - 1));
    edit->setEnabled(hasPages);
    slider->setEnabled(hasPages);
    centerButton->setEnabled(hasPages);
    goToButton->setEnabled(hasPages);
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
    centerButton->setStyleSheet(gridTheme.buttonQSS.isEmpty() ? theme.goToFlowWidget.buttonQSS : gridTheme.buttonQSS);
    goToButton->setIcon(gridTheme.goToIcon.isNull() ? theme.goToFlowWidget.goToIcon : gridTheme.goToIcon);
    goToButton->setStyleSheet(gridTheme.buttonQSS.isEmpty() ? theme.goToFlowWidget.buttonQSS : gridTheme.buttonQSS);
}
