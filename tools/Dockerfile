# Dev image: the official pspdev toolchain plus a host C compiler,
# so the pure core can be tested on the host inside the same container.
# Pinned by digest so builds stay reproducible; to upgrade, pull the new
# pspdev/pspdev and replace the digest.
FROM pspdev/pspdev@sha256:54895e6f5afb71b8f4f6915ee6d5023e1e087ca7afdf2dcb1d7a694a5233e45f
RUN apk add --no-cache build-base
