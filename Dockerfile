ARG DOCKER_ARCH
ARG DEBIAN_VERSION
FROM ${DOCKER_ARCH}debian:${DEBIAN_VERSION} as build_env

# Default packages
RUN apt-get -y update && apt-get -y install gnupg2 libavformat-dev libavutil-dev libavcodec-dev v4l-utils pkg-config xxd build-essential ccache

# Add RPI packages
ARG DEBIAN_VERSION
RUN echo "deb http://archive.raspberrypi.org/debian/ $DEBIAN_VERSION main" > /etc/apt/sources.list.d/raspi.list
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 82B129927FA3303E
RUN apt-get -y update && apt-get -y install libcamera-dev liblivemedia-dev
