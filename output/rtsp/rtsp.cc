extern "C" {

#include "util/http/http.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/buffer_lock.h"
#include "device/device.h"
#include "util/opts/log.h"
#include "util/opts/fourcc.h"
#include "util/opts/control.h"
#include "output/output.h"
#include "rtsp.h"

};

#ifdef USE_RTSP

#include <string>
#include <memory>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <set>

#include <BasicUsageEnvironment.hh>
#include <RTSPServerSupportingHTTPStreaming.hh>
#include <OnDemandServerMediaSubsession.hh>
#include <H264VideoStreamFramer.hh>
#include <H264VideoRTPSink.hh>

static pthread_t rtsp_thread;
static std::set<class DynamicH264Stream *> rtsp_streams;
static std::recursive_mutex rtsp_streams_lock;
static rtsp_options_t *rtsp_options;

static const char *stream_name = "stream.h264";

class DynamicH264Stream : public FramedSource
{
public:
  DynamicH264Stream(UsageEnvironment& env)
    : FramedSource(env), fHaveStartedReading(False), locked_buf(NULL), locked_buf_offset(0)
  {
  }

  void doGetNextFrame()
  {
    {
      std::unique_lock streams_lock(rtsp_streams_lock);
      if (!fHaveStartedReading) {
        rtsp_streams.insert(this);
        fHaveStartedReading = True;
      }
    }

    send_buffer();
  }

  void doStopGettingFrames()
  {
    {
      std::unique_lock streams_lock(rtsp_streams_lock);
      rtsp_streams.erase(this);
      fHaveStartedReading = false;
    }

    set_buffer(NULL);
  }

  void receive_buf(buffer_t *buf)
  {
    std::unique_lock lk(lock);

    if (!fHaveStartedReading) {
      return;
    }

    if (locked_buf || !isCurrentlyAwaitingData()) {
      if (rtsp_options) {
        rtsp_options->dropped++;
      }
      return;
    }

    if (!fHadKeyFrame) {
      fHadKeyFrame = buf->flags.is_keyframe;
    }

    if (!fHadKeyFrame) {
      if (!fRequestedKeyFrame) {
        device_video_force_key(buf->buf_list->dev);
        fRequestedKeyFrame = true;
      }
      if (rtsp_options) {
        rtsp_options->dropped++;
      }
      return;
    }

    rtsp_options->frames++;
    set_buffer(buf);
  }

  void set_buffer(buffer_t *buf)
  {
    std::unique_lock lk(lock);

    if (locked_buf) {
      buffer_consumed(locked_buf, "rstp");
      locked_buf = NULL;
    }

    locked_buf = buf;
    locked_buf_offset = 0;
    buffer_use(buf);
  }

  bool send_buffer()
  {
    std::unique_lock lk(lock);

    if (!locked_buf)
      return false;

    if (!isCurrentlyAwaitingData())
      return false;

    if (locked_buf->used - locked_buf_offset > fMaxSize) {
      fNumTruncatedBytes = locked_buf->used - fMaxSize - locked_buf_offset;
      fFrameSize = fMaxSize;
    } else {
      fNumTruncatedBytes = 0;
      fFrameSize = locked_buf->used - locked_buf_offset;
    }

    memcpy(fTo, (char*)locked_buf->start + locked_buf_offset, fFrameSize);
    locked_buf_offset += fFrameSize;

    if (locked_buf->used == locked_buf_offset) {
      set_buffer(NULL);
    } else if (rtsp_options->allow_truncated) {
      rtsp_options->truncated++;
      set_buffer(NULL);
    }

    afterGetting(this);
    return true;
  }

  Boolean fHaveStartedReading;
  Boolean fHadKeyFrame;
  Boolean fRequestedKeyFrame;

  std::recursive_mutex lock;
  buffer_t *locked_buf;
  unsigned locked_buf_offset;
};

class DynamicH264VideoFileServerMediaSubsession : public OnDemandServerMediaSubsession
{
public:
  DynamicH264VideoFileServerMediaSubsession(UsageEnvironment& env, Boolean reuseFirstSource)
    : OnDemandServerMediaSubsession(env, reuseFirstSource)
  {
  }

  virtual FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate)
  {
    estBitrate = 500; // kbps, estimate
    return H264VideoStreamFramer::createNew(envir(), new DynamicH264Stream(envir()));
  }

  virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* /*inputSource*/)
  {
    return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
  }
};

