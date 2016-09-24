package com.oslorde.extra;

import android.util.Log;

import java.io.File;

public class Utils {
    public static void log(String message) {
        Log.d("Oslorde_DexDump", message);
    }

    public static void log(Object message) {
        log(String.valueOf(message));
    }

    static void logW(String message) {
        Log.w("Oslorde_DexDump", message);
    }

    static void logE(String message) {
        Log.e("Oslorde_DexDump", message);
    }

    public static void log(Throwable e) {
        Log.e("Oslorde_DexDump", Log.getStackTraceString(e));
    }

    public static void log(String msg, Throwable e) {
        Log.e("Oslorde_DexDump", msg + ',' + Log.getStackTraceString(e));
    }

    static boolean rDelete(File file) {
        boolean b;
        if (file.isFile())
            b = file.delete();
        else {
            File[] files = file.listFiles();
            if (files != null)
                for (File f : files) {
                    rDelete(f);
                }
            b = file.delete();
        }
        return b;
    }
}
