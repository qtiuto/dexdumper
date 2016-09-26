package com.oslorde.extra;

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;

import com.oslorde.dexdumper.BuildConfig;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.channels.FileChannel;
import java.util.IdentityHashMap;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import dalvik.system.BaseDexClassLoader;
import dalvik.system.DexFile;

public class DexDumper {
    private static final String ABI_ARM="arm";
    private static final String ABI_ARM_MIRROR="armeabi";
    private static final String ABI_ARMV7_MIRROR="armeabi-v7a";
    private static final String ABI_ARM64="arm64";
    private static final String ABI_ARM64_MIRROR="arm64-v8a";

    private static final int DEX_FILE_START;
    private static String sStorePath=Environment.getExternalStorageDirectory().getPath()+"/DexDump";

    static {
        if(Build.VERSION.SDK_INT>=24){
            DEX_FILE_START=1;
        }else DEX_FILE_START=0;

    }

    private static native void dumpDexV21(long dex_vector,String outDir,boolean isMr1);

    private static native void dumpDexV23(long[] dex_arr,String outDir,boolean isNougat);

    private static native void dumpDexV16(long cookie, String outDir);

    private static native void dumpDexV19ForArt(long cookie,String outDir);

    private static native void setMode(int mode);

    /**
     *For now, multi-delegated classloader is unresolved.
     * @param loader the lowest level classloader, despite hot patch.
     * @param storePath the path where the dexFile will be store. and the path will be clear before dump.
     *                  If null the value passed by last call will be used,
     * @param libSrc the source file of the library.
     * @param libDir the directory where the source library will be copied to,(A library can only be loaded by one classloader at the same time
     * @param libName the library name of the copied one.
     * @param mode the mode to dump dex,if null the default mode {@code MODE_LOOSE} will be used, the strictest mode is the most time-consuming;
     * @throws    DexDumpException if any preparation can't be finished
     * @return whether the loader is able to be extract,.
     */
    @SuppressLint("UnsafeDynamicallyLoadedCode")
    public static boolean dumpDex(BaseDexClassLoader loader, String storePath,File libSrc, File libDir,String libName,int mode){
        if( Looper.getMainLooper().getThread()==Thread.currentThread()){
            throw new DexDumpException("As the operation is time-consuming,don't run it in main thread");
        }
        if(libDir==null){
            libDir=new File(Environment.getDataDirectory(),"data/"+getPackageName()+"/libs");
        }
        if(!libDir.exists()&&!libDir.mkdir()){
            throw new DexDumpException(new IllegalArgumentException("The libDir"+libDir+" doesn't existed ,and can't be created"));
        }
        if(libDir.exists()&&libDir.isFile()&&libDir.delete()&&libDir.mkdir()){
            throw new DexDumpException(new IllegalArgumentException("The libDir"+libDir+" is file ,and can't be re-created as directory"));
        }
        if(!libDir.canExecute()||!libDir.canRead()||!libDir.canWrite()) {
            throw new DexDumpException("The directory "+libDir+" is denied to be load, check permission?");
        }
        if(libName==null)libName="libdex_dump.so";
        File libDst=new File(libDir,libName);
        if(libDst.exists()&&!libDst.delete()) {
            throw new DexDumpException("Failed to delete existed library");
        }
        if(libSrc==null){
            Application application=getApplication();
            String sourceApk="";
            try {
                ApplicationInfo info=application.getApplicationInfo();
                String abi=info.nativeLibraryDir;
                Utils.log("Abi=" + abi);
                Utils.log(info.packageName);
                if (info.dataDir.equals(abi) || TextUtils.isEmpty(abi) || abi.endsWith("lib") || new File(abi).getName().startsWith(info.packageName)) {
                    abi=getFirstSupportedAbi();
                }else {
                    abi=abi.substring(abi.lastIndexOf('/')+1);
                    switch (abi){
                        case ABI_ARM:
                            if(getFirstSupportedAbi().equals(ABI_ARM_MIRROR)){
                                abi=ABI_ARM_MIRROR;
                            }else abi=ABI_ARMV7_MIRROR;
                            break;
                        case ABI_ARM64:abi=ABI_ARM64_MIRROR;break;
                        default:break;
                    }
                }
                Context context=application.createPackageContext(BuildConfig.APPLICATION_ID, Context.CONTEXT_RESTRICTED);
                sourceApk=context.getApplicationInfo().sourceDir;
                ZipFile zipFile=new ZipFile(sourceApk);
                Utils.log("LoadingLib=" + abi + "/libdex_dump.so");
                ZipEntry entry=zipFile.getEntry("lib/"+abi+"/libdex_dump.so");
                InputStream stream=zipFile.getInputStream(entry);
                byte[] bytes=new byte[1024];int read;
                FileOutputStream out=new FileOutputStream(libDst);
                while ((read=stream.read(bytes))>0){
                    out.write(bytes,0,read);
                }
                stream.close();out.close();
            } catch (Exception e) {
                throw new DexDumpException("Failed to fetch lib from source apk",e);
            }
        }else if(!fileChannelCopy(libSrc,libDst)) {
            throw new DexDumpException("Failed to copy library");
        }
        try {
            System.load(libDst.getAbsolutePath());
        }catch (Throwable e){
            Utils.log(e);
            throw new DexDumpException("Failed to load libirary",e);
        }
        if(storePath==null) storePath=sStorePath;
        if(storePath.charAt(storePath.length()-1)=='/'){
            storePath=storePath.substring(0,storePath.length()-1);
        }
        if(resetStorePath(storePath)){
            sStorePath=storePath;
            try {
                return dumpDexImpl(loader, mode);
            } catch (Exception e) {
                throw new DexDumpException(e);
            }
        }else throw new DexDumpException("The store path can't be cleaned");
    }

