package com.oslorde.extra;

import android.os.Build;
import android.os.Environment;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.util.Arrays;
import java.util.List;
import java.util.ListIterator;
import java.util.zip.ZipFile;

import dalvik.system.BaseDexClassLoader;
import dalvik.system.DexClassLoader;
import dalvik.system.DexFile;

public class DexLoader {

    static final String TAG = "DexLoader";

    private DexLoader() {
    }
    public static int install(String dexPath,String outputDir){
        File dexOutputDir=new File(outputDir);
        if(dexOutputDir.exists()&&dexOutputDir.isFile()&&!dexOutputDir.delete()){
            Log.e(TAG,"outputDir is occupied ");
            return 0;
        }
        if(!dexOutputDir.exists()&&!dexOutputDir.mkdir()){
            Log.e(TAG, "make dir failed");
            return 0;
        }
        try {
            return installSecondaryDexes( dexOutputDir.getAbsolutePath(), dexPath);
        } catch (Exception e) {
            Log.e(TAG, Log.getStackTraceString(e));
        }
        return 0;
    }
    public static int install(String dexPath,String currentPackage,String installPackage){
        String dexOutputDir=Environment.getDataDirectory().getAbsolutePath()+"/data/"+currentPackage+"/dexOutput_"+installPackage;
        return install(dexPath,dexOutputDir);
    }
    private static int installSecondaryDexes( String dexOutputDir,
                                             String dexPath) throws IllegalArgumentException,
            IllegalAccessException, NoSuchFieldException,
            InvocationTargetException, NoSuchMethodException, IOException, ClassNotFoundException {
        if(Build.VERSION.SDK_INT>4) {
            DexClassLoader dexClassLoader = new DexClassLoader(dexPath, dexOutputDir, dexPath, getBaseDexClassLoader());
            Object baseDexElements = getDexElements(getPathList(getBaseDexClassLoader()));

            Object newDexElements = getDexElements(getPathList(dexClassLoader));
            Object allDexElements = combineArray(newDexElements, baseDexElements);
            Object pathList = getPathList(getBaseDexClassLoader());
            findField(pathList.getClass(),"dexElements").set(pathList,allDexElements);
            return Array.getLength(newDexElements);
        } else {
            File dexFiles=new File(dexPath);
            if(dexFiles.isDirectory())
                return installV4(getBaseDexClassLoader(), Arrays.asList(dexFiles.listFiles()));
            else return  installV4(getBaseDexClassLoader(),Arrays.asList(dexFiles));
        }
    }
    private static Object combineArray(Object firstArray, Object secondArray) {
        Class<?> localClass = firstArray.getClass().getComponentType();
        int firstArrayLength = Array.getLength(firstArray);
        int allLength = firstArrayLength + Array.getLength(secondArray);
        Object result = Array.newInstance(localClass, allLength);
        for (int k = 0; k < allLength; ++k) {
            if (k < firstArrayLength) {
                Array.set(result, k, Array.get(firstArray, k));
            } else {
                Array.set(result, k, Array.get(secondArray, k - firstArrayLength));
            }
        }
        return result;
    }

    private static Field findField(Class srcClass, String name)
            throws NoSuchFieldException {
        for (Class<?> clazz = srcClass; clazz != null; clazz = clazz
                .getSuperclass()) {
            try {
                Field field = clazz.getDeclaredField(name);
                if (!field.isAccessible()) {
                    field.setAccessible(true);
                }
                return field;
            } catch (NoSuchFieldException e) {
                // ignore and search next
            }
        }
        throw new NoSuchFieldException("Field " + name + " not found in "
                + srcClass);
    }


    /*private static Method findMethod(Object instance, String name,
                                     Class<?>... parameterTypes) throws NoSuchMethodException {
        for (Class<?> clazz = instance.getClass(); clazz != null; clazz = clazz
                .getSuperclass()) {
            try {
                Method method = clazz.getDeclaredMethod(name, parameterTypes);
                if (!method.isAccessible()) {
                    method.setAccessible(true);
                }
                return method;
            } catch (NoSuchMethodException e) {
                // ignore and search next
            }
        }
        throw new NoSuchMethodException("Method " + name + " with parameters "
                + Arrays.asList(parameterTypes) + " not found in "
                + instance.getClass());
    }*/

