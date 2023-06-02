extern "C" {
#include "webrtc.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/buffer_lock.h"
#include "device/device.h"
#include "output/output.h"
#include "util/http/http.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "util/opts/control.h"
#include "util/opts/opts.h"
#include "device/buffer.h"
};

#include "util/opts/helpers.hh"

#ifdef USE_LIBDATACHANNEL

#include <string>
#include <memory>
#include <optional>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <set>
#include <nlohmann/json.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/rtcpsrreporter.hpp>
#include <rtc/h264rtppacketizer.hpp>
#include <rtc/h264packetizationhandler.hpp>
#include <rtc/rtcpnackresponder.hpp>

using namespace std::chrono_literals;

class Client;

static std::set<std::shared_ptr<Client> > webrtc_clients;
static std::mutex webrtc_clients_lock;
static const auto webrtc_client_lock_timeout = 3 * 1000ms;
static const auto webrtc_client_max_json_body = 10 * 1024;
static const auto webrtc_client_video_payload_type = 102; // H264
static rtc::Configuration webrtc_configuration = {
  // .iceServers = { rtc::IceServer("stun:stun.l.google.com:19302") },
  .disableAutoNegotiation = true
};

struct ClientTrackData
{
  std::shared_ptr<rtc::Track> track;
	std::shared_ptr<rtc::RtcpSrReporter> sender;

  void startStreaming()
  {
    double currentTime_s = get_monotonic_time_us(NULL, NULL)/(1000.0*1000.0);
    sender->rtpConfig->setStartTime(currentTime_s, rtc::RtpPacketizationConfig::EpochStart::T1970);
    sender->startRecording();
  }

  void sendTime()
  {
    double currentTime_s = get_monotonic_time_us(NULL, NULL)/(1000.0*1000.0);

    auto rtpConfig = sender->rtpConfig;
    uint32_t elapsedTimestamp = rtpConfig->secondsToTimestamp(currentTime_s);

    sender->rtpConfig->timestamp = sender->rtpConfig->startTimestamp + elapsedTimestamp;
    auto reportElapsedTimestamp = sender->rtpConfig->timestamp - sender->previousReportedTimestamp;
    if (sender->rtpConfig->timestampToSeconds(reportElapsedTimestamp) > 1) {
      sender->setNeedsToReport();
    }
  }

  bool wantsFrame() const
  {
    if (!track)
      return false;

    return track->isOpen();
  }
};

class Client
{
public:
  Client(std::shared_ptr<rtc::PeerConnection> pc_)
    : pc(pc_)
  {
    id.resize(20);
    for (auto & c : id) {
      c = 'a' + (rand() % 26);
    }
    id = "rtc-" + id;
    name = strdup(id.c_str());
  }

  ~Client()
  {
    free(name);
  }

  bool wantsFrame() const
  {
    if (!pc || !video)
      return false;
    if (pc->state() != rtc::PeerConnection::State::Connected)
      return false;
    return video->wantsFrame();
  }

  void pushFrame(buffer_t *buf)
  {
    if (!video || !video->track) {
      return;
    }

    if (!had_key_frame) {
      had_key_frame = buf->flags.is_keyframe;
    }

    if (!had_key_frame) {
      if (!requested_key_frame) {
        device_video_force_key(buf->buf_list->dev);
        requested_key_frame = true;
      }
      return;
    }

    rtc::binary data((std::byte*)buf->start, (std::byte*)buf->start + buf->used);
    video->sendTime();
    video->track->send(data);
  }

public:
  char *name = NULL;
  std::string id;
  std::shared_ptr<rtc::PeerConnection> pc;
  std::shared_ptr<ClientTrackData> video;
  std::mutex lock;
  std::condition_variable wait_for_complete;
  bool had_key_frame = false;
  bool requested_key_frame = false;
};

std::shared_ptr<Client> findClient(std::string id)
{
  std::unique_lock lk(webrtc_clients_lock);
  for (auto client : webrtc_clients) {
    if (client && client->id == id) {
      return client;
    }
  }

  return std::shared_ptr<Client>();
}

void removeClient(const std::shared_ptr<Client> &client, const char *reason)
{
  std::unique_lock lk(webrtc_clients_lock);
  webrtc_clients.erase(client);
  LOG_INFO(client.get(), "Client removed: %s.", reason);
}

