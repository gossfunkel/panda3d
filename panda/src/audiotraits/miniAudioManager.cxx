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

#include "miniAudioManager.h"
#include "miniAudioSound.h"
#include "virtualFileSystem.h"

TypeHandle MiniAudioManager::_type_handle;

// TODO debug macros?

//ReMutex MiniAudioManager::_lock;
int MiniAudioManager::_active_managers = 0;
pset<MiniAudioManager *> *MiniAudioManager::_managers = nullptr;
ma_resource_manager_config *MiniAudioManager::_resource_mgr_conf = nullptr;
ma_resource_manager *MiniAudioManager::_resource_mgr = nullptr;

/**
 * Factory Function
 */
AudioManager *Create_MiniAudioManager() {
  audio_debug("Create_MiniAudioManager()");
  //ReMutexHolder holder(_lock);
  //PT(AudioManager) new_man = new MiniAudioManager;
  //return new_man;
  return new MiniAudioManager;
}

/*
 * Device data callback: forward all audio to it
 */
static void mini_audio_device_callback(ma_device *device, void *output,
                                       const void *input,
                                       ma_uint32 frame_count) {
  (void)input;
  ma_engine *engine = (ma_engine *)device->pUserData;
  ma_engine_read_pcm_frames(engine, output, frame_count, nullptr);
}

MiniAudioManager::
MiniAudioManager() {
  //ReMutexHolder holder(_lock);
  audio_cat.init();
  _active = audio_active;
  _is_valid = false;
  _volume = audio_volume;
  _play_rate = 1.0f;
  _cache_limit = audio_cache_limit;
  _concurrent_sound_limit = 0;
  _num_concurrent_sounds = 0;
  _distance_factor = 1.0f;
  _doppler_factor = 1.0f;
  _drop_off_factor = 1.0f;
  l_pos = LVector3(0, 0, 0);
  l_vel = LVector3(0, 0, 0);
  l_fwd = LVector3(0, 0, 1);
  l_up = LVector3(0, 1, 0);

  ma_device_config device_config =
    ma_device_config_init(ma_device_type_playback);
  device_config.playback.format = ma_format_f32;
  device_config.playback.channels = 2;
  device_config.sampleRate = 48000;
  device_config.dataCallback = &mini_audio_device_callback;
  device_config.pUserData = &_engine;

  if (ma_device_init(NULL, &device_config, &_device) != MA_SUCCESS) {
    audio_error("Failed to initialise MiniAudio device.");
    return;
  }
  audio_cat.info() << "Using MiniAudio device " << _device.playback.name <<
    "." << std::endl;

  if (_managers == nullptr)
    _managers = new pset<MiniAudioManager *>;
  _managers->insert(this);

  if (MiniAudioManager::_resource_mgr == nullptr) {
    MiniAudioManager::_resource_mgr_conf = new ma_resource_manager_config();
    *MiniAudioManager::_resource_mgr_conf = ma_resource_manager_config_init();
    MiniAudioManager::_resource_mgr_conf->decodedFormat     = _device.playback.format;
    MiniAudioManager::_resource_mgr_conf->decodedChannels   = _device.playback.channels;
    MiniAudioManager::_resource_mgr_conf->decodedSampleRate = _device.sampleRate;
#ifdef HAVE_THREADS
    MiniAudioManager::_resource_mgr_conf->jobThreadCount = 2;
#endif
    MiniAudioManager::_resource_mgr = new ma_resource_manager();
    if (ma_resource_manager_init(
          MiniAudioManager::_resource_mgr_conf,
          MiniAudioManager::_resource_mgr)
        != MA_SUCCESS) {
      ma_device_uninit(&_device);
      audio_error("Failed to initialise MiniAudio resource manager.");
    }
  }

  ma_engine_config audio_engine_conf;
  audio_engine_conf = ma_engine_config_init();
  audio_engine_conf.pResourceManager = MiniAudioManager::_resource_mgr;
  audio_engine_conf.pDevice = &_device;
  audio_engine_conf.noAutoStart = MA_TRUE;
  if (ma_engine_init(&audio_engine_conf, &_engine) != MA_SUCCESS) {
    ma_resource_manager_uninit(MiniAudioManager::_resource_mgr);
    ma_device_uninit(&_device);
    audio_error("Failed to initialise MiniAudio engine.");
    return;
  };

  audio_3d_set_listener_attributes(
      0, 0, 0,
      0, 0, 0,
      1, 0, 0,
      0, 0, 1);

  _num_concurrent_sounds = 0;

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

  _is_valid = true;
  _active = true;
}

int MiniAudioManager::get_speaker_setup() {
  return _device.playback.channels;
}

