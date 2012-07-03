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

#ifndef __REPO_H__
#define __REPO_H__

#include <stdint.h>

#include <string>
#include <set>

#include "tree.h"
#include "commit.h"
#include "object.h"

#define ORI_PATH_DIR "/.ori"
#define ORI_PATH_VERSION "/.ori/version"
#define ORI_PATH_UUID "/.ori/id"
#define ORI_PATH_DIRSTATE "/.ori/dirstate"
#define ORI_PATH_HEAD "/.ori/HEAD"
#define ORI_PATH_LOG "/.ori/ori.log"
#define ORI_PATH_TMP "/.ori/tmp/"
#define ORI_PATH_OBJS "/.ori/objs/"

#define EMPTY_COMMIT "0000000000000000000000000000000000000000000000000000000000000000"

#define LARGEFILE_MINIMUM (1024 * 1024)

class BasicRepo
{
public:
    BasicRepo() {}
    virtual ~BasicRepo() {}

    // Repo information
    // TODO: is this consistent with the model?
    virtual std::string getHead() = 0;

    // Objects
    virtual int getObjectRaw(
            Object::ObjectInfo *info,
            std::string &raw_data) = 0;

    // TODO: add options to query?
    virtual std::set<std::string> listObjects() = 0;

    // TODO: change return value to std::auto_ptr<Object>?
    virtual Object addObjectRaw(
            const Object::ObjectInfo &info,
            const std::string &raw_data) = 0;
    /*virtual Object addObjectRaw(
            Object::ObjectInfo info,
            bytestream *raw_data) = 0;

    virtual std::string addObject(
            Object::ObjectInfo info,
            const std::string &payload
            ) = 0;*/
};

class HistoryCB
{
public:
    virtual ~HistoryCB() { };
    virtual std::string cb(const std::string &commitId, Commit *c) = 0;
};

class Repo : public BasicRepo
{
public:
    Repo(const std::string &root = "");
    ~Repo();
    bool open(const std::string &root = "");
    void close();
    void save();
    // Object Operations
    std::string addSmallFile(const std::string &path);
    std::pair<std::string, std::string>
        addLargeFile(const std::string &path);
    std::pair<std::string, std::string>
        addFile(const std::string &path);
    std::string addBlob(const std::string &blob, Object::Type type);
    std::string addTree(/* const */ Tree &tree);
    std::string addCommit(/* const */ Commit &commit);
    Object getObject(const std::string &objId);
    std::string getPayload(const std::string &objId);
    size_t getObjectLength(const std::string &objId);
    Object::Type getObjectType(const std::string &objId);
    std::string verifyObject(const std::string &objId);
    bool purgeObject(const std::string &objId);
    size_t sendObject(const char *objId);
    bool copyObject(const std::string &objId, const std::string &path);
    std::set<std::string> listObjects();
    Commit getCommit(const std::string &commitId);
    Tree getTree(const std::string &treeId);
    bool hasObject(const std::string &objId);
    // BasicRepo implementation
    int getObjectRaw(Object::ObjectInfo *info, std::string &raw_data);
    Object addObjectRaw(const Object::ObjectInfo &info,
            const std::string &raw_data);
    // Reference Counting Operations
    std::map<std::string, Object::BRState> getRefs(const std::string &objId);
    std::map<std::string, std::map<std::string, Object::BRState> >
        getRefCounts();
    std::map<std::string, std::set<std::string> > computeRefCounts();
    // Pruning Operations
    // void pruneObject(const std::string &objId);
    // Grafting Operations
    std::set<std::string> getSubtreeObjects(const std::string &treeId);
    std::set<std::string> walkHistory(HistoryCB &cb);
    std::string lookup(const Commit &c, const std::string &path);
    std::string graftSubtree(Repo *r,
                             const std::string &srcPath,
                             const std::string &dstPath);
    // Working Directory Operations
    std::string getHead();
    void updateHead(const std::string &commitId);
    // General Operations
    std::string getRootPath();
    std::string getLogPath();
    std::string getTmpFile();
    std::string getUUID();
    std::string getVersion();
    // High Level Operations
    void pull(BasicRepo *r);
    // Static Operations
    static std::string findRootPath(const std::string &path = "");
private:
    // Helper Functions
    void createObjDirs(const std::string &objId);
public: // Hack to enable rebuild operations
    std::string objIdToPath(const std::string &objId);
private:
    // Variables
    std::string rootPath;
    std::string id;
    std::string version;
};

extern Repo repository;

#endif /* __REPO_H__ */