    public static boolean shrinkDex(ClassLoader classLoader,int length){
        try {
            Object dexPathList =  getPathList(classLoader);
            Field jlrField = findField(Class.forName("dalvik.system.BaseDexClassLoader"), "dexElements");
            Object[] combined = (Object[]) jlrField.get(dexPathList);
            Object[] original= (Object[]) Array.newInstance(combined.getClass().getComponentType(),combined.length-length);
            System.arraycopy(combined,0,original,0,original.length);
            jlrField.set(dexPathList,original);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    }

    private static void expandFieldArray(Object instance, String fieldName,
                                         Object[] extraElements) throws NoSuchFieldException,
            IllegalArgumentException, IllegalAccessException {
        Field jlrField = findField(instance.getClass(), fieldName);
        Object[] original = (Object[]) jlrField.get(instance);
        Object[] combined = (Object[]) Array.newInstance(original.getClass()
                .getComponentType(), original.length + extraElements.length);
        System.arraycopy(original, 0, combined, 0, original.length);
        System.arraycopy(extraElements, 0, combined, original.length,
                extraElements.length);
        Log.i(TAG, "installDex 4");
        jlrField.set(instance, combined);
        Log.i(TAG, "installDex 5");
    }
    private static BaseDexClassLoader getBaseDexClassLoader() {
        return (BaseDexClassLoader) DexLoader.class.getClassLoader();
    }

    private static Object getDexElements(Object paramObject)
            throws IllegalArgumentException, NoSuchFieldException, IllegalAccessException {
        return findField(paramObject.getClass(), "dexElements").get(paramObject);
    }

    private static Object getPathList(Object baseDexClassLoader)
            throws IllegalArgumentException, NoSuchFieldException, IllegalAccessException, ClassNotFoundException {
        return findField(Class.forName("dalvik.system.BaseDexClassLoader"), "pathList").get(baseDexClassLoader);
    }

    /**
     * Installer for platform versions 4 to 13.
     */
        private static int installV4(ClassLoader loader,
                                    List<File> additionalClassPathEntries)
                throws IllegalArgumentException, IllegalAccessException,
                NoSuchFieldException, IOException {
                /*
                 * The patched class loader is expected to be a descendant of
                 * dalvik.system.DexClassLoader. We modify its fields mPaths,
                 * mFiles, mZips and mDexs to append additional DEX file entries.
                 */
           int extraSize = additionalClassPathEntries.size();
            Field pathField = findField(loader.getClass(), "path");
            StringBuilder path = new StringBuilder(
                    (String) pathField.get(loader));
            String[] extraPaths = new String[extraSize];
            File[] extraFiles = new File[extraSize];
            ZipFile[] extraZips = new ZipFile[extraSize];
            DexFile[] extraDexs = new DexFile[extraSize];
            for (ListIterator<File> iterator = additionalClassPathEntries
                    .listIterator(); iterator.hasNext();) {
                File additionalEntry = iterator.next();
                String entryPath = additionalEntry.getAbsolutePath();
                path.append(':').append(entryPath);
                int index = iterator.previousIndex();
                extraPaths[index] = entryPath;
                extraFiles[index] = additionalEntry;
                extraZips[index] = new ZipFile(additionalEntry);
                extraDexs[index] = DexFile.loadDex(entryPath, entryPath
                        + ".dex", 0);
            }
            pathField.set(loader, path.toString());
            expandFieldArray(loader, "mPaths", extraPaths);
            expandFieldArray(loader, "mFiles", extraFiles);
            expandFieldArray(loader, "mZips", extraZips);
            expandFieldArray(loader, "mDexs", extraDexs);
            return extraSize;
        }


}