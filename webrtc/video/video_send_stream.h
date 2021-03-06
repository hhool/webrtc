/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_
#define WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_

#include <map>
#include <vector>

#include "webrtc/call/bitrate_allocator.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/call.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/video/encoded_frame_callback_adapter.h"
#include "webrtc/video/encoder_state_feedback.h"
#include "webrtc/video/payload_router.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video/video_capture_input.h"
#include "webrtc/video/vie_channel.h"
#include "webrtc/video/vie_encoder.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class BitrateAllocator;
class CallStats;
class CongestionController;
class ProcessThread;
class RtpRtcp;
class ViEChannel;
class ViEEncoder;
class VieRemb;

namespace internal {

class VideoSendStream : public webrtc::VideoSendStream,
                        public webrtc::CpuOveruseObserver,
                        public webrtc::BitrateAllocatorObserver {
 public:
  VideoSendStream(int num_cpu_cores,
                  ProcessThread* module_process_thread,
                  CallStats* call_stats,
                  CongestionController* congestion_controller,
                  BitrateAllocator* bitrate_allocator,
                  VieRemb* remb,
                  const VideoSendStream::Config& config,
                  const VideoEncoderConfig& encoder_config,
                  const std::map<uint32_t, RtpState>& suspended_ssrcs);

  ~VideoSendStream() override;

  // webrtc::SendStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(NetworkState state) override;
  bool DeliverRtcp(const uint8_t* packet, size_t length) override;

  // webrtc::VideoSendStream implementation.
  VideoCaptureInput* Input() override;
  void ReconfigureVideoEncoder(const VideoEncoderConfig& config) override;
  Stats GetStats() override;

  // webrtc::CpuOveruseObserver implementation.
  void OveruseDetected() override;
  void NormalUsage() override;

  typedef std::map<uint32_t, RtpState> RtpStateMap;
  RtpStateMap GetRtpStates() const;

  int GetPaddingNeededBps() const;

  // Implements BitrateAllocatorObserver.
  void OnBitrateUpdated(uint32_t bitrate_bps,
                        uint8_t fraction_loss,
                        int64_t rtt) override;

 private:
  struct EncoderSettings {
    VideoCodec video_codec;
    int min_transmit_bitrate_bps;
  };
  static bool EncoderThreadFunction(void* obj);
  void EncoderProcess();

  void ConfigureSsrcs();

  SendStatisticsProxy stats_proxy_;
  EncodedFrameCallbackAdapter encoded_frame_proxy_;
  const VideoSendStream::Config config_;
  std::map<uint32_t, RtpState> suspended_ssrcs_;

  ProcessThread* const module_process_thread_;
  CallStats* const call_stats_;
  CongestionController* const congestion_controller_;
  BitrateAllocator* const bitrate_allocator_;
  VieRemb* const remb_;

  rtc::PlatformThread encoder_thread_;
  rtc::Event encoder_wakeup_event_;
  volatile int stop_encoder_thread_;
  rtc::CriticalSection encoder_settings_crit_;
  rtc::Optional<EncoderSettings> pending_encoder_settings_
      GUARDED_BY(encoder_settings_crit_);

  OveruseFrameDetector overuse_detector_;
  PayloadRouter payload_router_;
  EncoderStateFeedback encoder_feedback_;
  ViEChannel vie_channel_;
  ViEReceiver* const vie_receiver_;
  ViEEncoder vie_encoder_;
  VideoCodingModule* const vcm_;
  // TODO(pbos): Move RtpRtcp ownership to VideoSendStream.
  // RtpRtcp modules, currently owned by ViEChannel but ownership should
  // eventually move here.
  const std::vector<RtpRtcp*> rtp_rtcp_modules_;
  VideoCaptureInput input_;
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_
