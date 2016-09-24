c:
cd \Windows
adb -s e17eccb install -r E:\AppProjects\HookManager\btamodel\dexdumpmodule-release.apk
adb -s e17eccb shell am start -n com.oslorde.dexdumper/com.oslorde.extra.MainActivity -a android.intent.action.MAIN -c android.intent.category.LAUNCHER
exit