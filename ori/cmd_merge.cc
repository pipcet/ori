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

#include <stdbool.h>
#include <stdint.h>

#include <string>
#include <iostream>
#include <iomanip>
#include <utility>

#include <sys/stat.h>

#include <ori/debug.h>
#include <ori/oriutil.h>
#include <ori/dag.h>
#include <ori/objecthash.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

int
cmd_merge(int argc, const char *argv[])
{
    if (argc != 2) {
	cout << "merge takes one arguments!" << endl;
	cout << "usage: ori merge <commit 1>" << endl;
	return 1;
    }

    ObjectHash p1 = repository.getHead();
    ObjectHash p2 = ObjectHash::fromHex(argv[1]);

    // Find lowest common ancestor
    DAG<ObjectHash, Commit> cDag = repository.getCommitDag();
    ObjectHash lca;

    lca = cDag.findLCA(p1, p2);
    cout << "LCA: " << lca.hex() << endl;

    // Construct tree diffs to ancestor to find conflicts
    TreeDiff td1, td2;

    Commit c1 = repository.getCommit(p1);
    Commit c2 = repository.getCommit(p2);
    Commit cc;

    Tree t1 = repository.getTree(c1.getTree());
    Tree t2 = repository.getTree(c2.getTree());
    Tree tc = Tree();
    
    if (lca != EMPTY_COMMIT) {
	Commit cc = repository.getCommit(lca);
	tc = repository.getTree(cc.getTree());
    }

    td1.diffTwoTrees(t1.flattened(&repository), tc.flattened(&repository));
    td2.diffTwoTrees(t2.flattened(&repository), tc.flattened(&repository));

    printf("Tree 1:\n");
    for (size_t i = 0; i < td1.entries.size(); i++) {
        printf("%c   %s\n",
                td1.entries[i].type,
                td1.entries[i].filepath.c_str());
    }
    printf("Tree 2:\n");
    for (size_t i = 0; i < td2.entries.size(); i++) {
        printf("%c   %s\n",
                td2.entries[i].type,
                td2.entries[i].filepath.c_str());
    }

    // Compute merge
    TreeDiff mdiff;
    mdiff.mergeTrees(td1, td2);

    printf("Merged Tree:\n");
    for (size_t i = 0; i < mdiff.entries.size(); i++) {
        printf("%c   %s\n",
                mdiff.entries[i].type,
                mdiff.entries[i].filepath.c_str());
    }

    // Setup merge state
    MergeState state;
    state.setParents(p1, p2);

    repository.setMergeState(state);

    // Create diff of working directory updates
    TreeDiff wdiff;
    wdiff.mergeChanges(td1, mdiff);

    printf("Merged Tree Diff:\n");
    for (size_t i = 0; i < wdiff.entries.size(); i++) {
        printf("%c   %s\n",
                wdiff.entries[i].type,
                wdiff.entries[i].filepath.c_str());
    }

    printf("Updating working directory\n");
    // Update working directory as necessary
    for (size_t i = 0; i < wdiff.entries.size(); i++) {
	TreeDiffEntry e = wdiff.entries[i];
	string path  = repository.getRootPath() + e.filepath;

	if (e.type == TreeDiffEntry::NewFile) {
	    printf("N       %s\n", e.filepath.c_str());
	    repository.copyObject(e.hashes.first, path);
	} else if (mdiff.entries[i].type == TreeDiffEntry::NewDir) {
	    printf("N       %s\n", e.filepath.c_str());
	    mkdir(path.c_str(), 0755);
	} else if (mdiff.entries[i].type == TreeDiffEntry::DeletedFile) {
	    printf("D       %s\n", e.filepath.c_str());
	    Util_DeleteFile(path);
	} else if (mdiff.entries[i].type == TreeDiffEntry::DeletedDir) {
	    printf("D       %s\n", e.filepath.c_str());
	    rmdir(path.c_str()); 
	} else if (mdiff.entries[i].type == TreeDiffEntry::Modified) {
	    printf("U       %s\n", e.filepath.c_str());
	    repository.copyObject(e.hashes.first, path);
	} else {
	    printf("Unsupported TreeDiffEntry type %c\n", mdiff.entries[i].type);
	    NOT_IMPLEMENTED(false);
	}
    }

    // XXX: Automatically commit if everything is resolved

    return 0;
}

