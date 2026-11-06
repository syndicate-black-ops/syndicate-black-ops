#!/usr/bin/env bash
# Sanitizer soak on a stock dedicated server: runs hlds_linux (a folder made by tests/soak/setup_hlds.sh) in the
# sniper image with the image's 32-bit libasan preloaded, through a map cycle, with jk_botti bots when the folder has
# them, and collects AddressSanitizer / UBSan reports and crashes. No network (--network none, sv_lan 1).
#
#   tests/soak/run_soak.sh <server dir> <out dir> [seconds per map] [bots] [map...]
#     seconds per map  default 180;  bots  default 6 ("jk_botti addbot" each map; 0 without Metamod)
#     maps  default: every co_, br_ and bk_ map of gamedir/syndicate/maps
#   Env: SOAK_TOTAL (seconds, default 3600: stops after the map that crosses it), DOCKER_MEMORY (6g), DOCKER_CPUS (2),
#        SOAK_COMMANDS: extra console commands sent once per map after the bots (";"-separated)
#
# Output in <out dir>: console.log (the server's stdout), asan.<pid> / ubsan.<pid> (sanitizer reports),
# summary.txt (maps run, crashes, distinct reports). Exit status 1 if there was a report or a crash.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SRV=$(cd "${1:?server dir}" && pwd); OUT=${2:?out dir}; PERMAP=${3:-180}; BOTS=${4:-6}; shift 4 2>/dev/null || shift $#
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
MAPS=("$@")
if [ ${#MAPS[@]} -eq 0 ]; then
  for m in "$ROOT"/gamedir/syndicate/maps/{co,br,bk}_*.bsp; do MAPS+=("$(basename "$m" .bsp)"); done
fi
SNIPER_IMAGE=${SNIPER_IMAGE:-registry.gitlab.steamos.cloud/steamrt/sniper/sdk:3.0.20260805.254768@sha256:1c33c507bc75d012e77df5727f93b0d5b8c3f7c8d4142ba5f7a16882cc92e014}
TOTAL=${SOAK_TOTAL:-3600}
printf '%s\n' "${MAPS[@]}" > "$OUT/maps.txt"
cat > "$OUT/driver.sh" <<EOF
#!/bin/bash
# inside the container: hlds reads its console from a fifo; one map after the other, a restart after a crash
cd '$SRV'
start=\$(date +%s)
i=0
for map in \$(cat '$OUT/maps.txt'); do
  i=\$((i+1))
  now=\$(date +%s); [ \$((now - start)) -ge $TOTAL ] && break
  if [ -z "\${pid:-}" ] || ! kill -0 "\$pid" 2>/dev/null; then
    [ -n "\${pid:-}" ] && echo "=== SOAK: server exited (status \$(wait \$pid; echo \$?)) before \$map" >> '$OUT/console.log'
    rm -f /tmp/in; mkfifo /tmp/in
    echo "=== SOAK: start on \$map" >> '$OUT/console.log'
    LD_PRELOAD=/usr/lib32/libasan.so.6 ./hlds_linux -game syndicate -port 27999 -nomaster +sv_lan 1 +maxplayers 12 +map "\$map" < /tmp/in >> '$OUT/console.log' 2>&1 &
    pid=\$!
    exec 3>/tmp/in
  else
    echo "=== SOAK: changelevel \$map" >> '$OUT/console.log'
    echo "changelevel \$map" >&3
  fi
  sleep 15
  [ \$i -eq 1 ] && grep -c libasan /proc/\$pid/maps > '$OUT/asan-mapped.txt' 2>&1
  for b in \$(seq 1 $BOTS); do echo "jk_botti addbot" >&3; sleep 1; done
  IFS=';' read -ra extra <<< "${SOAK_COMMANDS:-}"
  for c in "\${extra[@]}"; do echo "\$c" >&3; done
  t=0
  while [ \$t -lt $PERMAP ] && kill -0 "\$pid" 2>/dev/null; do sleep 10; t=\$((t+10)); done
  echo status >&3
  sleep 1
  kill -0 "\$pid" 2>/dev/null || echo "=== SOAK: server died on \$map" >> '$OUT/console.log'
done
echo quit >&3; sleep 5; kill "\$pid" 2>/dev/null; wait
echo "=== SOAK: done" >> '$OUT/console.log'
EOF
chmod +x "$OUT/driver.sh"
: > "$OUT/console.log"
MOUNTS=(-v "$SRV:$SRV" -v "$OUT:$OUT" -v "$ROOT:$ROOT:ro")
# the server folder's symlinks: the stock install and the sanitizer build
for t in $(find "$SRV" -maxdepth 6 -type l -exec readlink -f {} \; | xargs -n1 dirname | sort -u); do
  case $t in "$ROOT"*|"$SRV"*|"$OUT"*) ;; *) MOUNTS+=(-v "$t:$t:ro");; esac
done
nice -n 15 docker run --rm --name "sbo-soak-$$" --network none --memory="${DOCKER_MEMORY:-6g}" --cpus="${DOCKER_CPUS:-2}" \
  -u "$(id -u):$(id -g)" -e HOME=/tmp "${MOUNTS[@]}" -e LD_LIBRARY_PATH="$SRV" \
  -e ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:log_path=$OUT/asan:handle_segv=1:allow_user_segv_handler=1:detect_odr_violation=0" \
  -e UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0:log_path=$OUT/ubsan:suppressions=$ROOT/tests/fuzz/ubsan.supp" \
  "$SNIPER_IMAGE" "$OUT/driver.sh"

{
  echo "maps started: $(grep -c '^=== SOAK: \(start\|changelevel\)' "$OUT/console.log")"
  echo "server exits / deaths: $(grep -c '^=== SOAK: server \(exited\|died\)' "$OUT/console.log")"
  grep '^=== SOAK: server' "$OUT/console.log"
  echo "AddressSanitizer reports: $(cat "$OUT"/asan.* 2>/dev/null | grep -c 'ERROR: AddressSanitizer')"
  cat "$OUT"/asan.* 2>/dev/null | grep -A12 'ERROR: AddressSanitizer' | grep -E 'ERROR|#[0-9] ' | head -60
  echo "UBSan reports (distinct locations):"
  cat "$OUT"/ubsan.* "$OUT/console.log" 2>/dev/null | grep 'runtime error' | sed -E 's/^[^:]*\/src\//src\//; s/address 0x[0-9a-f]+/address X/' \
    | sort | uniq -c | sort -rn | head -80
} > "$OUT/summary.txt"
cat "$OUT/summary.txt"
! grep -q '^=== SOAK: server' "$OUT/console.log" && ! grep -q 'ERROR: AddressSanitizer' "$OUT"/asan.* "$OUT"/ubsan.* "$OUT/console.log" 2>/dev/null
