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
#include "util/http/json.hh"

#ifdef USE_LIBDATACHANNEL

#include <string>
#include <memory>
#include <optional>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <set>
#include <rtc/peerconnection.hpp>
#include <rtc/rtcpsrreporter.hpp>
#include <rtc/h264rtppacketizer.hpp>
#include <rtc/h264packetizationhandler.hpp>
#include <rtc/rtcpnackresponder.hpp>

#include "third_party/magic_enum/include/magic_enum.hpp"

using namespace std::chrono_literals;

class Client;

static webrtc_options_t *webrtc_options;
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

  void describePeerConnection(nlohmann::json &message)
  {
    nlohmann::json ice_servers = nlohmann::json::array();

    for (const auto &ice_server : pc->config()->iceServers) {
      nlohmann::json json;

      std::string url;

      if (ice_server.type == rtc::IceServer::Type::Turn) {
        json["username"] = ice_server.username;
        json["credential"] = ice_server.password;
        url = ice_server.relayType == rtc::IceServer::RelayType::TurnTls ? "turns:" : "turn:";
      } else {
        url = "stun:";
      }

      url += ice_server.hostname + ":" + std::to_string(ice_server.port);
      json["urls"] = url;

      ice_servers.push_back(json);
    }

    message["iceServers"] = ice_servers;
  }

public:
  char *name = NULL;
  std::string id;
  std::shared_ptr<rtc::PeerConnection> pc;
  std::shared_ptr<ClientTrackData> video;
  std::mutex lock;
  std::condition_variable wait_for_complete;
  std::vector<rtc::Candidate> pending_remote_candidates;
  bool has_set_sdp_answer = false;
  bool had_key_frame = false;
  bool requested_key_frame = false;
};

std::shared_ptr<Client> webrtc_find_client(std::string id)
{
  std::unique_lock lk(webrtc_clients_lock);
  for (auto client : webrtc_clients) {
    if (client && client->id == id) {
      return client;
    }
  }

  return std::shared_ptr<Client>();
}

static void webrtc_remove_client(const std::shared_ptr<Client> &client, const char *reason)
{
  std::unique_lock lk(webrtc_clients_lock);
  webrtc_clients.erase(client);
  LOG_INFO(client.get(), "Client removed: %s.", reason);
}

static std::shared_ptr<ClientTrackData> webrtc_add_video(const std::shared_ptr<rtc::PeerConnection> pc, const uint8_t payloadType, const uint32_t ssrc, const std::string cname, const std::string msid)
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

static void webrtc_parse_ice_servers(rtc::Configuration &config, const nlohmann::json &message)
{
  auto ice_servers = message.find("ice_servers");
  if (ice_servers == message.end() || !ice_servers->is_array())
    return;

  if (webrtc_options->disable_client_ice) {
    LOG_VERBOSE(NULL, "ICE server from SDP request ignored due to `disable_client_ice`: %s",
      ice_servers->dump().c_str());
    return;
  }

  for (const auto& ice_server : *ice_servers) {
    try {
      auto urls = ice_server["urls"];

      // convert non array to array
      if (!urls.is_array()) {
        urls = nlohmann::json::array();
        urls.push_back(ice_server["urls"]);
      }

      for (const auto& url : urls) {
        auto iceServer = rtc::IceServer(url.get<std::string>());
        if (iceServer.type == rtc::IceServer::Type::Turn) {
          if (ice_server.contains("username"))
            iceServer.username = ice_server["username"].get<std::string>();
          if (ice_server.contains("credential"))
            iceServer.password = ice_server["credential"].get<std::string>();
        }

        config.iceServers.push_back(iceServer);
        LOG_VERBOSE(NULL, "Added ICE server from SDP request json: %s", url.dump().c_str());
      }

    } catch (nlohmann::detail::exception &e) {
      LOG_VERBOSE(NULL, "Failed to parse ICE server: %s: %s",
        ice_server.dump().c_str(), e.what());
    }
  }
}

static std::shared_ptr<Client> webrtc_peer_connection(rtc::Configuration config, const nlohmann::json &message)
{
  webrtc_parse_ice_servers(config, message);

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

      if(state == rtc::PeerConnection::State::Connected) {
        // Start streaming once the client is connected, to ensure a keyframe is sent to start the stream.
        std::unique_lock lock(client->lock);
        client->video->startStreaming();
      } else if (state == rtc::PeerConnection::State::Disconnected ||
        state == rtc::PeerConnection::State::Failed ||
        state == rtc::PeerConnection::State::Closed)
      {
        webrtc_remove_client(client, "stream closed");
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
  auto client = webrtc_peer_connection(webrtc_configuration, message);
  LOG_INFO(client.get(), "Stream requested.");

  client->video = webrtc_add_video(client->pc, webrtc_client_video_payload_type, rand(), "video", "");

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
      client->describePeerConnection(message);
      http_write_response(stream, "200 OK", "application/json", message.dump().c_str(), 0);
      LOG_VERBOSE(client.get(), "Local SDP Offer: %s", std::string(message["sdp"]).c_str());
    } else {
      http_500(stream, "Not complete");
    }
  } catch(const std::exception &e) {
    http_500(stream, e.what());
    webrtc_remove_client(client, e.what());
  }
}

