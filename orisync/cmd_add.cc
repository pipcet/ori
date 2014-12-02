/*
 * Copyright (c) 2013 Stanford University
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

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <iostream>

#include <ori/localrepo.h>

#include "orisyncconf.h"

using namespace std;

extern OriSyncConf rc;
extern RWLock rcLock; 

int
cmd_add(int mode, const char *argv)
{
    //OriSyncConf rc = OriSyncConf();

    //this check need to be resolved 
    /*
    if (argc != 2)
    {
        cout << "Sepcify a repository to add" << endl;
        cout << "usage: orisync add <repository>" << endl;
    }
    */

    // XXX: verify repo
    //
    if (mode == 0) {
        OriSyncConf rc = OriSyncConf();
        rc.addRepo(argv, true);
    } else {
        RWKey::sp key = rcLock.writeLock();
        rc.addRepo(argv, false);
        LOG("repo added %s", argv);
    }

    return 0;
}

