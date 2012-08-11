
#define _WITH_DPRINTF

#include <cstring>
#include <stdbool.h>
#include <stdint.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <map>

#include "debug.h"
#include "server.h"
#include "util.h"
#include "localrepo.h"

extern LocalRepo repository;

SshServer::SshServer()
{
}

/*typedef std::vector<char *> args_vec;
args_vec sep_args(char *str) {
    size_t len = strlen(str);
    args_vec rval;

    size_t last = 0;
    for (size_t i = 1; i < len; i++) {
        if (str[i] == ' ' || str[i] == '\n') {
            str[i] = '\0';
            rval.push_back(str+last);
            last = i+1;
        }
    }
    //rval.push_back(str+last);

    return rval;
}*/

#define OK 0
#define ERROR 1

void printError(const std::string &what)
{
    fdwstream fs(STDOUT_FILENO);
    fs.writeInt<uint8_t>(ERROR);
    fs.writePStr(what);
    fflush(stdout);
}

void SshServer::serve() {
    fdstream fs(STDIN_FILENO, -1);

    LocalRepoLock::ap lock(repository.lock());
    if (!lock.get()) {
        printError("Couldn't lock repo");
        exit(1);
    }

    printf("READY\n");
    fflush(stdout);
    fsync(STDOUT_FILENO);

    while (true) {
        // Get command
        std::string command;
        if (fs.readPStr(command) == 0) break;

        if (command == "hello") {
            cmd_hello();
        }
        else if (command == "list objs") {
            cmd_listObjs();
        }
        else if (command == "list commits") {
            cmd_listCommits();
        }
        else if (command == "readobjs") {
            cmd_readObjs();
        }
        else if (command == "get head") {
            cmd_getHead();
        }
        else {
            printError("Unknown command");
        }

        fflush(stdout);
        fsync(STDOUT_FILENO);
    }

    fflush(stdout);
    fsync(STDOUT_FILENO);
}

void SshServer::cmd_hello()
{
    fdwstream fs(STDOUT_FILENO);
    fs.writeInt<uint8_t>(OK);
    fs.writePStr(ORI_PROTO_VERSION);
}

void SshServer::cmd_listObjs()
{
    fdwstream fs(STDOUT_FILENO);
    fs.writeInt<uint8_t>(OK);

    std::set<ObjectInfo> objects = repository.listObjects();
    fs.writeInt<uint64_t>(objects.size());
    for (std::set<ObjectInfo>::iterator it = objects.begin();
            it != objects.end();
            it++) {
        const std::string &blob = (*it).toString();
        assert(blob.size() == ObjectInfo::SIZE);
        fs.write(blob.data(), blob.size());
    }
}

void SshServer::cmd_listCommits()
{
    fdwstream fs(STDOUT_FILENO);
    fs.writeInt<uint8_t>(OK);

    const std::vector<Commit> &commits = repository.listCommits();
    fs.writeInt<uint32_t>(commits.size());
    for (size_t i = 0; i < commits.size(); i++) {
        std::string blob = commits[i].getBlob();
        fs.writePStr(blob);
    }
}

void SshServer::cmd_readObjs()
{
    // Read object ids
    fprintf(stderr, "Reading object IDs\n");
    fdstream in(STDIN_FILENO, -1);
    uint32_t numObjs = in.readInt<uint32_t>();
    fprintf(stderr, "Transmitting %u objects\n", numObjs);
    std::vector<ObjectHash> objs;
    for (size_t i = 0; i < numObjs; i++) {
        ObjectHash hash;
        in.readHash(hash);
        objs.push_back(hash);
    }

    fdwstream fs(STDOUT_FILENO);
    fs.writeInt<uint8_t>(OK);
    repository.transmit(&fs, objs);
}

void SshServer::cmd_getHead()
{
    fdwstream fs(STDOUT_FILENO);
    fs.writeInt<uint8_t>(OK);
    fs.writeHash(repository.getHead());
}



void ae_flush() {
    fflush(stdout);
    fsync(STDOUT_FILENO);
}

int cmd_sshserver(int argc, const char *argv[])
{
    atexit(ae_flush);

    Util_SetBlocking(STDIN_FILENO, true);
    // TODO: necessary?
    Util_SetBlocking(STDOUT_FILENO, true);

    // Disable output buffering
    setvbuf(stdout, NULL, _IONBF, 0); // libc
#ifdef __APPLE__
    fcntl(STDOUT_FILENO, F_NOCACHE, 1); // os x
#endif /* __APPLE__ */

    if (argc < 2) {
        printError("Need repository name");
        exit(1);
    }
    if (!repository.open(argv[1])) {
        printError("No repo found");
        exit(101);
    }

    if (ori_open_log(&repository) < 0) {
        printError("Couldn't open log");
        exit(1);
    }

    fprintf(stderr, "Starting server\n");

    SshServer server;
    server.serve();
    return 0;
}
