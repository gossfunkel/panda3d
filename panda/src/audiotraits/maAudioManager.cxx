/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file config_maAudio.h
 * @author Katie & J0y
 */

#include "maAudioManager.h"

using std::string;

int MaAudioManager::_active_managers = 0;
bool MaAudioManager::_ma_active = false;

// TODO replace this with an inline function, takes callback fn ptr, returns status
#define check_ma(result, failcond, outstr) if ((result) != MA_SUCCESS) {  \
  (failcond); audio_error(outstr); return nullptr; }

/**
 * Factory Function
 */
AudioManager *Create_MaAudioManager() {
  audio_debug("Create_MaAudioManager()");
  return new MaAudioManager;
}

MaAudioManager::
MaAudioManager() {
  ReMutexHolder holder(_lock);

  audio_cat.init();

  if (_managers == nullptr) {
    _managers = new Managers;

    ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.samplerate = 48000;
    //config.dataCallback
    //config.pUserData

    check_ma(ma_device_init(NULL, &device_config, &_device), ,
             "Failed to initialise MiniAudio device.");

    ma_device_start(&_device);

    audio_cat.info() << "Using MiniAudio device " << device.playback.name <<
      "." << std::endl;

    resource_mgr_conf = ma_resource_manager_config_init();
    resource_mgr_conf.decodedFormat     = _device.playback.format;
    resource_mgr_conf.decodedChannels   = _device.playback.channels;
    resource_mgr_conf.decodedSampleRate = _device.sampleRate;
#ifdef HAVE_THREADS
    resource_mgr_conf.jobThreadCount = 2;
#endif

    // this will probably be removed, but just in case, here's where we
    //  can assign a custom VFS to the resource mgr
    //resource_mgr_conf.pVFS = VirtualFileSystem::get_global_pointer();
    check_ma(
      ma_resource_manager_init(&resource_mgr_conf, &_resource_mgr),
      ma_device_uninit(&_device),
      "Failed to initialise MiniAudio resource manager."
    );
  }

  _managers->insert(this);

  ma_engine_config audio_engine_conf;
  audio_engine_conf = ma_engine_config_init();
  audio_engine_conf.pResourceManager = &_resource_mgr;
  audio_engine_conf.noAutoStart = MA_TRUE;
  check_ma(
    ma_engine_init(&audio_engine_conf, &_audio_engine),
    ma_device_uninit(&_device),
    "Failed to initialise MiniAudio engine."
  );

  audio_3d_set_listener_attributes(
      0, 0, 0,
      0, 0, 0,
      1, 0, 0,
      0, 0, 1);
  // TODO check set, if not, set _is_valid = false;

  int sg_flags = 0;
  ma_sound_group_init(&_audio_engine, sg_flags, nullptr, &_all_sounds_grp);

  // we'll do this when p3d is ready for it, or remove the noAutoStart line
  ma_engine_start(&_audio_engine);

  if (audio_cat.is_debug()) {
    audio_cat.debug() << "MA ... " << var << std::endl;
  }
}

/**
 * This is what creates a sound instance.
 */
PT(AudioSound) MaAudioManager::
get_sound(const Filename &file_name, bool positional, int mode) {
  // TODO check cache for source
  // TODO check cache size; if limit is hit, pop one from _expiring_sources
  return new MaAudioSound(this, file_name, positional, mode);
}

PT(AudioSound) MaAudioManager::
get_sound(MovieAudio *source, bool positional, int mode) {
  // TODO check cache for source
  // TODO check cache size; if limit is hit, pop one from _expiring_sources
  return new MaAudioSound(this, source, positional, mode);
}

/*
 * Deletes a cached source from the expiration cache, if not in use
 */
