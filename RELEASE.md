## Variants

Download correct version for your platform:

- Variant: **raspi**: Raspberry PI compatible build with USB, CSI, WebRTC, RTSP support
- Variant: **generic**: All other platforms with USB and MJPEG support only for time being
- System: **bullseye**: Debian Bullseye (11) compatible build
- System: **bookworm**: Debian Bookworm (12) compatible build
- Platform: **amd64**: x86/64 compatible build
- Platform: **arm32**: ARM 32-bit kernel: PIs 0.2W, 2B, and higher, Orange PIs, Rock64, etc. No support for RPI0.
- Platform: **arm64**: ARM 64-bit kernel: PIs 0.2W, 3B, and higher, Orange PIs, Rock64, etc. No support for RPI0 and RPI2B.

## Install on Raspberry PI or any other platform

Copy the below and paste into terminal:

```bash
PACKAGE=camera-streamer-$(test -e /etc/default/raspberrypi-kernel && echo raspi || echo generic)_#{GIT_VERSION}.$(. /etc/os-release; echo $VERSION_CODENAME)_$(dpkg --print-architecture).deb
wget "https://github.com/ayufan/camera-streamer/releases/download/v#{GIT_VERSION}/$PACKAGE"
sudo apt install "$PWD/$PACKAGE"
```

Enable one of provided systemd configuration:

```bash
ls -al /usr/share/camera-streamer/examples/
systemctl enable /usr/share/camera-streamer/examples/camera-streamer-raspi-v3-12MP.service
systemctl start camera-streamer-raspi-v3-12MP
```

You can also copy an existing service and fine tune it:

```bash
cp /usr/share/camera-streamer/examples/camera-streamer-raspi-v3-12MP.service /etc/systemd/system/camera-streamer.service
edit /etc/systemd/system/camera-streamer.service
systemctl enable camera-streamer
systemctl start camera-streamer
```
