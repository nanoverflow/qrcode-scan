package org.cimbar.camerafilecopy;

import org.opencv.android.CameraActivity;
import org.opencv.android.CameraBridgeViewBase.CvCameraViewFrame;
import org.opencv.android.OpenCVLoader;
import org.opencv.core.Mat;
import org.opencv.android.CameraBridgeViewBase;
import org.opencv.android.CameraBridgeViewBase.CvCameraViewListener2;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.webkit.MimeTypeMap;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.TextView;
import android.widget.Toast;
import androidx.annotation.Nullable;
import androidx.core.view.GestureDetectorCompat;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

public class MainActivity extends CameraActivity implements CvCameraViewListener2 {
    private static final String TAG = "cfc::MainActivity";
    private static final int CREATE_FILE = 11;

    private GestureDetectorCompat mDetector;
    private Toast introToast;

    private CameraBridgeViewBase mOpenCvCameraView;
    private ModeSelToggle mModeSwitch;
    private TextView mStatsText;
    private Button mResetStatsBtn;
    private final Handler mUiHandler = new Handler(Looper.getMainLooper());
    private final Runnable mStatsPoller = new Runnable() {
        @Override
        public void run() {
            refreshStatsPanel();
            mUiHandler.postDelayed(this, 250);
        }
    };

    private int modeVal = 0;
    private int detectedMode = 68;
    private String dataPath;
    private String activePath;
    private boolean nativeReady = false;

    public MainActivity() {
        Log.i(TAG, "Instantiated new " + this.getClass());
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "called onCreate");
        super.onCreate(savedInstanceState);

        //! [ocv_loader_init]
        if (OpenCVLoader.initLocal()) {
            Log.i(TAG, "OpenCV loaded successfully");
            System.loadLibrary("cfc-cpp");
            nativeReady = true;
        } else {
            Log.e(TAG, "OpenCV initialization failed!");
            (Toast.makeText(this, "OpenCV initialization failed!", Toast.LENGTH_LONG)).show();
            return;
        }
        //! [ocv_loader_init]

        //! [keep_screen]
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        //! [keep_screen]

        this.dataPath = this.getFilesDir().getPath();

        setContentView(R.layout.activity_main);

        mOpenCvCameraView = (CameraBridgeViewBase) findViewById(R.id.main_surface);
        mOpenCvCameraView.setVisibility(SurfaceView.VISIBLE);
        mOpenCvCameraView.setCvCameraViewListener(this);

