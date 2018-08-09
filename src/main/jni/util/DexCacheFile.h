//
// Created by asus on 2016/9/22.
//
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#ifndef HOOKMANAGER_DEXOUTFILE_H
#define HOOKMANAGER_DEXOUTFILE_H


class DexCacheFile {
    bool shouldFlush = false;
    int fd;
    const uint32_t fileSize;
    uint32_t _offset = 0;
    char *buf;

public:
    DexCacheFile(int _fd, uint32_t _fileSize);

    DexCacheFile(const DexCacheFile &dexCacheFile) = delete;

    void operator=(DexCacheFile &dexCacheFile) = delete;


    inline uint32_t tell() { return _offset; }

    inline void seek(uint32_t offset_) { seek(offset_, SEEK_SET); }

    inline void offset(uint32_t offset_) {
        seek(offset_, SEEK_CUR);
    }

    inline uint8_t *getCache(uint32_t offset) {
        if (offset >= fileSize) return nullptr;
        return (uint8_t *) (buf + offset);
    }

    void flush();

    bool seek(uint32_t offset_, int start);

    bool write(const void *src, size_t size);

    bool pwrite(const void *src, size_t size, uint32_t offset);

    ~DexCacheFile();

};


#endif //HOOKMANAGER_DEXOUTFILE_H
