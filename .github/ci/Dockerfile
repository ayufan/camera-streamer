ARG DOCKER_ARCH
ARG DEBIAN_VERSION
ARG DOCKER_REPO
FROM --platform=linux/${DOCKER_ARCH} ${DOCKER_REPO}debian:${DEBIAN_VERSION} AS build_env

RUN apt-get -y update && apt-get -y install gnupg2

# Add RPI packages (the whole base system, since RPI ships its own GCC)
ARG DEBIAN_VERSION
ARG BUILD_TYPE="generic"
RUN [ "$BUILD_TYPE" != "raspi" ] || \
  ( \
    ( [ "$(dpkg --print-architecture)" != "armhf" ] || echo "deb http://raspbian.raspberrypi.org/raspbian/ $DEBIAN_VERSION main contrib non-free rpi" > /etc/apt/sources.list ) && \
    gpg --keyserver keyserver.ubuntu.com --recv-keys 9165938D90FDDD2E 82B129927FA3303E && \
    gpg --export 9165938D90FDDD2E 82B129927FA3303E > /etc/apt/trusted.gpg.d/raspi.gpg && \
    echo "deb http://archive.raspberrypi.org/debian/ $DEBIAN_VERSION main" > /etc/apt/sources.list.d/raspi.list && \
    apt-get -y update && \
    apt-get -y install libcamera-dev liblivemedia-dev \
  )

# Default packages
RUN apt-get -y install build-essential xxd cmake ccache git-core pkg-config \
  libavformat-dev libavutil-dev libavcodec-dev libssl-dev v4l-utils debhelper

FROM build_env AS build
ADD / /src
WORKDIR /src
RUN git clean -ffdx
RUN git submodule update --init --recursive --recommend-shallow
RUN git submodule foreach --recursive git clean -ffdx

FROM build AS deb_make
ARG GIT_VERSION
ARG BUILD_TYPE="generic"
ENV DEB_BUILD_PROFILES="$BUILD_TYPE"

RUN apt-get build-dep -y $PWD
RUN . /etc/os-release && \
  export RELEASE_SUFFIX="$VERSION_CODENAME" && \
  dpkg-buildpackage -us -uc -b

RUN mkdir -p /deb && mv ../*.deb /deb/
