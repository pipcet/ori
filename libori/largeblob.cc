/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <cstdlib>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <errno.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <openssl/sha.h>

#ifdef OPENSSL_NO_SHA256
#error "SHA256 not supported!"
#endif

#include "debug.h"
#include "util.h"
#include "largeblob.h"
#include "chunker.h"

using namespace std;

/********************************************************************
 *
 *
 * LBlobEntry
 *
 *
 ********************************************************************/

LBlobEntry::LBlobEntry(const LBlobEntry &l)
    : hash(l.hash), length(l.length)
{
}

LBlobEntry::LBlobEntry(const ObjectHash &h, uint16_t l)
    : hash(h), length(l)
{
}

LBlobEntry::~LBlobEntry()
{
}

/********************************************************************
 *
 *
 * LargeBlob
 *
 *
 ********************************************************************/

LargeBlob::LargeBlob(Repo *r)
{
    repo = r;
}

LargeBlob::~LargeBlob()
{
}

class FileChunkerCB : public ChunkerCB
{
public:
    FileChunkerCB(LargeBlob *l)
    {
	lb = l;
	lbOff = 0;
        buf = NULL;
    }
    ~FileChunkerCB()
    {
        if (buf)
            delete[] buf;
        if (srcFd > 0)
            ::close(srcFd);
    }
    int open(const string &path)
    {
	struct stat sb;

        bufLen = 8 * 1024 * 1024;
	buf = new uint8_t[bufLen];
	if (buf == NULL)
	    return -ENOMEM;

	srcFd = ::open(path.c_str(), O_RDONLY);
	if (srcFd < 0)
	    return -errno;

	if (fstat(srcFd, &sb) < 0) {
	    close(srcFd);
	    return -errno;
	}
	fileLen = sb.st_size;
        fileOff = 0;

	return 0;
    }
    virtual void match(const uint8_t *b, uint32_t l)
    {
        // Add the fragment into the repository
        // XXX: Journal for cleanup!
        string blob = string((const char *)b, l);
        ObjectHash hash = Util_HashString(blob);
        lb->repo->addObject(ObjectInfo::Blob, hash, blob);

        // Add the fragment to the LargeBlob object.
	lb->parts.insert(make_pair(lbOff, LBlobEntry(hash, l)));
	lbOff += l;
    }
    virtual int load(uint8_t **b, uint64_t *l, uint64_t *o)
    {
        if (*b == NULL)
            *b = buf;

        if (fileOff == fileLen)
            return 0;

        // Sanity checking
        ASSERT(*b == buf);
        ASSERT(*l <= bufLen);
        ASSERT(*o <= bufLen);

        if (*o != 0 || *o != *l) {
            ASSERT(*o > 32); // XXX: Must equal hashLen
            ASSERT(*l > 32);
            memcpy(buf, buf + *o - 32, *l - *o + 32);
            *l = *l - *o + 32;
            *o = 32;
        }

        uint64_t toRead = MIN(bufLen - *l, fileLen - fileOff);
        int status;

        status = read(srcFd, buf + *l, toRead);
        if (status < 0) {
            perror("Cannot read large file");
            PANIC();
            return -1;
        }
        ASSERT(status == (int)toRead);

        fileOff += status;
        *l += status;
        //*o = 32;

	return 1;
    }
private:
    // Output large blob
    LargeBlob *lb;
    uint64_t lbOff;
    // Input file
    int srcFd;
    uint64_t fileLen;
    uint64_t fileOff;
    // RK buffer
    uint8_t *buf;
    uint64_t bufLen;
};

void
LargeBlob::chunkFile(const string &path)
{
    int status;
    FileChunkerCB cb = FileChunkerCB(this);
    Chunker<4096, 2048, 8192> c = Chunker<4096, 2048, 8192>();

    status = cb.open(path);
    if (status < 0) {
	perror("Cannot open large file for chunking");
	PANIC();
	return;
    }

    totalHash = Util_HashFile(path);

    c.chunk(&cb);
}

void
LargeBlob::extractFile(const string &path)
{
    int fd;
    map<uint64_t, LBlobEntry>::iterator it;

    fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
	        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        perror("Cannot open file for writing");
        PANIC();
        return;
    }

    for (it = parts.begin(); it != parts.end(); it++)
    {
        int status;
        string tmp;

        Object::sp o(repo->getObject((*it).second.hash));
        tmp = o->getPayload();
        ASSERT(tmp.length() == (*it).second.length);

        status = ::write(fd, tmp.data(), tmp.length());
        if (status < 0) {
            perror("write to large object failed");
            PANIC();
            return;
        }

        ASSERT(status == (int)tmp.length());
    }

#ifdef DEBUG
    ObjectHash extractedHash = Util_HashFile(path);
    ASSERT(extractedHash == totalHash);
#endif /* DEBUG */
}

ssize_t
LargeBlob::read(uint8_t *buf, size_t s, off_t off) const
{
    // TODO: this is not as clean as I would like
    map<uint64_t, LBlobEntry>::const_iterator it =
        parts.upper_bound(off);
    it--;

    if (it == parts.end()) {
        LOG("offset %lu larger than large blob", off);
        return 0;
    }

    off_t part_off = off - (*it).first;
    if (part_off >= (*it).second.length) {
        LOG("offset %lu larger than last blob in LB", off);
        return 0;
    }
    if (part_off < 0) {
        LOG("part_off less than 0");
        return -EIO;
    }

    int left = (*it).second.length - part_off;
    if (left <= 0) {
        LOG("incorrect computation of left! (%d)", left);
        return -EIO;
    }

    size_t to_read = MIN((size_t)left, s);

    Object::sp o(repo->getObject((*it).second.hash));
    ASSERT(o->getInfo().type == ObjectInfo::Blob);
    const std::string &payload = o->getPayload();
    memcpy(buf, payload.data()+part_off, to_read);

    return to_read;
}

const string
LargeBlob::getBlob()
{
    strwstream ss;
    ss.writeHash(totalHash);

    size_t num = parts.size();
    ss.writeInt(num);

    for (map<uint64_t, LBlobEntry>::iterator it = parts.begin();
            it != parts.end();
            it++)
    {
        ss.writeHash((*it).second.hash);
        ss.writeInt((*it).second.length);
    }

    return ss.str();
}

void
LargeBlob::fromBlob(const string &blob)
{
    strstream ss(blob);
    ss.readHash(totalHash);

    size_t num = ss.readInt<size_t>();

    uint64_t off = 0;
    for (size_t i = 0; i < num; i++) {
        ObjectHash hash;
        ss.readHash(hash);
        size_t length = ss.readInt<uint16_t>();

        parts.insert(make_pair(off, LBlobEntry(hash, length)));

        off += length;
    }
}

size_t
LargeBlob::totalSize() const
{
    size_t total = 0;
    for (map<uint64_t, LBlobEntry>::const_iterator it = parts.begin();
            it != parts.end(); it++)
    {
        total += (*it).second.length;
    }

    return total;
}

