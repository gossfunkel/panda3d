/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file miniAudioDataSource.cxx
 * @author darktohka
 * @date 2026-08-23
 */

#include "miniAudioDataSource.h"
#include "movieAudio.h"

#include <algorithm>

const ma_data_source_vtable MiniAudioDataSource::_vtable = {
  &MiniAudioDataSource::data_source_read,
  &MiniAudioDataSource::data_source_seek,
  &MiniAudioDataSource::data_source_get_data_format,
  &MiniAudioDataSource::data_source_get_cursor,
  &MiniAudioDataSource::data_source_get_length,
  0, // looping is handled by miniaudio
  0, // looping start position is handled by miniaudio
};

MiniAudioDataSource::
MiniAudioDataSource(MovieAudio *source) :
  _cursor(source != nullptr ? source->open() : nullptr),
  _valid(false),
  _format(ma_format_s16),
  _channels(0),
  _sample_rate(0),
  _length_in_frames(0) {

  if (_cursor == nullptr) {
    audio_error("Failed to open MovieAudio data source.");
    return;
  }

  _channels = _cursor->audio_channels();
  _sample_rate = _cursor->audio_rate();

  if (_channels == 0 || _sample_rate == 0) {
    audio_error("MovieAudio data source has invalid format.");
    return;
  }

  double length = _cursor->length();

  if (length > 0.0 && _sample_rate > 0) {
    _length_in_frames = (ma_uint64)(length * (double)_sample_rate);
  }

  ma_data_source_config config = ma_data_source_config_init();
  config.vtable = &_vtable;

  if (ma_data_source_init(&config, &_base) != MA_SUCCESS) {
    audio_error("Failed to initialise MovieAudio data source.");
    return;
  }

  _valid = true;
}

MiniAudioDataSource::
~MiniAudioDataSource() {
  if (_valid) {
    ma_data_source_uninit(&_base);
    _valid = false;
  }
}

/**
 * Called by miniaudio to read frames of audio. The cursor decodes via the
 * VFS, and returns interleaved 16-bit samples. Miniaudio will handle
 * converting the format and resampling to the engine rate.
 */
ma_result MiniAudioDataSource::
data_source_read(ma_data_source *p_data_source, void *p_frames_out,
                 ma_uint64 frame_count, ma_uint64 *p_frames_read) {
  MiniAudioDataSource *self = from_data_source(p_data_source);

  if (p_frames_read != nullptr) {
    *p_frames_read = 0;
  }

  if (self->_cursor == nullptr) {
    return MA_AT_END;
  }

  if (p_frames_out == nullptr) {
    // A forward seek. Skip the requested number of frames.
    // Prefer seeking the cursor over decoding-and-discarding.
    if (self->_cursor->can_seek()) {
      ma_uint64 current_frame = self->cursor_frame();
      ma_uint64 target_frame = current_frame + frame_count;

      if (target_frame >= self->_length_in_frames) {
        // The skip runs past the end of the audio.
        self->_cursor->seek((double)self->_length_in_frames /
                            (double)self->_sample_rate);
        ma_uint64 skipped = (self->_length_in_frames > current_frame)
                            ? (self->_length_in_frames - current_frame) : 0;

        if (p_frames_read != nullptr) {
          *p_frames_read = skipped;
        }

        return MA_AT_END;
      }

      self->_cursor->seek((double)target_frame / (double)self->_sample_rate);

      if (p_frames_read != nullptr) {
        *p_frames_read = frame_count;
      }

      return MA_SUCCESS;
    }

    // The cursor cannot seek; fall back to decoding and discarding.
    ma_uint64 skipped = self->read_frames(nullptr, frame_count);

    if (p_frames_read != nullptr) {
      *p_frames_read = skipped;
    }

    return (skipped == 0) ? MA_AT_END : MA_SUCCESS;
  }

  // A normal read into the buffer.
  ma_uint64 frames_read = self->read_frames(p_frames_out, frame_count);

  if (p_frames_read != nullptr) {
    *p_frames_read = frames_read;
  }

  return (frames_read == 0) ? MA_AT_END : MA_SUCCESS;
}

/**
 * Reads up to frame_count frames from the cursor.
 *
 * A null buffer discards the samples (used on sources that cannot seek).
 * We can't pass the null buffer straight to cursor->read_samples().
 * That is why we can't use cursor->skip_samples() either.
 * Not all read_samples implementations guard against a null buffer.
 * Instead, we decode into a scratch buffer and throw it away.
 */
ma_uint64 MiniAudioDataSource::
read_frames(void *buffer, ma_uint64 frame_count) {
  int16_t scratch[16384];
  int16_t *output = (int16_t *)buffer;

  if (output == nullptr) {
    output = scratch; // Discard path: decode and throw away.
  }

  ma_uint64 frames_read = 0;

  while (frames_read < frame_count) {
    int chunk = (int)std::min<ma_uint64>(frame_count - frames_read,
                                         16384 / _channels);
    int n = _cursor->read_samples(chunk, output);
    if (n <= 0) {
      break; // Reached the end of the audio.
    }

    output += n * _channels;
    frames_read += (ma_uint64)n;

    if (n < chunk) {
      break; // Reached the end of the audio.
    }
  }

  return frames_read;
}

/**
 * Called by miniaudio to seek to an absolute frame index.
 */
ma_result MiniAudioDataSource::
data_source_seek(ma_data_source *p_data_source, ma_uint64 frame_index) {
  MiniAudioDataSource *self = from_data_source(p_data_source);

  if (self->_cursor == nullptr || !self->_cursor->can_seek()) {
    return MA_NOT_IMPLEMENTED;
  }

  self->_cursor->seek((double)frame_index / (double)self->_sample_rate);
  return MA_SUCCESS;
}

/**
 * Called by miniaudio to retrieve the data format of the source.
 */
ma_result MiniAudioDataSource::
data_source_get_data_format(ma_data_source *p_data_source, ma_format *p_format,
                            ma_uint32 *p_channels, ma_uint32 *p_sample_rate,
                            ma_channel *p_channel_map, size_t channel_map_cap) {
  MiniAudioDataSource *self = from_data_source(p_data_source);

  if (p_format != nullptr) {
    *p_format = self->_format;
  }
  if (p_channels != nullptr) {
    *p_channels = self->_channels;
  }
  if (p_sample_rate != nullptr) {
    *p_sample_rate = self->_sample_rate;
  }
  if (p_channel_map != nullptr && channel_map_cap > 0) {
    p_channel_map[0] = MA_CHANNEL_NONE;
  }

  return MA_SUCCESS;
}

/**
 * Called by miniaudio to retrieve the current read position.
 */
ma_result MiniAudioDataSource::
data_source_get_cursor(ma_data_source *p_data_source, ma_uint64 *p_cursor) {
  MiniAudioDataSource *self = from_data_source(p_data_source);

  if (self->_cursor == nullptr) {
    return MA_AT_END;
  }

  double pos = self->_cursor->tell();

  if (pos < 0.0) {
    pos = 0.0;
  }

  *p_cursor = (ma_uint64)(pos * (double)self->_sample_rate);
  return MA_SUCCESS;
}

/**
 * Called by miniaudio to retrieve the total length of the source.
 */
ma_result MiniAudioDataSource::
data_source_get_length(ma_data_source *p_data_source, ma_uint64 *p_length) {
  MiniAudioDataSource *self = from_data_source(p_data_source);

  if (self->_length_in_frames == 0) {
    // The length is unknown.
    return MA_NOT_IMPLEMENTED;
  }

  *p_length = self->_length_in_frames;
  return MA_SUCCESS;
}