void MiniAudioManager::set_speaker_setup(SpeakerModeCategory cat) {
  audio_warning("MiniAudio does not support setting channel setup.\n"
                << cat << " channels not set.");
}

/*
 * Parse config settings and return pointer to equivalent miniaudio node.
 */
bool MiniAudioManager::configure_filters(FilterProperties *config) {
  const FilterProperties::ConfigVector &conf = config->get_config();
  if (_global_fx == nullptr)
    // TODO node config
    ma_node_init(&_engine.nodeGraph, nullptr, nullptr, _global_fx);
  //else
    // TODO reinit() _global_fx
  ma_node_graph *node_graph = &_engine.nodeGraph;
  ma_node *prev_node = _global_fx;
  ma_uint32 channels = _device.playback.channels;
  ma_uint32 samp_rate = _engine.sampleRate;
  /*
  for (FilterProperties::FilterConfig conf_item : conf) {
    ma_node new_node;
    switch (conf_item._type) {
      case FilterProperties::FT_lowpass:
        ma_loshelf_node *ls_node = (ma_loshelf_node *)&new_node;
        ma_loshelf_node_config *ls_node_conf;
        *ls_node_conf = ma_loshelf_node_config_init(
            channels, samp_rate, 1.f, conf_item.b, conf_item.a
        );
        ma_loshelf_node_init(node_graph, ls_node_conf, nullptr, ls_node);
        break;
      case FilterProperties::FT_highpass:
        // ma_hishelf_node
        ma_hishelf_node *hs_node = (ma_hishelf_node *)&new_node;
        ma_hishelf_node_config *hs_node_conf;
        *hs_node_conf = ma_hishelf_node_config_init(
            channels, samp_rate, 1.f, conf_item.b, conf_item.a
        );
        ma_hishelf_node_init(node_graph, hs_node_conf, nullptr, hs_node);
        break;
      case FilterProperties::FT_echo:
        // ma_delay_node
        ma_delay_node *ec_node = (ma_delay_node *)&new_node;
        ma_delay_node_config *ec_node_conf;
        ma_uint32 echo_delay_frames = samp_rate * (ma_uint32)conf_item.c;
        *ec_node_conf = ma_delay_node_config_init(
            channels, samp_rate, echo_delay_frames, conf_item.d
        );
        ma_delay_node_init(node_graph, ec_node_conf, nullptr, &ec_node);
        break;
      case FilterProperties::FT_flange:
        // TODO p3d_flanger_node
        break;
      case FilterProperties::FT_distort:
        // p3d_distort_node
        break;
      case FilterProperties::FT_normalize:
        // TODO p3d_normalize_node
        break;
      case FilterProperties::FT_parameq:
        // ma_biquad_node ?
        break;
      case FilterProperties::FT_pitchshift:
        // TODO p3d_repitch_node
        break;
      case FilterProperties::FT_chorus:
        // TODO p3d_chorus_node
        break;
      case FilterProperties::FT_sfxreverb:
        // ma_reverb_node (miniaudio extras)
        break;
      case FilterProperties::FT_compress:
        // TODO p3d_compress_node
        break;
      default:
        audio_error("Malformed filter config passed to MiniAudio Manager.");
        return false;
    }
    if (ma_node_attach_output_bus(prev_node, 0, new_node, 0) != MA_SUCCESS) {
      audio_error("Failed to set filter " << conf_item._type << ".");
      break;
    }
    if (ma_node_attach_output_bus(
          new_node, 0,
          ma_node_graph_get_endpoint(node_graph), 0
          ) != MA_SUCCESS) {
      audio_error("Failed to set filter " << conf_item._type << ".");
      break;
    }
    prev_node = &new_node;
  }
  // ConfigVector is a typedef of pvector<FilterConfig>
  //struct FilterConfig {
  //  FilterType  _type;
  //  PN_stdfloat       _a,_b,_c,_d;
  //  PN_stdfloat       _e,_f,_g,_h;
  //  PN_stdfloat       _i,_j,_k,_l;
  //  PN_stdfloat       _m,_n;
  //};

  // TODO save the first node in the new chain to _global_fx ?
  */
  return true;
}

/**
 * Creates a MiniAudioSound object, and adds it to the cache.
 * Note: if mode is set to SM_stream, the AudioSound will not be
 * kept in the manager's cache, so will not be culled if stop()
 * (and the MiniAudioSound destructor) are not called.
 * MiniAudio buffers streaming sounds in one second 'pages', so
 * take care not to fill the user's memory with streaming sounds.
 */
