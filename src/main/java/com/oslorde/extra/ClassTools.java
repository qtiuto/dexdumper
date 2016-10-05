package com.oslorde.extra;

import android.util.SparseArray;

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
    private static ByteOut byteOut;
    private static ClassLoader loader;
    private static ClassLoader systemClassLoader;
    private static Map<String, SparseArray<Field>> cachedFTables;
    private static Map<String, SparseArray<Method>> cachedVTables;

    private static native int getFieldOffset(Field field);

    private static native int getMethodVIdx(Method method);

    private static native byte[][] getVMethodsNative(Member reflectTarget, boolean isField);//getDeclaredMethods is not reliable and stable,
    // Maybe a custom designed method is required, not implemented now

    static void init(ClassLoader loader) {
        ClassTools.loader = loader;
        systemClassLoader = String.class.getClassLoader();
        byteOut = new ByteOut();
        cachedFTables = new WeakHashMap<>(32);
        cachedVTables = new WeakHashMap<>(32);
    }

    static void clear() {
        byteOut = null;
        cachedFTables = null;
        cachedVTables = null;
    }
    //jni callback
    private static Class findClass(String className) {
        try {
            return Class.forName(className, false, loader);//in case loadClass does not work for array classes
            // as I'm not sure will there be an implementation doesn't allow loading array classes directly;
            // true may block thread
        } catch (Throwable e) {
            Utils.log(e.getMessage());
        }
        return null;
    }

    /**
     * In order the get the field from the ids fastest,we consider the declared class as the best.No overriding!
     */
    public static byte[] getFieldFromOffset(String className, int offset) {
       // Utils.logOslorde("Into get Field Offset c="+className+"offset="+offset);
        Class cls = findClass(className);
        if (cls == null) {
            Utils.logE("Invalid class name for field:" + className);
            return null;
        }
        SparseArray<Field> fTable;
        if((fTable=cachedFTables.get(className))==null){

            fTable=getAllInstanceFields(cls);
            synchronized (cachedFTables) {
                cachedFTables.put(className, fTable);}

        }
        Field field = fTable.get(offset);
        if (field == null) return null;
        ByteOut out = byteOut;//I think that concurrence possibility is low
        synchronized (out) {
            Class topClass = field.getDeclaringClass();
            appendClassType(topClass, out);
            out.writeAscii('|');
            writeMUtf8(field.getName(), out);
            out.writeAscii('|');
            appendClassType(field.getType(), out);
            if (topClass != cls) {
                ArrayList<Class> belows = new ArrayList<>(4);
                belows.add(0, cls);
                while ((cls = cls.getSuperclass()) != topClass) belows.add(cls);
                for (int i = 0, N = belows.size(); i < N; ++i) {
                    Class belowClass = belows.get(i);
                    out.writeAscii('|');
                    appendClassType(belowClass, out);
                }
            }
            out.writeAscii('\0');
            byte[] ret = out.toByteArray();
            out.reset();
            return ret;
        }
    }

    /**
     * For virtual method,the method we get is actually the topmost method, so we consider the lowest class as the best to avoid overriding
     */
    public static byte[] getMethodFromIndex(String className, int methodIndex) {
        Class cls = findClass(className);
        if (cls == null) {
            Utils.logE("Invalid class name for method:" + className);
            return null;
        }
        SparseArray<Method> vTable;
        if ((vTable = cachedVTables.get(className)) == null) {
            vTable = getAllVMethods(cls);
            synchronized (cachedVTables) {
                cachedVTables.put(className, vTable);}
        }
        Method vMethod = vTable.get(methodIndex);
        if (vMethod == null) return null;
        ByteOut out = byteOut;
        synchronized (out) {
            Class topClass = vMethod.getDeclaringClass();
            appendClassType(cls, out);
            out.writeAscii('|');
            writeMUtf8(vMethod.getName(), out);
            out.writeAscii('|');
            appendClassType(vMethod.getReturnType(), out);
            out.writeAscii('|');
            Class[] paras = vMethod.getParameterTypes();
            for(Class cl:paras){
                appendClassType(cl, out);
            }
            if (cls.isInterface()) cls = int[].class;// act as sub class of Object;
            if (topClass != cls) {
                ArrayList<Class> supers = new ArrayList<>(4);
                while ((cls = cls.getSuperclass()) != topClass) supers.add(cls);
                supers.add(topClass);
                for (Class superClass : supers) {
                    out.writeAscii('|');
                    appendClassType(superClass, out);
                }
            }
            out.writeAscii('\0');
            byte[] ret = out.toByteArray();
            out.reset();
            return ret;
        }
    }

    //java is utf16 encoding
    public static void writeMUtf8(String src, ByteOut out) {
        int len = src.length();
        char c;
        for (int i = 0; i < len; ++i) {
            c = src.charAt(i);
            if (c != 0 && c <= 127) {
                out.writeAscii(c);
            } else {
                if (c <= 2047) {
                    out.write((byte) (0xc0 | (0x1f & (c >> 6))));
                    out.write((byte) (0x80 | (0x3f & c)));
                } else {
                    out.write((byte) (0xe0 | (0x0f & (c >> 12))));
                    out.write((byte) (0x80 | (0x3f & (c >> 6))));
                    out.write((byte) (0x80 | (0x3f & c)));
                }
            }
        }
    }

    //Convenience for native callback
    public static byte[] getDexSHA1Hash(String path){
        try {
            File file=new File(path);
            if(!file.canRead()){
                if(!file.setReadable(true)){
                    Utils.logE("can not set dex file to be readable");
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
            Utils.log("Error getSha1value", e);
        }
        return null;
    }

    private static SparseArray<Field> getAllInstanceFields(Class cls){
        ArrayList<Field> list = getAllInstanceFieldsNotCall(cls, null, false);
        SparseArray<Field> fieldOffsetArr=new SparseArray<>();
        for(Field field:list){
            fieldOffsetArr.put(getFieldOffset(field),field);//super class offset must be less.
        }
        return fieldOffsetArr;
    }

    private static ArrayList<Field> getAllInstanceFieldsNotCall(Class cls, ArrayList<Field> list, boolean isSuper) {
        if(list==null) list=new ArrayList<>();
        Class superClass=cls.getSuperclass();
        if(superClass!=null)
            getAllInstanceFieldsNotCall(superClass, list, true);
        try {
            Field[] fields = cls.getDeclaredFields();
            for (Field field : fields) {
                if (!Modifier.isStatic(field.getModifiers())) {
                    if (!isSuper || !Modifier.isPrivate(field.getModifiers()))
                        list.add(field);//just for a little memory saving,no filter is ok.
                }
            }
        } catch (NoClassDefFoundError e) {
            Utils.log(e);
        }
        return list;
    }

    private static SparseArray<Method> getAllVMethods(Class cls){
        if (cls.isArray() || cls.isInterface()) cls = Object.class;
        ArrayList<Method> list = getAllVMethodsNotCall(cls, null);
        SparseArray<Method> vIdxArr=new SparseArray<>();
        for(Method method:list){
            vIdxArr.put(getMethodVIdx(method),method);
        }
        return vIdxArr;
    }

    private static ArrayList<Method> getAllVMethodsNotCall(Class cls, ArrayList<Method> list) {
        if(list==null) list=new ArrayList<>();
        do {
            Method[] methods;
            try {
                methods = cls.getDeclaredMethods();
                for (Method method : methods) {
                    int flags = method.getModifiers();
                    if (!Modifier.isPrivate(flags) && !Modifier.isStatic(flags)) {
                        //int index=getMethodIndex(list,method);
                        //if(index==-1)
                        list.add(method);//as the overriden method share the same method,they will be replace when added into the sparsearray.
                        //else list.set(index,method);
                    }
                }
            } catch (NoClassDefFoundError e) {
                //In some cases, class of newer api level is  the return type or one of parameter types;
                //then NoClassDefFoundError is thrown;
                Utils.log(e);
            }

        } while ((cls = cls.getSuperclass()) != null);

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

    private static void appendClassType(Class type, ByteOut out) {
        Class component;
        while ((component = type.getComponentType()) != null) {
            out.writeAscii('[');
            type = component;
        }
        if (type.isPrimitive()) {
            if (type == int.class) out.writeAscii('I');
            else if (type == boolean.class) out.writeAscii('Z');
            else if (type == long.class) out.writeAscii('J');
            else if (type == byte.class) out.writeAscii('B');
            else if (type == float.class) out.writeAscii('F');
            else if (type == double.class) out.writeAscii('D');
            else if (type == char.class) out.writeAscii('C');
            else if (type == short.class) out.writeAscii('S');
            else if (type == void.class) out.writeAscii('V');
        } else {
            out.writeAscii('L');
            writeMUtf8(type.getName().replace('.', '/'), out);
            out.writeAscii(';');
        }
    }

    private static boolean isSystemClass(Class cls) {
        return cls.getClassLoader() == systemClassLoader;
    }

    private interface Weigher<T> {
        boolean isTheSameWeight(T first, T sec);
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
