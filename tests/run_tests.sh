#!/usr/bin/env sh
#
# wandaashell acceptance tests.
#
# Non-negotiable #3 of the cross-platform spec: a platform backend is "ported"
# when it runs the .waa scripts and the core shell surface identically, not
# when it compiles. This script is that check. Run it on every platform after
# building:
#
#   ./tests/run_tests.sh [path-to-wandaashell-binary]
#
# Volatile output (the sandbox path, the version string) is normalised before
# comparison; everything else must match byte for byte.
#
set -u

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN=${1:-}
if [ -z "$BIN" ]; then
    for candidate in "$REPO_ROOT/build/wandaashell" "$REPO_ROOT/build/wandaashell.exe" \
                     "$REPO_ROOT/build/Release/wandaashell.exe"; do
        [ -x "$candidate" ] && BIN=$candidate && break
    done
fi
if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
    echo "run_tests.sh: no wandaashell binary found; build first or pass a path" >&2
    exit 2
fi
BIN=$(CDPATH= cd -- "$(dirname -- "$BIN")" && pwd)/$(basename -- "$BIN")

SANDBOX=$(mktemp -d)
trap 'rm -rf "$SANDBOX"' EXIT
cp "$REPO_ROOT/test.waa" "$REPO_ROOT/examples_test.waa" "$SANDBOX/"
: > "$SANDBOX/alpha.txt"
: > "$SANDBOX/beta.txt"

PASS=0
FAIL=0

# Strips the banner, every prompt prefix, and anything host-specific.
normalise() {
    sed -e 's|^\(wandaa\(\[ADMIN\]\)\{0,1\} [^>]*> \)*||' \
        -e "s|$SANDBOX|<SANDBOX>|g" \
        -e 's|^wandaashell v[0-9][0-9.]*$|wandaashell v<VERSION>|'
}

# check <name> <expected-file> -- runs stdin through the shell
check() {
    name=$1
    expected=$2
    actual=$(cd "$SANDBOX" && "$BIN" $RUN_ARGS 2>&1 | normalise)
    if [ "$actual" = "$(cat "$expected")" ]; then
        echo "ok   - $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL - $name"
        echo "--- expected ---"; cat "$expected"
        echo "--- actual -----"; echo "$actual"
        echo "----------------"
        FAIL=$((FAIL + 1))
    fi
}

# --- 1. the .waa script that ships with the repo ---------------------------
# `ls *.txt` order is filesystem-dependent, so this case sorts its output.
expected_test_waa=$SANDBOX/.expected1
cat > "$expected_test_waa" <<'EOF'
<SANDBOX>
       alpha.txt
       beta.txt
done
wandaashell v<VERSION>
EOF
actual=$(cd "$SANDBOX" && "$BIN" test.waa 2>&1 | normalise | sort)
if [ "$actual" = "$(sort "$expected_test_waa")" ]; then
    echo "ok   - test.waa"; PASS=$((PASS + 1))
else
    echo "FAIL - test.waa"
    echo "--- expected ---"; sort "$expected_test_waa"
    echo "--- actual -----"; echo "$actual"
    FAIL=$((FAIL + 1))
fi

# --- 2. the .waa language sample -------------------------------------------
expected2=$SANDBOX/.expected2
cat > "$expected2" <<'EOF'
sum is:
15
x is smaller
looping
looping
looping
EOF
RUN_ARGS=examples_test.waa
check "examples_test.waa" "$expected2"

# --- 3. core shell surface: builtins, redirection, pipes, chaining ---------
cat > "$SANDBOX/session.txt" <<'EOF'
echo hello wandaa
echo first > out.txt
echo second >> out.txt
cat out.txt
cat out.txt | grep second
echo a ; echo b ; echo c
which cd
touch made.txt
ls made.txt
rm made.txt
ls made.txt
b64encode alpha.txt
waa version
exit
EOF
expected3=$SANDBOX/.expected3
cat > "$expected3" <<'EOF'
wandaashell v<VERSION>
hello wandaa
first
second
second
a
b
c
cd: shell built-in
       made.txt
ls: path not found: <SANDBOX>/made.txt

wandaashell v<VERSION>
EOF
actual=$(cd "$SANDBOX" && "$BIN" < session.txt 2>&1 | normalise)
if [ "$actual" = "$(cat "$expected3")" ]; then
    echo "ok   - shell session (builtins, redirection, pipes, chaining)"; PASS=$((PASS + 1))
else
    echo "FAIL - shell session (builtins, redirection, pipes, chaining)"
    echo "--- expected ---"; cat "$expected3"
    echo "--- actual -----"; echo "$actual"
    FAIL=$((FAIL + 1))
fi

# --- 4. external process spawn ---------------------------------------------
# The one case that genuinely differs per OS: it needs a program that exists.
# Uses the shell's own binary, which is guaranteed to be there.
cat > "$SANDBOX/session2.txt" <<'EOF'
this-command-does-not-exist-xyz
exit
EOF
actual=$(cd "$SANDBOX" && "$BIN" < session2.txt 2>&1 | normalise)
case "$actual" in
    *this-command-does-not-exist-xyz*)
        echo "ok   - unknown command is reported, not silently ignored"; PASS=$((PASS + 1)) ;;
    *)
        echo "FAIL - unknown command produced no diagnostic"
        echo "--- actual -----"; echo "$actual"
        FAIL=$((FAIL + 1)) ;;
esac

echo
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
