package com.oslorde.extra;

import android.util.Log;
import android.util.SparseArray;

import com.oslorde.sharedlibrary.Utils;

import java.io.File;
import java.io.FileInputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.WeakHashMap;


public final class ClassTools {
    private static ClassLoader loader;
    private static Map<String,SparseArray<Field>> cachedFTables=new WeakHashMap<>(32);
    private static Map<String,SparseArray<Method>> cachedVTables=new WeakHashMap<>(32);
    public static void setLoader(ClassLoader loader) {
        ClassTools.loader = loader;
    }
    private static synchronized native int getFieldOffset(Field field);
    private static synchronized native int getMethodVIdx(Method method);
    //jni callback
    public static Class findClass(String className){
        try {
            return loader.loadClass(className);//cached by system
        } catch (Throwable e) {
            Utils.log(e.getMessage());
        }
        return null;
    }
    public static synchronized Field getFieldFromOffset(String className, int offset) {
       // Utils.logOslorde("Into get Field Offset c="+className+"offset="+offset);
        SparseArray<Field> fTable;
        if((fTable=cachedFTables.get(className))==null){
            Class cls=findClass(className);
            if(cls==null){
                Utils.logOslorde("Invalid class name f="+className);
                return null;
            }
            fTable=getAllInstanceFields(cls);
            cachedFTables.put(className,fTable);
        }
        return fTable.get(offset);
    }

    public static synchronized String convertMember(Member member){
        if(member instanceof Method){
            Method method= (Method) member;
            StringBuilder retBuilder=new StringBuilder();
            appendClassType(method.getDeclaringClass(),retBuilder);
            retBuilder.append('|');
            retBuilder.append(method.getName());
            retBuilder.append('|');
            Class[] paras=method.getParameterTypes();
            for(Class cl:paras){
                appendClassType(cl,retBuilder);
            }
            return retBuilder.toString();
        }else if(member instanceof Field) {
            Field field= (Field) member;
            StringBuilder retBuilder=new StringBuilder();
            appendClassType(field.getDeclaringClass(),retBuilder);
            retBuilder.append('|');
            retBuilder.append(field.getName());
            retBuilder.append('|');
            appendClassType(field.getType(),retBuilder);
            return retBuilder.toString();
        }
        return null;
    }

    public static synchronized Method getMethodFromIndex(String className, int methodIndex) {
        //Utils.logOslorde("Java getMethod Invoked,clsName="+className+",vIdx="+methodIndex);
        SparseArray<Method> vTable;
        if((vTable=cachedVTables.get(className))==null){
            Class cls=findClass(className);
            if(cls==null){
                Utils.logOslorde("Invalid class name m="+className);
                return null;
            }
            vTable=getAllVMethods(cls);
            cachedVTables.put(className,vTable);
        }
        //Utils.log("Method gotten of index:"+method);
        //retBuilder.append('\0');
        return vTable.get(methodIndex);
    }
    public static byte[] getDexSHA1Hash(String path){
        try {
            File file=new File(path);
            if(!file.canRead()){
                if(!file.setReadable(true)){
                    Log.w("dexdumper","can not set dex file to be readable");
                }
            }
            FileInputStream in = new FileInputStream(path);
            byte[] arr = new byte[in.available()];
            in.read(arr);
            MessageDigest digest = MessageDigest.getInstance("SHA1");
            digest.update(arr, 32, arr.length - 32);
            byte[] hash = digest.digest();
            System.arraycopy(hash,0,arr,12,hash.length);
            return arr;
        }catch (Exception e){
            Utils.logOslorde("DexDumper,Error getSha1value",e);
        }
        return null;
    }
    private static SparseArray<Field> getAllInstanceFields(Class cls){
        ArrayList<Field> list=getAllInstanceFields(cls,null,false);
        SparseArray<Field> fieldOffsetArr=new SparseArray<>();
        for(Field field:list){
            fieldOffsetArr.put(getFieldOffset(field),field);//super class offset must be less.
        }
        System.gc();
        return fieldOffsetArr;
    }
    private static ArrayList<Field> getAllInstanceFields(Class cls,ArrayList<Field> list,boolean isSuper){
        if(list==null) list=new ArrayList<>();
        Class superClass=cls.getSuperclass();
        if(superClass!=null)
            getAllInstanceFields(superClass,list,true);
        Field[] fields=cls.getDeclaredFields();
        for(Field field:fields){
            if(!Modifier.isStatic(field.getModifiers())){
                if(!isSuper||!Modifier.isPrivate(field.getModifiers()))
                    list.add(field);//just for a little memory saving,no filter is ok.
            }
        }
        return list;
    }
    private static SparseArray<Method> getAllVMethods(Class cls){
        ArrayList<Method> list=getAllVMethods(cls,null);
        SparseArray<Method> vIdxArr=new SparseArray<>();
        for(Method method:list){
            vIdxArr.put(getMethodVIdx(method),method);
        }
        System.gc();
        return vIdxArr;
    }
    private static ArrayList<Method> getAllVMethods(Class cls,ArrayList<Method> list){
        if(list==null) list=new ArrayList<>();
        Class superClass=cls.getSuperclass();
        if(superClass!=null)
            getAllVMethods(superClass,list);
        Method[] methods=cls.getDeclaredMethods();
        for(Method method:methods){
            int flags=method.getModifiers();
            if(!Modifier.isPrivate(flags)&&!Modifier.isStatic(flags)){
                //int index=getMethodIndex(list,method);
                //if(index==-1)
                    list.add(method);//as the overriden method share the same method,they will be replace when added into the sparsearray.
                //else list.set(index,method);
            }
        }
        return list;
    }
    @Deprecated
    private static ArrayList<Method> getAllVirtualMethods(Class cls){
        return getAllVirtualMethods(cls,null,null);
    }
    @Deprecated
    private static ArrayList<Method> getAllVirtualMethods(Class cls, ArrayList<Method> list,List<Class> ifTable){
        if(list==null) list=new ArrayList<>();
        if(ifTable==null) ifTable=new ArrayList<>();
        Class superClass=cls.getSuperclass();
        boolean hasSuper=superClass!=null;
        if(hasSuper)
            getAllVirtualMethods(superClass,list,ifTable);
        Method[] methods=cls.getDeclaredMethods();

        for(Method method:methods){
            int flags=method.getModifiers();
            if(!Modifier.isPrivate(flags)&&!Modifier.isStatic(flags)){
                if(hasSuper){
                    //Don't check whether a final method is overrode as an exception will throw at class definition;
                    int index= getMethodIndex(list, method);

                    //Replace super method if it's overrode;
                    if(index!=-1){
                        list.set(index,method);
                        continue;
                    }
                }
                list.add(method);
            }
        }
        //Check whether there are miranda methods as
        //these methods are hidden from the return results
        //check virtual first and interface second
        //only new interface can generate miranda methods
        int idx=ifTable.size();
        getAllInterfaces(cls,ifTable);//In case al the interface of this class are implemented
        if(Modifier.isAbstract(cls.getModifiers())){
            List<Method> mirandaMethods=new ArrayList<>();
            for(int newSize=ifTable.size();idx<newSize;++idx){
                Class inter=ifTable.get(idx);
                methods= inter.getDeclaredMethods();
                out:
                for(Method m:methods){
                    if(getMethodIndex(list,m)!=-1){
                        for(Method mir:mirandaMethods){
                            if(isTheSameNameAndSig(m,mir))
                                continue out;
                        }
                        mirandaMethods.add(m);//which first add which,no restriction in return type.
                    }
                }

            }
        }
        return list;
    }

