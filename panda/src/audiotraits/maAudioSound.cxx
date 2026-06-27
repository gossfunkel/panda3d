/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file maAudioSound.h
 * @author Katie <katherineegoss@gmail.com>
 * @date 2026-06-02
 */
#include "maAudioSound.h"

TypeHandle MaAudioSound::_type_handle;

MaAudioSound::
MaAudioSound(MaAudioManager *manager,
             Filename &file_name,
             bool positional,
             int mode) :
  AudioSound(positional),
  _manager(manager),
  _volume(1.0f),
  _balance(0),
  _play_rate(1.0),
  _min_dist(1.0f),
  _max_dist(1000000000.0f),
  _drop_off_factor(1.0f),
  _length(0.0),
  _loop_count(1),
  _loops_completed(0),
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
  //ReMutexHolder holder(_lock);

  std::string src_fn = file_name.get_basename();
  // larger files (e.g. soundtracks/music) should be set to stream mode
  _ma_flags = (mode == StreamMode{SM_stream})
    ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM // decode in 1s pages
    : MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC; // load to ram later
  //_ma_flags |= MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE; // decode to ram

  cache();

  ma_format format;
  ma_uint32 channels, sample_rate;
  ma_sound_get_format(&_ma_sound, &format, &channels, &sample_rate, nullptr, 0);
  if (positional) {
    if (channels != 1)
      audio_warning("Copied stereo sound \"" << _basename
                    << "\" will not be spatialized");
  }
  if (sample_rate != _manager->_device.config.playback.sampleRate)
    audio_error("Source sample rate mismatch with MiniAudio device sample rate");

  length();
}


/**
 * Copy constructor (to be used with make_copy).
 */
MaAudioSound::
MaAudioSound(const MaAudioSound &copy_sound) :
  AudioSound(copy_sound.is_positional()),
  _manager(copy_sound._manager),
  _volume(copy_sound._volume),
  _balance(copy_sound._balance),
  _play_rate(copy_sound._play_rate),
  _min_dist(copy_sound._min_dist),
  _max_dist(copy_sound._max_dist),
  _drop_off_factor(copy_sound._drop_off),
  _length(copy_sound._length),
  _loop_count(copy_sound._loop_count),
  _loops_completed(0),
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
  //ReMutexHolder holder(_lock);
  cache();

  ma_format format;
  ma_uint32 channels, sample_rate;
  ma_sound_get_format(&_ma_sound, &format, &channels, &sample_rate, nullptr, 0);
  if (positional) {
    if (channels != 1)
      audio_warning("Copied stereo sound \"" << _basename
                    << "\" will not be spatialized");
  }
  if (sample_rate != _manager->_device.config.playback.sampleRate)
    audio_error("Source sample rate mismatch with MiniAudio device sample rate");
}

