#include "maAudioSound.h"

MaAudioSound::
MaAudioSound(MaAudioManager *manager,
             Filename &file_name,
             bool positional,
             int mode) :
  AudioSound(positional),
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

  // protect against user accessing engine from multiple threads
  //ReMutexHolder holder(MaAudioManager::_lock);

  if (!require_sound_data()) {
    cleanup();
    return;
  }

  std::string src_fn = file_name.get_basename();
  // larger files (e.g. soundtracks/music) should be set to stream mode
  _ma_flags = (mode == StreamMode{SM_stream})
    ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM // decode in 1s pages
    : MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC; // load to ram later
  _ma_flags |= (loop_sound)
    ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING : 0;
  //_ma_flags |= MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE; // decode to ram
  check_ma(ma_sound_init_from_file(
      &manager->_engine, src_fn, _ma_flags, &manager->_all_sounds_grp,
      NULL, _ma_sound
  ), "Failed to initialise AudioSound");

  // we removed _sd, so need to get length from source (FIXME?)
  _length = _ma_sound->rangeEndInPCMFrames - _ma_sound->rangeBegInPCMFrames;

  if (positional) {
    // FIXME get sound channels properly
    if (_ma_sound->_channels != 1) {
      audio_warning("stereo sound " << file_name << " will not be spatialized");
    }
  }
}


/**
 * Copy constructor (to be used with make_copy).
 */
MaAudioSound::
MaAudioSound(const MaAudioSound &copy_sound) :
  AudioSound(copy_sound.is_positional()),
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
  _cone_outer_gain(copy_sound._cone_outer_gain),
  _location(copy_sound._location);
  _velocity(copy_sound._velocity);
  _direction(copy_sound._direction);
{

  //ReMutexHolder holder(MaAudioManager::_lock);

  if (positional) {
    if (_ma_sound->_channels != 1) {
      audio_warning("copied stereo sound " << _basename << " will not be spatialized");
    }
  }

  std::string src_fn = file_name.get_basename();
  _ma_flags = (mode == StreamMode{SM_stream})
    ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM // decode in 1s pages
    : MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC; // load to ram later
  _ma_flags |= (loop_sound)
    ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING : 0;
  //_ma_flags |= MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE; // decode to ram
  check_ma(ma_sound_init_from_file(
      &_manager->_engine, src_fn, _ma_flags, &_manager->_all_sounds_grp,
      NULL, _ma_sound
  ), "Failed to initialise copied AudioSound");
}

PT(AudioSound) MaAudioSound::
make_copy() const {
  PT(AudioSound) copy_sound = new MaAudioSound(*this);

  // throw errors if the copied-to node doesn't match the copied-from
  nassertr(copy_sound->is_valid() == this->is_valid(), nullptr);
  nassertr(copy_sound->has_sound_data() == this->has_sound_data(), nullptr);

  return copy_sound;
}

void loop_cb(void *loop_ctr, ma_sound *sound_ptr) {
}

void MaAudioSound::
play() {
  _paused = false;
  if (is_active()) return;
  set_active(true);

  if (get_loop()) {
    _loopctr.loops = 0;
    _loopctr.loop_count = get_loop_count();
    // if loop count isn't 0, we manually loop
    if (loopctr.loop_count)
      ma_sound_set_end_callback(
          &_ma_sound,
          [&](void *loop_ctr, ma_sound *sound_ptr){
            if (++loop_ctr->loops < loop_ctr->loop_count)
              ma_sound_start(sound_ptr);
          },
          (void *)&_loopctr;
        );
    else // otherwise, we let miniaudio loop it forever
      ma_sound_set_looping(&_ma_sound, true);
  }

  if (_manager->_num_concurrent_sounds <
      _manager->_concurrent_sound_limit) {
    ++_manager->_num_concurrent_sounds;
    _manager->_active_sounds.emplace_back(&this);
    ma_sound_start(&_ma_sound);
  } else
    audio_error("Maximum concurrently playing sounds reached, cannot play sound");
}

void MaAudioSound::
stop() {
  _paused = false;
  if (!is_active()) return;
  set_active(false);
  if (ma_sound_is_looping(&_ma_sound))
    ma_sound_set_looping(&_ma_sound, false);
  if (ma_sound_is_playing(&_ma_sound))
    ma_sound_stop(&_ma_sound);

  auto as_it = _manager->_active_sounds.begin();
  while (&(*as_it) != &this) {
    if (as_it == _manager->_active_sounds.end()) {
      audio_error("Stopped sound not found in active sounds array");
      return;
    }
    as_it = as_it.next();
  }
}

MaAudioSound::
~MaAudioSound() {
  cleanup();
}

void MaAudioSound::
cleanup() {
  // TODO ensure sound is stopped?
  ma_sound_uninit(&_ma_sound);
}
