package com.oslorde.extra;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;

import com.oslorde.dexdumper.R;
import com.oslorde.sharedlibrary.Utils;

import java.io.File;

import dalvik.system.BaseDexClassLoader;

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        String packageName="com.jjwxc.reader";
        try {
            ApplicationInfo info=getPackageManager().getApplicationInfo(packageName,0);
            DexLoader.install(info.sourceDir,getPackageName(),packageName);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }

        new Thread(){
            @Override
            public void run() {
                super.run();
                try {
                    Thread.sleep(1000);
                    DexDumper.dumpDex((BaseDexClassLoader) getClassLoader(),
                            getDir("test_dex",MODE_PRIVATE).getPath(),null,
                            new File(getFilesDir(),"libs"),null,DumpMode.MODE_LOOSE);
                }catch (Throwable e){
                    Utils.log(e);
                }

            }
        }.start();

    }
}
