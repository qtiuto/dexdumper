APP_ABI := arm64-v8a,armeabi-v7a,x86,x86_64
APP_PLATFORM := android-17
#NDK_TOOLCHAIN_VERSION := clang
APP_CPPFLAGS := -fexceptions -frtti#允许异常功能，及运行时类型识别
APP_CPPFLAGS +=-std=c++11 #允许使用c++11的函数等功能
APP_CPPFLAGS +=-g2
APP_CPPFLAGS +=-fpermissive  #此项有效时表示宽松的编译形式，比如没有用到的代码中有错误也可以通过编译
APP_STL :=c++_static
APP_ALLOW_MISSING_DEPS :=true