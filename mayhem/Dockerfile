# syntax=docker/dockerfile:1.7
#
# Hypersomnia commit image: upstream's CMake (HEADLESS, no WebRTC) built twice by mayhem/build.sh —
# ASan+UBSan+SanCov for the fuzz harnesses, normal flags for upstream's Catch unit tests.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# Build deps from apt: libc++ (upstream's Introspector-generator code generator always builds with
# -stdlib=libc++), libsodium (upstream links the system one on Linux), nlohmann-json (signalling headers).
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        libc++-19-dev libc++abi-19-dev libsodium-dev nlohmann-json3-dev \
 && rm -rf /var/lib/apt/lists/*

# Upstream's git submodules, which a plain checkout (and so the build context) lacks: fetched once at
# the commits upstream pins them to, into /opt/toolchains/deps/<name>-<commit>/src (read-only).
# build.sh copies each into its (empty) submodule path, so the build and its re-runs stay offline.
RUN set -eu; fetch() { \
      d="/opt/toolchains/deps/$1-$3/src"; mkdir -p "$d"; cd "$d"; \
      git init -q; git remote add origin "$2"; git fetch -q --depth 1 origin "$3"; \
      git checkout -q FETCH_HEAD; [ "$(git rev-parse HEAD)" = "$3" ]; rm -rf .git; cd /; }; \
    fetch Introspector-generator https://github.com/TeamHypersomnia/Introspector-generator.git 1b1a47226589a0b185da560b1a44835ac407278d; \
    fetch lodepng https://github.com/lvandeve/lodepng.git 18964554bc769255401942e0e6dfd09f2fab2093; \
    fetch rapidjson https://github.com/Tencent/rapidjson 083f359f5c36198accc2b9360ce1e32a333231d9; \
    fetch yojimbo https://github.com/TeamHypersomnia/yojimbo.git 571b0afd1884d05afcaf6a9d03c048c317c338a7; \
    fetch imgui https://github.com/TeamHypersomnia/imgui d7153a8dd5608be7a2e05d1b4c57e5eb179f48ad; \
    fetch polypartition https://github.com/ivanfratric/polypartition.git 4921a9ba4d1f9948508dfe5fc36ff64773ab1416; \
    fetch rectpack2D https://github.com/TeamHypersomnia/rectpack2D.git bbc9d0ec05b9b508f6c781667fec718bf9e78d21; \
    fetch Catch https://github.com/philsquared/Catch.git d71b4617e9935f8589870af211f5b7552d743654; \
    chmod -R a+rX,a-w /opt/toolchains/deps

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