std::shared_ptr<ClientTrackData> addVideo(const std::shared_ptr<rtc::PeerConnection> pc, const uint8_t payloadType, const uint32_t ssrc, const std::string cname, const std::string msid)
{
  auto video = rtc::Description::Video(cname, rtc::Description::Direction::SendOnly);
  video.addH264Codec(payloadType);
  video.setBitrate(1000);
  video.addSSRC(ssrc, cname, msid, cname);
  auto track = pc->addTrack(video);
  auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, cname, payloadType, rtc::H264RtpPacketizer::defaultClockRate);
  auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(rtc::H264RtpPacketizer::Separator::LongStartSequence, rtpConfig);
  auto h264Handler = std::make_shared<rtc::H264PacketizationHandler>(packetizer);
  auto srReporter = std::make_shared<rtc::RtcpSrReporter>(rtpConfig);
  h264Handler->addToChain(srReporter);
  auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
  h264Handler->addToChain(nackResponder);
  track->setMediaHandler(h264Handler);
  return std::shared_ptr<ClientTrackData>(new ClientTrackData{track, srReporter});
}

std::shared_ptr<Client> createPeerConnection(const rtc::Configuration &config)
{
  auto pc = std::make_shared<rtc::PeerConnection>(config);
  auto client = std::make_shared<Client>(pc);
  auto wclient = std::weak_ptr(client);

  pc->onTrack([wclient](std::shared_ptr<rtc::Track> track) {
    if(auto client = wclient.lock()) {
      LOG_DEBUG(client.get(), "onTrack: %s", track->mid().c_str());
    }
  });

  pc->onLocalDescription([wclient](rtc::Description description) {
    if(auto client = wclient.lock()) {
      LOG_DEBUG(client.get(), "onLocalDescription: %s", description.typeString().c_str());
    }
  });

  pc->onSignalingStateChange([wclient](rtc::PeerConnection::SignalingState state) {
    if(auto client = wclient.lock()) {
      LOG_DEBUG(client.get(), "onSignalingStateChange: %d", (int)state);
    }
  });

  pc->onStateChange([wclient](rtc::PeerConnection::State state) {
    if(auto client = wclient.lock()) {
      LOG_DEBUG(client.get(), "onStateChange: %d", (int)state);

      if (state == rtc::PeerConnection::State::Disconnected ||
        state == rtc::PeerConnection::State::Failed ||
        state == rtc::PeerConnection::State::Closed)
      {
        removeClient(client, "stream closed");
      }
    }
  });

  pc->onGatheringStateChange([wclient](rtc::PeerConnection::GatheringState state) {
    if(auto client = wclient.lock()) {
      LOG_DEBUG(client.get(), "onGatheringStateChange: %d", (int)state);

      if (state == rtc::PeerConnection::GatheringState::Complete) {
        client->wait_for_complete.notify_all();
      }
    }
  });

  std::unique_lock lk(webrtc_clients_lock);
  webrtc_clients.insert(client);
  return client;
}

static bool webrtc_h264_needs_buffer(buffer_lock_t *buf_lock)
{
  std::unique_lock lk(webrtc_clients_lock);
  for (auto client : webrtc_clients) {
    if (client->wantsFrame())
      return true;
  }

  return false;
}

static void webrtc_h264_capture(buffer_lock_t *buf_lock, buffer_t *buf)
{
  std::unique_lock lk(webrtc_clients_lock);
  for (auto client : webrtc_clients) {
    if (client->wantsFrame())
      client->pushFrame(buf);
  }
}

static void http_webrtc_request(http_worker_t *worker, FILE *stream, const nlohmann::json &message)
{
  auto client = createPeerConnection(webrtc_configuration);
  LOG_INFO(client.get(), "Stream requested.");

  client->video = addVideo(client->pc, webrtc_client_video_payload_type, rand(), "video", "");

  try {
    {
      std::unique_lock lock(client->lock);
      client->pc->setLocalDescription();
      client->wait_for_complete.wait_for(lock, webrtc_client_lock_timeout);
    }

    if (client->pc->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
      auto description = client->pc->localDescription();
      nlohmann::json message;
      message["id"] = client->id;
      message["type"] = description->typeString();
      message["sdp"] = std::string(description.value());
      http_write_response(stream, "200 OK", "application/json", message.dump().c_str(), 0);
      LOG_VERBOSE(client.get(), "Local SDP Offer: %s", std::string(message["sdp"]).c_str());
    } else {
      http_500(stream, "Not complete");
    }
  } catch(const std::exception &e) {
    http_500(stream, e.what());
    removeClient(client, e.what());
  }
}

