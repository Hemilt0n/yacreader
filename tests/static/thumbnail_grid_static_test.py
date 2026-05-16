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
        'thumbnailGrid->setFlowRightToLeft(doubleMangaPage)' in viewer,
        'Viewer keeps thumbnail grid RTL state in sync with manga mode'))
    checks.append(require(
        'thumbnailGrid->updateConfig(settings)' in viewer,
        'Viewer forwards option reloads to the thumbnail grid'))
    checks.append(require(
        'thumbnailGrid->updateConfig(config.getSettings())' in viewer,
        'Manga mode option changes rebuild the thumbnail grid layout'))
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

    return 0 if all(checks) else 1


if __name__ == '__main__':
    sys.exit(main())