PT(AudioSound) MiniAudioManager::
get_sound(const Filename &file_name, bool positional, int mode) {
  //ReMutexHolder holder(_lock);
  if (!is_valid()) {
    return get_null_sound();
  }

  Filename path = file_name;
  VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
  vfs->resolve_filename(path, get_model_path());
  if (path.empty()) {
    audio_error("get_sound - invalid filename");
    return get_null_sound();
  }

  // Obtain a MovieAudio for the resolved path.
  // MovieAudio opens the file through the VirtualFileSystem, so
  // sounds stored in the VFS (like multifiles) can be loaded.
  PT(MovieAudio) mva = MovieAudio::get(path);

  if (mva->get_filename().empty()) {
    audio_error("get_sound - unsupported audio file: " << path);
    return get_null_sound();
  }

  return get_sound(mva, positional, mode);
}

/*
 * Construct a new sound using a MovieAudio source.
 * The MovieAudio is decoded through its MovieAudioCursor
 * (which reads through the VFS), rather than being handed
 * to miniaudio's own file decoders.
 */
PT(AudioSound) MiniAudioManager::
get_sound(MovieAudio *source, bool positional, int mode) {
  if (!is_valid()) {
    return get_null_sound();
  }

  if (source == nullptr || source->get_filename().empty()) {
    audio_error("get_sound - invalid MovieAudio source");
    return get_null_sound();
  }

  Filename path = source->get_filename();

  if (mode != StreamMode{SM_stream}) {
    if (_cache_counts.find(path) == _cache_counts.end() &&
        _cache_counts.size() >= _cache_limit) {
      audio_error("Cache limit reached; cannot load new sound file");
      return get_null_sound();
    }
  }

  MiniAudioSound *new_ma_sound =
    new MiniAudioSound(this, source, path, positional, mode);

  if (!new_ma_sound->is_valid()) {
    // The sound failed to load; return a null sound instead.
    delete new_ma_sound;
    return get_null_sound();
  }

  if (mode != StreamMode{SM_stream}) {
    // TODO this must be done thread-safely
    _all_sounds.emplace_back((WPT(MiniAudioSound))new_ma_sound);
    new_ma_sound->_manager_it = --_all_sounds.end();
  }
  return (PT(AudioSound))new_ma_sound;
}

/*
 * Deletes a reference to a sound if not active.
 */
void MiniAudioManager::uncache_sound(const Filename &file_name) {
  //ReMutexHolder holder(_lock);

  Filename path = file_name;
  VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
  vfs->resolve_filename(path, get_model_path());
  pdeque<WPT(MiniAudioSound)> all_sounds(_all_sounds);
  for (auto sound_it = all_sounds.begin();
       sound_it != all_sounds.end(); sound_it++) {
    if (PT(MiniAudioSound) s_ptr = sound_it->lock()) {
      if (s_ptr->get_name() == file_name.get_basename() ||
          s_ptr->get_name() == path.get_basename())
        s_ptr->uncache();
    }
  }
}

/*
 * Clears data from cache for sources with no playing sounds.
 */
void MiniAudioManager::clear_cache() {
  //ReMutexHolder holder(_lock);
  audio_cat.debug() << "Clearing audio cache..." << std::endl;

  pdeque<WPT(MiniAudioSound)> all_sounds(_all_sounds);
  for (auto sound_it = all_sounds.begin();
       sound_it != all_sounds.end(); sound_it++) {
    if (PT(MiniAudioSound) s_ptr = sound_it->lock())
        s_ptr->uncache();
  }
  _all_sounds.clear();
}

/*
 * Modify the number of files to allow caching to memory.
 * Destructively stops and unloads sounds if cache is shrunk.
 * Caution: sounds with SM_stream mode are not kept in the cache.
 * MiniAudio buffers streaming sounds in one second 'pages', so
 * take care not to fill the user's memory with streaming sounds.
 */
void MiniAudioManager::set_cache_limit(unsigned int count) {
  //ReMutexHolder holder(_lock);
  _cache_limit = count;

  audio_cat.debug() << "Trimming audio cache for new limit of "
                    << count << "." << std::endl;
  clear_cache();
  // step through all sounds, stopping them and unloading them from
  // MiniAudio until we have reached the new cache limit
  pdeque<WPT(MiniAudioSound)> all_sounds(_all_sounds);
  for (auto sound_it = all_sounds.begin();
       sound_it != all_sounds.end() && _cache_counts.size() > count;
       sound_it++) {
    if (PT(MiniAudioSound) s_ptr = sound_it->lock()) {
      s_ptr->stop();
      s_ptr->uncache();
    }
  }
  if (_cache_counts.size() > count) {
    audio_error("Could not uncache sounds to reduce cache size to new limit");
  }
}

