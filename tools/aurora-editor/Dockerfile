# syntax=docker/dockerfile:1.7
# SBO Aurora particle editor (tools/aurora-editor/README.md). Build from the repository root:
#   docker buildx build -f tools/aurora-editor/Dockerfile -t sbo-aurora-editor:local --load .
# Optional named contexts (each defaults to the repository's own files, or to nothing):
#   --build-context aurora_src=<dir>     particlesys/particlemgr sources to compile (default src/cl_dll/aurora)
#   --build-context extra_assets=<dir>   more files over the shipped assets (aurora/, sprites/ ...)
#   --build-context valve_sprites=<dir>  Half-Life's valve/sprites: VALVE CONTENT, LOCAL TEST IMAGES ONLY
#                                        (never in an image that is pushed or published; default: none). Only the
#                                        sprites listed in tools/aurora-editor/valve-sprites.allow are copied.

# ---- the game's particle code (overridable) ----
FROM scratch AS aurora_src
COPY src/cl_dll/aurora/particlesys.h src/cl_dll/aurora/particlesys.cpp src/cl_dll/aurora/particlemgr.h src/cl_dll/aurora/particlemgr.cpp /

FROM scratch AS extra_assets
COPY tools/aurora-editor/assets-empty/ /

FROM scratch AS valve_sprites
COPY tools/aurora-editor/assets-empty/ /

# ---- SBO's particlesys + the engine shim -> WebAssembly; native parity test ----
FROM emscripten/emsdk:3.1.64 AS core
WORKDIR /build
COPY tools/aurora-editor/core/ core/
COPY --from=aurora_src / stage/
COPY src/cl_dll/parsemsg.h src/cl_dll/parsemsg.cpp src/game_shared/aurora_shared.h stage/
COPY gamedir/syndicate/aurora/ scripts/
COPY tools/aurora-editor/tests/parity.mjs tests/parity.mjs
RUN sh core/build.sh wasm stage out && sh core/build.sh native stage native \
 && node tests/parity.mjs native/aurora-native out/aurora-core.mjs scripts > out/parity.txt \
 && tail -n 3 out/parity.txt

# ---- the Half-Life sprites that can be particles (valve-sprites.allow), nothing else ----
FROM python:3.12-slim AS valve_filter
COPY --from=valve_sprites / /in/
COPY tools/aurora-editor/valve-sprites.allow /allow.txt
RUN mkdir -p /out && cd /in && for f in *; do \
      [ -f "$f" ] || continue; \
      n=$(printf '%s' "$f" | tr 'A-Z' 'a-z'); \
      if grep -v '^#' /allow.txt | grep -qxF "$n"; then cp "$f" "/out/$n"; fi; \
    done && ls /out | wc -l

# ---- runtime: static editor + upload server (Python standard library only) ----
FROM python:3.12-slim
RUN useradd --system --uid 10001 --home-dir /app editor
WORKDIR /app
COPY tools/aurora-editor/server/ server/
COPY tools/aurora-editor/web/ web/
COPY tools/aurora-editor/core/aurora-keys.json web/core/aurora-keys.json
COPY --from=core /build/out/aurora-core.mjs /build/out/aurora-core.wasm /build/out/parity.txt web/core/
COPY gamedir/syndicate/aurora/ assets/sbo/aurora/
COPY gamedir/syndicate/sprites/ assets/sbo/sprites/
COPY --from=extra_assets / assets/sbo/
COPY --from=valve_filter /out/ assets/valve/sprites/
USER editor
ENV AURORA_PORT=8095 AURORA_DATA=/tmp/aurora-sessions
EXPOSE 8095
CMD ["python3", "-u", "server/server.py"]
