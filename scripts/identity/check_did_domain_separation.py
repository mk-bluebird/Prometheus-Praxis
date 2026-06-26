#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys
from collections import defaultdict


DID_REGEX = re.compile(r'did:[a-z0-9]+:[a-zA-Z0-9:\-._]+')

def extract_dids(text: str) -> set[str]:
    return set(DID_REGEX.findall(text))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Static scan for direct DID reuse across privacy domains."
    )
    parser.add_argument(
        "--aln-root",
        type=str,
        required=True,
        help="Root directory containing ALN files.",
    )
    parser.add_argument(
        "--identity-namespace",
        type=str,
        required=True,
        help="Namespace for identity ALN contracts (e.g. ALE.IDENTITY).",
    )
    parser.add_argument(
        "--policy-contract",
        type=str,
        required=True,
        help="Expected DID domain policy contract name.",
    )
    args = parser.parse_args()

    aln_root = pathlib.Path(args.aln_root)
    if not aln_root.is_dir():
        print(f"[ERROR] ALN root does not exist: {aln_root}", file=sys.stderr)
        return 1

    did_usage: defaultdict[str, set[str]] = defaultdict(set)

    for path in aln_root.rglob("*.aln"):
        text = path.read_text(encoding="utf-8", errors="ignore")
        # Skip the identity namespace itself; correlation is expected there.
        if args.identity_namespace in text:
            continue
        for did in extract_dids(text):
            did_usage[did].add(str(path))

    violations = []
    for did, files in did_usage.items():
        if len(files) > 1:
            violations.append((did, files))

    if violations:
        print("[ERROR] Found raw DID reuse across multiple ALN files.", file=sys.stderr)
        for did, files in violations:
            print(f"  DID: {did}", file=sys.stderr)
            for f in sorted(files):
                print(f"    -> {f}", file=sys.stderr)
        print(
            "[HINT] Use scoped alias DIDs (ALE.IDENTITY.BRAINDID.ALIASING.V1) "
            "instead of reusing the same DID string across domains.",
            file=sys.stderr,
        )
        return 1

    print("[OK] No raw DID reuse across ALN domains detected.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
