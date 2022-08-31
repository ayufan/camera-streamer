extern "C" {
#include "http/http.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "opts/control.h"
#include "webrtc.h"
};

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

template <class T> std::weak_ptr<T> make_weak_ptr(std::shared_ptr<T> ptr) { return ptr; }

struct ClientTrackData
{
  std::shared_ptr<rtc::Track> track;
	std::shared_ptr<rtc::RtcpSrReporter> sender;

  void startStreaming()
  {
    double currentTime_s = get_monotonic_time_us(NULL,NULL)/(1000.0*1000.0);
    sender->rtpConfig->setStartTime(currentTime_s, rtc::RtpPacketizationConfig::EpochStart::T1970);
    sender->startRecording();
  }

  void sendTime()
  {
    double currentTime_s = get_monotonic_time_us(NULL,NULL)/(1000.0*1000.0);

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
    : pc(pc_), use_low_res(false)
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
    return video.value()->wantsFrame();
  }

  void pushFrame(buffer_t *buf, bool low_res)
  {
    auto self = this;

    if (!video || !video.value()->track) {
      return;
    }

    if (use_low_res != low_res) {
      return;
    }

    if (!had_key_frame) {
      if (!h264_is_key_frame(buf)) {
        device_video_force_key(buf->buf_list->dev);
        LOG_VERBOSE(self, "Skipping as key frame was not yet sent.");
        return;
      }
      had_key_frame = true;
    }

    rtc::binary data((std::byte*)buf->start, (std::byte*)buf->start + buf->used);
    video.value()->sendTime();
    video.value()->track->send(data);
  }

public:
  char *name;
  std::string id;
  std::shared_ptr<rtc::PeerConnection> pc;
  std::optional<std::shared_ptr<ClientTrackData>> video;
  std::mutex lock;
  std::condition_variable wait_for_complete;
  bool had_key_frame;
  bool use_low_res;
};

static std::set<std::shared_ptr<Client> > clients;
static std::mutex clients_lock;
static const auto client_lock_timeout = 30 * 100ms;
static const auto client_max_json_body = 10 * 1024;
static const auto client_video_payload_type = 102; // 96 - VP8

rtc::Configuration getRtcConfiguration()
{
  rtc::Configuration config;
  std::string stunServer = "stun:stun.l.google.com:19302";
  config.iceServers.emplace_back(stunServer);
  config.disableAutoNegotiation = true;
  return config;
}

std::shared_ptr<Client> findClient(std::string id)
{
  std::unique_lock lk(clients_lock);
  for (auto client : clients) {
    if (client && client->id == id) {
      return client;
    }
  }

  return std::shared_ptr<Client>();
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

  pc->onSignalingStateChange([wclient = make_weak_ptr(client)](rtc::PeerConnection::SignalingState state) {
    if(auto client = wclient.lock()) {
      LOG_DEBUG(client.get(), "onSignalingStateChange: %d", (int)state);
    }
  });

  pc->onStateChange([wclient = make_weak_ptr(client)](rtc::PeerConnection::State state) {
    if(auto client = wclient.lock()) {
      LOG_DEBUG(client.get(), "onStateChange: %d", (int)state);

      if (state == rtc::PeerConnection::State::Disconnected ||
        state == rtc::PeerConnection::State::Failed ||
        state == rtc::PeerConnection::State::Closed)
      {
        std::unique_lock lk(clients_lock);
        clients.erase(client);
        LOG_INFO(client.get(), "Stream closed.");
      }
    }
  });

  pc->onGatheringStateChange([wclient = make_weak_ptr(client)](rtc::PeerConnection::GatheringState state) {
    if(auto client = wclient.lock()) {
      LOG_DEBUG(client.get(), "onGatheringStateChange: %d", (int)state);

      if (state == rtc::PeerConnection::GatheringState::Complete) {
        client->wait_for_complete.notify_all();
      }
    }
  });

  std::unique_lock lk(clients_lock);
  clients.insert(client);
  return client;
}

