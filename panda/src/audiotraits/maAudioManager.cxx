/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file maAudio.h
 * @author Katie <katherineegoss@gmail.com> & J0y
 * @date 2026-06-02
 */

#include "maAudioManager.h"
#include "maAudioSound.h"

using std::string;

TypeHandle MaAudioManager::_type_handle;

// TODO debug macros?

//ReMutex MaAudioManager::_lock;
pset<MaAudioManager *> *MaAudioManager::_managers = nullptr;
int MaAudioManager::_active_managers = 0;
bool MaAudioManager::_ma_active = false;

/**
 * Factory Function
 */
AudioManager *Create_MaAudioManager() {
  audio_debug("Create_MaAudioManager()");
  //ReMutexHolder holder(_lock);
  return new MaAudioManager;
}

MaAudioManager::
MaAudioManager() {
  //ReMutexHolder holder(_lock);
  audio_cat.init();

  if (_managers == nullptr) _managers = new Managers;

  ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.samplerate = 48000;
  //config.dataCallback
  //config.pUserData

  check_ma(ma_device_init(NULL, &device_config, &_device),
           "Failed to initialise MiniAudio device.");

  ma_device_start(&_device);

  audio_cat.info() << "Using MiniAudio device " << device.playback.name <<
    "." << std::endl;

  // TODO share resource manager between MA engines
  resource_mgr_conf = ma_resource_manager_config_init();
  // TODO we need to make a custom ma_decoding_backend_vtable for
  //  vorbis etc and set it on the config
  resource_mgr_conf.decodedFormat     = _device.playback.format;
  resource_mgr_conf.decodedChannels   = _device.playback.channels;
  resource_mgr_conf.decodedSampleRate = _device.sampleRate;
#ifdef HAVE_THREADS
  resource_mgr_conf.jobThreadCount = 2;
#endif

  // here we could assign the p3d VFS to the resource mgr
  //  this would get us access to minified files
  //resource_mgr_conf.pVFS = VirtualFileSystem::get_global_pointer();
  if (ma_resource_manager_init(&resource_mgr_conf, &_resource_mgr)
      != MA_SUCCESS {
    ma_device_uninit(&_device);
    audio_error("Failed to initialise MiniAudio resource manager.");
  )

  _managers->insert(this);

  ma_engine_config audio_engine_conf;
  audio_engine_conf = ma_engine_config_init();
  audio_engine_conf.pResourceManager = &_resource_mgr;
  audio_engine_conf.noAutoStart = MA_TRUE;
  check_ma(
    ma_engine_init(&audio_engine_conf, &_engine),
    &ma_device_uninit, &(&_device),
    "Failed to initialise MiniAudio engine."
  );

  audio_3d_set_listener_attributes(
      0, 0, 0,
      0, 0, 0,
      1, 0, 0,
      0, 0, 1);

  _num_sources_cached = 0;

  // TODO default flags for global sound group:
  //  0 loads without decoding at init time
  //  DECODE decodes to memory at init time
  //  ASYNC decodes and loads at play time
  int sg_flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC;
  ma_sound_group_init(&_engine, sg_flags, nullptr, &_all_sounds_grp);

  // we'll do this when p3d is ready for it, or remove the noAutoStart line
  ma_engine_start(&_engine);

  if (audio_cat.is_debug())
    audio_cat.debug() << "MiniAudio version: " << ma_version_string()
                      << std::endl;
  // TODO miniaudio config? logging?
}

int MaAudioManager::get_speaker_setup() {
  return _device.playback.channels;
}

void MaAudioManager::set_speaker_setup(SpeakerModeCategory cat) {
  audio_warning("MiniAudio does not support setting channel setup.\n"
                << cat " channels not set.");
}

bool MaAudioManager::configure_filters(FilterProperties *config) {
  // TODO create filter nodes per config
}

/**
 * Creates a MaAudioSound object, and adds it to the cache.
 * Note: if mode is set to SM_stream, the AudioSound will not be
 * kept in the manager's cache, so will not be culled if stop()
 * (and the MaAudioSound destructor) are not called.
 * MiniAudio buffers streaming sounds in one second 'pages', so
 * take care not to fill the user's memory with streaming sounds.
 */
PT(AudioSound) MaAudioManager::
get_sound(const Filename &file_name, bool positional, int mode) {
  //ReMutexHolder holder(_lock);
  if (mode != StreamMode{SM_stream}) {
    auto cached_it = _cache_counts.find(file_name);
    if (cached_it == _cache_counts.end()) {
      if (_cache_counts.size() >= _cache_limit) {
        audio_error("Cache limit reached; cannot load new sound file");
        return _null_sound;.
      } else {
        _cached_it.emplace({file_name, 1});
      }
    } else cached_it->second++;
  }

  PT(AudioSound) new_sound =
    new MaAudioSound(this, file_name, positional, mode);

  if (mode != StreamMode{SM_stream})
    new_sound->_manager_it =
      _all_sounds.emplace_back((WPT(AudioSound)(*new_sound)));
  return new_sound;
}

/*
 * Construct a new sound using a MovieAudio source.
 * Note: this only uses the MovieAudio for its filename; does not use
 * a MovieAudioCursor.
 */
PT(AudioSound) MaAudioManager::
get_sound(MovieAudio &source, bool positional, int mode) {
  return get_sound(source.get_filename(), positional, mode);
}

/*
 * Deletes a reference to a sound if not active.
 */
void MaAudioManager::uncache_sound(const Filename &file_name) {
  //ReMutexHolder holder(_lock);

  // TODO should we use the miniaudio vfs?
  Filename path = file_name;
  VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
  vfs->resolve_filename(path, get_model_path());
  auto sound_it = _all_sounds.begin();
  while (sound_it != _all_sounds.end())
    if (auto s_ptr = sound_it->lock()) {
      if (s_ptr.file_name == file_name ||
          s_ptr.file_name == path)
        s_ptr.uncache();
        // should this uncache all/any sounds with this filename?
        //return;
    } else // pointer has expired
      _all_sounds.erase(sound_it);
}

/*
 * Clears data from cache for sources with no playing sounds.
 */
void MaAudioManager::clear_cache() {
  //ReMutexHolder holder(_lock);
  audio_cat.debug() << "Clearing audio cache..." << std::endl;

  for (auto sound_it : _all_sounds) {
    if (auto s_ptr = sound_it->lock()) s_ptr->uncache();
    else _all_sounds.erase(sound_it);
  }
}

/*
 * Modify the number of files to allow caching to memory.
 * Destructively stops and unloads sounds if cache is shrunk.
 * Caution: sounds with SM_stream mode are not kept in the cache.
 * MiniAudio buffers streaming sounds in one second 'pages', so
 * take care not to fill the user's memory with streaming sounds.
 */
void MaAudioManager::set_cache_limit(unsigned int count) {
  //ReMutexHolder holder(_lock);
  _cache_limit = count;

  audio_cat.debug() << "Trimming audio cache for new limit of "
                    << count << "." << std::endl;
  clear_cache();
  // step through all sounds, stopping them and unloading them from
  // MiniAudio until we have reached the new cache limit
  for (auto sound_it = _all_sounds.begin();
       _cache_counts.size() > count; sound_it.next()) {
    if (auto s_ptr = sound_it->lock()) {
      if (s_ptr == _all_sounds.end()) {
        audio_error("Could not uncache sounds to reduce cache size to new limit");
        return;
      }
      s_ptr.stop();
      s_ptr.uncache();
    } else // pointer has expired
      _all_sounds.erase(sound_it);
  }
}

unsigned int MaAudioManager::get_cache_limit() const {
  return _cache_limit;
}

/*
 * Sets global volume (gain) setting on our MiniAudio engine
 */
void MaAudioManager::set_volume(PN_stdfloat volume) {
  //ReMutexHolder holder(_lock);
  if (_volume != volume) {
    _volume = volume;
    check_ma(ma_engine_set_volume(&_engine, volume),
        "Failed to set MiniAudio engine global volume.");
  }
}

/*
 * Gets the global volume (gain) setting on our MiniAudio engine
 */
PN_stdfloat MaAudioManager::get_volume() const {
  _volume = ma_engine_get_volume(&_engine);
  return _volume;
}

/*
 * Gets a pointer to the MiniAudio resource manager we use
 */
ma_resource_manager *MaAudioManager::
get_resource_manager() {
  return &_resource_mgr;
}

/**
 * Turn on/off active flag, stopping all if false. Caution!
 *  This method may cause undefined behaviour; use others.
 */
void MaAudioManager::set_active(bool flag) {
  //ReMutexHolder holder(_lock);
  if (_active!=flag) {
    _active=flag;
    if (!flag) {
      audio_cat.debug() << "Stopping all sounds." << std::endl;
      for (auto sound_it : _all_sounds)
        sound_it->stop();
    } else {
      audio_cat.debug() << "Setting all cached sounds active!" << std::endl;

      for (auto sound_it : _all_sounds)
        sound_it->set_active(_active);
    }
  }
}

bool MaAudioManager::get_active() const {
  return _active;
}

void MaAudioManager::
set_concurent_sound_limit(unsigned int) {
  //ReMutexHolder holder(_lock);
  _concurrent_sound_limit = limit;
  reduce_sounds_playing_to(_concurrent_sound_limit);
}

unsigned int MaAudioManager::get_concurrent_sound_limit() const {
  return _concurrent_sound_limit;
}

void MaAudioManager::reduce_sounds_playing_to(unsigned int count) {
  //ReMutexHolder holder(_lock);

  audio_cat.debug() << "Reducing playing sounds to " << count
                    << "." << std::endl;
  for (auto sound_it = _all_sounds.begin();
       _num_concurrent_sounds > count; sound_it.next()) {
    if (auto s_ptr = sound_it->lock()) {
      if (s_ptr == _all_sounds.end() {
        audio_error("Could not stop sounds to reduce concurrent sound limit");
        return;
      }
      s_ptr.stop();
    } else // pointer has expired
      _all_sounds.erase(sound_it);
  }
}

void MaAudioManager::stop_all_sounds() {
  audio_cat.debug() << "Stopping all sounds." << std::endl;
  reduce_sounds_playing_to(0);
}

/*
 * Must be called every frame(?)
 * Do housework on any buffers and playing sounds
 */
void MaAudioManager::update() {
  //ReMutexHolder holder(_lock);
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
  //ReMutexHolder holder(_lock);
  CoordinateSystem cs = get_default_coordinate_system();
  switch (cs) {
  case CS_yup_right:
    l_pos = {px, py, pz};
    l_vel = {vx, vy, vz};
    l_fwd = {fx, fy, fz};
    l_up = {ux, uy, uz};
    ma_engine_listener_set_position(&_engine, 0, px, py, pz);
    ma_engine_listener_set_velocity(&_engine, 0, vx, vy, vz);
    ma_engine_listener_set_direction(&_engine, 0, fx, fy, fz);
    ma_engine_listener_set_world_up(&_engine, 0, ux, uy, uz);
    break;
  case CS_zup_right:
    l_pos = {px, pz, -py};
    l_vel = {vx, vz, -vy};
    l_fwd = {fx, fz, -fy};
    l_up = {ux, uz, -uy};
    ma_engine_listener_set_position(&_engine, 0, px, -pz, py);
    ma_engine_listener_set_velocity(&_engine, 0, vx, -vz, vy);
    ma_engine_listener_set_direction(&_engine, 0, fx, -fz, fy);
    ma_engine_listener_set_world_up(&_engine, 0, ux, -uz, uy);
    break;
  case CS_yup_left:
    l_pos = {px, py, -pz};
    l_vel = {vx, vy, -vz};
    l_fwd = {fx, fy, -fz};
    l_up = {ux, uy, -uz};
    ma_engine_listener_set_position(&_engine, 0, px, py, -pz);
    ma_engine_listener_set_velocity(&_engine, 0, vx, vy, -vz);
    ma_engine_listener_set_direction(&_engine, 0, fx, fy, -fz);
    ma_engine_listener_set_world_up(&_engine, 0, ux, uy, -uz);
    break;
  case CS_zup_left:
    l_pos = {px, pz, py};
    l_vel = {vx, vz, vy};
    l_fwd = {fx, fz, fy};
    l_up = {ux, uz, uy};
    ma_engine_listener_set_position(&_engine, 0, px, pz, py);
    ma_engine_listener_set_velocity(&_engine, 0, vx, vz, vy);
    ma_engine_listener_set_direction(&_engine, 0, fx, fz, fy);
    ma_engine_listener_set_world_up(&_engine, 0, ux, uz, uy);
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
  l_pos = ma_engine_listener_get_position(&_engine, 0);
  l_vel = ma_engine_listener_get_velocity(&_engine, 0);
  l_fwd = ma_engine_listener_get_direction(&_engine, 0);
  l_up = ma_engine_listener_get_world_up(&_engine, 0);
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
  //ReMutexHolder holder(_lock);
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
  //ReMutexHolder holder(_lock);
  ma_sound_group_set_doppler_factor(&_all_sounds_grp, factor);
}

PN_stdfloat MaAudioManager::
audio_3d_get_doppler_factor() const {
  ma_sound_group_get_doppler_factor(&_all_sounds_grp);
}

void MaAudioManager::
audio_3d_set_drop_off_factor(PN_stdfloat factor) {
  //ReMutexHolder holder(_lock);
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
  audio_cat.debug() << "Shutting down Audio Managers." << std::endl;
  //ReMutexHolder holder(_lock);
  if (_managers != nullptr)
    for (Managers::iterator man_it : _managers)
      man_it->cleanup();

  nassertv(_active_managers == 0);
}

~MaAudioManager() {
  //ReMutexHolder holder(_lock);
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
 *  the audio engine, device, and resource manager.
 * Warning: invalidates all pointers to sounds!
 */
void MaAudioManager::
cleanup() {
  audio_cat.debug() << "Cleaning up Audio Manager..." << std::endl;
  //ReMutexHolder holder(_lock);
  for (auto sound_it : _all_sounds) {
    if (auto s_ptr = sound_it->lock()) {
      s_ptr->stop();
      _all_sounds.erase(s_ptr);
      delete s_ptr;
    } else _all_sounds.erase(sound_it);
  }

  ma_device_uninit(&_device);
  ma_engine_uninit(&_engine);
  ma_resource_manager_uninit(&_resource_mgr);
}
