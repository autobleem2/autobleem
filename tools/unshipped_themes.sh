# Sourced by the packaging scripts: drop_unshipped_themes THEMES_DIR removes from a staged copy of
# autobleem-themes' Themes/ every folder tools/unshipped_themes.txt lists (the repository keeps them; no
# package ships them).
drop_unshipped_themes() {
    local dir="$1" name list
    list="$(dirname "${BASH_SOURCE[0]}")/unshipped_themes.txt"
    while IFS= read -r name || [ -n "$name" ]; do
        name="${name%$'\r'}"
        case "$name" in ''|'#'*) continue ;; esac
        [ -d "$dir/$name" ] || continue
        rm -rf "${dir:?}/$name"
        echo "    theme $name is not shipped (tools/unshipped_themes.txt)"
    done < "$list"
}