extern "C" bool http_webrtc_needs_buffer()
{
  std::unique_lock lk(clients_lock);
  for (auto client : clients) {
    if (client->wantsFrame())
      return true;
  }

  return false;
}

extern "C" void http_webrtc_capture(buffer_t *buf)
{
  std::unique_lock lk(clients_lock);
  for (auto client : clients) {
    if (client->wantsFrame()) {
      client->pushFrame(buf, false);
    }
  }
}

extern "C" void http_webrtc_low_res_capture(buffer_t *buf)
{
  std::unique_lock lk(clients_lock);
  for (auto client : clients) {
    if (client->wantsFrame()) {
      client->pushFrame(buf, true);
    }
  }
}

static void http_webrtc_request(http_worker_t *worker, FILE *stream, const nlohmann::json &message)
{
  auto client = createPeerConnection(getRtcConfiguration());
  LOG_INFO(client.get(), "Stream requested.");

  client->video = addVideo(client->pc, client_video_payload_type, rand(), "video", "");
  if (message.contains("res")) {
    client->use_low_res = message["res"] == "low";
  }

  {
    std::unique_lock lock(client->lock);
    client->pc->setLocalDescription();
    client->wait_for_complete.wait_for(lock, client_lock_timeout);
  }

  if (client->pc->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
    auto description = client->pc->localDescription();
    nlohmann::json message;
    message["id"] = client->id;
    message["type"] = description->typeString();
    message["sdp"] = std::string(description.value());
    http_write_response(stream, "200 OK", "application/json", message.dump().c_str(), 0);
  } else {
    http_500(stream, "Not complete");
  }
}

static void http_webrtc_answer(http_worker_t *worker, FILE *stream, const nlohmann::json &message)
{
  if (!message.contains("id") || !message.contains("sdp")) {
    http_500(stream, "no sdp or id");
    return;
  }

  if (auto client = findClient(message["id"])) {
    LOG_INFO(client.get(), "Answer received.");
    auto answer = rtc::Description(std::string(message["sdp"]), std::string(message["type"]));
    client->pc->setRemoteDescription(answer);
    client->video.value()->startStreaming();
    http_write_response(stream, "200 OK", "application/json", "{}", 0);
  } else {
    http_404(stream, "No client found");
  }
}

static void http_webrtc_offer(http_worker_t *worker, FILE *stream, const nlohmann::json &message)
{
  if (!message.contains("sdp")) {
    http_500(stream, "no sdp");
    return;
  }

  auto offer = rtc::Description(std::string(message["sdp"]), std::string(message["type"]));
  auto client = createPeerConnection(getRtcConfiguration());
  LOG_INFO(client.get(), "Offer received.");

  client->video = addVideo(client->pc, client_video_payload_type, rand(), "video", "");
  client->video.value()->startStreaming();

  {
    std::unique_lock lock(client->lock);
    client->pc->setRemoteDescription(offer);
    client->pc->setLocalDescription();
    client->wait_for_complete.wait_for(lock, client_lock_timeout);
  }

  if (client->pc->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
    auto description = client->pc->localDescription();
    nlohmann::json message;
    message["type"] = description->typeString();
    message["sdp"] = std::string(description.value());
    http_write_response(stream, "200 OK", "application/json", message.dump().c_str(), 0);
  } else {
    http_500(stream, "Not complete");
  }
}

nlohmann::json http_parse_json_body(http_worker_t *worker, FILE *stream)
{
  std::string text;

  size_t i = 0;
  size_t n = (size_t)worker->content_length;
  if (n < 0 || n > client_max_json_body)
    n = client_max_json_body;

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

extern "C" void http_run_webrtc()
{
}

#else // USE_LIBDATACHANNEL

extern "C" bool http_webrtc_needs_buffer()
{
  return false;
}

extern "C" void http_webrtc_capture(buffer_t *buf)
{
}

extern "C" void http_webrtc_low_res_capture(buffer_t *buf)
{
}

extern "C" void http_webrtc_offer(http_worker_t *worker, FILE *stream)
{
  http_404(stream, NULL);
}

extern "C" void http_run_webrtc()
{
}

#endif // USE_LIBDATACHANNEL
