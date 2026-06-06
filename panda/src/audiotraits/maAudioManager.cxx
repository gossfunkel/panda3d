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

TypeHandle MaAudioManager::_type_handle;

ReMutex MaAudioManager::_lock;
int MaAudioManager::_active_managers = 0;
bool MaAudioManager::_ma_active = false;
ma_device *MaAudioManager::_device = nullptr;
ma_resource_manager_config MaAudioManager::_resource_mgr_conf;
ma_resource_manager MaAudioManager::_resource_mgr;
ma_engine MaAudioManager::_audio_engine;

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
    resource_mgr_conf.decodedFormat = _device.playback.format;
    resource_mgr_conf.decodedChannels = _device.playback.channels;
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
  return new MaAudioSound(this, file_name, positional, mode);
}

PT(AudioSound) MaAudioManager::
get_sound(MovieAudio *source, bool positional, int mode) {
  return new MaAudioSound(this, source, positional, mode);
}

void MaAudioManager::uncache_sound(const Filename &file_name) {
  ReMutexHolder holder(_lock);
  // TODO
}

void MaAudioManager::clear_cache() {
  ReMutexHolder holder(_lock);
  // TODO
}

void MaAudioManager::set_cache_limit(unsigned int count) {
  ReMutexHolder holder(_lock);
  // TODO
}

unsigned int MaAudioManager::get_cache_limit() const {
  // TODO
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
    // Tell our AudioSounds to adjust:
    AllSounds::iterator i=_all_sounds.begin();
    for (; i!=_all_sounds.end(); ++i) {
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
    _position[0] = px;
    _position[1] = py;
    _position[2] = pz;

    _velocity[0] = vx;
    _velocity[1] = vy;
    _velocity[2] = vz;

    _forward_up[0] = fx;
    _forward_up[1] = fy;
    _forward_up[2] = fz;

    _forward_up[3] = ux;
    _forward_up[4] = uy;
    _forward_up[5] = uz;
    ma_engine_listener_set_position(&_audio_engine, 0, px, py, pz);
    ma_engine_listener_set_velocity(&_audio_engine, 0, vx, vy, vz);
    ma_engine_listener_set_direction(&_audio_engine, 0, fx, fy, fz);
    ma_engine_listener_set_world_up(&_audio_engine, 0, ux, uy, uz);
    break;
  case CS_zup_right:
    _position[0] = px;
    _position[1] = pz;
    _position[2] = -py;

    _velocity[0] = vx;
    _velocity[1] = vz;
    _velocity[2] = -vy;

    _forward_up[0] = fx;
    _forward_up[1] = fz;
    _forward_up[2] = -fy;

    _forward_up[3] = ux;
    _forward_up[4] = uz;
    _forward_up[5] = -uy;
    ma_engine_listener_set_position(&_audio_engine, 0, px, -pz, py);
    ma_engine_listener_set_velocity(&_audio_engine, 0, vx, -vz, vy);
    ma_engine_listener_set_direction(&_audio_engine, 0, fx, -fz, fy);
    ma_engine_listener_set_world_up(&_audio_engine, 0, ux, -uz, uy);
    break;
  case CS_yup_left:
    _position[0] = px;
    _position[1] = py;
    _position[2] = -pz;

    _velocity[0] = vx;
    _velocity[1] = vy;
    _velocity[2] = -vz;

    _forward_up[0] = fx;
    _forward_up[1] = fy;
    _forward_up[2] = -fz;

    _forward_up[3] = ux;
    _forward_up[4] = uy;
    _forward_up[5] = -uz;
    ma_engine_listener_set_position(&_audio_engine, 0, px, py, -pz);
    ma_engine_listener_set_velocity(&_audio_engine, 0, vx, vy, -vz);
    ma_engine_listener_set_direction(&_audio_engine, 0, fx, fy, -fz);
    ma_engine_listener_set_world_up(&_audio_engine, 0, ux, uy, -uz);
    break;
  case CS_zup_left:
    _position[0] = px;
    _position[1] = pz;
    _position[2] = py;

    _velocity[0] = vx;
    _velocity[1] = vz;
    _velocity[2] = vy;

    _forward_up[0] = fx;
    _forward_up[1] = fz;
    _forward_up[2] = fy;

    _forward_up[3] = ux;
    _forward_up[4] = uz;
    _forward_up[5] = uy;
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
  ma_vec3f l_pos = ma_engine_listener_get_position(&_audio_engine, 0);
  ma_vec3f l_vel = ma_engine_listener_get_velocity(&_audio_engine, 0);
  ma_vec3f l_fwd = ma_engine_listener_get_direction(&_audio_engine, 0);
  ma_vec3f l_up = ma_engine_listener_get_world_up(&_audio_engine, 0);
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
  // TODO do we need to uninit all AudioSounds?
  ma_device_uninit(&_device);
  ma_engine_uninit(&_audio_engine);
}
