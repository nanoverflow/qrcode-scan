# Cimbar 接收端（cfc）构建说明

本目录是官方 [CameraFileCopy](https://github.com/sz3/cfc) 的本地副本，并增加了详细接收统计 UI。

## 增强点

- 右侧统计面板：摄像头/解码/有效帧帧率、KB/s、进度、成功率、流水线耗时
- 重置统计
- 应用名改为「Cimbar 接收端」

## 依赖

1. Android Studio（本机已有）
2. NDK `29.0.14206865`（与 `app/build.gradle` 中 `ndkVersion` 一致）
3. CMake ≥ 3.16.3
4. OpenCV Android SDK（`opencvsdk` 指向解压根目录，内含 `sdk/`）
5. `app/src/main/assets` submodule（`cimbar-js-bits`，发送页 WebView 用）

## 配置 `gradle.properties`

```properties
opencvsdk=D:/develop/bar-code/deps/OpenCV-android-sdk
org.gradle.java.home=C:/Users/day/AppData/Local/Programs/Android Studio/jbr
```

## Android Studio

1. File → Open → `D:\develop\bar-code\cfc`
2. 等待 Gradle Sync
3. 连接 arm64 真机（官方仅保证 arm64-v8a）
4. Run `app`

## 与 Qt 发送端联调

1. 电脑运行 `qt-sender`，打开文件，开始发送（模式建议 B，15 fps）
2. 手机打开本 App，保持横屏，对准屏幕
3. 观察右侧：有效帧 fps 上升、进度条增长、吞吐 KB/s
4. 完成后选择保存位置

## 原生接口

| JNI | 作用 |
|-----|------|
| `processImageJNI` | 每帧解码 |
| `getStatsJNI` | 管道分隔的统计快照 |
| `resetStatsJNI` | 重置速率窗口与计数基准 |
| `shutdownJNI` | 停止解码线程池 |