static void http_webrtc_answer(http_worker_t *worker, FILE *stream, const nlohmann::json &message)
{
  if (!message.contains("id") || !message.contains("sdp")) {
    http_400(stream, "no sdp or id");
    return;
  }

  if (auto client = webrtc_find_client(message["id"])) {
    LOG_INFO(client.get(), "Answer received.");
    LOG_VERBOSE(client.get(), "Remote SDP Answer: %s", std::string(message["sdp"]).c_str());

    try {
      auto answer = rtc::Description(std::string(message["sdp"]), std::string(message["type"]));
      client->pc->setRemoteDescription(answer);
      client->has_set_sdp_answer = true;

      // If there are any pending candidates that make it in before the answer request, add them now.
      for(auto const &candidate : client->pending_remote_candidates) {
        client->pc->addRemoteCandidate(candidate);
      }
      client->pending_remote_candidates.clear();
      http_write_response(stream, "200 OK", "application/json", "{}", 0);
    } catch(const std::exception &e) {
      http_500(stream, e.what());
      webrtc_remove_client(client, e.what());
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
  auto client = webrtc_peer_connection(webrtc_configuration, message);

  LOG_INFO(client.get(), "Offer received.");
  LOG_VERBOSE(client.get(), "Remote SDP Offer: %s", std::string(message["sdp"]).c_str());

  try {
    client->video = webrtc_add_video(client->pc, webrtc_client_video_payload_type, rand(), "video", "");

    {
      std::unique_lock lock(client->lock);
      client->pc->setRemoteDescription(offer);
      client->has_set_sdp_answer = true;
      client->pc->setLocalDescription();
      client->wait_for_complete.wait_for(lock, webrtc_client_lock_timeout);
    }

    if (client->pc->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
      auto description = client->pc->localDescription();
      nlohmann::json message;
      message["type"] = description->typeString();
      message["sdp"] = std::string(description.value());
      client->describePeerConnection(message);
      http_write_response(stream, "200 OK", "application/json", message.dump().c_str(), 0);

      LOG_VERBOSE(client.get(), "Local SDP Answer: %s", std::string(message["sdp"]).c_str());
    } else {
      http_500(stream, "Not complete");
    }
  } catch(const std::exception &e) {
    http_500(stream, e.what());
    webrtc_remove_client(client, e.what());
  }
}

static void http_webrtc_remote_candidate(http_worker_t *worker, FILE *stream, const nlohmann::json &message)
{
  if (!message.contains("candidates") || !message.contains("id") || !message["candidates"].is_array()) {
    http_400(stream, "candidates is not array or no id");
    return;
  }

  auto client = webrtc_find_client(message["id"]);
  if (!client) {
    http_404(stream, "No client found");
    return;
  }

  for (auto const & entry : message["candidates"]) {
    try {
      auto remoteCandidate = rtc::Candidate(
        entry["candidate"].get<std::string>(),
        entry["sdpMid"].get<std::string>());
  
      std::unique_lock lock(client->lock);
      // The ICE candidate http requests can race the sdp answer http request and win. But it's invalid to set the ICE
      // candidates before the SDP answer is set.
      if (client->has_set_sdp_answer) {
        client->pc->addRemoteCandidate(remoteCandidate);
      } else {
        client->pending_remote_candidates.push_back(remoteCandidate);
      }
    } catch (nlohmann::detail::exception &e) {
      http_400(stream, e.what());
      return;
    }
  }

  http_write_response(stream, "200 OK", "application/json", "{}", 0);
}

extern "C" void http_webrtc_offer(http_worker_t *worker, FILE *stream)
{
  auto message = http_parse_json_body(worker, stream, webrtc_client_max_json_body);

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
  } else if (type == "remote_candidate") {
    http_webrtc_remote_candidate(worker, stream, message);
  } else {
    http_400(stream, (std::string("Not expected: " + type)).c_str());
  }
}

extern "C" int webrtc_server(webrtc_options_t *options)
{
  webrtc_options = options;

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
