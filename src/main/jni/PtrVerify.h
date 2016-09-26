#ifndef HOOKMANAGER_PTRVERIFY_H
#define HOOKMANAGER_PTRVERIFY_H

#include <android/log.h>

extern int isbadreadptr(const void *ptr, int length);

int isbadwriteptr(const void *ptr, int length);

template<typename T>
int isBadPtr(T *ptr) {
    return isbadreadptr(ptr, sizeof(T));
}

template<typename T>
int isBadWritePtr(T *ptr) {
    return isbadwriteptr(ptr, sizeof(T));
}

#endif //HOOKMANAGER_PTRVERIFY_H