    private static String getFirstSupportedAbi() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return Build.SUPPORTED_ABIS[0];
        }else return Build.CPU_ABI;
    }

    /**
     *For now, multi-delegated classloader is unresolved.
     * @param loader the lowest level classloader, despite hot patch.
     * @param storePath the path where the dexFile will be store. and the path will be clear before dump.
     *                  If null the value passed by last call will be used,
     * @param mode the mode to dump dex,if null the default mode {@code MODE_LOOSE} will be used, the strictest mode is the most time-consuming;
     * @return whether the loader is able to be extract,.
     */

    public static boolean dumpDex(BaseDexClassLoader loader, String storePath,int mode){
        File libDir=new File(Environment.getDataDirectory()+"/data/"+getPackageName()+"/libs/");
        return dumpDex(loader,storePath,null,libDir,null,mode);
    }

    private static boolean resetStorePath(String storePath){
        File file=new File(storePath);
        return (!file.exists()||Utils.rDelete(file))&&file.mkdirs();
    }

    private static String getPackageName(){
        try {
            return getApplication().getPackageName();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "";
    }

    private static Application getApplication(){
        try {
            final Method CurrentApplication = Class.forName("android.app.ActivityThread").
                    getDeclaredMethod("currentApplication");
            Application application = null;
            if (Build.VERSION.SDK_INT < 18) {
                Handler handler = new Handler(Looper.getMainLooper());
                final Application[] outA = new Application[1];
                while (outA[0] == null) {
                    handler.postAtFrontOfQueue(new Runnable() {
                        @Override
                        public void run() {
                            try {
                                outA[0] = (Application) CurrentApplication.invoke(null);
                            } catch (Exception e) {
                                e.printStackTrace();
                            }
                        }
                    });
                }
                application = outA[0];
            } else while (application == null) {
                application = (Application) CurrentApplication.invoke(null);
            }
            return application;
        } catch (Exception e) {
            throw new DexDumpException("Failed to Find Application", e);
        }
    }
    /**
     *
     * @param loader the lowest level classloader, despite hot patch.
     * @return whether the loader is able to be extract.
     */
    private static boolean dumpDexImpl(ClassLoader loader, int mode) throws Exception {
        int sdkVer= Build.VERSION.SDK_INT;
        ClassTools.init(loader);
        setMode(mode);
        java.lang.Process logP = null;
        try {
            Runtime.getRuntime().exec("logcat -c");
            logP = Runtime.getRuntime().exec("logcat -f " + sStorePath + "/log.txt -s Oslorde_DexDump");
        } catch (Exception e) {
            Utils.log("Logcat Failed");
        }
        IdentityHashMap<ClassLoader, Object> loaders = new IdentityHashMap<>();
        Out:
        do{
            if(!(loader instanceof BaseDexClassLoader)){
                if (loader != null && !loader.getClass().getSimpleName().equals("BootClassLoader")) {
                    Utils.logW("the final class loader  is not BootClassLoader but " + loader.getClass().getName() + ". May there should be a fix to get proper classLoader");
                    //TODO:For custom classloader,this is only a experimental fix,you may need to find out true classloaders or direct native cookies from its fields or methods
                    //TODO:due to some dynamic code fix manners, e.g. AndFix
                    Utils.logW("Experimental fix start");
                    Field[] fields = loader.getClass().getDeclaredFields();
                    try {
                        for (Field f : fields) {
                            f.setAccessible(true);
                            Object ob = f.get(loader);
                            if (ob instanceof ClassLoader) {
                                if (!loaders.containsKey(ob)) {
                                    loader = (ClassLoader) ob;
                                    continue Out;
                                }
                            }
                        }
                    } catch (IllegalAccessException e) {
                        Utils.log(e);
                    }
                }
                // reach boot classloader or null or other.
                break;
            }
            loaders.put(loader, null);

            Field field = ReflectUtils.findField(BaseDexClassLoader.class, "pathList");
            Object pathList = field.get(loader);
            if (pathList == null) {
                loader = loader.getParent();
                continue;
            }
            field = ReflectUtils.findField(pathList.getClass(), "dexElements");
            Object dexElements = field.get(pathList);
            if (dexElements == null) {
                loader = loader.getParent();
                continue;
            }
            Class Element = dexElements.getClass().getComponentType();
            Utils.log("elements count:" + Array.getLength(dexElements));
            for (int i = 0, length = Array.getLength(dexElements); i < length; ++i) {
                Object element = Array.get(dexElements, i);
                field = ReflectUtils.findField(Element, "dexFile");
                DexFile dexFile = (DexFile) field.get(element);

                String dexName = dexFile.getName();
                Utils.log("loader path" + i + " :" + dexName);
                FileOutputStream out;
                File file;
                //necessary?
                    /*Enumeration<String> classes=dexFile.entries();
                    file=new File(sStorePath,"Classes.txt");
                    out = new FileOutputStream(file,true);
                    out.write(("-----"+dexName+"-----\n").getBytes());
                    while (classes.hasMoreElements()){
                        String clsName=classes.nextElement();
                        try {
                            //loader.loadClass(clsName);
                            out.write(clsName.getBytes());
                            //out.write('\n');
                        }catch (Throwable ignored){
                            Utils.log(clsName+" not found");
                        }
                    }
                    out.close();*/
                //TODO:before art encrypted dexFile may be loaded directly by custom
                //TODO:classLoader you can changed the code to get the cookie by yourself
                field = ReflectUtils.findField(DexFile.class, "mCookie");
                Object mCookie = field.get(dexFile);
                String realDexName;
                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.KITKAT) {
                    realDexName = dexName + System.lineSeparator();
                } else realDexName = dexName + '\n';
                if (dexName.startsWith("/data/")) {
                    dexName = dexName.substring(6);
                }
                if (dexName.charAt(0) == '/') dexName = dexName.substring(1);
                dexName = dexName.replace("_", "(-)");
                dexName = dexName.replace('/', '_');
                if (dexName.length() > 25) {
                    dexName = dexName.substring(0, 25);
                    dexName = dexName.concat("...");
                }
                String dexDir = sStorePath + '/' + dexName;
                file = new File(dexDir);
                if (!file.exists() && !file.mkdir()) {
                    throw new DexDumpException("Error ocurred when make dexDir");
                }
                File storeInfoTxt = new File(dexDir, "DexInfo.txt");
                if (storeInfoTxt.exists() && storeInfoTxt.isDirectory() && !storeInfoTxt.delete()) {
                    throw new DexDumpException("Couldn't delete dir:" + storeInfoTxt.getPath());
                }
                try {
                    out = new FileOutputStream(storeInfoTxt, true);
                    out.write(realDexName.getBytes());
                    out.close();
                } catch (Exception e) {
                    throw new DexDumpException("Error occured when writing dexName into DexInfo.txt", e);
                }

                if (isArray(mCookie)) {
                    int N = Array.getLength(mCookie);
                    Utils.log("Cookie Length:" + N);
                    long[] dex_files = new long[N - DEX_FILE_START];
                    for (int j = DEX_FILE_START; j < N; j++) {
                        //the cookie may be int array(32 bit) or long array(64 bit).
                        Number address = (Number) Array.get(mCookie, j);
                        Utils.log("Address" + j + ":" + address);
                        dex_files[j - DEX_FILE_START] = address.longValue();
                    }
                    Utils.log("Detected SDK Int 23+, invoke dumpDexV23");
                    dumpDexV23(dex_files, dexDir, sdkVer == 24);
                } else if (mCookie instanceof Number) {//sdk 21-22
                    Utils.log("Address:" + mCookie);
                    if (sdkVer >= 21) {
                        Utils.log("Detected SDK Int 21-22, invoke dumpDexV21");
                        dumpDexV21(((Number) mCookie).longValue(), dexDir, sdkVer == 22);
                    } else if (sdkVer >= 19 && isArtInUse()) {
                        Utils.log("Detected Art in SDK Int 19-20 art, invoke dumpDexV19ForArt");
                        dumpDexV19ForArt(((Number) mCookie).longValue(), dexDir);
                    } else {
                        Utils.log("Detected SDK Int 16-20, invoke dumpDexV16");
                        dumpDexV16(((Number) mCookie).longValue(), dexDir);
                    }
                }

            }

            //reverse order, but no other influence.
            loader=loader.getParent();
        } while (!loaders.containsKey(loader));
        ClassTools.clear();
        if (logP != null) {
            logP.getOutputStream().write("^C".getBytes());
            logP.destroy();
        }
        return true;
    }
    //faster than getClass().isArray()
    private static boolean isArray(Object arr){
        return  arr instanceof Object[]|| arr instanceof int[]
                || arr instanceof long[] || arr instanceof byte[]
                || arr instanceof boolean[] ||arr instanceof short[];
    }
    //private static byte[] SHA1Dex(String path,int offset,int write)
    private static boolean fileChannelCopy(File s, File t) {
        FileInputStream fi = null;
        FileOutputStream fo = null;
        FileChannel in = null;
        FileChannel out = null;
        try {
            fi = new FileInputStream(s);
            fo = new FileOutputStream(t);
            in = fi.getChannel();//得到对应的文件通道
            out = fo.getChannel();//得到对应的文件通道
            in.transferTo(0, in.size(), out);//连接两个通道，并且从in通道读取，然后写入out通道
            return true;
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                if (fi != null) fi.close();
                if (in != null) in.close();
                if (fo != null) fo.close();
                if (out != null) out.close();
            } catch (IOException e) {
                e.printStackTrace();
            }

        }
        return false;
    }
    private static boolean isArtInUse() {
        final String vmVersion = System.getProperty("java.vm.version");
        return vmVersion != null && vmVersion.startsWith("2");
    }

    static class DexDumpException extends RuntimeException{
        public DexDumpException(String detailMessage) {
            super(detailMessage);
        }

        public DexDumpException(String detailMessage, Throwable throwable) {
            super(detailMessage, throwable);
        }

        public DexDumpException(Throwable throwable) {
            super(throwable);
        }
    }
}
