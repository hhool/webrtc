/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/webrtcmediaengine.h"

#include <algorithm>

#ifdef HAVE_WEBRTC_VIDEO
#include "webrtc/media/engine/webrtcvideoengine2.h"
#else
#include "webrtc/media/engine/nullwebrtcvideoengine.h"
#endif
#include "webrtc/media/engine/webrtcvoiceengine.h"

namespace cricket {

class WebRtcMediaEngine2
#ifdef HAVE_WEBRTC_VIDEO
    : public CompositeMediaEngine<WebRtcVoiceEngine, WebRtcVideoEngine2> {
#else
    : public CompositeMediaEngine<WebRtcVoiceEngine, NullWebRtcVideoEngine> {
#endif
 public:
  WebRtcMediaEngine2(webrtc::AudioDeviceModule* adm,
                     WebRtcVideoEncoderFactory* encoder_factory,
                     WebRtcVideoDecoderFactory* decoder_factory) {
    voice_.SetAudioDeviceModule(adm);
    video_.SetExternalDecoderFactory(decoder_factory);
    video_.SetExternalEncoderFactory(encoder_factory);
  }
};

}  // namespace cricket

cricket::MediaEngineInterface* CreateWebRtcMediaEngine(
    webrtc::AudioDeviceModule* adm,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory) {
  return new cricket::WebRtcMediaEngine2(adm, encoder_factory,
                                         decoder_factory);
}

void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine) {
  delete media_engine;
}

namespace cricket {

// Used by PeerConnectionFactory to create a media engine passed into
// ChannelManager.
MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    WebRtcVideoEncoderFactory* encoder_factory,
    WebRtcVideoDecoderFactory* decoder_factory) {
  return CreateWebRtcMediaEngine(adm, encoder_factory, decoder_factory);
}

namespace {
// Remove mutually exclusive extensions with lower priority.
void DiscardRedundantExtensions(
    std::vector<webrtc::RtpExtension>* extensions,
    rtc::ArrayView<const char*> extensions_decreasing_prio) {
  RTC_DCHECK(extensions);
  bool found = false;
  for (const char* name : extensions_decreasing_prio) {
    auto it = std::find_if(extensions->begin(), extensions->end(),
        [name](const webrtc::RtpExtension& rhs) {
          return rhs.name == name;
        });
    if (it != extensions->end()) {
      if (found) {
        extensions->erase(it);
      }
      found = true;
    }
  }
}
}  // namespace

bool ValidateRtpExtensions(const std::vector<RtpHeaderExtension>& extensions) {
  bool id_used[14] = {false};
  for (const auto& extension : extensions) {
    if (extension.id <= 0 || extension.id >= 15) {
      LOG(LS_ERROR) << "Bad RTP extension ID: " << extension.ToString();
      return false;
    }
    if (id_used[extension.id - 1]) {
      LOG(LS_ERROR) << "Duplicate RTP extension ID: " << extension.ToString();
      return false;
    }
    id_used[extension.id - 1] = true;
  }
  return true;
}

std::vector<webrtc::RtpExtension> FilterRtpExtensions(
    const std::vector<RtpHeaderExtension>& extensions,
    bool (*supported)(const std::string&),
    bool filter_redundant_extensions) {
  RTC_DCHECK(ValidateRtpExtensions(extensions));
  RTC_DCHECK(supported);
  std::vector<webrtc::RtpExtension> result;

  // Ignore any extensions that we don't recognize.
  for (const auto& extension : extensions) {
    if (supported(extension.uri)) {
      result.push_back({extension.uri, extension.id});
    } else {
      LOG(LS_WARNING) << "Unsupported RTP extension: " << extension.ToString();
    }
  }

  // Sort by name, ascending, so that we don't reset extensions if they were
  // specified in a different order (also allows us to use std::unique below).
  std::sort(result.begin(), result.end(),
      [](const webrtc::RtpExtension& rhs, const webrtc::RtpExtension& lhs) {
        return rhs.name < lhs.name;
      });

  // Remove unnecessary extensions (used on send side).
  if (filter_redundant_extensions) {
    auto it = std::unique(result.begin(), result.end(),
        [](const webrtc::RtpExtension& rhs, const webrtc::RtpExtension& lhs) {
          return rhs.name == lhs.name;
        });
    result.erase(it, result.end());

    // Keep just the highest priority extension of any in the following list.
    static const char* kBweExtensionPriorities[] = {
      kRtpTransportSequenceNumberHeaderExtension,
      kRtpAbsoluteSenderTimeHeaderExtension,
      kRtpTimestampOffsetHeaderExtension
    };
    DiscardRedundantExtensions(&result, kBweExtensionPriorities);
  }

  return result;
}
}  // namespace cricket
