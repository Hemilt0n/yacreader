# CI 调试记录 — 缩略图网格 & 幻灯片

> 记录在 GitHub Actions 上编译测试过程中遇到的问题及解决方案，方便后续维护。

## 运行环境

- **Runner**: `windows-2022`
- **编译器**: MSVC 2022 (VS 2022 Enterprise)
- **构建系统**: CMake + Ninja
- **Qt**: 6.9.3 (aqtinstall, `win64_msvc2022_64`)
- **Qt 模块**: `qt5compat qtmultimedia qtimageformats qtshadertools qtspeech`
- **后端**: `DECOMPRESSION_BACKEND=7zip`, `PDF_BACKEND=pdfium`

---

## Issue 1: `mouse_handler.cpp` 编译失败 — 前向声明不完整

**运行**: #25962232537 (CI Test)

**错误**:
```
mouse_handler.cpp(32): error C2027: use of undefined type 'ThumbnailGridWidget'
mouse_handler.cpp(99): error C2027: use of undefined type 'ThumbnailGridWidget'
viewer.h(36): note: see declaration of 'ThumbnailGridWidget'
```

**原因**: `mouse_handler.cpp` 中调用 `viewer->thumbnailGrid->isVisible()`，但只通过 `viewer.h` 获得了 `class ThumbnailGridWidget;` 前向声明，前向声明不足以调用成员函数。

**修复** (`697ba19e`): 在 `mouse_handler.cpp` 中添加 `#include "thumbnail_grid_widget.h"`。

**教训**: 任何 `.cpp` 文件中如果通过指针调用某个类的方法（而非仅声明指针变量），必须 `#include` 该类的完整头文件。

---

## Issue 2: 运行时缺少 `pdfium.dll`

**运行**: #25962747506 (Build & Upload)

**现象**: `YACReader.exe` 启动提示找不到 `pdfium.dll`，无法运行。

**原因**: `windeployqt` 只能检测 Qt 的直接依赖，`pdfium` 是第三方库（通过 CMake `find_package(pdfium)` 引入），不在 windeployqt 的检测范围内。

**修复** (`bb4a3ebc`): 在 CI 工作流中显式复制：
```cmd
copy dependencies\pdfium\win\x64\pdfium.dll artifacts\
```

**教训**: 所有非 Qt 的第三方 DLL 都需要在 `windeployqt` 之后手动复制到产物目录。相关文件路径见 `cmake/Findpdfium.cmake`。

---

## Issue 3: 运行时缺少 `Qt6Core5Compat.dll` — 打开漫画闪退

**运行**: #25963351806 (Build & Upload)

**现象**: 程序能启动，打开漫画文件时立即闪退，无错误提示。

**原因**: YACReader 使用 `QTextCodec`（Qt5 兼容类）解码漫画文件元数据（如 ZIP 条目标题中的 CJK 字符），该功能在 Qt6 中被移至 `Qt5Compat` 模块。`windeployqt` 不会自动打包仅被间接引用的模块 DLL（项目通过静态库链接 `Qt5Compat`，运行时加载时才会发现缺失）。

**修复** (`7900e9a1`): 显式复制：
```cmd
copy C:\Qt\6.9.3\msvc2022_64\bin\Qt6Core5Compat.dll artifacts\
copy C:\Qt\6.9.3\msvc2022_64\bin\Qt6Sql.dll artifacts\
copy C:\Qt\6.9.3\msvc2022_64\bin\Qt6ShaderTools.dll artifacts\
```

同时移除了 `windeployqt --no-compiler-runtime` 中的 `--no-compiler-runtime`，让 VC++ 运行时 DLL 也被包含。

**教训**: `windeployqt` 只能检测**直接链接**的 Qt 模块。通过项目内部静态库间接引用的模块（如 `Qt5Compat`、`QtSql`、`QtShaderTools`）不会被自动打包。排查方法:
1. 检查 CMakeLists 中 `find_package` / `target_link_libraries` 引用了哪些 Qt 模块
2. 在 CI 产物中对比 `C:\Qt\版本\msvc2022_64\bin\` 下已安装的模块 DLL 和产物目录
3. 用 Dependency Walker 或 `dumpbin /dependents` 检查 exe 的运行时依赖

---

## CI 工作流参考

完整的构建+打包流程：

```yaml
- name: Collect binaries
  shell: cmd
  run: |
    set PATH=C:\Qt\6.9.3\msvc2022_64\bin;%PATH%
    mkdir artifacts
    copy build\bin\YACReader.exe artifacts\
    windeployqt --no-translations artifacts\YACReader.exe
    copy dependencies\pdfium\win\x64\pdfium.dll artifacts\
    copy C:\Qt\6.9.3\msvc2022_64\bin\Qt6Core5Compat.dll artifacts\
    copy C:\Qt\6.9.3\msvc2022_64\bin\Qt6Sql.dll artifacts\
    copy C:\Qt\6.9.3\msvc2022_64\bin\Qt6ShaderTools.dll artifacts\
    if exist build\bin\*.dll copy build\bin\*.dll artifacts\
```

---

## 最终通过的运行

| 运行编号 | 工作流 | 结果 |
|---------|--------|------|
| 25962429720 | CI Test | Build ✅ Tests ✅ |
| 25964046993 | Build & Upload | Build ✅ Tests ✅ 完整产物 |
