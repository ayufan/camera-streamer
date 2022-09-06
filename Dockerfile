ARG DOCKER_ARCH
ARG DEBIAN_VERSION
FROM ${DOCKER_ARCH}debian:${DEBIAN_VERSION} as build_env

# Default packages
RUN apt-get -y update && apt-get -y install gnupg2 build-essential xxd cmake ccache git-core pkg-config \
  libavformat-dev libavutil-dev libavcodec-dev libssl-dev v4l-utils

# Add RPI packages
ARG DEBIAN_VERSION
ARG BUILD_TYPE="non-rpi"
RUN [ "$BUILD_TYPE" != "rpi" ] || \
  ( \
    echo "deb http://archive.raspberrypi.org/debian/ $DEBIAN_VERSION main" > /etc/apt/sources.list.d/raspi.list && \
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 82B129927FA3303E && \
    apt-get -y update && \
    apt-get -y install libcamera-dev liblivemedia-dev \
  )

FROM scratch as build
COPY --from=build_env / .

ADD / /src
WORKDIR /src
RUN git clean -ffdx
RUN git submodule update --init --recursive --recommend-shallow
RUN git submodule foreach git clean -ffdx
RUN make -j$(nproc)
