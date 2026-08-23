/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file miniAudioDataSource.h
 * @author darktohka
 * @date 2026-08-23
 */

#ifndef MINIAUDIODATASOURCE_H
#define MINIAUDIODATASOURCE_H

#include "pandabase.h"

#include "config_audio.h"
#include "movieAudioCursor.h"
#include "pointerTo.h"
#include "miniaudio.h"

class MovieAudio;

/**
 * A miniaudio ma_data_source that reads its audio through a MovieAudioCursor.
 *
 * This lets the MiniAudio backend decode sound files using Panda3D's own
 * MovieAudio decoders, which read through the VirtualFileSystem.  That means
 * sounds stored inside multifiles work as expected, and we support the same
 * audio formats as the rest of Panda3D (wav, flac, ogg, vorbis, opus, ffmpeg, etc.)
 * instead of only the formats miniaudio can decode natively (wav, flac, mp3).
 */
class EXPCL_MINI_AUDIO MiniAudioDataSource {
public:
  MiniAudioDataSource(MovieAudio *source);
  ~MiniAudioDataSource();

  INLINE bool is_valid() const;

  INLINE ma_data_source *get_data_source();

private:
  static MiniAudioDataSource *from_data_source(ma_data_source *p_data_source);

  static ma_result data_source_read(ma_data_source *p_data_source,
                                    void *p_frames_out, ma_uint64 frame_count,
                                    ma_uint64 *p_frames_read);
  static ma_result data_source_seek(ma_data_source *p_data_source,
                                    ma_uint64 frame_index);
  static ma_result data_source_get_data_format(ma_data_source *p_data_source,
                                               ma_format *p_format,
                                               ma_uint32 *p_channels,
                                               ma_uint32 *p_sample_rate,
                                               ma_channel *p_channel_map,
                                               size_t channel_map_cap);
  static ma_result data_source_get_cursor(ma_data_source *p_data_source,
                                          ma_uint64 *p_cursor);
  static ma_result data_source_get_length(ma_data_source *p_data_source,
                                          ma_uint64 *p_length);

  static const ma_data_source_vtable _vtable;

  // Reads up to frame_count frames from the cursor into buffer.
  ma_uint64 read_frames(void *buffer, ma_uint64 frame_count);

  // Returns the cursor's current position in frames at the source sample rate.
  INLINE ma_uint64 cursor_frame() const;

  ma_data_source_base _base;
  PT(MovieAudioCursor) _cursor;
  bool _valid;

  ma_format _format;
  ma_uint32 _channels;
  ma_uint32 _sample_rate;
  ma_uint64 _length_in_frames;
};

#include "miniAudioDataSource.I"

#endif /* MINIAUDIODATASOURCE_H */
