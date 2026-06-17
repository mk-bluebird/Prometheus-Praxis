# filename: tools/chat-context.sh
# purpose : AI-chat focused context builder (minimal-token snippets around matches).
# usage   : ./tools/chat-context.sh PATTERN [--lang lua|kt|rs] [--lines N]

set -eu

if [ "$#" -lt 1 ]; then
  echo "usage: $0 PATTERN [--lang lua|kt|rs] [--lines N]" >&2
  exit 1
fi

pattern="$1"; shift
lang=""
lines=12

while [ "$#" -gt 0 ]; then
  case "$1" in
    --lang) lang="$2"; shift 2 ;;
    --lines) lines="$2"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done

file_glob="*"
case "$lang" in
  lua) file_glob="*.lua" ;;
  kt)  file_glob="*.kt" ;;
  rs)  file_glob="*.rs" ;;
  "")  file_glob="*";;
  *) echo "unsupported --lang $lang" >&2; exit 1 ;;
esac

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Use grep -R with context, but filter by extension with find.
find . -type f -name "$file_glob" \
  ! -path '*/.git/*' ! -path '*/target/*' ! -path '*/build/*' |
while IFS= read -r f; do
  if grep -q -- "$pattern" "$f"; then
    echo "=== $f ==="
    grep -n -C "$lines" -- "$pattern" "$f"
    echo
  fi
done
