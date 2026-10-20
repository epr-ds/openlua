ARG PSPDEV_IMAGE=pspdev/pspdev:latest
FROM ${PSPDEV_IMAGE}

RUN apk add --no-cache ccache

ENV CCACHE_DIR=/ccache \
    CCACHE_COMPRESS=true \
    CCACHE_MAXSIZE=2G
