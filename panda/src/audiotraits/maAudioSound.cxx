#include "maAudioSound.h"

MaAudioSound::
MaAudioSound(MaAudioManager *manager,
             MaAudioManager::DataSource *data_src,
             Filename &file_name,
             bool positional,
             int mode) :
  AudioSound(positional),
  _data_source(data_src),
  _playing_loops(0),
  _playing_rate(0.0),
  _loops_completed(0),
  _manager(manager),
  _volume(1.0f),
  _balance(0),
  _play_rate(1.0),
  _min_dist(1.0f),
  _max_dist(1000000000.0f),
  _drop_off_factor(1.0f),
  _length(0.0),
  _loop_count(1),
  _loop_start(0),
  _desired_mode(mode),
  _start_time(0.0),
  _current_time(0.0),
  _basename(file_name.get_basename()),
  _active(manager->get_active()),
  _paused(false),
  _cone_inner_angle(360.0f),
  _cone_outer_angle(360.0f),
  _cone_outer_gain(0.0f)
{
  _location = ma_vec3f(0.0f, 0.0f, 0.0f);
  _velocity = ma_vec3f(0.0f, 0.0f, 0.0f);
  _direction = ma_vec3f(0.0f, 0.0f, 0.0f);

  //ReMutexHolder holder(MaAudioManager::_lock);

  if (!require_sound_data()) {
    cleanup();
    return;
  }

  // TODO we removed _sd, so need to get length from source
  _length = _sd->_length;

  if (positional) {
    if (_sd->_channels != 1) {
      audio_warning("stereo sound " << file_name << " will not be spatialized");
    }
  }

  // TODO do we get access from friend class? should we do this in the AudioManager get_sound method?
  check_ma(ma_sound_init_from_data_source(&manager->_audio_engine,
                                          &data_src->data_src, flags,
                                          &manager->_all_sounds_grp),
                                          , "Failed to initialise AudioSound");
  // TODO save comments somewhere now we removed SoundDatas
  release_sound_data(false);
}


/**
 * Copy constructor (to be used with make_copy).
 */
MaAudioSound::
MaAudioSound(const MaAudioSound &copy_sound) :
  AudioSound(copy_sound.is_positional()),
  _data_source(copy_sound._data_source),
  _playing_loops(copy_sound._playing_loops),
  _playing_rate(copy_sound._playing_rate),
  _loops_completed(copy_sound._loops_completed),
  _manager(copy_sound._manager),
  _volume(copy_sound._volume),
  _balance(copy_sound._balance),
  _play_rate(copy_sound._play_rate),
  _min_dist(copy_sound._min_dist),
  _max_dist(copy_sound._max_dist),
  _drop_off_factor(copy_sound._drop_off),
  _length(copy_sound._length),
  _loop_count(copy_sound._loop_count),
  _loop_start(copy_sound._loop_start),
  _desired_mode(copy_sound._desired_mode),
  _start_time(copy_sound._start_time),
  _current_time(0.0),
  _basename(copy_sound._basename),
  _active(copy_sound._active),
  _paused(copy_sound._paused),
  _cone_inner_angle(copy_sound._cone_inner_angle),
  _cone_outer_angle(copy_sound._cone_outer_angle),
  _cone_outer_gain(copy_sound._cone_outer_gain)
{
  _location = copy_sound._location;
  _velocity = copy_sound._velocity;
  _direction = copy_sound._direction;

  //ReMutexHolder holder(MaAudioManager::_lock);

  if (!require_sound_data()) {
    cleanup();
    return;
  }

  _length = _sd->_length;
  if (positional) {
    if (_sd->_channels != 1) {
      audio_warning("copied stereo sound " << _movie->get_filename() << " will not be spatialized");
    }
  }

  // TODO save comments somewhere now we removed SoundDatas
  // cursor has a get_raw_comment method that returns a vector_string
  release_sound_data(false);
}

MaAudioSound::
~MaAudioSound() {
  cleanup();
}

void MaAudioSound::
cleanup() {
  ma_resource_manager_data_source_uninit(&data_src);
}
