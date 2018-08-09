package com.oslorde.extra;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.os.Bundle;

import com.oslorde.dexdumper.R;

import java.io.File;

import dalvik.system.BaseDexClassLoader;

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        /*String packageName = "com.cib.qdzg";//for test only
        String invokerPackage="com.oslorde.hookmanager";
        PackageManager pm=getPackageManager();
        Intent intent=pm.getLaunchIntentForPackage(invokerPackage);
        intent.putExtra("IMMEDIATE_DEX_DUMP_PACKAGE",packageName);
        //startActivity(intent);
        //finish();*/
        test("com.qidian.QDReader");
    }

    private void test(String packageName) {
        LogServer.getInstance().start();
        //MyLog.enableRemoteLog(false);
        //Utils.log(System.getProperty("java.io.tmpdir", "."));
        if(packageName!=null){
            try {
                ApplicationInfo info=getPackageManager().getApplicationInfo(packageName,0);
                //DexLoader.install(info.sourceDir, getPackageName(), packageName);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }


        new Thread(){
            @Override
            public void run() {
                super.run();
                try {
                    Thread.sleep(2000);
                    DexDumper.dumpDex((BaseDexClassLoader) getClassLoader(),
                            getDir("test_dex",MODE_PRIVATE).getPath(),null,
                            new File(getFilesDir(), "libs"), null, DumpMode.MODE_FIX_CODE);

                }catch (Throwable e){
                    Utils.log(e);
                }

            }
        }.start();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        LogServer.getInstance().shutdown();
        System.exit(0);
    }
}
