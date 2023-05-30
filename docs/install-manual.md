# Install

This describes a set of manual instructions to run camera streamer.

## Validate your system

**This streamer does only use hardware, and does not support any software decoding or encoding**.
It does require system to provide:

1. ISP (`/dev/video13`, `/dev/video14`, `/dev/video15`)
2. JPEG encoder (`/dev/video31`)
3. H264 encoder (`/dev/video11`)
4. JPEG/H264 decoder (for UVC cameras, `/dev/video10`)
5. At least LTS kernel (5.15, 6.1) for Raspberry PIs

You can validate the presence of all those devices with:

```bash
uname -a
v4l2-ctl --list-devices
```

The `5.15 kernel` or `6.1 kernel` is easy to get since this is LTS kernel for Raspberry PI OS:

```bash
apt-get update
apt-get dist-upgrade
reboot
```

Ensure that your `/boot/config.txt` has enough of GPU memory (required for JPEG re-encoding):

```text
# Example for IMX519
dtoverlay=vc4-kms-v3d,cma-128
gpu_mem=128 # preferred 160 or 256MB
dtoverlay=imx519

# Example for Arducam 64MP
gpu_mem=128
dtoverlay=arducam_64mp,media-controller=1

# Example for USB cam
gpu_mem=128
```

## Compile

```bash
git clone https://github.com/ayufan-research/camera-streamer.git --recursive
apt-get -y install libavformat-dev libavutil-dev libavcodec-dev libcamera-dev liblivemedia-dev v4l-utils pkg-config xxd build-essential cmake libssl-dev

cd camera-streamer/
make
sudo make install
```

## Use it

There are three modes of operation implemented offering different
compatibility to performance.

### Use a preconfigured systemd services

The simplest is to use preconfigured `service/camera-streamer*.service`.
Those can be used as an example, and can be configured to fine tune parameters.

Example:

```bash
systemctl enable $PWD/service/camera-streamer-arducam-16MP.service
systemctl start camera-streamer-arducam-16MP
```

If everything was OK, there will be web-server at `http://<IP>:8080/`.

Error messages can be read `journalctl -xef -u camera-streamer-arducam-16MP`.
