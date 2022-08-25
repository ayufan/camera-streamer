import argparse
import asyncio
import json
import logging
import os
import platform
import ssl

from aiohttp import web

from aiortc import RTCPeerConnection, RTCSessionDescription, VideoStreamTrack, MediaStreamTrack
from aiortc.contrib.media import MediaPlayer, MediaRelay
from aiortc.rtcrtpsender import RTCRtpSender
from av.frame import Frame
from av.packet import Packet
from typing import Dict, Optional, Set, Union
from fractions import Fraction

pcs = set()
requested_key_frame = False

class RemoteStream(MediaStreamTrack):
    def __init__(self, kind):
        super().__init__()
        self.kind = kind
        self._queue = asyncio.Queue()

    async def recv(self) -> Union[Frame, Packet]:
        data = await self._queue.get()
        if data is None:
            raise MediaStreamError
        return data

def force_codec(pc, sender, forced_codec):
    kind = forced_codec.split("/")[0]
    codecs = RTCRtpSender.getCapabilities(kind).codecs
    transceiver = next(t for t in pc.getTransceivers() if t.sender == sender)
    transceiver.setCodecPreferences(
        [codec for codec in codecs if codec.mimeType == forced_codec]
    )

async def index(request):
    content = """
    <html>
    <head>
        <meta charset="UTF-8"/>
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>WebRTC webcam</title>
        <style>
        button {
            padding: 8px 16px;
        }

        video {
            width: 100%;
        }

        .option {
            margin-bottom: 8px;
        }

        #media {
            max-width: 1280px;
        }
        </style>
    </head>
    <body onload="start()">

    <div class="option">
        <input id="use-stun" type="checkbox" checked/>
        <label for="use-stun">Use STUN server</label>
    </div>
    <button id="start" onclick="start()">Start</button>
    <button id="stop" style="display: none" onclick="stop()">Stop</button>

    <div id="media">
        <h2>Media</h2>

        <audio id="audio" autoplay="true"></audio>
        <video id="video" autoplay="true" playsinline="true"></video>
    </div>

    <script>
        function negotiate() {
            pc.addTransceiver('video', {direction: 'recvonly'});
            pc.addTransceiver('audio', {direction: 'recvonly'});
            return pc.createOffer().then(function(offer) {
                return pc.setLocalDescription(offer);
            }).then(function() {
                // wait for ICE gathering to complete
                return new Promise(function(resolve) {
                    if (pc.iceGatheringState === 'complete') {
                        resolve();
                    } else {
                        function checkState() {
                            if (pc.iceGatheringState === 'complete') {
                                pc.removeEventListener('icegatheringstatechange', checkState);
                                resolve();
                            }
                        }
                        pc.addEventListener('icegatheringstatechange', checkState);
                    }
                });
            }).then(function() {
                var offer = pc.localDescription;
                return fetch('/offer', {
                    body: JSON.stringify({
                        sdp: offer.sdp,
                        type: offer.type,
                    }),
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    method: 'POST'
                });
            }).then(function(response) {
                return response.json();
            }).then(function(answer) {
                return pc.setRemoteDescription(answer);
            }).catch(function(e) {
                alert(e);
            });
        }

        function start() {
            var config = {
                sdpSemantics: 'unified-plan'
            };

            if (document.getElementById('use-stun').checked) {
                config.iceServers = [{urls: ['stun:stun.l.google.com:19302']}];
            }

            pc = new RTCPeerConnection(config);

            // connect audio / video
            pc.addEventListener('track', function(evt) {
                if (evt.track.kind == 'video') {
                    document.getElementById('video').srcObject = evt.streams[0];
                } else {
                    document.getElementById('audio').srcObject = evt.streams[0];
                }
            });

            document.getElementById('start').style.display = 'none';
            negotiate();
            document.getElementById('stop').style.display = 'inline-block';
        }

        function stop() {
            document.getElementById('stop').style.display = 'none';

            // close peer connection
            setTimeout(function() {
                pc.close();
            }, 500);
        }
    </script>
    </body>
    </html>
    """
    return web.Response(content_type="text/html", text=content)

async def offer(request):
    params = await request.json()
    offer = RTCSessionDescription(sdp=params["sdp"], type=params["type"])

    pc = RTCPeerConnection()

    global pcs
    pcs.add(pc)

    global requested_key_frame
    requested_key_frame = True

    @pc.on("connectionstatechange")
    async def on_connectionstatechange():
        print("Connection state is %s" % pc.connectionState)
        if pc.connectionState == "failed":
            await pc.close()
            pcs.discard(pc)

    video_sender = pc.addTrack(RemoteStream("video"))
    force_codec(pc, video_sender, "video/H264")
    await pc.setRemoteDescription(offer)

    answer = await pc.createAnswer()
    await pc.setLocalDescription(answer)

    return web.Response(
        content_type="application/json",
        text=json.dumps(
            { "sdp": pc.localDescription.sdp, "type": pc.localDescription.type }
        ),
    )

pts = 0

def push_frame(video_packet):
    global pts
    video_packet = Packet(video_packet)
    video_packet.pts = pts
    video_packet.time_base = Fraction(1, 48000)
    pts += 1
    for pc in pcs:
        for t in pc.getTransceivers():
            if t.sender.kind == "video" and t.sender.track:
                t.sender.track._queue.put_nowait(video_packet)

def wants_frame():
    for pc in pcs:
        for t in pc.getTransceivers():
            if t.sender.kind == "video" and t.sender.track:
                return True

    return False

def request_key_frame():
    global requested_key_frame
    if requested_key_frame:
        requested_key_frame = False
        return True
    return False

async def on_shutdown(app):
    # close peer connections
    coros = [pc.close() for pc in pcs]
    await asyncio.gather(*coros)
    pcs.clear()


if True:
    parser = argparse.ArgumentParser(description="WebRTC webcam demo")
    parser.add_argument("--cert-file", help="SSL certificate file (for HTTPS)")
    parser.add_argument("--key-file", help="SSL key file (for HTTPS)")
    parser.add_argument(
        "--host", default="0.0.0.0", help="Host for HTTP server (default: 0.0.0.0)"
    )
    parser.add_argument(
        "--port", type=int, default=8000, help="Port for HTTP server (default: 8080)"
    )
    parser.add_argument("--verbose", "-v", action="count")

    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)

    if args.cert_file:
        ssl_context = ssl.SSLContext()
        ssl_context.load_cert_chain(args.cert_file, args.key_file)
    else:
        ssl_context = None

    app = web.Application()
    app.on_shutdown.append(on_shutdown)
    app.router.add_get("/", index)
    app.router.add_post("/offer", offer)
    web.run_app(app, host=args.host, port=args.port, ssl_context=ssl_context)
