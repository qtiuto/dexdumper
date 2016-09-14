package com.oslorde.extra;

import android.util.Log;

import java.io.File;

/**
 * Created by asus on 2016/9/14.
 */
public class Utils {
    public static void log(String message) {
        Log.d("I/DexDumper", message);
    }

    public static void log(Object message) {
        log(String.valueOf(message));
    }

    public static void logW(String message) {
        Log.w("W/DexDumper", message);
    }

    public static void logE(String message) {
        Log.e("E/DexDumper", message);
    }

    public static void log(Throwable e) {
        Log.e("E/DexDumper", Log.getStackTraceString(e));
    }

    public static void log(String msg, Throwable e) {
        Log.e("E/DexDumper", msg + ',' + Log.getStackTraceString(e));
    }

    public static boolean rDelete(File file) {
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
