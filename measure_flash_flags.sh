#!/usr/bin/env bash
# Measures the flash-size cost of every Makefile ENABLE_* flag.
#
# Builds a baseline using the Makefile's own current `?=` defaults (no
# overrides), then for each flag toggles it to the opposite value and
# rebuilds, diffing .text+.data against the baseline. Results are always
# normalized to "cost when the flag is ON" regardless of which direction it
# had to be toggled to test that (i.e. for a flag that defaults to 1, this
# measures what disabling it saves, then reports that as the ON-state cost).
#
# Since flags interact (see Makefile.md), this necessarily measures each
# flag's cost against whatever else is currently enabled in the Makefile,
# not in isolation - see Makefile.md for the caveats this implies.
#
# Usage: ./measure_flash_flags.sh [markdown-output-file]
#   With no argument, prints the results table to stdout only.
#
# Requires the `uvk5` Docker image to already be built and to reflect the
# source you want measured - this script does NOT build/rebuild it:
#   docker build --build-arg ALPINE_TAG=3.22 -t uvk5 .

set -uo pipefail

IMAGE=uvk5
MAX_JOBS=6
FLASH_LIMIT=61440   # 60 KiB, from firmware.ld's FLASH region length

cd "$(dirname "$0")"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "error: docker image '$IMAGE' not found - build it first:" >&2
    echo "  docker build --build-arg ALPINE_TAG=3.22 -t $IMAGE ." >&2
    exit 1
fi

RESULTS_DIR=$(mktemp -d)
trap 'rm -rf "$RESULTS_DIR"' EXIT

mapfile -t FLAGS < <(grep -oP '^ENABLE_\S+(?=\s+\?=)' Makefile)

get_default() {
    grep -oP "^${1}\s*\?=\s*\K[0-9]+" Makefile | head -1
}

# $1 = make override string (may be empty), $2 = output file to write the result to
run_build() {
    local override="$1" outfile="$2" out text data bss overflow
    out=$(docker run --rm "$IMAGE" bash -c "cd /app && make -s EDITION_STRING=Custom TARGET=f4hwn.probe $override 2>&1")
    read -r text data bss _ < <(printf '%s\n' "$out" \
        | awk '/^[[:space:]]*[0-9]+[[:space:]]+[0-9]+[[:space:]]+[0-9]+[[:space:]]+[0-9]+/{print $1, $2, $3; exit}')
    if [[ -n "${text:-}" ]]; then
        printf 'OK %s %s %s\n' "$text" "$data" "$bss" > "$outfile"
        return
    fi
    overflow=$(printf '%s\n' "$out" | grep -oP 'overflowed by \K[0-9]+' | head -1)
    if [[ -n "${overflow:-}" ]]; then
        printf 'OVERFLOW %s\n' "$overflow" > "$outfile"
        return
    fi
    printf 'FAIL\n' > "$outfile"
    printf '%s\n' "$out" > "${outfile}.log"
}

echo "== Building baseline (current Makefile defaults, no overrides) ==" >&2
run_build "" "$RESULTS_DIR/_baseline"
read -r bstatus btext bdata bbss < "$RESULTS_DIR/_baseline"
if [[ "$bstatus" != OK ]]; then
    echo "Baseline build did not succeed cleanly ($bstatus) - aborting." >&2
    cat "$RESULTS_DIR/_baseline" >&2
    [[ -f "$RESULTS_DIR/_baseline.log" ]] && cat "$RESULTS_DIR/_baseline.log" >&2
    exit 1
fi
baseline_total=$((btext + bdata))
headroom=$((FLASH_LIMIT - baseline_total))
echo "Baseline: text=$btext data=$bdata total=$baseline_total headroom=$headroom bytes (of $FLASH_LIMIT)" >&2

echo "== Measuring ${#FLAGS[@]} flags (up to $MAX_JOBS builds in parallel) ==" >&2
running=0
tested=0
for flag in "${FLAGS[@]}"; do
    default=$(get_default "$flag")
    if [[ -z "$default" ]]; then
        printf 'SKIP\n' > "$RESULTS_DIR/$flag"
        continue
    fi
    toggle=$((1 - default))
    (
        run_build "${flag}=${toggle}" "$RESULTS_DIR/$flag"
        echo "  [$flag] default=$default, tested $toggle -> $(cat "$RESULTS_DIR/$flag")" >&2
    ) &
    running=$((running + 1))
    tested=$((tested + 1))
    if (( running >= MAX_JOBS )); then
        wait -n
        running=$((running - 1))
    fi
done
wait
echo "== Done: $tested flags tested ==" >&2

# --- emit results table ---
{
printf '| Flag | Default | Cost when ON (flash bytes) | Notes |\n'
printf '|---|---|---|---|\n'
for flag in "${FLAGS[@]}"; do
    default=$(get_default "$flag")
    [[ -z "$default" ]] && continue
    toggle=$((1 - default))
    read -r status a b c < "$RESULTS_DIR/$flag"
    case "$status" in
        OK)
            if [[ "$default" == "1" ]]; then
                # toggled OFF; cost of ON = baseline - toggled
                cost=$(( (btext + bdata) - (a + b) ))
            else
                # toggled ON; cost of ON = toggled - baseline
                cost=$(( (a + b) - (btext + bdata) ))
            fi
            printf '| `%s` | %s | %+d | measured directly |\n' "$flag" "$default" "$cost"
            ;;
        OVERFLOW)
            # This can happen testing either direction: usually default=0 and
            # toggling ON overflows, but a flag can also make OFF bigger than
            # ON (e.g. ENABLE_LTO - disabling it removes a size optimization,
            # not a feature), so the sign must follow which state was tested.
            est=$((a + headroom))
            if [[ "$default" == "1" ]]; then
                printf '| `%s` | %s | ~-%d (est.) | disabling overflowed the baseline by %d bytes, i.e. the OFF state is bigger - ON saves an estimated %d bytes (overflow + %d bytes headroom) |\n' \
                    "$flag" "$default" "$est" "$a" "$est" "$headroom"
            else
                printf '| `%s` | %s | ~+%d (est.) | enabling overflowed the baseline by %d bytes; cost estimated as overflow + %d bytes headroom |\n' \
                    "$flag" "$default" "$est" "$a" "$headroom"
            fi
            ;;
        FAIL)
            printf '| `%s` | %s | N/A | build failed for a reason other than flash overflow when toggled to %s - see %s.log |\n' "$flag" "$default" "$toggle" "$flag"
            ;;
        SKIP)
            printf '| `%s` | ? | N/A | could not determine default value |\n' "$flag"
            ;;
    esac
done
} | tee "${1:-/dev/stdout}"

# preserve failure logs alongside the script's own directory for inspection
for f in "$RESULTS_DIR"/*.log; do
    [[ -e "$f" ]] || continue
    cp "$f" "./$(basename "$f" .log)-build-failure.log"
    echo "Saved build failure log: $(basename "$f" .log)-build-failure.log" >&2
done
