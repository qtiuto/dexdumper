//
// Created by Karven on 2016/9/25.
//

#include "PtrVerify.h"
#include <signal.h>
#include <setjmp.h>

/*
x86/Linux、x86/Solaris、SPARC/Solaris will sigal SIGSEGV
x86/FreeBSD、x86/NetBSD、x86/OpenBSD MacOS will sigal SIGBUS
*/

#if defined(__MACH__) && defined(__FreeBSD__) && defined(__NetBSD__) && defined(__OpenBSD__)\
 && defined(__DragonFly__)
#define ERROR_SIGNAL SIGBUS
#else
#define ERROR_SIGNAL SIGSEGV
#endif
static sigjmp_buf badreadjmpbuf;


static void badreadfunc(int signo) {
    /*write(STDOUT_FILENO, "catch\n", 6);*/
    siglongjmp(badreadjmpbuf, 1);
}

int isbadwriteptr(const void *ptr, int length) {
    struct sigaction sa, osa;
    int ret = 0;

    /*init new handler struct*/
    sa.sa_handler = badreadfunc;
    memset(&sa.sa_mask, 0, sizeof(sa.sa_mask));
    sa.sa_flags = 0;

    /*retrieve old and set new handlers*/
    if (sigaction(ERROR_SIGNAL, &sa, &osa) < 0)
        return (-1);

    if (sigsetjmp(badreadjmpbuf, 1) == 0) {
        int i, hi = length / sizeof(int), remain = length % sizeof(int);
        int *pi = (int *) ptr;
        char *pc = (char *) ptr + hi;
        for (i = 0; i < hi; i++) {

            int value = pi[i];
            pi[i] = value;
        }
        for (i = 0; i < remain; i++) {
            char value = pc[i];
            pc[i] = value;
        }
    }
    else {
        ret = 1;
    }

    /*restore prevouis signal actions*/
    if (sigaction(ERROR_SIGNAL, &osa, NULL) < 0)
        return (-1);

    return ret;
}

int isbadreadptr(const void *ptr, int length) {
    struct sigaction sa, osa;
    int ret = 0;

    /*init new handler struct*/
    sa.sa_handler = badreadfunc;
    memset(&sa.sa_mask, 0, sizeof(sa.sa_mask));
    sa.sa_flags = 0;

    /*retrieve old and set new handlers*/
    if (sigaction(ERROR_SIGNAL, &sa, &osa) < 0)
        return (-1);

    if (sigsetjmp(badreadjmpbuf, 1) == 0) {
        int i, hi = length / sizeof(int), remain = length % sizeof(int);
        int *pi = (int *) ptr;
        char *pc = (char *) ptr + hi;
        for (i = 0; i < hi; i++) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
            *(pi + i);
#pragma clang diagnostic pop
        }
        for (i = 0; i < remain; i++) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
            *(pc + i);
#pragma clang diagnostic pop
        }
    }
    else {
        ret = 1;
    }

    /*restore prevouis signal actions*/
    if (sigaction(ERROR_SIGNAL, &osa, NULL) < 0)
        return (-1);

    return ret;
}
