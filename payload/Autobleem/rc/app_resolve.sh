#!/bin/sh
#
# Sourced by app_env.sh: which of a multi-platform App's binaries this machine runs, read from its app.ini -
# the shell copy of the launcher's AppManifest rule (docs/app-format-plan.md), for a run.sh started by hand.
# The launcher itself resolves the ini and exports the answer (AB_APP_EXEC, AB_APP_ARGS, AB_APP_LIB,
# AB_APP_KEY), so none of this runs under it. tests/rc/test_app_resolve.cpp holds the two to the same answers.
#
# The rule: for each platform key, most specific first ($AB_PLATFORM_KEYS, else the list the launcher left in
# System/platform_keys), Exec.<key>= if the ini has it, else Exec= with {key} replaced; the first that names
# an existing file is the program. Args=, Lib= and Env= then resolve for that key the same way.
#
# Needs AB_APP_DIR and AB_ROOT; POSIX sh and awk (busybox has both).

# ab_ini_value <lower-case key>: the key's value in app.ini (the last one wins, as in IniFile), with '#'
# comments and the blanks around '=' dropped; fails when the ini has no such key
ab_ini_value() {
    awk -v want="$1" '
        { sub(/\r$/, ""); sub(/#.*/, "") }
        index($0, "=") > 0 {
            k = substr($0, 1, index($0, "=") - 1)
            v = substr($0, index($0, "=") + 1)
            gsub(/^[ \t]+|[ \t]+$/, "", k)
            gsub(/^[ \t]+|[ \t]+$/, "", v)
            if (tolower(k) == want) { found = v; have = 1 }
        }
        END { if (have) { printf "%s", found; exit 0 } exit 1 }' "$AB_APP_DIR/app.ini"
}

# ab_key_value <name> <key>: <name>.<key> if present, else <name>, with {key} replaced; fails when neither
ab_key_value() {
    ab_value=$(ab_ini_value "$1.$2") || ab_value=$(ab_ini_value "$1") || return 1
    printf '%s' "$ab_value" | sed "s/{key}/$2/g"
}

# ab_export_env "A=1;B=2": exports each NAME=value
ab_export_env() {
    ab_old_ifs=$IFS
    IFS=';'
    for ab_pair in $1; do
        ab_name=$(printf '%s' "${ab_pair%%=*}" | sed 's/^[ \t]*//; s/[ \t]*$//')
        case "$ab_pair" in *=*) ab_val=$(printf '%s' "${ab_pair#*=}" | sed 's/^[ \t]*//; s/[ \t]*$//') ;; *) ab_val= ;; esac
        [ -n "$ab_name" ] && export "$ab_name=$ab_val"
    done
    IFS=$ab_old_ifs
}

# ab_resolve_app: sets and exports AB_APP_EXEC, AB_APP_KEY, AB_APP_ARGS, AB_APP_LIB and the ini's Env=;
# fails (and sets nothing) when no binary is there for any key, or the ini names none
ab_resolve_app() {
    [ -f "$AB_APP_DIR/app.ini" ] || return 1
    ab_keys=$AB_PLATFORM_KEYS
    if [ -z "$ab_keys" ] && [ -f "$AB_ROOT/System/platform_keys" ]; then
        ab_keys=$(cat "$AB_ROOT/System/platform_keys")
    fi
    for ab_key in $ab_keys; do
        ab_name=$(ab_key_value exec "$ab_key") || continue
        [ -n "$ab_name" ] || continue
        case "$ab_name" in
            /*) ab_path=$ab_name ;;
            *) ab_path="$AB_APP_DIR/$ab_name" ;;
        esac
        [ -f "$ab_path" ] || continue

        AB_APP_EXEC=$ab_path
        AB_APP_KEY=$ab_key
        AB_APP_ARGS=$(ab_key_value args "$ab_key") || AB_APP_ARGS=
        ab_lib=$(ab_key_value lib "$ab_key") || ab_lib=
        case "$ab_lib" in
            '') AB_APP_LIB= ;;
            /*) AB_APP_LIB=$ab_lib ;;
            *) AB_APP_LIB="$AB_APP_DIR/$ab_lib" ;;
        esac
        export AB_APP_EXEC AB_APP_KEY AB_APP_ARGS AB_APP_LIB
        # the plain list first, then the key's own on top
        ab_env=$(ab_ini_value env) && ab_export_env "$(printf '%s' "$ab_env" | sed "s/{key}/$ab_key/g")"
        ab_env=$(ab_ini_value "env.$ab_key") && ab_export_env "$ab_env"
        return 0
    done
    return 1
}
