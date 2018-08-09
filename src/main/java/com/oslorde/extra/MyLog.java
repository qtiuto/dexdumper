package com.oslorde.extra;


import android.util.Log;

public final class MyLog {
    public static final int LOG_REMOTE_CONFIG=8;
    private static native void logNative(int priority, String tag, String msg);
    private static boolean log(int priority,String tag,String msg){
        try {
            logNative(priority,tag,msg);
            return true;
        }catch (Throwable e){
            e.printStackTrace();
            Log.println(priority,tag,msg);
        }
        return false;
    }

    private static native void enableRemoteLogNative(boolean enable);

    public static boolean enableRemoteLog(boolean enable){
        try{
            enableRemoteLogNative(enable);
            return true;
        }catch (Throwable e){
        }
        return false;
    }

    public static void e(String tag,String msg){
        log(Log.ERROR,tag,msg);
    }
    public static void d(String tag,String msg){
        log(Log.DEBUG,tag,msg);
    }
    public static void v(String tag,String msg){
        log(Log.VERBOSE,tag,msg);
    }
    public static void i(String tag,String msg){
        log(Log.INFO,tag,msg);
    }
    public static void w(String tag,String msg){
        log(Log.WARN,tag,msg);
    }

    public static void sendConfig(String name,String content){
        log(LOG_REMOTE_CONFIG,name,content);
    }
}
