#!/usr/bin/env bash
# Port → Tonality pull side of the accountability protocol (port/PORT.md in the
# Tonality repo). Regenerates the versioned-data export from the Tonality
# checkout, diffs it against the vendored fixtures, and on drift refreshes them
# + rewrites fixtures/PIN.json. Run at the start of any port work session and
# before tagging any release.
#
#   tools/refresh_fixtures.sh [--check]
#
# --check: report drift but do not refresh (exit 1 on drift).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TONALITY="${TONALITY_REPO:-$HOME/Documents/Tonality}"
PY="$TONALITY/.venv/bin/python3.13"
FIXTURES="$REPO_ROOT/fixtures/tonality"
CHECK_ONLY=0
[ "${1:-}" = "--check" ] && CHECK_ONLY=1

[ -x "$PY" ] || { echo "error: Tonality venv python not found at $PY (set TONALITY_REPO)"; exit 2; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

"$PY" "$TONALITY/scripts/export_versioned_data.py" --out "$TMP" >/dev/null
cp "$TONALITY/tests/golden/conformance.json" "$TMP/"

FILES=(set_class_table.json manifest.json bundle.json conformance.json)
drift=()
for f in "${FILES[@]}"; do
  if ! cmp -s "$TMP/$f" "$FIXTURES/$f"; then
    drift+=("$f")
  fi
done

commit="$(git -C "$TONALITY" rev-parse HEAD)"

if [ "${#drift[@]}" -eq 0 ]; then
  echo "no drift: vendored fixtures match engine at $commit — parity claims stand."
  exit 0
fi

echo "DRIFT in: ${drift[*]} (engine at $commit)"
if [ "$CHECK_ONLY" -eq 1 ]; then
  echo "--check: not refreshing. A notice in the Tonality repo's"
  echo "integrations/tonality-core/ is guaranteed only when the PORTED surface"
  echo "changed; goldens for non-ported tools churn routinely without one."
  echo "Either way: rerun without --check, then re-run the parity harness —"
  echo "green => routine churn outside the ported slices (cite any notice in"
  echo "the refresh PR); red => STOP and file a brief on the integrations"
  echo "channel (the pin failed to guard the surface — a protocol bug)."
  exit 1
fi

for f in "${FILES[@]}"; do
  cp "$TMP/$f" "$FIXTURES/$f"
done

{
  echo '{'
  echo "  \"tonality_commit\": \"$commit\","
  echo "  \"generated_at\": \"$(date -u +%Y-%m-%d)\","
  echo '  "export_schema_version": "export.1",'
  echo '  "note": "Fixtures generated from the Tonality engine (the spec'"'"'s source of truth) via scripts/export_versioned_data.py + tests/golden/conformance.json. Every parity claim in this repo is parity WITH this engine commit. Refresh via tools/refresh_fixtures.sh.",'
  echo '  "files": {'
  sep=','
  for i in "${!FILES[@]}"; do
    f="${FILES[$i]}"
    [ "$i" -eq $(( ${#FILES[@]} - 1 )) ] && sep=''
    sha="$(shasum -a 256 "$FIXTURES/$f" | cut -d' ' -f1)"
    echo "    \"$f\": \"$sha\"$sep"
  done
  echo '  }'
  echo '}'
} > "$REPO_ROOT/fixtures/PIN.json"

echo "refreshed fixtures + PIN.json (now pinned to $commit)."
echo "next: re-run the parity harness (see README), then commit fixtures/ + the"
echo "parity result in ONE PR, linking the integrations/tonality-core/ notice as cause."
