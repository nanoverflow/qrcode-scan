# Cimbar Qt Sender

基于 [libcimbar](https://github.com/sz3/libcimbar) 的 Windows Qt6 发送端：打开本地文件，编码为彩色 cimbar 帧流并播放，供 Android 接收端解码。

## 功能

- 打开任意文件并编码为喷泉码 + zstd（可选）码流
- 可配置：
  - **模式**：`B` / `Bm` / `Bu` / `4C`（与官方 `cimbar_send` / CFC 一致）
  - **目标帧率** 1–60 fps
  - **压缩等级** 0–22（0 = 不压缩）
  - **黑边 padding**
  - **Encode ID**（-1 = 每次自动递增）
  - **循环播放**
  - **全屏**（F11）
- 右侧实时统计：文件大小、压缩后大小、每帧容量、块进度、实测 fps、估算载荷吞吐等

## 依赖

- Qt 6.8+（Widgets）
- OpenCV 4.x（core / imgproc / imgcodecs）
- Visual Studio 2022/2026 + CMake ≥ 3.21
- 同级目录的 `../libcimbar` 源码

本机示例路径：

- Qt：`D:\dev\Qt\6.8.3\msvc2022_64`
- OpenCV：`D:\software\opencv4.14\build`

## 构建

在 PowerShell 中：

```powershell
$qt = "D:\dev\Qt\6.8.3\msvc2022_64"
$opencv = "D:\software\opencv4.14\build"

cmake -S D:\develop\bar-code\qt-sender -B D:\develop\bar-code\qt-sender\build `
  -G "Visual Studio 18 2026" -A x64 `
  "-DCMAKE_PREFIX_PATH=$qt;$opencv" `
  "-DOpenCV_DIR=$opencv"

cmake --build D:\develop\bar-code\qt-sender\build --config Release --parallel

& "$qt\bin\windeployqt.exe" D:\develop\bar-code\qt-sender\build\Release\cimbar_qt_sender.exe --release
Copy-Item "$opencv\x64\vc16\bin\opencv_world4140.dll" D:\develop\bar-code\qt-sender\build\Release\ -Force
```

运行：

```powershell
D:\develop\bar-code\qt-sender\build\Release\cimbar_qt_sender.exe
```

## 使用建议

1. 把窗口放到手机摄像头能完整拍到的显示器区域；尽量全屏、提高对比度。
2. 默认模式 **B** + **15 fps** 较稳；环境差可降到 8–10 fps，或改用 `Bm`/`Bu`。
3. 与 Android 端模式保持一致（自动检测也可）。
4. Space 开始/停止；F11 全屏。
