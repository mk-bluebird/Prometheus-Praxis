#!/usr/bin/env bash
# filename: tools/chat-context.sh
# purpose : AI-chat focused context builder (minimal-token snippets around matches).
# usage   : ./tools/chat-context.sh PATTERN [--lang lua|kt|rs|aln] [--lines N]

set -eu

if [ "$#" -lt 1 ]; then
  echo "usage: $0 PATTERN [--lang lua|kt|rs|aln] [--lines N]" >&2
  exit 1
fi

pattern="$1"
shift

lang=""
lines=12

while [ "$#" -gt 0 ]; do
  case "$1" in
    --lang)
      if [ "$#" -lt 2 ]; then
        echo "missing value for --lang" >&2
        exit 1
      fi
      lang="$2"
      shift 2
      ;;
    --lines)
      if [ "$#" -lt 2 ]; then
        echo "missing value for --lines" >&2
        exit 1
      fi
      lines="$2"
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

file_glob="*"
case "$lang" in
  lua) file_glob="*.lua" ;;
  kt)  file_glob="*.kt" ;;
  rs)  file_glob="*.rs" ;;
  aln) file_glob="*.aln" ;;
  "")  file_glob="*";;
  *)   echo "unsupported --lang $lang" >&2; exit 1 ;;
esac

# Move to repo root (tools/..)
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# If ALN mode is requested, use structural block extraction around PARTICLE..END.
if [ "$lang" = "aln" ]; then
  find . -type f -name "$file_glob" \
    ! -path '*/.git/*' ! -path '*/target/*' ! -path '*/build/*' |
  while IFS= read -r f; do
    # Collect matching line numbers
    matches="$(grep -n "$pattern" "$f" | cut -d: -f1 || true)"
    [ -z "$matches" ] && continue

    echo "=== $f ==="
    awk -v matches="$matches" '
      BEGIN {
        n = split(matches, m, " ");
        for (i = 1; i <= n; ++i) {
          matchLine[m[i]] = 1
        }
      }
      /^PARTICLE[[:space:]]/ {
        inBlock = 1
        blockStart = NR
        delete blockLines
        blockLen = 0
      }
      {
        if (inBlock) {
          blockLen++
          blockLines[blockLen] = $0
          if (/^END[[:space:]]*$/) {
            inBlock = 0
            shouldPrint = 0
            for (ln = blockStart; ln < blockStart + blockLen; ++ln) {
              if (matchLine[ln]) {
                shouldPrint = 1
                break
              }
            }
            if (shouldPrint) {
              for (i = 1; i <= blockLen; ++i) {
                print blockLines[i]
              }
              print "--"
            }
          }
        }
      }
    ' "$f"
  done
  exit 0
fi

# Default: line-context grep for non-ALN modes.
find . -type f -name "$file_glob" \
  ! -path '*/.git/*' ! -path '*/target/*' ! -path '*/build/*' |
while IFS= read -r f; do
  if grep -q -- "$pattern" "$f"; then
    echo "=== $f ==="
    grep -n -C "$lines" -- "$pattern" "$f"
    echo
  fi
done
