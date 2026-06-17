# filename: tools/chat-profile.sh
# purpose : Ultra-fast repo profiling for AI-chat pre-analysis (Lua/Kotlin/Rust focus).
# usage   : ./tools/chat-profile.sh --json|--text

set -eu

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mode="text"
if [ "${1:-}" = "--json" ]; then
  mode="json"
fi

cd "$repo_root"

count_files() {
  find . -type f \( -name '*.lua' -o -name '*.kt' -o -name '*.rs' \) \
    ! -path '*/.git/*' ! -path '*/target/*' ! -path '*/build/*' | wc -l | tr -d ' '
}
count_lines_lang() {
  lang="$1"
  find . -type f -name "*.${lang}" \
    ! -path '*/.git/*' ! -path '*/target/*' ! -path '*/build/*' \
    -exec wc -l {} + | awk 'END{print $1+0}'
}

lua_files=$(find . -type f -name '*.lua' ! -path '*/.git/*' ! -path '*/target/*' ! -path '*/build/*' | wc -l | tr -d ' ')
kt_files=$(find . -type f -name '*.kt' ! -path '*/.git/*' ! -path '*/target/*' ! -path '*/build/*' | wc -l | tr -d ' ')
rs_files=$(find . -type f -name '*.rs' ! -path '*/.git/*' ! -path '*/target/*' ! -path '*/build/*' | wc -l | tr -d ' ')

lua_lines=$(count_lines_lang lua)
kt_lines=$(count_lines_lang kt)
rs_lines=$(count_lines_lang rs)

if [ "$mode" = "json" ]; then
  cat <<EOF
{
  "repo_root": "$repo_root",
  "summary": {
    "lua": { "files": $lua_files, "lines": $lua_lines },
    "kotlin": { "files": $kt_files, "lines": $kt_lines },
    "rust": { "files": $rs_files, "lines": $rs_lines },
    "total_files": $(count_files)
  }
}
EOF
else
  echo "Repo: $repo_root"
  echo "Lua    : $lua_files files, $lua_lines lines"
  echo "Kotlin : $kt_files files, $kt_lines lines"
  echo "Rust   : $rs_files files, $rs_lines lines"
fi