class DynamicRTSPServer: public RTSPServerSupportingHTTPStreaming
{
public:
  static DynamicRTSPServer* createNew(UsageEnvironment& env, Port ourPort,
				      UserAuthenticationDatabase* authDatabase,
				      unsigned reclamationTestSeconds = 65)
  {
    int ourSocket = setUpOurSocket(env, ourPort);
    if (ourSocket == -1) return NULL;

    return new DynamicRTSPServer(env, ourSocket, ourPort, authDatabase, reclamationTestSeconds);
  }

protected:
  DynamicRTSPServer(UsageEnvironment& env, int ourSocket, Port ourPort,
		    UserAuthenticationDatabase* authDatabase, unsigned reclamationTestSeconds)
    : RTSPServerSupportingHTTPStreaming(env, ourSocket, ourPort, authDatabase, reclamationTestSeconds)
  {
  }

  // called only by createNew();
  virtual ~DynamicRTSPServer()
  {
  }

protected: // redefined virtual functions
  virtual ServerMediaSession* lookupServerMediaSession(char const* streamName, Boolean isFirstLookupInSession)
  {
    if (strcmp(streamName, stream_name) == 0) {
      LOG_INFO(NULL, "Requesting %s stream...", streamName);
    } else {
      LOG_INFO(NULL, "No stream available: '%s'", streamName);
      return NULL;
    }

    auto sms = RTSPServer::lookupServerMediaSession(streamName);

    if (sms && isFirstLookupInSession) { 
      // Remove the existing "ServerMediaSession" and create a new one, in case the underlying
      // file has changed in some way:
      removeServerMediaSession(sms); 
      sms = NULL;
    }

    sms = ServerMediaSession::createNew(envir(), streamName, streamName, "streamed by the LIVE555 Media Server");;
    OutPacketBuffer::maxSize = 2000000; // allow for some possibly large H.264 frames

    auto subsession = new DynamicH264VideoFileServerMediaSubsession(envir(), false);
    sms->addSubsession(subsession);
    addServerMediaSession(sms);
    return sms;
  }
};

static void rtsp_frame_finish()
{
  std::unique_lock lk(rtsp_streams_lock);
  for (auto *stream : rtsp_streams) {
    stream->send_buffer();
  }
  if (rtsp_options) {
    rtsp_options->clients = rtsp_streams.size();
  }
}

static void *rtsp_server_thread(void *opaque)
{
  UsageEnvironment* env = (UsageEnvironment*)opaque;
  BasicTaskScheduler0* taskScheduler = (BasicTaskScheduler*)&env->taskScheduler();

  while (true) {
    rtsp_frame_finish();
    taskScheduler->SingleStep(0);
  }
  return NULL;
}

static bool rtsp_h264_needs_buffer(buffer_lock_t *buf_lock)
{
  std::unique_lock lk(rtsp_streams_lock);
  return rtsp_streams.size() > 0;
}

static void rtsp_h264_capture(buffer_lock_t *buf_lock, buffer_t *buf)
{
  std::unique_lock lk(rtsp_streams_lock);
  for (auto *stream : rtsp_streams) {
    stream->receive_buf(buf);
  }
}

extern "C" int rtsp_server(rtsp_options_t *options)
{
  rtsp_options = options;

  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
  UserAuthenticationDatabase* authDB = NULL;

#ifdef ACCESS_CONTROL
  // To implement client access control to the RTSP server, do the following:
  authDB = new UserAuthenticationDatabase;
  authDB->addUserRecord("username1", "password1"); // replace these with real strings
  // Repeat the above with each <username>, <password> that you wish to allow
  // access to the server.
#endif

  RTSPServer* rtspServer;
  rtspServer = DynamicRTSPServer::createNew(*env, options->port, authDB);
  if (rtspServer == NULL) {
    LOG_ERROR(NULL, "Failed to create RTSP server: %s", env->getResultMsg());
    return -1;
  }
  LOG_INFO(NULL, "Running RTSP server on '%d'", options->port);

  // if (rtspServer->setUpTunnelingOverHTTP(80) || rtspServer->setUpTunnelingOverHTTP(8000) || rtspServer->setUpTunnelingOverHTTP(8080)) {
  //   LOG_INFO(NULL, "Running RTSP-over-HTTP tunneling on '%d'", rtspServer->httpServerPortNum());
  //   *env << "(We use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-HTTP tunneling, or for HTTP live streaming (for indexed Transport Stream files only).)\n";
  // } else {
  //   LOG_INFO(NULL, "The RTSP-over-HTTP is not available.");
  // }

  buffer_lock_register_check_streaming(&video_lock, rtsp_h264_needs_buffer);
  buffer_lock_register_notify_buffer(&video_lock, rtsp_h264_capture);

  pthread_create(&rtsp_thread, NULL, rtsp_server_thread, env);
  options->running = true;
  return 0;

error:
  return -1;
}

#else // USE_RTSP

extern "C" int rtsp_server(rtsp_options_t *options)
{
  return 0;
}

#endif // USE_RTSP