    private static int getMethodIndex(ArrayList<Method> list, Method method) {
        return indexOf(list, method, new Weigher<Method>() {
            @Override
            public boolean isTheSameWeight(Method f, Method s) {
                int modifier=s.getModifiers();
                Class firstClass=f.getDeclaringClass();
                Class secClass=s.getDeclaringClass();
                return isTheSameNameAndSig(f, s) && firstClass != secClass
                        //In case
                        // 1. the return type from some methods from interfaces are changed(From the interface type to the class type)
                        //   and stub methods are generated by complier.
                        //2.bridge method generated from generic programming.
                        && (Modifier.isProtected(modifier) || Modifier.isPublic(modifier) ||
                        firstClass.getClassLoader().equals(secClass.getClassLoader())
                                && getPackageName(firstClass).equals(getPackageName(secClass)));
                //cannot override friendly method that is not in the same package since 4.1 above
                // but the method is designed for art only,and shall we ignore classloader restriction?
            }
        });
    }

    private static boolean isTheSameNameAndSig(Method f, Method s) {
        return f.getName().equals(s.getName()) && Arrays.equals(
                f.getParameterTypes(), s.getParameterTypes());
    }

    private static List<Class> getAllInterfaces(Class cls,List<Class> list){
        if(list==null) list=new ArrayList<>();
        Class[] interfaces=cls.getInterfaces();
        for(Class c:interfaces){
            if(!list.contains(c))
                list.add(c);//don't use hash set to maintain order
            getAllInterfaces(c,list);
        }
        return list;
    }
    public static String getPackageName(Class cls) {
        String name = cls.getName();
        int last = name.lastIndexOf('.');
        return last == -1 ? "" : name.substring(0, last);
    }
    private static <T> int indexOf(ArrayList<T> list,T member,Weigher<T> weigher){
        for(int i=0,N=list.size();i<N;++i){
            if(weigher.isTheSameWeight(member,list.get(i))){
                return i;
            }
        }
        return -1;
    }
    private interface Weigher<T>{
        boolean isTheSameWeight(T first, T sec);
    }

    private static void appendClassType(Class type, StringBuilder builder){
        Class component ;
        while ((component=type.getComponentType())!=null){
            builder.append('[');
            type=component;
        }
        if(type.isPrimitive()){
            if(type==int.class) builder.append('I');
            else if(type==boolean.class) builder.append('Z');
            else if(type==long.class) builder.append('J');
            else if(type==byte.class) builder.append('B');
            else if(type==float.class) builder.append('F');
            else if(type==double.class) builder.append('D');
            else if(type==char.class) builder.append('C');
            else if(type==short.class) builder.append('S');
            else if(type==void.class) builder.append('V');
        } else builder.append('L').append(type.getName().replace('.','/')).append(';');
    }

    /*public static byte[] getMethodProtoString(Method method){
        StringBuilder retBuilder=new StringBuilder();
        Class[] paraTypes=method.getParameterTypes();
        for(Class type:paraTypes){
           appendClassType(type,retBuilder);
        }
        retBuilder.append('\0');
        return retBuilder.toString().getBytes();
    }*/
}
