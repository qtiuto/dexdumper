//
// Created by asus on 2016/9/22.
//

#include "DexCacheFile.h"


bool DexCacheFile::seek(uint32_t offset_, int start) {
    switch (start) {
        case SEEK_CUR:
            _offset += offset_;
            if (_offset > fileSize)
                throw "SEEK_CUR index out";
            break;
        case SEEK_END:
            _offset = fileSize - offset_ - 1;
            if (_offset >= fileSize) throw "SEEK_END index out";
            break;
        case SEEK_SET:
            if (offset_ >= fileSize)
                throw "SEEK_SET index out";
            _offset = offset_;
            break;
        default: {
            return false;
        }
    }
    return true;

}

static inline ssize_t cWrite(int fd, const void *buf, size_t size) {
    return pwrite(fd, buf, size, 0);
}

void DexCacheFile::flush() {
    cWrite(fd, buf, fileSize);
    fsync(fd);
    shouldFlush = false;
}

DexCacheFile::DexCacheFile(int _fd, uint32_t _fileSize) : fd(dup(_fd)), fileSize(_fileSize) {
    //No openCheck as this is checked before;
    buf = new char[fileSize];
    pread(_fd, buf, fileSize, 0);
}

bool DexCacheFile::write(const void *src, size_t size) {
    if (_offset + size > fileSize)
        throw "Write index out";
    memcpy(buf + _offset, src, size);
    _offset += size;
    shouldFlush = true;
    return true;
}

bool DexCacheFile::pwrite(const void *src, size_t size, uint32_t offset) {
    if (offset + size > fileSize)
        throw "Write index out";
    memcpy(buf + offset, src, size);
    shouldFlush = true;
    return true;
}

DexCacheFile::~DexCacheFile() {
    if (shouldFlush) flush();
    close(fd);
    delete[] buf;
}