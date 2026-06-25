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
  //_ma_flags |= (loop_sound)
  //  ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING : 0;
  //_ma_flags |= MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE; // decode to ram

  if (positional) {
    // FIXME get sound channels properly
    if (_ma_sound->_channels != 1) {
      audio_warning("stereo sound " << file_name << " will not be spatialized");
    }
  }

  cache();

  ma_sound_get_length_in_seconds(&_ma_sound, &_length);
}


/**
 * Copy constructor (to be used with make_copy).
 */
MaAudioSound::
MaAudioSound(const MaAudioSound &copy_sound) :
  AudioSound(copy_sound.is_positional()),
  _playing_loops(copy_sound._playing_loops),
  _playing_rate(copy_sound._playing_rate),
  _loops_completed(0),
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
  _time(0.),
  _basename(copy_sound._basename),
  _active(copy_sound._active),
  _paused(copy_sound._paused),
  _cone_inner_angle(copy_sound._cone_inner_angle),
  _cone_outer_angle(copy_sound._cone_outer_angle),
  _cone_outer_gain(copy_sound._cone_outer_gain),
  _location(copy_sound._location),
  _velocity(copy_sound._velocity),
  _direction(copy_sound._direction),
  _ma_flags(copy_sound._ma_flags) {

  //ReMutexHolder holder(MaAudioManager::_lock);

  if (positional) {
    if (_ma_sound->_channels != 1) {
      audio_warning("copied stereo sound " << _basename << " will not be spatialized");
    }
  }

  cache();

  set_loop(copy_sound.get_loop());
}

PT(AudioSound) MaAudioSound::make_copy() const {
  PT(AudioSound) copy_sound = new MaAudioSound(*this);

  nassertr(copy_sound->is_valid() == this->is_valid(), nullptr);

  return copy_sound;
}

/*
 * Loads the sound to MiniAudio, if not already loaded.
 * TODO can we inline these?
 */
void MaAudioSound::
cache() {
  if (_ma_sound == nullptr) {
    auto cache_it = _manager->_cache_counts.find(get_name());
    if (cache_it == _manager->_cache_counts.end())
      _manager->_cache_counts.emplace({get_name(), 1});
    else cache_it->second++;

    std::string src_filename = get_name().get_basename();
    check_ma(
      ma_sound_init_from_file(
        &manager->_engine, src_filename, _ma_flags,
        &_manager->_all_sounds_grp,
        NULL, &_ma_sound),
      "Failed to initialise AudioSound");
    if (loop_count > 1)
      ma_sound_set_end_callback(
        &_ma_sound,
        [&](void *sound, ma_sound *sound_ptr){
          if (++sound->_loops_completed < sound->_loop_count)
            ma_sound_start(sound_ptr);
          else
            sound->stop();
        },
        (void *)&this;
      );
    else
      ma_sound_set_end_callback(&_ma_sound, loop_cb, nullptr);
  }
}

/*
 * If the sound is stopped, remove from memory.
 */
bool MaAudioSound::
uncache() {
  if (ma_sound_is_playing(&_ma_sound)) return false;
  set_active(false);
  if (_ma_sound == nullptr) return true;
  auto cache_it = _manager->_cache_counts.find(get_name());
  if (cache_it != _manager->_cache_counts.end()) {
    if (--cache_it->second <= 0)
      _manager->_cache_counts.erase(cache_it);
  }
  return (ma_sound_uninit(&_ma_sound) == MA_SUCCESS);
}

void loop_cb(void *ma_audio_sound, ma_sound *sound_ptr) {
  ma_audio_sound->stop();
}

void MaAudioSound::
play() {
  _paused = false;
  if (is_active()) return;
  set_active(true);
  if (_manager->_num_concurrent_sounds >=
      _manager.get_concurrent_sound_limit()) {
    audio_error("Maximum concurrently-playing sounds reached; cannot play sound");
    return;
  }
  ++_manager->_num_concurrent_sounds;
  cache();

  _manager->_active_sounds.emplace_back(&this);
  ma_sound_start(&_ma_sound);
}

void MaAudioSound::
stop() {
  _paused = false;
  if (!is_active()) return;
  set_active(false);
  --_manager->_num_concurrent_sounds;

  if (ma_sound_is_looping(&_ma_sound))
    ma_sound_set_looping(&_ma_sound, false);
  if (ma_sound_is_playing(&_ma_sound))
    ma_sound_stop(&_ma_sound);
}