        mStatsText = findViewById(R.id.stats_text);
        mResetStatsBtn = findViewById(R.id.reset_stats_btn);
        mResetStatsBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (nativeReady) {
                    resetStatsJNI();
                    refreshStatsPanel();
                }
            }
        });

        // set up mode toggle
        mModeSwitch = (ModeSelToggle) findViewById(R.id.mode_switch);
        mModeSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    modeVal = detectedMode;
                } else {
                    modeVal = 0;
                }
            }
        });

        // Set up the swipe gestures
        mDetector = new GestureDetectorCompat(this, new FlingGestureListener());

        // and the hint toast
        introToast = Toast.makeText(this, "对准电脑上的彩色码流开始接收；右侧为详细统计", Toast.LENGTH_LONG);
    }

    @Override
    public void onStart() {
        super.onStart();
        introToast.show();
        // reset autodetect
        mModeSwitch.setChecked(false);
        modeVal = 0;
    }

    @Override
    public void onPause()
    {
        mUiHandler.removeCallbacks(mStatsPoller);
        shutdownJNI();
        super.onPause();
        if (mOpenCvCameraView != null)
            mOpenCvCameraView.disableView();
    }

    @Override
    public void onResume()
    {
        super.onResume();
        if (mOpenCvCameraView != null)
            mOpenCvCameraView.enableView();
        if (nativeReady) {
            mUiHandler.removeCallbacks(mStatsPoller);
            mUiHandler.post(mStatsPoller);
        }
    }

    @Override
    protected List<? extends CameraBridgeViewBase> getCameraViewList() {
        return Collections.singletonList(mOpenCvCameraView);
    }

    @Override
    public void onDestroy() {
        mUiHandler.removeCallbacks(mStatsPoller);
        shutdownJNI();
        super.onDestroy();
        if (mOpenCvCameraView != null)
            mOpenCvCameraView.disableView();
    }

    @Override
    public void onCameraViewStarted(int width, int height) {
    }

    @Override
    public void onCameraViewStopped() {
    }

    @Override
    public Mat onCameraFrame(CvCameraViewFrame frame) {
        // get current camera frame as OpenCV Mat object
        Mat mat = frame.rgba();

        // native call to process current camera frame
        String res = processImageJNI(mat.getNativeObjAddr(), this.dataPath, this.modeVal);

        // res will contain a file path if we completed a transfer. Ask the user where to save it
        if (res.startsWith("/")) {
            if (res.length() == 2 && res.charAt(1) == '4') {
                detectedMode = 4;
            }
            else if (res.length() == 3 && res.charAt(1) == '6' && res.charAt(2) == '6') {
                detectedMode = 66;
            }
            else if (res.length() == 3 && res.charAt(1) == '6' && res.charAt(2) == '7') {
                detectedMode = 67;
            }
            else {
                detectedMode = 68;
            }
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mModeSwitch.setChecked(true);
                    mModeSwitch.setModeVal(detectedMode);
                }
            });

        }
        else if (!res.isEmpty()) {
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType(getMimeType(res));
            intent.putExtra(Intent.EXTRA_TITLE, res);
            // can't get putExtra to work for extra values, so we'll save it in the class
            this.activePath = this.dataPath + "/" + res;
            startActivityForResult(intent, CREATE_FILE);
        }

        // return processed frame for live preview
        return mat;
    }

    private void refreshStatsPanel() {
        if (!nativeReady || mStatsText == null)
            return;
        final String raw = getStatsJNI();
        mStatsText.setText(formatStats(raw));
    }

    private static String modeLabel(int mode) {
        switch (mode) {
            case 0: return "自动";
            case 4: return "4C";
            case 66: return "Bu";
            case 67: return "Bm";
            case 68: return "B";
            default: return String.valueOf(mode);
        }
    }

    private static String formatStats(String raw) {
        if (raw == null || raw.isEmpty()) {
            return "无统计数据";
        }
        String[] p = raw.split("\\|", -1);
        if (p.length < 20) {
            return "统计解析失败: " + raw;
        }

        try {
            long cameraFrames = Long.parseLong(p[0]);
            long scanned = Long.parseLong(p[1]);
            long decoded = Long.parseLong(p[2]);
            long perfect = Long.parseLong(p[3]);
            long bytes = Long.parseLong(p[4]);
            long backlog = Long.parseLong(p[5]);
            long filesDecoded = Long.parseLong(p[6]);
            long filesInFlight = Long.parseLong(p[7]);
            long threads = Long.parseLong(p[8]);
            int mode = Integer.parseInt(p[9]);
            int detectedMode = Integer.parseInt(p[10]);
            double progress = Double.parseDouble(p[11]);
            double cameraFps = Double.parseDouble(p[12]);
            double decodeFps = Double.parseDouble(p[13]);
            double perfectFps = Double.parseDouble(p[14]);
            double bytesPerSec = Double.parseDouble(p[15]);
            double elapsedSec = Double.parseDouble(p[16]);
            double scanMs = Double.parseDouble(p[17]);
            double extractMs = Double.parseDouble(p[18]);
            double decodeMs = Double.parseDouble(p[19]);
            String progressCsv = p.length > 20 ? p[20] : "";

            double scanOkPct = scanned > 0 ? (100.0 * decoded / scanned) : 0.0;
            double decodeOkPct = decoded > 0 ? (100.0 * perfect / decoded) : 0.0;
            double kbps = bytesPerSec * 8.0 / 1000.0;
            double KBps = bytesPerSec / 1024.0;

            return String.format(Locale.US,
                    "运行时长: %.1f s\n" +
                    "模式: %s  检测: %s\n" +
                    "线程: %d  积压: %d\n" +
                    "\n" +
                    "—— 帧率 ——\n" +
                    "摄像头: %.1f fps\n" +
                    "解码尝试: %.1f fps\n" +
                    "有效帧: %.1f fps\n" +
                    "\n" +
                    "—— 吞吐 ——\n" +
                    "瞬时: %.1f KB/s (%.1f kbps)\n" +
                    "累计数据: %.2f KB (%d B)\n" +
                    "\n" +
                    "—— 进度 ——\n" +
                    "当前文件: %.1f%%\n" +
                    "多流进度: %s\n" +
                    "已完成文件: %d\n" +
                    "传输中: %d\n" +
                    "\n" +
                    "—— 计数 ——\n" +
                    "相机帧: %d\n" +
                    "扫描: %d\n" +
                    "解码: %d\n" +
                    "完美帧: %d\n" +
                    "扫描成功率: %.1f%%\n" +
                    "解码成功率: %.1f%%\n" +
                    "\n" +
                    "—— 耗时均值 ——\n" +
                    "scan: %.1f ms\n" +
                    "extract: %.1f ms\n" +
                    "decode: %.1f ms",
                    elapsedSec,
                    modeLabel(mode), modeLabel(detectedMode),
                    threads, backlog,
                    cameraFps, decodeFps, perfectFps,
                    KBps, kbps,
                    bytes / 1024.0, bytes,
                    progress * 100.0,
                    progressCsv.isEmpty() ? "-" : progressCsv,
                    filesDecoded, filesInFlight,
                    cameraFrames, scanned, decoded, perfect,
                    scanOkPct, decodeOkPct,
                    scanMs, extractMs, decodeMs);
        } catch (Exception e) {
            return "统计格式异常: " + e.getMessage();
        }
    }

    // mimetype helper
    protected static String getMimeType(String filename) {
        String mt = null;
        int extIdx = filename.lastIndexOf('.');
        if (extIdx >= 0) {
            String ext = filename.substring(extIdx + 1).toLowerCase();
            if (!ext.isEmpty()) {
                mt = MimeTypeMap.getSingleton().getMimeTypeFromExtension(ext);
            }
        }
        if (mt == null) {
            mt = "*/*";
        }
        return mt;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (resultCode == RESULT_OK && requestCode == CREATE_FILE) {
            if (this.activePath == null)
                return;

            // copy this.activePath (tempfile) to the user-specified location
            try (
                    InputStream istream = new FileInputStream(this.activePath);
                    OutputStream ostream = getContentResolver().openOutputStream(data.getData())
            ) {
                byte[] buf = new byte[8192];
                int length;
                while ((length = istream.read(buf)) > 0) {
                    ostream.write(buf, 0, length);
                }
                ostream.flush();
            } catch (Exception e) {
                Log.e(TAG, "failed to write file " + e.toString());
            } finally {
                try {
                    new File(this.activePath).delete();
                } catch (Exception e) {}
                this.activePath = null;
            }
        }
    }

    private native String processImageJNI(long mat, String path, int modeInt);
    private native void shutdownJNI();
    private native String getStatsJNI();
    private native void resetStatsJNI();

    @Override
    public boolean onTouchEvent(MotionEvent event){
        if (this.mDetector.onTouchEvent(event)) {
            return true;
        }
        return super.onTouchEvent(event);
    }
    @SuppressLint("ClickableViewAccessibility")
    class FlingGestureListener extends GestureDetector.SimpleOnGestureListener {
        private static final String TAG = "Gestures";

        // We only want fling gestures to trigger the view transitions, not scrolling.
        @Override
        public boolean onFling(MotionEvent event1, MotionEvent event2,
                               float velocityX, float velocityY) {
            final int THRESHOLD = 100;
            final int VEL_THRESHOLD = 100;

            if (Math.abs(velocityY) < VEL_THRESHOLD)
                return false;
            if (Math.abs(event1.getY() - event2.getY()) < THRESHOLD)
                return false;
            if (mOpenCvCameraView != null)
                mOpenCvCameraView.disableView();
            if (introToast != null)
                introToast.cancel();

            Intent intent = new Intent(MainActivity.this, WebViewActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
            return true;
        }
    }

}
