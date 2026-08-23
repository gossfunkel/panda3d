/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file miniAudioDataSource.I
 * @author darktohka
 * @date 2026-08-23
 */

INLINE MiniAudioDataSource *MiniAudioDataSource::
from_data_source(ma_data_source *p_data_source) {
  return reinterpret_cast<MiniAudioDataSource *>(p_data_source);
}

INLINE bool MiniAudioDataSource::is_valid() const {
  return _valid;
}

INLINE ma_data_source *MiniAudioDataSource::get_data_source() {
  return &_base;
}

INLINE ma_uint64 MiniAudioDataSource::cursor_frame() const {
  if (_cursor == nullptr) {
    return 0;
  }

  double seconds = _cursor->tell();

  if (seconds <= 0.0) {
    return 0;
  }

  ma_uint64 frame = (ma_uint64)(seconds * (double)_sample_rate);

  if (frame > _length_in_frames) {
    frame = _length_in_frames;
  }

  return frame;
}
