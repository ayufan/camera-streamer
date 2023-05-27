ARG DOCKER_ARCH
ARG DEBIAN_VERSION
FROM ${DOCKER_ARCH}debian:${DEBIAN_VERSION} as build_env

# Default packages
RUN apt-get -y update && apt-get -y install gnupg2 build-essential xxd cmake ccache git-core pkg-config \
  libavformat-dev libavutil-dev libavcodec-dev libssl-dev v4l-utils
RUN apt-get -y install debhelper

ARG DEBIAN_VERSION
ARG BUILD_TYPE="generic"

# Add RPI packages
RUN [ "$BUILD_TYPE" != "raspi" ] || \
  ( \
    echo "deb http://archive.raspberrypi.org/debian/ $DEBIAN_VERSION main" > /etc/apt/sources.list.d/raspi.list && \
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 82B129927FA3303E && \
    apt-get -y update && \
    apt-get -y install libcamera-dev liblivemedia-dev \
  )

# Add ArduCAM packages
RUN [ "$BUILD_TYPE" != "arducam" ] || \
  ( \
    echo "deb http://archive.raspberrypi.org/debian/ $DEBIAN_VERSION main" > /etc/apt/sources.list.d/raspi.list && \
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 82B129927FA3303E && \
    apt-get -y update && \
    apt-get -y install wget sudo liblivemedia-dev && \
    curl --fail -L -o install_pivariety_pkgs.sh https://github.com/ArduCAM/Arducam-Pivariety-V4L2-Driver/releases/download/install_script/install_pivariety_pkgs.sh && \
    if [ "$(dpkg --print-architecture)" = "arm64" ]; then \
      bash install_pivariety_pkgs.sh -p libcamera_dev -v $DEBIAN_VERSION-arm64-v5; \
    else \
      bash install_pivariety_pkgs.sh -p libcamera_dev -v $DEBIAN_VERSION-v5; \
    fi \
  )

FROM build_env as build
ADD / /src
WORKDIR /src
RUN git clean -ffdx
RUN git submodule update --init --recursive --recommend-shallow
RUN git submodule foreach --recursive git clean -ffdx

FROM build as deb_make
ARG GIT_VERSION
ARG BUILD_TYPE="generic"
ENV DEB_BUILD_PROFILES="$BUILD_TYPE"

RUN apt-get build-dep -y $PWD
RUN . /etc/os-release && \
  export RELEASE_SUFFIX="$VERSION_CODENAME" && \
  dpkg-buildpackage -us -uc -b

RUN mkdir -p /deb && mv ../*.deb /deb/