PT(AudioSound) MaAudioSound::make_copy() const {
  PT(AudioSound) copy_sound = new MaAudioSound(*this);

  nassertr(copy_sound->is_valid() == this->is_valid(), nullptr);

  return copy_sound;
}`

/*
 * Loads the sound to MiniAudio, if not already loaded.
 * TODO can we inline these?
 */
void MaAudioSound::
cache() {
  //ReMutexHolder holder(MaAudioManager::_lock);
  //ReMutexHolder holder(_lock);
  if (_ma_sound == nullptr) {
    auto cache_it = _manager->_cache_counts.find(_basename);
    if (cache_it == _manager->_cache_counts.end())
      _manager->_cache_counts.emplace({_basename, 1});
    else cache_it->second++;

    _ma_flags |= (_loop)
      ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING : 0;
    check_ma(
      ma_sound_init_from_file(
        &manager->_engine, _basename, _ma_flags,
        &_manager->_all_sounds_grp,
        NULL, &_ma_sound),
      "Failed to initialise AudioSound");
    set_loop(_loop);
}

/*
 * If the sound is stopped, remove from memory.
 */
bool MaAudioSound::
uncache() {
  //ReMutexHolder holder(MaAudioManager::_lock);
  //ReMutexHolder holder(_lock);
  if (ma_sound_is_playing(&_ma_sound)) return false;
  set_active(false);
  _ma_flags |= (!MA_SOUND_FLAG_ASYNC) | MA_SOUND_FLAG_DECODE;
  if (_ma_sound == nullptr) return true;
  auto cache_it = _manager->_cache_counts.find(_basename);
  if (cache_it != _manager->_cache_counts.end()) {
    if (--cache_it->second <= 0)
      _manager->_cache_counts.erase(cache_it);
  }
  return (ma_sound_uninit(&_ma_sound) == MA_SUCCESS);
}

void MaAudioSound::
play() {
  //ReMutexHolder holder(MaAudioManager::_lock);
  //ReMutexHolder holder(_lock);
  _paused = false;
  if (is_active()) return;
  set_active(true);
  if (_manager->_num_concurrent_sounds >=
      _manager.get_concurrent_sound_limit()) {
    audio_error("Maximum concurrently-playing sounds reached; cannot play sound");
    return;
  }
  _manager->_num_concurrent_sounds++;
  if (_loop_count != 1) _loop = true;
  cache();

  _manager->_active_sounds.emplace_back(&this);
  ma_sound_start(&_ma_sound);
}

void MaAudioSound::
stop() {
  //ReMutexHolder holder(MaAudioManager::_lock);
  //ReMutexHolder holder(_lock);
  if (!is_valid()) return;
  _paused = false;
  if (!is_active()) return;
  set_active(false);
  _manager->_num_concurrent_sounds--;

  set_loop(false);
}

/*
 * Used by callback for MiniAudio to allow our loop controls.
 * Increments loops_completed counter and checks if it has reached
 * the _loop_count. If so, it stops and finishes the sound. Returns
 * true if the loop_count has been reached, and false otherwise.
 */
bool MaAudioSound::loop_completed() {
  //ReMutexHolder holder(_lock);
  if (++_loops_completed >= _loop_count) {
    _loops_completed = 0;
    finished();
    return true;
  }
  return false;
}

/*
 * Sets looping on or off for a sound using MiniAudio's looping for
 * infinite loops, or an anonymous callback function to count loops.
 * The callback uses the loop_completed method to update and check
 * the local variables.
 */
void MaAudioSound::
set_loop(bool loop) {
  //ReMutexHolder holder(_lock);
  if (loop) { // enable looping
    // if loop count isn't 0, we manually loop
    if (_loop_count && !ma_sound_is_looping(&_ma_sound)) {
      ma_sound_set_start_time_in_milliseconds(
          &_ma_sound, (ma_uint64)(_loop_start/1000.));
      // here we create an anonymous function to restart the sound
      //  from the _loop_start every time it ends until _loop_count
      //  loops have been executed. loop_completed() cleans up at end
      _end_cb = [&](void *data, ma_sound *ma_sound_ptr) noexcept {
        if (!loop_completed()) {
          ma_sound_set_start_time_in_milliseconds(
            ma_sound_ptr,
            (ma_uint64)(_start_time()/1000.));
          ma_sound_start(ma_sound_ptr);
        }
      };
    } else { // otherwise, we let miniaudio loop it forever
      ma_sound_set_looping(&_ma_sound, true);
    }
  } else { // disable looping
    ma_sound_set_looping(&_ma_sound, false);
    _end_cb = [&](void *data, ma_sound *sound_ptr) noexcept {
      finished();
    };
    _ma_flags |= 0;
  }
  ma_sound_set_end_callback(&_ma_sound, _end_cb, nullptr);
  _loop = loop;
}

bool MaAudioSound::get_loop() const {
  return _loop;
}

void MaAudioSound::
set_loop_count(unsigned long loop_count) {
  //ReMutexHolder holder(_lock);
  _loop_count = loop_count;
  set_loop((loop_count == 1) ? false : true);
}

PN_stdfloat MaAudioSound::get_loop_count() const {
  return _loop_count;
}

void MaAudioSound::set_loop_start(PN_stdfloat loop_start) {
  //ReMutexHolder holder(_lock);
  _loop_start = loop_start;
  if (get_loop() && get_active())
    ma_sound_set_start_time_in_milliseconds(
        &_ma_sound, (ma_uint64)(loop_start/1000.));
}

PN_stdfloat MaAudioSound::get_loop_start() {
  return _loop_start;
}

void MaAudioSound::set_time(PN_stdfloat time) {
  //ReMutexHolder holder(_lock);
  _time = time;
  ma_sound_seek_to_second(&_ma_sound, time);
}

PN_stdfloat MaAudioSound::get_time() const {
  //ReMutexHolder holder(_lock);
  _time = ma_sound_get_time_in_seconds(&_ma_sound);
  return _time;
}

void MaAudioSound::set_volume(PN_stdfloat volume) {
  //ReMutexHolder holder(_lock);
  _volume = volume;
  ma_sound_set_volume(&_ma_sound, volume);
}

PN_stdfloat MaAudioSound::get_volume() const {
  //ReMutexHolder holder(_lock);
  _volume = ma_sound_get_volume(&_ma_sound);
  return _volume;
}

void MaAudioSound::set_balance(PN_stdfloat balance_right) {
  //ReMutexHolder holder(_lock);
  _balance = balance_right;
  ma_sound_set_pan(&_ma_sound, balance_right);
}

PN_stdfloat MaAudioSound::get_balance() const {
  //ReMutexHolder holder(_lock);
  _balance = ma_sound_get_pan(&_ma_sound);
  return _balance;
}

void MaAudioSound::set_play_rate(PN_stdfloat play_rate) {
  //ReMutexHolder holder(_lock);
  _play_rate = play_rate;
  ma_sound_set_pitch(&_ma_sound, play_rate);
}

PN_stdfloat MaAudioSound::get_play_rate() const {
  //ReMutexHolder holder(_lock);
  _play_rate = ma_sound_get_pitch(&_ma_sound);
  return _play_rate;
}

void MaAudioSound::set_active(bool active) {
  //ReMutexHolder holder(_lock);
  if (!active && _active)
    if (ma_sound_is_playing(&_ma_sound)) stop();
}

bool MaAudioSound::get_active() const {
  //ReMutexHolder holder(_lock);
  if (!ma_sound_is_playing(&_ma_sound))
    _active = false;
  return _active;
}

void MaAudioSound::
set_finished_event(std::string event) {
  //ReMutexHolder holder(_lock);
  _finished_event = std::move(event);
}

const std::string &MaAudioSound::
get_finished_event() const {
  return _finished_event;
}

PN_stdfloat MaAudioSound::length() const {
  //ReMutexHolder holder(_lock);
  ma_sound_get_length_in_seconds(&_ma_sound, &_length);
  return _length;
}

const std::string &get_name() const {
  return _basename;
}

void MaAudioSound::set_3d_attributes(
      PN_stdfloat px, PN_stdfloat py, PN_stdfloat pz,
      PN_stdfloat vx, PN_stdfloat vy, PN_stdfloat vz) {
  //ReMutexHolder holder(_lock);
  _position = (LVector3){px, py, pz};
  _velocity = (LVector3){vx, vy, vz};
  ma_sound_set_position(&_ma_sound, px, py, pz);
  ma_sound_set_velocity(&_ma_sound, vx, vy, vz);
}

void MaAudioSound::get_3d_attributes(
      PN_stdfloat *px, PN_stdfloat *py, PN_stdfloat *pz,
      PN_stdfloat *vx, PN_stdfloat *vy, PN_stdfloat *vz) {
  //ReMutexHolder holder(_lock);
  _position = (LVector3)ma_sound_get_position(&_ma_sound);
  *px = _position.x;
  *py = _position.y;
  *pz = _position.z;
  _velocity = (LVector3)ma_sound_get_velocity(&_ma_sound);
  *vx = _velocity.x;
  *vy = _velocity.y;
  *vz = _velocity.z;
}

void MaAudioSound::set_3d_direction(LVector3 d) {
  //ReMutexHolder holder(_lock);
  _direction = d;
  ma_sound_set_direction(&_ma_sound, d.x, d.y, d.z);
}

LVector3 MaAudioSound::get_3d_direction() const {
  //ReMutexHolder holder(_lock);
  _direction = (LVector3)ma_sound_get_direction(&_ma_sound);
  return _direction;
}

void MaAudioSound::set_3d_min_distance(PN_stdfloat dist) {
  //ReMutexHolder holder(_lock);
  _min_dist = dist;
  ma_sound_set_min_distance(&_ma_sound, dist);
}

PN_stdfloat MaAudioSound::get_3d_min_distance() const {
  //ReMutexHolder holder(_lock);
  _min_dist = ma_sound_get_min_distance(&_ma_sound);
  return _min_dist;
}

void MaAudioSound::set_3d_max_distance(PN_stdfloat dist) {
  //ReMutexHolder holder(_lock);
  _max_dist = dist;
  ma_sound_set_max_distance(&_ma_sound, dist);
}

PN_stdfloat MaAudioSound::get_3d_max_distance() const {
  //ReMutexHolder holder(_lock);
  _max_dist = ma_sound_get_max_distance(&_ma_sound);
  return _max_dist;
}

void MaAudioSound::set_3d_drop_off_factor(PN_stdfloat factor) {
  //ReMutexHolder holder(_lock);
  _drop_off_factor = factor;
  ma_sound_set_rolloff(&_ma_sound, factor);
}

PN_stdfloat MaAudioSound::get_3d_drop_off_factor() const {
  //ReMutexHolder holder(_lock);
  _drop_off_factor = ma_sound_get_rolloff(&_ma_sound);
  return _drop_off_factor;
}

/*
 * radians
 */
void MaAudioSound::set_3d_cone_inner_angle(PN_stdfloat angle) {
  //ReMutexHolder holder(_lock);
  _cone_inner_angle = angle;
  ma_sound_set_cone(
    &_ma_sound, angle, _cone_outer_angle, _cone_outer_gain);
}

PN_stdfloat MaAudioSound::get_3d_cone_inner_angle() const {
  //ReMutexHolder holder(_lock);
  ma_sound_get_cone(
    &_ma_sound,
    &_cone_inner_angle,
    &_cone_outer_angle,
    &_cone_outer_gain);
  return _cone_inner_angle;
}

/*
 * radians
 */
void MaAudioSound::set_3d_cone_outer_angle(PN_stdfloat angle) {
  //ReMutexHolder holder(_lock);
  _cone_outer_angle = angle;
  ma_sound_set_cone(
    &_ma_sound, _cone_inner_angle, angle, _cone_outer_gain);
}

PN_stdfloat MaAudioSound::get_3d_cone_outer_angle() const {
  //ReMutexHolder holder(_lock);
  ma_sound_get_cone(
    &_ma_sound,
    &_cone_inner_angle,
    &_cone_outer_angle,
    &_cone_outer_gain);
  return _cone_outer_angle;
}

/*
 * radians
 */
void MaAudioSound::set_3d_cone_outer_gain(PN_stdfloat gain) {
  //ReMutexHolder holder(_lock);
  _cone_outer_gain = gain;
  ma_sound_set_cone(
    &_ma_sound, _cone_inner_angle, _cone_outer_angle, gain);
}

PN_stdfloat MaAudioSound::get_3d_cone_outer_gain() const {
  //ReMutexHolder holder(_lock);
  ma_sound_get_cone(
    &_ma_sound,
    &_cone_inner_angle,
    &_cone_outer_angle,
    &_cone_outer_gain);
  return _cone_outer_gain;
}

void MaAudioSound::finished() {
  //ReMutexHolder holder(AudioManager::_lock);
  //ReMutexHolder holder(_lock);
  if (!is_valid()) return;

  stop();
  _time = _length;
  if (!_finished_event.empty()) throw_event(_finished_event);
}

MaAudioSound::
~MaAudioSound() {
  cleanup();
}

AudioSound::SoundStatus MaAudioSound::
status() const {
  //ReMutexHolder holder(_lock);
  if (!is_valid()) return AudioSound::BAD;
  if (_ma_sound == nullptr) return AudioSound::BAD;
  if (ma_sound_is_playing(&_ma_sound)) return AudioSound::PLAYING;
  return AudioSound::READY;
}

/**
 * Returns the comments attached to this audio file.
 */
const vector_string& OpenALAudioSound::
get_raw_comment() const {
  return _comment;
}

void MaAudioSound::
cleanup() {
  //ReMutexHolder holder(AudioManager::_lock);
  //ReMutexHolder holder(_lock);
  stop();
  _manager->_all_sounds.erase(_manager_it);
  ma_sound_uninit(&_ma_sound);
}