unsigned int MiniAudioManager::get_cache_limit() const {
  return _cache_limit;
}

/*
 * Sets global volume (gain) setting on our MiniAudio engine
 */
void MiniAudioManager::set_volume(PN_stdfloat volume) {
  //ReMutexHolder holder(_lock);
  if (_volume != volume) {
    _volume = volume;
    if(ma_engine_set_volume(&_engine, volume) != MA_SUCCESS)
        audio_error("Failed to set MiniAudio engine global volume.");
  }
}

/*
 * Gets the global volume (gain) setting on our MiniAudio engine
 */
PN_stdfloat MiniAudioManager::get_volume() const {
  return _volume;
}

/**
 * Turn on/off active flag, stopping all if false. Caution!
 *  This method may cause undefined behaviour; use others.
 */
void MiniAudioManager::set_active(bool flag) {
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

bool MiniAudioManager::get_active() const {
  return _active;
}

void MiniAudioManager::
set_concurrent_sound_limit(unsigned int limit) {
  //ReMutexHolder holder(_lock);
  _concurrent_sound_limit = limit;
  reduce_sounds_playing_to(_concurrent_sound_limit);
}

unsigned int MiniAudioManager::get_concurrent_sound_limit() const {
  return _concurrent_sound_limit;
}

void MiniAudioManager::reduce_sounds_playing_to(unsigned int count) {
  //ReMutexHolder holder(_lock);

  audio_cat.debug() << "Reducing playing sounds to " << count
                    << "." << std::endl;
  pdeque<WPT(MiniAudioSound)> all_sounds(_all_sounds);
  for (auto sound_it = all_sounds.begin();
       sound_it != all_sounds.end() && _num_concurrent_sounds > count;
       sound_it++) {
    if (auto s_ptr = sound_it->lock()) {
      s_ptr->stop();
    }
  }
}

void MiniAudioManager::stop_all_sounds() {
  audio_cat.debug() << "Stopping all sounds." << std::endl;
  reduce_sounds_playing_to(0);
}

/*
 * Must be called every frame(?)
 * Do housework on any buffers and playing sounds
 */
void MiniAudioManager::update() {
  //ReMutexHolder holder(_lock);
}

/* MiniAudio uses y-up by default, this function compensates for any
 *  disparity between the P3D coord system and the audio engine. Please
 *  take care of this coordinate difference if you're going beyond the
 *  exposed API of this module.
 */
void MiniAudioManager::
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

void MiniAudioManager::
audio_3d_get_listener_attributes(
    PN_stdfloat *px, PN_stdfloat *py, PN_stdfloat *pz, //pos
    PN_stdfloat *vx, PN_stdfloat *vy, PN_stdfloat *vz, //vel
    PN_stdfloat *fx, PN_stdfloat *fy, PN_stdfloat *fz, //fwd
    PN_stdfloat *ux, PN_stdfloat *uy, PN_stdfloat *uz) { //up
  l_pos = LVector3(ma_engine_listener_get_position(&_engine, 0).x, ma_engine_listener_get_position(&_engine, 0).y, ma_engine_listener_get_position(&_engine, 0).z);
  l_vel = LVector3(ma_engine_listener_get_velocity(&_engine, 0).x, ma_engine_listener_get_velocity(&_engine, 0).y, ma_engine_listener_get_velocity(&_engine, 0).z);
  l_fwd = LVector3(ma_engine_listener_get_direction(&_engine, 0).x, ma_engine_listener_get_direction(&_engine, 0).y, ma_engine_listener_get_direction(&_engine, 0).z);
  l_up = LVector3(ma_engine_listener_get_world_up(&_engine, 0).x, ma_engine_listener_get_world_up(&_engine, 0).y, ma_engine_listener_get_world_up(&_engine, 0).z);
  CoordinateSystem cs = get_default_coordinate_system();
  switch (cs) {
  case CS_yup_right:
    *px = l_pos.get_x(); *py = l_pos.get_y(); *pz = l_pos.get_z();
    *vx = l_vel.get_x(); *vy = l_vel.get_y(); *vz = l_vel.get_z();
    *fx = l_fwd.get_x(); *fy = l_fwd.get_y(); *fz = l_fwd.get_z();
    *ux = l_up.get_x(); *uy = l_up.get_y(); *uz = l_up.get_z();
    break;
  case CS_zup_right:
    *px = l_pos.get_x(); *py = l_pos.get_z(); *pz = -l_pos.get_y();
    *vx = l_vel.get_x(); *vy = l_vel.get_z(); *vz = -l_vel.get_y();
    *fx = l_fwd.get_x(); *fy = l_fwd.get_z(); *fz = -l_fwd.get_y();
    *ux = l_up.get_x(); *uy = l_up.get_z(); *uz = -l_up.get_y();
    break;
  case CS_yup_left:
    *px = l_pos.get_x(); *py = l_pos.get_y(); *pz = -l_pos.get_z();
    *vx = l_vel.get_x(); *vy = l_vel.get_y(); *vz = -l_vel.get_z();
    *fx = l_fwd.get_x(); *fy = l_fwd.get_y(); *fz = -l_fwd.get_z();
    *ux = l_up.get_x(); *uy = l_up.get_y(); *uz = -l_up.get_z();
    break;
  case CS_zup_left:
    *px = l_pos.get_x(); *py = l_pos.get_z(); *pz = l_pos.get_y();
    *vx = l_vel.get_x(); *vy = l_vel.get_z(); *vz = l_vel.get_y();
    *fx = l_fwd.get_x(); *fy = l_fwd.get_z(); *fz = l_fwd.get_y();
    *ux = l_up.get_x(); *uy = l_up.get_z(); *uz = l_up.get_y();
    break;
  default:
    nassert_raise("Invalid coordinate system given to MiniAudio.");
  }
}

/*
 * NOT IMPLEMENTED (yet?)
 */
void MiniAudioManager::
audio_3d_set_distance_factor(PN_stdfloat factor) {
  //ReMutexHolder holder(_lock);
  return;
}

/*
 * NOT IMPLEMENTED (yet?)
 */
PN_stdfloat MiniAudioManager::
audio_3d_get_distance_factor() const {
  return 1.f;
}

void MiniAudioManager::
audio_3d_set_doppler_factor(PN_stdfloat factor) {
  //ReMutexHolder holder(_lock);
  _doppler_factor = factor;
  ma_sound_group_set_doppler_factor(&_all_sounds_grp, factor);
}

PN_stdfloat MiniAudioManager::
audio_3d_get_doppler_factor() const {
  return ma_sound_group_get_doppler_factor(&_all_sounds_grp);
}

void MiniAudioManager::
audio_3d_set_drop_off_factor(PN_stdfloat factor) {
  //ReMutexHolder holder(_lock);
  _drop_off_factor = factor;
  ma_sound_group_set_rolloff(&_all_sounds_grp, factor);
}

PN_stdfloat MiniAudioManager::
audio_3d_get_drop_off_factor() const {
  return ma_sound_group_get_rolloff(&_all_sounds_grp);
}

/**
 * Shut down the entire audio system. Invalidates all AudioManagers and
 *  AudioSounds in the system, even if they're active! This cannot be undone
 */
void MiniAudioManager::
shutdown() {
  audio_cat.debug() << "Shutting down Audio Managers." << std::endl;
  //ReMutexHolder holder(_lock);
  if (_managers != nullptr) {
    for (MiniAudioManager *man_ptr : *_managers) {
      man_ptr->cleanup();
    }
  }
  ma_resource_manager_uninit(MiniAudioManager::_resource_mgr);
  MiniAudioManager::_resource_mgr = nullptr;
}

MiniAudioManager::
~MiniAudioManager() {
  //ReMutexHolder holder(_lock);
  if (_managers != nullptr) {
    auto man_it = _managers->find(this);
    if (man_it != _managers->end()) {
      _managers->erase(man_it);
    }
  }
  cleanup();
}

/**
 * For debug but you can use it for release if you don't mind the cost
 */
bool MiniAudioManager::
is_valid() {
  return _is_valid;
}

/*
 * Stop and remove all playing sounds, remove all sources, and uninitialise
 *  the audio engine, device, and resource manager.
 * Warning: invalidates all pointers to sounds!
 */
void MiniAudioManager::
cleanup() {
  audio_cat.debug() << "Cleaning up Audio Manager..." << std::endl;
  //ReMutexHolder holder(_lock);
  if (!_is_valid) {
    // Already cleaned up; make sure the device is not uninitialised twice.
    return;
  }
  _is_valid = false;

  pdeque<WPT(MiniAudioSound)> all_sounds(_all_sounds);
  for (auto sound_it = all_sounds.begin();
       sound_it != all_sounds.end(); sound_it++) {
    if (PT(MiniAudioSound) s_ptr = sound_it->lock()) {
      s_ptr->stop();
      s_ptr->cleanup();
    }
  }
  _all_sounds.clear();

  ma_sound_group_uninit(&_all_sounds_grp);
  ma_engine_uninit(&_engine);
  ma_device_uninit(&_device);
}
