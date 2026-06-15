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
  // TODO check these values set, if not, set _is_valid = false;

  _num_sources_cached = 0;

  // TODO flags for global sound group
  int sg_flags = 0;
  ma_sound_group_init(&_audio_engine, sg_flags, nullptr, &_all_sounds_grp);

  // we'll do this when p3d is ready for it, or remove the noAutoStart line
  ma_engine_start(&_audio_engine);

  if (audio_cat.is_debug()) {
    audio_cat.debug() << "MA ... " << var << std::endl;
  }
}

/**
 * Creates a MaAudioSound object, constructing a source if it's new.
 */
PT(AudioSound) MaAudioManager::
get_sound(const Filename &file_name, bool positional, int mode) {
  auto data_src_it = _data_sources.find(file_name);
  if (data_src_it == _data_sources.end()) {
    // make new DataSource
    DataSource *new_src = _data_sources.emplace_back(DataSource(
        file_name,
        false,
        1,
        0,
        data_src
    ));
    if (_num_sources_cached < _cache_limit) {
      ma_resource_manager_data_source_init(&new_src->data_src);
      _cache_order.emplace_back(new_src);
      new_src->cached = true;
      new_src->active_sounds = 1;
      _num_sources_cached++;
    }
  } else { // source file is already loaded to _data_sources
    data_src_it->refcount++;
    if (!data_src_it->cached && _num_sources_cached < _cache_limit) {
      ma_resource_manager_data_source_init(&data_src_it->data_src);
      _cached_sources.emplace(file_name, &(*data_src_it));
      data_src_it->cached = true;
      new_src->active_sounds = 1;
      _num_sources_cached++;
    } else if (data_src_it->cached) {
      data_src_it->active_sounds++;
    }
    // TODO this constructor
    _all_sounds.emplace_back(MaAudioSound(
          this,
          &(*data_src_it),
          file_name,
          positional,
          mode
    ));
  }
  PT(AudioSound) new_sound =
    (AudioSound *)(MaAudioSound *)_all_sounds.back();
  return new_sound;
}

/*
 * Construct a new sound using a MovieAudio source. Note: this will not
 * benefit from MiniAudio's resource management.
 */
PT(AudioSound) MaAudioManager::
get_sound(MovieAudio *source, bool positional, int mode) {
  auto data_src_it = _data_sources.find(source.get_filename());
  if (data_src_it == _data_sources.end()) {
    ma_movie_audio new_ma_ma;
    DataSource *new_src = _data_sources.emplace_back(DataSource(
        file_name,
        false,
        1,
        0,
        new_ma_ma;
    ));
    if (_num_sources_cached < _cache_limit) {
      ma_movie_audio_init(source, &new_src->data_src);
      _cache_order.emplace_back(new_src);
      new_src->cached = true;
      new_src->active_sounds = 1;
      _num_sources_cached++;
    }
  } else { // source file is already loaded to _data_sources
    data_src_it->refcount++;
    if (!data_src_it->cached && _num_sources_cached < _cache_limit) {
      // TODO make conditional on length
      //int flags = (loop_sound) ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING : 0;
      int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM; // decode in 1s pages
      //int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE; // decode to ram
      //int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC; load to ram later
      check_ma(ma_movie_audio_init(*resource_mgr, source->filename.get_basename(),
                          flags, &data_src_it->data_src), [](return NULL),
                          "Failed to initialise MovieAudio DataSource");
      _cached_sources.emplace(file_name, &(*data_src_it));
      data_src_it->cached = true;
      new_src->active_sounds = 1;
      _num_sources_cached++;
    } else if (data_src_it->cached) {
      data_src_it->active_sounds++;
    }
    // TODO this constructor
    _all_sounds.emplace_back(MaAudioSound(
          this,
          &(*data_src_it),
          source->filename,
          positional,
          mode
    ));
  }
  PT(AudioSound) new_sound =
    (AudioSound *)(MaAudioSound *)_all_sounds.back();
  return new_sound;
}

/*
 * Deletes a cached source from the expiration cache, if not in use
 * by an active AudioSound.
 */