/*
 * Sets looping on or off for a sound.
 */
void MaAudioSound::
set_loop(bool loop) {
  if (loop && !_loop) {
    // if loop count isn't 0, we manually loop
    if (!_loop && _loop_count && !ma_sound_is_looping(&_ma_sound))
      ma_sound_set_end_callback(
          &_ma_sound,
          [&](void *sound, ma_sound *sound_ptr){
            if (++sound->_loops_completed < sound->_loop_count) {
              // TODO play from _loop_start
              ma_sound_start(sound_ptr);
            } else {
              // FIXME sound is not supposed to call stop() when
              //  ending of its own volition (see finished_event)
              sound->stop();
            }
          },
          (void *)&this;
        );
    else // otherwise, we let miniaudio loop it forever
      ma_sound_set_looping(&_ma_sound, true);
  } else if (!loop && _loop) {
    if (_loop && _loop_count && !ma_sound_is_looping(&_ma_sound))
      ma_sound_set_end_callback(&_ma_sound, loop_cb, nullptr);
    else if (ma_sound_is_looping(&_ma_sound))
      ma_sound_set_looping(&_ma_sound, false);
  }
  _loop = loop;
}

bool MaAudioSound::get_loop() const {
  return _loop;
}

void MaAudioSound::
set_loop_count(unsigned long loop_count) {
  _loop_count = loop_count;
  set_loop((loop_count == 1) ? false : true);
}

PN_stdfloat MaAudioSound::get_loop_count() const {
  return _loop_count;
}

void MaAudioSound::set_loop_start(PN_stdfloat loop_start) {
  // TODO loop start
}

PN_stdfloat MaAudioSound::get_loop_start() {
  return _loop_start;
}

void MaAudioSound::set_time(PN_stdfloat time) {
  _time = time;
  ma_sound_seek_to_second(&_ma_sound, time);
}

PN_stdfloat MaAudioSound::get_time() const {
  ma_sound_get_time_in_seconds(&_ma_sound);
}

void MaAudioSound::set_volume(PN_stdfloat volume) {
  _volume = volume;
  ma_sound_set_volume(&_ma_sound, volume);
}

PN_stdfloat MaAudioSound::get_volume() const {
  _volume = ma_sound_get_volume(&_ma_sound);
  return _volume;
}

void MaAudioSound::set_balance(PN_stdfloat balance_right) {
  _balance = balance_right;
  ma_sound_set_pan(&_ma_sound, balance_right);
}

PN_stdfloat MaAudioSound::get_balance() const {
  _balance = ma_sound_get_pan(&_ma_sound);
  return _balance;
}

void MaAudioSound::set_play_rate(PN_stdfloat play_rate) {
  _play_rate = play_rate;
  ma_sound_set_pitch(&_ma_sound, play_rate);
}

PN_stdfloat MaAudioSound::get_play_rate() const {
  _play_rate = ma_sound_get_pitch(&_ma_sound);
  return _play_rate;
}

void MaAudioSound::set_active(bool active) {
  _active = active;
}

bool MaAudioSound::get_active() const {
  if (!ma_sound_is_playing(&_ma_sound))
    _active = false;
  return _active;
}

// TODO get/set_finished_event()

// TODO length()

// TODO get_name()

// TODO get/set_3d_attribs
//      get/set_3d_dir
//      get/set_3d_min_dist
//      get/set_3d_max_dist
//      get/set_3d_drop_off
//      get/set_cone_inner_ang
//      get/set_cone_outer_ang
//      get/set_cone_gain (outer)

// TODO wallahi i am finished()

MaAudioSound::
~MaAudioSound() {
  cleanup();
}

AudioSound::SoundStatus MaAudioSound::
status() const {
  if (!is_valid()) return AudioSound::BAD;
  if (_ma_sound == nullptr) return AudioSound::BAD;
  if (ma_sound_is_playing(&_ma_sound)) return AudioSound::PLAYING;
  return AudioSound::READY;
}

void MaAudioSound::
cleanup() {
  stop();
  _manager->_all_sounds.erase(_manager_it);
  ma_sound_uninit(&_ma_sound);
}
