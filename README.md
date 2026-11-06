# bar-code：libcimbar 跨端文件传输

用屏幕显示彩色矩阵码（cimbar），手机摄像头接收——**无需 Wi‑Fi / 蓝牙 / NFC**。

| 组件 | 路径 | 说明 |
|------|------|------|
| 上游编码库 | `libcimbar/` | [sz3/libcimbar](https://github.com/sz3/libcimbar) |
| **Qt 发送端** | `qt-sender/` | 打开文件 → 码流播放，可调帧率/模式/压缩等 |
| **Android 接收端** | `cfc/` | 基于官方 [sz3/cfc](https://github.com/sz3/cfc)，增加详细接收统计面板 |

## 快速流程

1. 电脑运行 `qt-sender`，打开要传的文件，点「开始发送」（建议全屏）。
2. 手机打开「Cimbar 接收端」，摄像头对准屏幕。
3. 右侧面板查看：摄像头/解码帧率、KB/s、进度、成功率和耗时等。
4. 传完后按提示保存文件。

发送与接收模式需兼容：默认 **B (68)**；也可让手机自动检测后锁定。

## Qt 发送端

详见 [`qt-sender/README.md`](qt-sender/README.md)。

已构建产物（若本机构建成功）：

```text
qt-sender/build/Release/cimbar_qt_sender.exe
```

可配置参数：模式 B/Bm/Bu/4C、fps、zstd 压缩等级、黑边、Encode ID、循环、全屏。

## Android 接收端

详见 [`cfc/README.md`](cfc/README.md)（上游说明）以及下方本仓库补充。

### 本仓库增强

- 右侧半透明 **接收统计** 面板（约 250ms 刷新）
  - 摄像头 fps / 解码尝试 fps / 有效帧 fps
  - 瞬时吞吐 KB/s 与 kbps、累计字节
  - 当前文件进度、多流进度、完成/在传文件数
  - 扫描/解码成功率、scan/extract/decode 平均耗时
- 「重置统计」按钮
- 中文界面文案

### 用 Android Studio 打开

1. 安装 **NDK 29.0.14206865**、**CMake 3.22.1+**、**SDK Platform 34**、**Build-Tools 35.0.1**。
2. 下载 [OpenCV Android SDK 4.12.0](https://github.com/opencv/opencv/releases/download/4.12.0/opencv-4.12.0-android-sdk.zip)，解压后应有 `OpenCV-android-sdk/sdk/`（本仓库已放在 `deps/OpenCV-android-sdk`）。
3. 编辑 `cfc/gradle.properties`（需 **JDK 17**；Android Studio JBR 若是 25 会不兼容 Gradle 8.2）：

```properties
opencvsdk=D:/develop/bar-code/deps/OpenCV-android-sdk
org.gradle.java.home=C:/Program Files/Microsoft/jdk-17.0.21.21-hotspot
```

4. 确保 submodule 资源已拉取：

```powershell
cd D:\develop\bar-code\cfc
git submodule update --init --recursive
```

5. Android Studio → Open → 选择 `D:\develop\bar-code\cfc` → 同步 Gradle → Run（arm64 真机）。

命令行构建示例：

```powershell
cd D:\develop\bar-code\cfc
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-17.0.21.21-hotspot"
.\gradlew.bat :app:assembleDebug
```

已验证 Debug APK：`cfc/app/build/outputs/apk/debug/`。

## 模式对照

| UI 名称 | 内部 mode | 说明 |
|---------|-----------|------|
| B | 68 | 默认高密度（0.6.x） |
| Bm | 67 | mini |
| Bu | 66 | micro |
| 4C | 4 | 旧版 4 色（0.5.x） |

## 发布 Release（两个文件）

每次发布上传：

1. `CimbarQtSender-windows-x64-vX.Y.Z.zip` — Windows Qt 发送端  
2. `CimbarReceiver-arm64-v8a-vX.Y.Z.apk` — Android 接收端  

### 本地一键打包 + 发布

先确保两端已构建成功，再：

```powershell
# 仅打包到 dist/release/
.\scripts\package-release.ps1 -Version 0.1.0

# 打包并创建 GitHub Release（需 gh auth login，且已设置 origin）
.\scripts\publish-release.ps1 -Version 0.1.0
# 草稿：
.\scripts\publish-release.ps1 -Version 0.1.0 -Draft
```

### CI 自动发布

推送 tag 或手动触发 Actions：

```powershell
git tag v0.1.0
git push origin v0.1.0
```

工作流：`.github/workflows/release.yml`  
会分别构建 Windows Qt ZIP 与 Android APK，并创建 GitHub Release 挂上这两个文件。

## 许可证

- `libcimbar`：MPL 2.0  
- `cfc` 包装代码：MIT  
- `qt-sender`：与本仓库一并提供，依赖 libcimbar（MPL）
