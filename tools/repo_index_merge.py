#!/usr/bin/env python3
"""Merge this checkout's tools/repo_index.py with the copy the download repository runs.

Two checkouts publishing in turn used to overwrite each other's page generator: repo_publish.sh copied
whichever tools/repo_index.py had the higher INDEX_VERSION, so a panel one session had added vanished when
the other published (2026-09-20, twice). Now every publish merges instead of copying:

    mine    this checkout's tools/repo_index.py
    theirs  <repo>/.tools/repo_index.py, what the repository runs today (every earlier publish merged in)
    base    the older of the two develop versions each side started from - mine's is
            tools/repo_index.py at `git merge-base HEAD origin/develop`, theirs' is stored next to it as
            <repo>/.tools/repo_index.base.py (+ .rev, the commit it came from)

and `git merge-file` does the three-way merge. Changes on distinct lines combine; the same lines changed
on both sides are a conflict, and then nothing is published - the conflict file is left for a person to
resolve (commit the resolution to develop and publish again). INDEX_VERSION is no longer a gate: the
merged copy gets the larger of the two, +1 when the merge changed what the repository had.

    repo_index_merge.py --mine tools/repo_index.py --theirs DIR/repo_index.py [--theirs-base DIR/repo_index.base.py]
                        [--theirs-rev DIR/repo_index.rev] --out OUT.py --out-base OUT.base.py --out-rev OUT.rev

Exit 0: OUT.py is the merged script (and the base files to store with it). Exit 2: a conflict, OUT.py
holds the markers. Exit 1: an error. A checkout without git (the server's rsync trees) has no base of its
own and merges over the repository's stored base, which is right as long as the checkout is at least as
new as that base - the usual case.
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

VERSION_RE = re.compile(r"^INDEX_VERSION = (\d+)", re.M)
PLACEHOLDER = "INDEX_VERSION = @@MERGE@@"


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def git(args, cwd):
    """stdout of a git command, or None when git or the repository is not there."""
    try:
        r = subprocess.run(["git"] + args, cwd=cwd, capture_output=True, text=True)
    except OSError:
        return None
    return r.stdout if r.returncode == 0 else None


def index_version(text):
    m = VERSION_RE.search(text)
    return int(m.group(1)) if m else 0


def neutral(text):
    """the script with its INDEX_VERSION line made the same everywhere, so the number never conflicts"""
    return VERSION_RE.sub(PLACEHOLDER, text, count=1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mine", required=True)
    ap.add_argument("--theirs", help="the repository's copy (missing = first publish: mine is used as is)")
    ap.add_argument("--theirs-base")
    ap.add_argument("--theirs-rev")
    ap.add_argument("--out", required=True)
    ap.add_argument("--out-base", required=True)
    ap.add_argument("--out-rev", required=True)
    a = ap.parse_args()

    mine = read(a.mine)
    tree = os.path.dirname(os.path.abspath(a.mine))
    rel = "tools/" + os.path.basename(a.mine)

    # mine's base: the file as develop had it where this checkout branched off
    mine_rev = None
    mine_base = None
    if git(["rev-parse", "--is-inside-work-tree"], tree):
        # the stored base revs are merge-bases with origin/develop, so a fetch makes them all known here
        git(["fetch", "-q", "origin"], tree)
        for ref in ("origin/develop", "develop"):
            mine_rev = (git(["merge-base", "HEAD", ref], tree) or "").strip() or None
            if mine_rev:
                break
        if not mine_rev:
            mine_rev = (git(["rev-parse", "HEAD"], tree) or "").strip() or None
        if mine_rev:
            mine_base = git(["show", "%s:%s" % (mine_rev, rel)], tree)

    theirs = read(a.theirs) if a.theirs and os.path.isfile(a.theirs) else None
    theirs_base = read(a.theirs_base) if a.theirs_base and os.path.isfile(a.theirs_base) else None
    theirs_rev = read(a.theirs_rev).strip() if a.theirs_rev and os.path.isfile(a.theirs_rev) else None

    def finish(text, base, rev, note):
        write(a.out, text)
        write(a.out_base, base if base is not None else text)
        write(a.out_rev, (rev or "") + "\n")
        print("repo_index.py: " + note)
        return 0

    if theirs is None:
        return finish(mine, mine_base, mine_rev, "no copy in the repository yet, this checkout's goes up")
    if neutral(theirs) == neutral(mine):
        v = max(index_version(mine), index_version(theirs))
        return finish(VERSION_RE.sub("INDEX_VERSION = %d" % v, theirs, count=1),
                      theirs_base if theirs_base is not None else mine_base,
                      theirs_rev if theirs_base is not None else mine_rev, "same as the repository's")

    # the base: the older of the two develop versions (an ancestor's file is what both sides changed from);
    # with only one known, that one
    base, base_rev, newer_base, newer_rev = None, None, None, None
    if mine_base is not None and theirs_base is not None:
        if theirs_rev and mine_rev and theirs_rev != mine_rev and \
                git(["merge-base", "--is-ancestor", theirs_rev, mine_rev], tree) is not None:
            base, base_rev, newer_base, newer_rev = theirs_base, theirs_rev, mine_base, mine_rev
        else:
            base, base_rev, newer_base, newer_rev = mine_base, mine_rev, theirs_base, theirs_rev
    elif theirs_base is not None:
        base, base_rev = theirs_base, theirs_rev
    elif mine_base is not None:
        base, base_rev = mine_base, mine_rev
    else:
        print("repo_index.py: no common base known (this checkout has no git and the repository stores no "
              "base) - cannot merge; publish from a git checkout of develop once", file=sys.stderr)
        return 1
    if newer_base is None:
        newer_base, newer_rev = base, base_rev

    with tempfile.TemporaryDirectory() as tmp:
        pm, pb, pt = (os.path.join(tmp, n) for n in ("mine", "base", "theirs"))
        write(pm, neutral(mine))
        write(pb, neutral(base))
        write(pt, neutral(theirs))
        r = subprocess.run(["git", "merge-file", "-p", "-L", "this checkout", "-L", "develop (base)",
                            "-L", "the repository", pm, pb, pt], capture_output=True, text=True)
        merged = r.stdout
    if r.returncode < 0 or (r.returncode > 0 and "<<<<<<<" not in merged):
        print("repo_index.py: git merge-file failed: " + r.stderr.strip(), file=sys.stderr)
        return 1

    v = max(index_version(mine), index_version(theirs))
    if neutral(merged) != neutral(theirs):
        v += 1
    merged = merged.replace(PLACEHOLDER, "INDEX_VERSION = %d" % v, 1)
    write(a.out, merged)
    if r.returncode > 0:
        print("repo_index.py: %d conflict(s) between this checkout's copy and the repository's - nothing "
              "published. Resolve %s (the markers name the sides), commit the result to develop and "
              "publish again." % (r.returncode, a.out), file=sys.stderr)
        return 2
    try:
        compile(merged, a.out, "exec")
    except SyntaxError as ex:
        print("repo_index.py: the merge does not parse (%s) - nothing published; see %s" % (ex, a.out),
              file=sys.stderr)
        return 2
    write(a.out_base, newer_base)
    write(a.out_rev, (newer_rev or "") + "\n")
    what = "merged with the repository's" if neutral(merged) != neutral(mine) else "this checkout's, over the repository's"
    print("repo_index.py: %s (INDEX_VERSION %d, base %s)" % (what, v, (newer_rev or "?")[:7]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
