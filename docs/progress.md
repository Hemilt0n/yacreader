# 缩略图网格 & 幻灯片 — 实现进度

> 基于 `docs/detailed-design.md`，按模块依赖关系分阶段实现。

## 状态图例

⬜ 未开始 | 🔵 进行中 | ✅ 已完成 | ❌ 阻塞

---

## P1 — 基础层（无依赖，可并行）

| 模块 | 文件 | Agent | 状态 | 说明 |
|------|------|-------|------|------|
| Configuration | `common/yacreader_global_gui.h`, `YACReader/configuration.h` | A | ⬜ | 新增 4 个 #define + 8 个 getter/setter |
| ShortcutsManager | `shortcuts_management/shortcuts_manager.h/.cpp` | B | ⬜ | 新增 4 个 Action ID + 默认快捷键 |
| Theme | `YACReader/themes/theme.h`, `theme_factory.h/.cpp` | C | ⬜ | 新增 ThumbnailGridTheme/SlideshowTheme 结构体 |

---

## P2 — 核心独立控件（依赖 P1）

| 模块 | 文件 | Agent | 状态 | 依赖 |
|------|------|-------|------|------|
| SlideshowController | `YACReader/slideshow_controller.h/.cpp` | D | ⬜ | P1 (Configuration) |
| ThumbnailGridToolBar | `YACReader/thumbnail_grid_toolbar.h/.cpp` | E | ⬜ | P1 (Theme) |

---

## P3 — 缩略图网格控件（依赖 P2）

| 模块 | 文件 | Agent | 状态 | 依赖 |
|------|------|-------|------|------|
| ThumbnailGridWidget | `YACReader/thumbnail_grid_widget.h/.cpp` | F | ⬜ | P2 (ToolBar) |

---

## P4 — Viewer 集成（依赖 P2+P3）

| 模块 | 文件 | Agent | 状态 | 依赖 |
|------|------|-------|------|------|
| Viewer 扩展 | `YACReader/viewer.h/.cpp` | G | ⬜ | P2+P3 |
| MainWindowViewer 扩展 | `YACReader/main_window_viewer.h/.cpp` | H | ⬜ | P4 (Viewer) |
| OptionsDialog 扩展 | `YACReader/options_dialog.h/.cpp` | H | ⬜ | P1 |
| CMakeLists 更新 | `YACReader/CMakeLists.txt` | H | ⬜ | 所有新文件 |

---

## P5 — 收尾

| 模块 | 状态 | 说明 |
|------|------|------|
| MouseHandler 扩展 | ⬜ | 网格视图可见时保护鼠标逻辑 |
| 集成测试 | ⬜ | 编译通过 + 基本功能手动验证 |
| 代码格式化 | ⬜ | `clang-format-windows.cmd` |

---

## 合并记录

| 时间 | 阶段 | 提交 | 说明 |
|------|------|------|------|
| - | - | - | 初始状态 |
