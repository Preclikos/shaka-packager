// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/webm/webm_audio_client.h"

#include "packager/media/base/audio_decoder_config.h"
#include "packager/media/base/channel_layout.h"
#include "packager/media/formats/webm/webm_constants.h"

namespace edash_packager {
namespace media {

WebMAudioClient::WebMAudioClient() {
  Reset();
}

WebMAudioClient::~WebMAudioClient() {
}

void WebMAudioClient::Reset() {
  channels_ = -1;
  samples_per_second_ = -1;
  output_samples_per_second_ = -1;
}

bool WebMAudioClient::InitializeConfig(
    const std::string& codec_id,
    const std::vector<uint8_t>& codec_private,
    int64_t seek_preroll,
    int64_t codec_delay,
    bool is_encrypted,
    AudioDecoderConfig* config) {
  DCHECK(config);
  SampleFormat sample_format = kSampleFormatPlanarF32;

  AudioCodec audio_codec = kUnknownAudioCodec;
  if (codec_id == "A_VORBIS") {
    audio_codec = kCodecVorbis;
  } else if (codec_id == "A_OPUS") {
    audio_codec = kCodecOpus;
  } else {
    LOG(ERROR) << "Unsupported audio codec_id " << codec_id;
    return false;
  }

  if (samples_per_second_ <= 0)
    return false;

  // Set channel layout default if a Channels element was not present.
  if (channels_ == -1)
    channels_ = 1;

  ChannelLayout channel_layout =  GuessChannelLayout(channels_);

  if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED) {
    LOG(ERROR) << "Unsupported channel count " << channels_;
    return false;
  }

  int samples_per_second = samples_per_second_;
  if (output_samples_per_second_ > 0)
    samples_per_second = output_samples_per_second_;

  // Always use 48kHz for OPUS.  See the "Input Sample Rate" section of the
  // spec: http://tools.ietf.org/html/draft-terriberry-oggopus-01#page-11
  if (audio_codec == kCodecOpus) {
    samples_per_second = 48000;
    sample_format = kSampleFormatF32;
  }

  const uint8_t* extra_data = NULL;
  size_t extra_data_size = 0;
  if (codec_private.size() > 0) {
    extra_data = &codec_private[0];
    extra_data_size = codec_private.size();
  }

  // Convert |codec_delay| from nanoseconds into frames.
  int codec_delay_in_frames = 0;
  if (codec_delay != -1) {
    codec_delay_in_frames =
        0.5 +
        samples_per_second * (static_cast<double>(codec_delay) /
                              base::Time::kNanosecondsPerSecond);
  }

  config->Initialize(
      audio_codec,
      sample_format,
      channel_layout,
      samples_per_second,
      extra_data,
      extra_data_size,
      is_encrypted,
      base::TimeDelta::FromMicroseconds(
          (seek_preroll != -1 ? seek_preroll : 0) / 1000),
      codec_delay_in_frames);
  return config->IsValidConfig();
}

bool WebMAudioClient::OnUInt(int id, int64_t val) {
  if (id == kWebMIdChannels) {
    if (channels_ != -1) {
      LOG(ERROR) << "Multiple values for id " << std::hex << id
                 << " specified. (" << channels_ << " and " << val << ")";
      return false;
    }

    channels_ = val;
  }
  return true;
}

bool WebMAudioClient::OnFloat(int id, double val) {
  double* dst = NULL;

  switch (id) {
    case kWebMIdSamplingFrequency:
      dst = &samples_per_second_;
      break;
    case kWebMIdOutputSamplingFrequency:
      dst = &output_samples_per_second_;
      break;
    default:
      return true;
  }

  if (val <= 0)
    return false;

  if (*dst != -1) {
    LOG(ERROR) << "Multiple values for id " << std::hex << id << " specified ("
               << *dst << " and " << val << ")";
    return false;
  }

  *dst = val;
  return true;
}

}  // namespace media
}  // namespace edash_packager