static void http_webrtc_answer(http_worker_t *worker, FILE *stream, const nlohmann::json &message)
{
  if (!message.contains("id") || !message.contains("sdp")) {
    http_400(stream, "no sdp or id");
    return;
  }

  if (auto client = findClient(message["id"])) {
    LOG_INFO(client.get(), "Answer received.");
    LOG_VERBOSE(client.get(), "Remote SDP Answer: %s", std::string(message["sdp"]).c_str());

    try {
      auto answer = rtc::Description(std::string(message["sdp"]), std::string(message["type"]));
      client->pc->setRemoteDescription(answer);
      client->video->startStreaming();
      http_write_response(stream, "200 OK", "application/json", "{}", 0);
    } catch(const std::exception &e) {
      http_500(stream, e.what());
      removeClient(client, e.what());
    }
  } else {
    http_404(stream, "No client found");
  }
}

static void http_webrtc_offer(http_worker_t *worker, FILE *stream, const nlohmann::json &message)
{
  if (!message.contains("sdp")) {
    http_400(stream, "no sdp");
    return;
  }

  auto offer = rtc::Description(std::string(message["sdp"]), std::string(message["type"]));
  auto client = createPeerConnection(webrtc_configuration);

  LOG_INFO(client.get(), "Offer received.");
  LOG_VERBOSE(client.get(), "Remote SDP Offer: %s", std::string(message["sdp"]).c_str());

  try {
    client->video = addVideo(client->pc, webrtc_client_video_payload_type, rand(), "video", "");
    client->video->startStreaming();

    {
      std::unique_lock lock(client->lock);
      client->pc->setRemoteDescription(offer);
      client->pc->setLocalDescription();
      client->wait_for_complete.wait_for(lock, webrtc_client_lock_timeout);
    }

    if (client->pc->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
      auto description = client->pc->localDescription();
      nlohmann::json message;
      message["type"] = description->typeString();
      message["sdp"] = std::string(description.value());
      http_write_response(stream, "200 OK", "application/json", message.dump().c_str(), 0);

      LOG_VERBOSE(client.get(), "Local SDP Answer: %s", std::string(message["sdp"]).c_str());
    } else {
      http_500(stream, "Not complete");
    }
  } catch(const std::exception &e) {
    http_500(stream, e.what());
    removeClient(client, e.what());
  }
}

nlohmann::json http_parse_json_body(http_worker_t *worker, FILE *stream)
{
  std::string text;

  size_t i = 0;
  size_t n = (size_t)worker->content_length;
  if (n < 0 || n > webrtc_client_max_json_body)
    n = webrtc_client_max_json_body;

  text.resize(n);

  while (i < n && !feof(stream)) {
    i += fread(&text[i], 1, n-i, stream);
  }
  text.resize(i);

  return nlohmann::json::parse(text);
}

extern "C" void http_webrtc_offer(http_worker_t *worker, FILE *stream)
{
  auto message = http_parse_json_body(worker, stream);

  if (!message.contains("type")) {
    http_400(stream, "missing 'type'");
    return;
  }

  std::string type = message["type"];

  LOG_DEBUG(worker, "Recevied: '%s'", type.c_str());

  if (type == "request") {
    http_webrtc_request(worker, stream, message);
  } else if (type == "answer") {
    http_webrtc_answer(worker, stream, message);
  } else if (type == "offer") {
    http_webrtc_offer(worker, stream, message);
  } else {
    http_400(stream, (std::string("Not expected: " + type)).c_str());
  }
}

extern "C" int webrtc_server(webrtc_options_t *options)
{
  for (const auto &ice_server : str_split(options->ice_servers, OPTION_VALUE_LIST_SEP_CHAR)) {
    webrtc_configuration.iceServers.push_back(rtc::IceServer(ice_server));
  }

  buffer_lock_register_check_streaming(&video_lock, webrtc_h264_needs_buffer);
  buffer_lock_register_notify_buffer(&video_lock, webrtc_h264_capture);
  options->running = true;
  return 0;
}

#else // USE_LIBDATACHANNEL

extern "C" void http_webrtc_offer(http_worker_t *worker, FILE *stream)
{
  http_404(stream, NULL);
}

extern "C" int webrtc_server(webrtc_options_t *options)
{
  return 0;
}

#endif // USE_LIBDATACHANNEL