void MaAudioManager::uncache_sound(const Filename &file_name) {
  ReMutexHolder holder(_lock);
  Filename path = file_name;

  // TODO use the miniaudio vfs?
  VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
  vfs->resolve_filename(path, get_model_path());

  auto data_src = _sample_cache.find(path);
  if (data_src == _sample_cache.end())
      data_src  = _sample_cache.find(file_name);

  // TODO check if data_src is in use by an active sound
  if (src_unused) {
    ma_data_source_uninit(data_src);
    _expiring_sources.pop_front();
    _source_cache.erase(data_src);
    delete data_src;
  } else {
    // TODO should this just stop the sound then remove it?
    audio_error("Sound is active, cannot be uncached.");
  }
}

/*
 * Empty the cache of data sources not in use by sounds
 */
void MaAudioManager::clear_cache() {
  ReMutexHolder holder(_lock);
  if (!_source_cache.empty()) discard_excess_cache(0);
}

void MaAudioManager::set_cache_limit(unsigned int count) {
  ReMutexHolder holder(_lock);
  if (((int)_expiring_sources.size() > count) {
    discard_excess_cache(count);
  }
  _cache_limit = count;
}

unsigned int MaAudioManager::get_cache_limit() const {
  return _cache_limit;
}

void MaAudioManager::discard_excess_cache() {
  ReMutexHolder holder(_lock);
  for (PT(ma_data_source) data_src = _expiring_sources.begin();
      data_src != _expiring_sources.end() &&
      (int)_expiring_sources.size() > sample_limit;
      ++data_src) {
    // TODO can we just use uncache_sound once it's fixed/finished?
    ma_data_source_uninit(*data_src);
    _expiring_sources.pop_front();
    _source_cache.erase(data_src);
    // TODO get name from hashmap before removal to print
    audio_debug("Expiring data source.");
    delete data_src;
  }
}

/*
 * Sets global volume (gain) setting on our MiniAudio engine
 */
void MaAudioManager::set_volume(PN_stdfloat volume) {
  ReMutexHolder holder(_lock);
  if (_volume != volume) {
    _volume = volume;
    check_ma(ma_engine_set_volume(&_audio_engine, volume), ,
        "Failed to set MiniAudio engine global volume.");
  }
}

/*
 * Gets the global volume (gain) setting on our MiniAudio engine
 */
PN_stdfloat MaAudioManager::get_volume() const {
  _volume = ma_engine_get_volume(&_audio_engine);
  return _volume;
}

/*
 * Gets a pointer to the MiniAudio resource manager we use
 */
PT(ma_resource_manager) MaAudioManager::get_resource_manager() {
  return PT(_resource_mgr);
}

/**
 * Turn on/off via active flag. Warning: not implemented.
 */
void MaAudioManager::set_active(bool flag) {
  ReMutexHolder holder(_lock);
  if (_active!=flag) {
    _active=flag;
    for (auto i : _all_sounds) {
      (**i).set_active(_active);
    }
  }
}

bool MaAudioManager::get_active() const {
  return _active;
}

void MaAudioManager::
set_concurent_sound_limit(unsigned int) {
  ReMutexHolder holder(_lock);
  _concurrent_sound_limit = limit;
  reduce_sounds_playing_to(_concurrent_sound_limit);
}

unsigned int MaAudioManager::get_concurrent_sound_limit() const {
  return _concurrent_sound_limit;
}

void MaAudioManager::reduce_sounds_playing_to(unsigned int count) {
  ReMutexHolder holder(_lock);
  // give all sounds that have finished playing a chance to stop first
  update();

  // TODO use _all_sounds_grp?
  int limit = _sounds_playing.size() - count;
  while (limit-- > 0) {
    SoundsPlaying::iterator sound = _sounds_playing.begin();
    nassertv(sound != _sounds_playing.end());
    // Stop should be called while holding a PT the sound- we have to make a
    // temporary one here so the refcount doesn't go to 0 and recurse, with
    // the destructor calling stop() and stop() calling the destructor.
    // TODO shouldn't the destructor just check if it's stopped already?
    PT(MaAudioSound) s = (*sound);
    s->stop();
  }
}

void MaAudioManager::stop_all_sounds() {
  reduce_sounds_playing_to(0);
}

/*
 * Must be called every frame. Do housework on buffers and playing sounds
 */
void MaAudioManager::update() {
  // TODO
}

/* MiniAudio uses y-up by default, this function compensates for any
 * disparity between the P3D coord system and the audio engine. Please
 * take care of this coordinate difference if you're going beyond the
 * exposed API of this module.
 */
void MaAudioManager::
audio_3d_set_listener_attributes(
    PN_stdfloat px, PN_stdfloat py, PN_stdfloat pz, //pos
    PN_stdfloat vx, PN_stdfloat vy, PN_stdfloat vz, //vel
    PN_stdfloat fx, PN_stdfloat fy, PN_stdfloat fz, //fwd
    PN_stdfloat ux, PN_stdfloat uy, PN_stdfloat uz) { //up
  CoordinateSystem cs = get_default_coordinate_system();
  switch (cs) {
  case CS_yup_right:
    l_pos = {px, py, pz};
    l_vel = {vx, vy, vz};
    l_fwd = {fx, fy, fz};
    l_up = {ux, uy, uz};
    ma_engine_listener_set_position(&_audio_engine, 0, px, py, pz);
    ma_engine_listener_set_velocity(&_audio_engine, 0, vx, vy, vz);
    ma_engine_listener_set_direction(&_audio_engine, 0, fx, fy, fz);
    ma_engine_listener_set_world_up(&_audio_engine, 0, ux, uy, uz);
    break;
  case CS_zup_right:
    l_pos = {px, pz, -py};
    l_vel = {vx, vz, -vy};
    l_fwd = {fx, fz, -fy};
    l_up = {ux, uz, -uy};
    ma_engine_listener_set_position(&_audio_engine, 0, px, -pz, py);
    ma_engine_listener_set_velocity(&_audio_engine, 0, vx, -vz, vy);
    ma_engine_listener_set_direction(&_audio_engine, 0, fx, -fz, fy);
    ma_engine_listener_set_world_up(&_audio_engine, 0, ux, -uz, uy);
    break;
  case CS_yup_left:
    l_pos = {px, py, -pz};
    l_vel = {vx, vy, -vz};
    l_fwd = {fx, fy, -fz};
    l_up = {ux, uy, -uz};
    ma_engine_listener_set_position(&_audio_engine, 0, px, py, -pz);
    ma_engine_listener_set_velocity(&_audio_engine, 0, vx, vy, -vz);
    ma_engine_listener_set_direction(&_audio_engine, 0, fx, fy, -fz);
    ma_engine_listener_set_world_up(&_audio_engine, 0, ux, uy, -uz);
    break;
  case CS_zup_left:
    l_pos = {px, pz, py};
    l_vel = {vx, vz, vy};
    l_fwd = {fx, fz, fy};
    l_up = {ux, uz, uy};
    ma_engine_listener_set_position(&_audio_engine, 0, px, pz, py);
    ma_engine_listener_set_velocity(&_audio_engine, 0, vx, vz, vy);
    ma_engine_listener_set_direction(&_audio_engine, 0, fx, fz, fy);
    ma_engine_listener_set_world_up(&_audio_engine, 0, ux, uz, uy);
    break;
  default:
    nassert_raise("Invalid coordinate system given to MiniAudio.");
  }
}

void MaAudioManager::
audio_3d_get_listener_attributes(
    PN_stdfloat *px, PN_stdfloat *py, PN_stdfloat *pz, //pos
    PN_stdfloat *vx, PN_stdfloat *vy, PN_stdfloat *vz, //vel
    PN_stdfloat *fx, PN_stdfloat *fy, PN_stdfloat *fz, //fwd
    PN_stdfloat *ux, PN_stdfloat *uy, PN_stdfloat *uz) { //up
  l_pos = ma_engine_listener_get_position(&_audio_engine, 0);
  l_vel = ma_engine_listener_get_velocity(&_audio_engine, 0);
  l_fwd = ma_engine_listener_get_direction(&_audio_engine, 0);
  l_up = ma_engine_listener_get_world_up(&_audio_engine, 0);
  CoordinateSystem cs = get_default_coordinate_system();
  switch (cs) {
  case CS_yup_right:
    *px = l_pos.x; *py = l_pos.y; *pz = l_pos.z;
    *vx = l_vel.x; *vy = l_vel.y; *vz = l_vel.z;
    *fx = l_fwd.x; *fy = l_fwd.y; *fz = l_fwd.z;
    *ux = l_up.x; *uy = l_up.y; *uz = l_up.z;
    break;
  case CS_zup_right:
    *px = l_pos.x; *py = l_pos.z; *pz = -l_pos.y;
    *vx = l_vel.x; *vy = l_vel.z; *vz = -l_vel.y;
    *fx = l_fwd.x; *fy = l_fwd.z; *fz = -l_fwd.y;
    *ux = l_up.x; *uy = l_up.z; *uz = -l_up.y;
    break;
  case CS_yup_left:
    *px = l_pos.x; *py = l_pos.y; *pz = -l_pos.z;
    *vx = l_vel.x; *vy = l_vel.y; *vz = -l_vel.z;
    *fx = l_fwd.x; *fy = l_fwd.y; *fz = -l_fwd.z;
    *ux = l_up.x; *uy = l_up.y; *uz = -l_up.z;
    break;
  case CS_zup_left:
    *px = l_pos.x; *py = l_pos.z; *pz = l_pos.y;
    *vx = l_vel.x; *vy = l_vel.z; *vz = l_vel.y;
    *fx = l_fwd.x; *fy = l_fwd.z; *fz = l_fwd.y;
    *ux = l_up.x; *uy = l_up.z; *uz = l_up.y;
    break;
  default:
    nassert_raise("Invalid coordinate system given to MiniAudio.");
  }
}

/*
 * NOT IMPLEMENTED (yet?)
 */
void MaAudioManager::
audio_3d_set_distance_factor(PN_stdfloat factor) {
  return;
}

/*
 * NOT IMPLEMENTED (yet?)
 */
PN_stdfloat MaAudioManager::
audio_3d_get_distance_factor() const {
  return 1.f;
}

void MaAudioManager::
audio_3d_set_doppler_factor(PN_stdfloat factor) {
  ma_sound_group_set_doppler_factor(&_all_sounds_grp, factor);
}

PN_stdfloat MaAudioManager::
audio_3d_get_doppler_factor() const {
  ma_sound_group_get_doppler_factor(&_all_sounds_grp);
}

void MaAudioManager::
audio_3d_set_drop_off_factor(PN_stdfloat factor) {
  ma_sound_group_set_rolloff(&_all_sounds_grp, factor);
}

PN_stdfloat MaAudioManager::
audio_3d_get_drop_off_factor() const {
  ma_sound_group_get_rolloff(&_all_sounds_grp);
}

/**
 * Call this at exit time to shut down the audio system.  This will invalidate
 * all currently-active AudioManagers and AudioSounds in the system.  If you
 * change your mind and want to play sounds again, you will have to recreate
 * all of these objects.
 */
void MaAudioManager::
shutdown() {
  ReMutexHolder holder(_lock);
  if (_managers != nullptr) {
    Managers::iterator mi;
    for (mi = _managers->begin(); mi != _managers->end(); ++mi) {
      (*mi)->cleanup();
    }
  }

  nassertv(_active_managers == 0);
}

~MaAudioManager() {
  ReMutexHolder holder(_lock);
  nassertv(_managers != nullptr);
  Managers::iterator mi = _managers->find(this);
  nassertv(mi != _managers->end());
  _managers->erase(mi);
  cleanup();
}

/**
 * This is mostly for debugging, but it it could be used to detect errors in a
 * release build if you don't mind the cpu cost.
 */
bool MaAudioManager::
is_valid() {
  return _is_valid;
}

void MaAudioManager::
cleanup() {
  // TODO do we need to uninit all AudioSounds? _all_sounds_grp?
  ma_device_uninit(&_device);
  ma_engine_uninit(&_audio_engine);
}
