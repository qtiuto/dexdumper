//
// Created by Karven on 2016/10/14.
//

#ifndef DEXDUMP_SPINLOCK_H
#define DEXDUMP_SPINLOCK_H

#include <atomic>

class SpinLock {
    std::atomic_flag locked = ATOMIC_FLAG_INIT ;
public:
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) { ; }
    }
    void unlock() {
        locked.clear(std::memory_order_release);
    }
};
#endif //DEXDUMP_SPINLOCK_H
