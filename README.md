# **DexDumper**
<p>&nbsp;&nbsp;&nbsp;&nbsp; This is just a <b>bridge</b>,for specific app, you may needed to change some code to re-compile the project,
note that it's just a gradle module, and it needs gradle-experimental plugin to compile,
but standard gradle plugin is ok if you change build.gradle.</p>
Actually, the plugin only works when it's running in the targeted process.so there's three way to work with it.
1.invoke it by xposed.
2.replace the true application in the target apk file,load the true application in your custom application,keep the signature if there's a verification.
 But this manner may fail if there's a very strict verification.
3.load the application into this apk with required res,assets,dex file,snd then invoke the dumpdex method.
##Note:
####Since anti-detect is app depended,choose the right manner to deal with it.
Some codes are tagged with todo, so you may need to ammend the code if these three cases are met
1.Total method replacement. the class along with the method is replaced by other class, instance method only, and static methods can be replaced totally without class change
2.Native method replacement. all the java codes are converted to native codes, and the native codes may be encrypted as well,so there may goes a very complex process.
3.Dex loaded by bytes directly by custom classloader. get it by reflection to get the cookie field. if it's stored by native code only, dlopen and dlsym

####Extra:
4.Dalvik support is pretty experimental as I don't know ins replacement rule and never test it in dalvik runtime.'
<p>5. I do suggest you to backsmali the dumped dex to  find out potential problems since some code may be skip due to bugs(there may be some bugs I don't know)
and after replacement, fix the application name in AndroidManifest.xml.</p>
6.Not all of the classes in the dex file is valid and not all the dex file is valid.Generally, the biggest one is the right one,extra dex Files and classes can be ignored.

<br>
<br>
_Memory usage is optimized to a minimum so if you want a faster performance, write a c++ cache class like the ByteOut.java._

_Chinese version introduction is aborted by my slow typing speed. English version is bad due to my poor English.Ha ha!
Some codes are bad enough because it's my first c++ practice,but I am too lazy to'change it._

