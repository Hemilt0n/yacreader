#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


def require(condition, message):
    if not condition:
        print(f"FAIL: {message}")
        return False
    print(f"PASS: {message}")
    return True


def main():
    viewer = read('YACReader/viewer.cpp')
    grid = read('YACReader/thumbnail_grid_widget.cpp')
    grid_header = read('YACReader/thumbnail_grid_widget.h')
    toolbar = read('YACReader/thumbnail_grid_toolbar.cpp')
    theme_factory = read('YACReader/themes/theme_factory.cpp')
    cmake = read('YACReader/CMakeLists.txt')

    checks = []
    checks.append(require(
        'thumbnailGrid, &ThumbnailGridWidget::highlightPage' in viewer,
        'Viewer connects Render page changes to ThumbnailGridWidget::highlightPage'))
    checks.append(require(
        'thumbnailGrid->setFlowRightToLeft(doubleMangaPage)' not in viewer,
        'Viewer keeps thumbnail grid order independent from manga reading direction'))
    checks.append(require(
        'thumbnailGrid->updateConfig(settings)' in viewer,
        'Viewer forwards option reloads to the thumbnail grid'))
    checks.append(require(
        'thumbnailGrid->updateConfig(config.getSettings())' not in viewer,
        'Manga mode option changes do not rebuild the thumbnail grid layout'))
    checks.append(require(
        'images/viewer_toolbar/thumbnailGrid.svg' in cmake and 'images/viewer_toolbar/slideshow.svg' in cmake and 'images/shortcuts/shortcuts_group_grid.svg' in cmake and 'images/shortcuts/shortcuts_group_slideshow.svg' in cmake,
        'Thumbnail grid and slideshow toolbar/shortcut icons are bundled as Qt resources'))
    checks.append(require(
        '.arg(gridP.textColor.name(), QString::number(' in theme_factory,
        'Thumbnail grid label stylesheet resolves both color and font-size placeholders'))
    checks.append(require(
        'gridTheme.editQSS' in toolbar and 'gridTheme.sliderQSS' in toolbar,
        'Thumbnail grid toolbar uses widget-specific QSS instead of QLabel border QSS'))
    lazy_pattern = re.compile(r'void ThumbnailGridWidget::setImageReady[^{]*\{(?P<body>.*?)\n\}', re.S)
    match = lazy_pattern.search(grid)
    body = match.group('body') if match else ''
    checks.append(require(
        match is not None and 'isVisible()' in body and 'displayThumbnail(index,' in body,
        'Thumbnail data is decoded only when the grid is visible'))
    checks.append(require(
        'ensureVisibleThumbnails' in grid,
        'Thumbnail grid refreshes visible thumbnails lazily after show/scroll/resize'))
    checks.append(require(
        'setImageProvider' in grid_header and 'rawImages' not in grid_header,
        'Thumbnail grid uses an image provider instead of duplicating raw page bytes'))
    checks.append(require(
        'QSignalBlocker' in toolbar,
        'Toolbar blocks slider feedback while setting page programmatically'))

    checks.append(require(
        'gridLayout->setOriginCorner(flowRightToLeft ? Qt::TopRightCorner : Qt::TopLeftCorner)' in grid and 'int col = i % columnCount;' in grid,
        'RTL grid layout uses QGridLayout origin corner while keeping page index order stable'))
    checks.append(require(
        'if (viewportWidth <= 0 && parentWidget())' in grid and 'viewportWidth = parentWidget()->width();' in grid,
        'Auto column calculation falls back to the parent width before the scroll viewport is laid out'))
    checks.append(require(
        'gridContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum)' in grid,
        'Grid container expands horizontally so the scroll area can compute multiple columns'))
    show_pattern = re.compile(r'void ThumbnailGridWidget::showEvent[^{]*\{(?P<body>.*?)\n\}', re.S)
    show_match = show_pattern.search(grid)
    show_body = show_match.group('body') if show_match else ''
    checks.append(require(
        show_match is not None and 'updateSize();' in show_body and 'QTimer::singleShot(0, this, [this]()' in show_body and 'centerSlide(currentHighlightIndex)' in show_body,
        'Showing the grid rebuilds after real widget geometry is available and recenters the current page'))
    checks.append(require(
        'const QSize displaySize = scaledThumbnailSize(img);' in grid and 'items[index].imageLabel->setFixedSize(displaySize);' in grid,
        'Thumbnail label frame adapts to each page aspect ratio instead of forcing portrait boxes'))
    checks.append(require(
        'QSize ThumbnailGridWidget::scaledThumbnailSize(const QImage &image) const' in grid and 'image.size().scaled(thumbnailSize, Qt::KeepAspectRatio)' in grid,
        'Adaptive thumbnail frame is bounded by the configured maximum size while preserving aspect ratio'))
    checks.append(require(
        'imageLabel->installEventFilter(this);' in grid and 'pageLabel->installEventFilter(this);' in grid,
        'Thumbnail image and page labels handle clicks directly instead of leaking double-clicks to the viewer'))
    event_pattern = re.compile(r'bool ThumbnailGridWidget::eventFilter[^{]*\{(?P<body>.*?)\n\}', re.S)
    event_match = event_pattern.search(grid)
    event_body = event_match.group('body') if event_match else ''
    checks.append(require(
        event_match is not None and 'QEvent::MouseButtonDblClick' in event_body and 'QEvent::MouseButtonRelease' in event_body and 'event->accept();' in event_body,
        'Grid consumes thumbnail double-click events and jumps on left-button release'))
    checks.append(require(
        'const int pageIndex = itemIndexForObject(watched);' in event_body and 'emit goToPage(static_cast<unsigned int>(pageIndex));' in event_body,
        'Thumbnail click target resolves to the stored page index before emitting goToPage'))
    checks.append(require(
        'int ThumbnailGridWidget::itemIndexForObject(QObject *watched) const' in grid and 'return items[i].pageIndex;' in grid,
        'Click handling remains page-index based after grid rebuilds or RTL layout changes'))

    return 0 if all(checks) else 1


if __name__ == '__main__':
    sys.exit(main())