void MaAudioManager::uncache_sound(const Filename &file_name) {
  ReMutexHolder holder(_lock);
  Filename path = file_name;

  // TODO use the miniaudio vfs?

  auto data_src = _cached_sources.find(path);
  if (data_src == _cached_sources.end()) {
    VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
    vfs->resolve_filename(path, get_model_path());
    data_src    = _cached_sources.find(path);
  }
  if (data_src == _cached_sources.end()) return;

  if (data_src->active_sounds) {
    audio_error("Sound is active, cannot be uncached.");
    return;
  }
  ma_data_source_uninit(&(*data_src));
  _cached_sources.erase(path);
  if (!data_src->refcount) {
    _data_sources.erase(path);
    delete data_src;
  }
}

/*
 * Garbage collects data sources
 */
void MaAudioManager::clear_cache() {
  ReMutexHolder holder(_lock);

  for (auto data_src_p_it : _cached_sources) {
    if (!(*data_src_p_it)->active_sounds) {
      _cached_sources.erase((*data_src_p_it)->file_name.get_basename());
    }
  }
  for (auto data_src_it : _data_sources) {
    if (!data_src_it->refcount) {
      if (data_src_it->cached)
        _cached_sources.erase(data_src_it->file_name.get_basename());
      _data_sources.erase(data_src_it->file_name.get_basename());
      delete data_src_it;
    }
  }
}

void MaAudioManager::set_cache_limit(unsigned int count) {
  ReMutexHolder holder(_lock);
  _cache_limit = count;
  while (_num_sources_cached > count) {
    _cached_sources.front()->cached = false;
    // TODO hashmap doesn't have this
    _cached_sources.pop_front();
    _num_sources_cached--;
  }
}

unsigned int MaAudioManager::get_cache_limit() const {
  return _cache_limit;
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
    auto sound_it = _sounds_playing.begin();
    nassertv(sound_it != _sounds_playing.end());
    /* Stop should be called while holding a PT the sound- we have to make a
     * temporary one here so the refcount doesn't go to 0 and recurse, with
     * the destructor calling stop() and stop() calling the destructor.
     */
    // TODO shouldn't the AudioSound destructor just check if it's stopped?
    PT(MaAudioSound) pt_sound = &(*sound_it);
    pt_sound->stop();
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
 *  disparity between the P3D coord system and the audio engine. Please
 *  take care of this coordinate difference if you're going beyond the
 *  exposed API of this module.
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
 * Shut down the entire audio system. Invalidates all AudioManagers and
 *  AudioSounds in the system, even if they're active! This cannot be undone
 */
void MaAudioManager::
shutdown() {
  ReMutexHolder holder(_lock);
  if (_managers != nullptr)
    for (Managers::iterator man_it : _managers)
      man_it->cleanup();

  nassertv(_active_managers == 0);
}

~MaAudioManager() {
  ReMutexHolder holder(_lock);
  nassertv(_managers != nullptr);
  Managers::iterator man_it = _managers->find(this);
  nassertv(man_it != _managers->end());
  _managers->erase(man_it);
  cleanup();
}

/**
 * For debug but you can use it for release if you don't mind the cost
 */
bool MaAudioManager::
is_valid() {
  return _is_valid;
}

/*
 * Stop and remove all playing sounds, remove all sources, and uninitialise
 *  the audio engine, device, and resource manager
 */
void MaAudioManager::
cleanup() {
  for (auto sound_it : _all_sounds) {
    sound_it->stop();
    _all_sounds.erase(sound_it);
    delete sound_it;
  }
  for (auto source_it : _data_sources) {
    if (source_it->movie_audio == nullptr)
      ma_resource_manager_data_source_uninit(source_it->data_src);
    else
      ma_movie_audio_uninit(source_it->data_src);
    _data_sources.erase(source_it);
    delete source_it;
  }

  ma_device_uninit(&_device);
  ma_engine_uninit(&_audio_engine);
  ma_resource_manager_uninit(&_resource_mgr);
}
