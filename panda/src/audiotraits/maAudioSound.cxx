#include "maAudioSound.h"

MaAudioSound::
MaAudioSound(MaAudioManager *manager,
             PT(ma_data_source) data_src,
             bool positional,
             int mode) :
  AudioSound(positional),
  _movie(nullptr), // TODO I think this needs a null MovieAudio object, not a ptr
  _sd(nullptr),
  _playing_loops(0),
  _playing_rate(0.0),
  _loops_completed(0),
  _source(0),
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
  _location[0] = 0.0f;
  _location[1] = 0.0f;
  _location[2] = 0.0f;
  _velocity[0] = 0.0f;
  _velocity[1] = 0.0f;
  _velocity[2] = 0.0f;
  _direction[0] = 0.0f;
  _direction[1] = 0.0f;
  _direction[2] = 0.0f;

  //ReMutexHolder holder(MaAudioManager::_lock);

  if (!require_sound_data()) {
    cleanup();
    return;
  }

  _length = _sd->_length;
  if (positional) {
    if (_sd->_channels != 1) {
      audio_warning("stereo sound " << file_name << " will not be spatialized");
    }
  }

  ma_resource_manager_data_source data_src;
  //int flags = (loop_sound) ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING : 0;
  int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM; // decode in 1s pages
  //int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE; // decode to ram
  //int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC; load to ram later
  check_ma(ma_resource_manager_data_source_init(manager->get_resource_mgr(),
        file_name.get_fullpath(), flags, &data_src));

  // TODO do we get access from friend class? should we do this in the AudioManager get_sound method?
  check_ma(ma_sound_init_from_data_source(&manager->_audio_engine, &data_src, flags,
        &manager->_all_sounds_grp), , "
  _comment = std::move(_sd->_comment);
  release_sound_data(false);
}

MaAudioSound::
MaAudioSound(MaAudioManager *manager,
             MovieAudio movie,
             bool positional,
             int mode) :
  AudioSound(positional),
  _movie(movie),
  _sd(nullptr),
  _playing_loops(0),
  _playing_rate(0.0),
  _loops_completed(0),
  _source(0),
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
  _basename(movie->get_filename().get_basename()),
  _active(manager->get_active()),
  _paused(false),
  _cone_inner_angle(360.0f),
  _cone_outer_angle(360.0f),
  _cone_outer_gain(0.0f)
{
  _location[0] = 0.0f;
  _location[1] = 0.0f;
  _location[2] = 0.0f;
  _velocity[0] = 0.0f;
  _velocity[1] = 0.0f;
  _velocity[2] = 0.0f;
  _direction[0] = 0.0f;
  _direction[1] = 0.0f;
  _direction[2] = 0.0f;

  //ReMutexHolder holder(MaAudioManager::_lock);

  if (!require_sound_data()) {
    cleanup();
    return;
  }

  _length = _sd->_length;
  if (positional) {
    if (_sd->_channels != 1) {
      audio_warning("stereo sound " << file_name << " will not be spatialized");
    }
  }

  ma_resource_manager_data_source data_src;
  //int flags = (loop_sound) ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING : 0;
  int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM; // decode in 1s pages
  //int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE; // decode to ram
  //int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC; load to ram later
  //ma_resource_manager_data_buffer_init_ex_external(
  //check_ma(ma_resource_manager_register_data(manager->get_resource_mgr(),
  // TODO figure this mess out
  //
  // TODO use miniaudio copy method

  _comment = std::move(_sd->_comment);
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
