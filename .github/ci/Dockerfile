ARG DOCKER_ARCH
ARG DEBIAN_VERSION
ARG DOCKER_REPO
FROM --platform=linux/${DOCKER_ARCH} ${DOCKER_REPO}debian:${DEBIAN_VERSION} AS build_env

RUN apt-get -y update && apt-get -y install gnupg2 wget

# Add RPI packages (the whole base system, since RPI ships its own GCC)
ARG DEBIAN_VERSION
ARG BUILD_TYPE="generic"
RUN [ "$BUILD_TYPE" != "raspi" ] || \
  ( \
    ( [ "$(dpkg --print-architecture)" != "armhf" ] || echo "deb http://raspbian.raspberrypi.org/raspbian/ $DEBIAN_VERSION main contrib non-free rpi" > /etc/apt/sources.list ) && \
    cd /tmp && \
    wget "http://archive.raspberrypi.org/debian/pool/main/r/raspberrypi-archive-keyring/raspberrypi-archive-keyring_2025.1+rpt1_all.deb" && \
    echo "2e727149d7acb8cc7f604e66d0049161039c8aa1eaf1175e54f9e69d963d60e4  raspberrypi-archive-keyring_2025.1+rpt1_all.deb" | sha256sum -c && \
    wget "https://archive.raspbian.org/raspbian/pool/main/r/raspbian-archive-keyring/raspbian-archive-keyring_20120528.4_all.deb" && \
    echo "eb2bc175ecfad128ece8222b42eefabd0a2846afd14f3af04364f4a047cbc88f  raspbian-archive-keyring_20120528.4_all.deb" | sha256sum -c && \
    dpkg -i *.deb && \
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
ARG DEB_REVISION="1"

RUN apt-get build-dep -y $PWD
RUN . /etc/os-release && \
  export RELEASE_SUFFIX="$VERSION_CODENAME-$DEB_REVISION" && \
  dpkg-buildpackage -us -uc -b

RUN mkdir -p /deb && mv ../*.deb /deb/
