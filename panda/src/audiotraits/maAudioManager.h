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


#ifndef MINIAUDIOMANAGER_H
#define MINIAUDIOMANAGER_H

#include "array"
#include "pandabase.h"

#include "audioManager.h"
#include "pmap.h"
#include "pset.h"
#include "movieAudioCursor.h"
#include "reMutex.h"
#include "vector_string.h"

#include "miniaudio.h"

class MaAudioSound;

typedef struct ma_movie_audio {
    ma_data_source_base base;
    MovieAudio *movie_audio;
};

static ma_result ma_movie_audio_read(
    ma_data_source* pDataSource,
    void* pFramesOut,
    ma_uint64 frameCount,
    ma_uint64* pFramesRead) {
  // TODO Read data here.
  // Output in the same format returned by
  // ma_movie_audio_get_data_format().
}

static ma_result ma_movie_audio_seek(
    ma_data_source* pDataSource,
    ma_uint64 frameIndex) {
  // Seek to a specific PCM frame here.
  // Return MA_NOT_IMPLEMENTED if seeking is not supported.
}

static ma_result ma_movie_audio_get_data_format(
    ma_data_source* pDataSource,
    ma_format* pFormat,
    ma_uint32* pChannels,
    ma_uint32* pSampleRate,
    ma_channel* pChannelMap,
    size_t channelMapCap) {
  // Return the format of the data here.
}

static ma_result ma_movie_audio_get_cursor(
    ma_data_source* pDataSource,
    ma_uint64* pCursor) {
  // TODO Retrieve the current position of the cursor here.
  // Return MA_NOT_IMPLEMENTED and set *pCursor to 0 if there is no
  // notion of a cursor.
}

static ma_result ma_movie_audio_get_length(
    ma_data_source* pDataSource,
    ma_uint64* pLength) {
  // TODO Retrieve the length in PCM frames here.
  // Return MA_NOT_IMPLEMENTED and set *pLength to 0 if there is no
  // notion of a length, or if the length is unknown.
}

static ma_data_source_vtable g_ma_movie_audio_vtable = {
  my_data_source_read,
  my_data_source_seek,
  my_data_source_get_data_format,
  my_data_source_get_cursor,
  my_data_source_get_length
};

ma_result ma_movie_audio_init(
    MovieAudio *movie_audio,
    ma_movie_audio* p_ma_MovieAudio) {
  ma_result result;
  ma_data_source_config baseConfig;

  baseConfig = ma_data_source_config_init();
  baseConfig.vtable = &g_ma_movie_audio_vtable;

  result = ma_data_source_init(&baseConfig, &p_ma_MovieAudio->base);
  if (result != MA_SUCCESS) return result;

  // TODO ensure this is properly initialised
  p_ma_MovieAudio->movie_audio = movie_audio;

  return MA_SUCCESS;
}

void ma_movie_audio_uninit(ma_movie_audio *p_ma_MovieAudio) {
  // TODO uninitialise/free MovieAudio here

  // You must uninitialize the base data source.
  ma_data_source_uninit(&p_ma_MovieAudio->base);
}

class EXPCL_MA_AUDIO MaAudioManager final : public AudioManager {
  friend class MaAudioSound;
  PT(ma_resource_manager) get_resource_manager();
  // protects access to audio manager fields in multithreaded user applications
  static ReMutex _lock;

  ma_device _device;
  int _active_managers;
  bool _ma_active;
  bool _is_valid;
  int _cache_limit;
  PN_stdfload _volume;
  PN_stdfloat _play_rate;
  bool _cleanup_required;

  ma_resource_manager_config _resource_mgr_conf;
  ma_resource_manager _resource_mgr;
  ma_engine _audio_engine;
  unsigned int _concurrent_sound_limit;

  typedef pset<OpenALAudioManager *> Managers;
  static Managers *_managers;

  /*
   * On loading any sound, a source is found or made for the given
   * filename. Although MiniAudio handles redundant loads, this struct
   * avoids tracking which sounds use which sources when we limit the
   * cache with the Panda3D API.
   */
  typedef struct DataSource {
    Filename file_name;
    bool cached;
    unsigned int refcount;
    unsigned int active_sounds;
    ma_resource_manager_data_source data_src;
  } DataSource;

  // source data handles
  phash_map<std::string, DataSource> _data_sources;
  unsigned int _num_sources_cached;
  // track age of cached sources for when cache is full with order
  // TODO ordered list?
  phash_map<std::string, DataSource *> _cached_sources;
  // This holds pointers to ma_data_sources available to uncache - these are
  //  held for a limited time after stopping in case of re-use
  pdeque<DataSource *> _expiring_sources;
  // This is where AudioSounds are stored in memory
  pdeque<MaAudioSound> _all_sounds;
  // This array contains pointers to playing sounds
  std::array<MaAudioSound *, _concurrent_sound_limit> _sounds_playing;

  PN_stdfloat _distance_factor;
  PN_stdfloat _doppler_factor;
  PN_stdfloat _drop_off_factor;

  ma_vec3 l_pos;
  ma_vec3 l_vel;
  ma_vec3 l_fwd;
  ma_vec3 l_up;

public:
  MaAudioManager();
  virtual ~MaAudioManager();

  virtual int get_speaker_setup();
  void set_speaker_setup(SpeakerModeCategory cat);
  bool configure_filters(FilterProperties *config);

  virtual void shutdown();

  virtual bool is_valid();

  virtual PT(AudioSound) get_sound(const Filename &file_name, bool positional = false, int mode=SM_heuristic);
  virtual PT(AudioSound) get_sound(MovieAudio *source, bool positional = false, int mode=SM_heuristic);

  virtual void uncache_sound(const Filename &file_name);
  virtual void clear_cache();
  virtual void set_cache_limit(unsigned int count);
  virtual unsigned int get_cache_limit() const;

  virtual void set_volume(PN_stdfloat volume);
  virtual PN_stdfloat get_volume() const;

  virtual void set_active(bool flag);
  virtual bool get_active() const;

  virtual void set_concurrent_sound_limit(unsigned int limit = 0);
  virtual unsigned int get_concurrent_sound_limit() const;

  virtual void reduce_sounds_playing_to(unsigned int count);
  virtual void stop_all_sounds();
  virtual void update(); // *must* be called every frame

  virtual void audio_3d_set_listener_attributes(
    PN_stdfloat px, PN_stdfloat py, PN_stdfloat pz,
    PN_stdfloat vx, PN_stdfloat vy, PN_stdfloat vz,
    PN_stdfloat fx, PN_stdfloat fy, PN_stdfloat fz,
    PN_stdfloat ux, PN_stdfloat uy, PN_stdfloat uz
  );

  virtual void audio_3d_get_listener_attributes(
    PN_stdfloat *px, PN_stdfloat *py, PN_stdfloat *pz,
    PN_stdfloat *vx, PN_stdfloat *vy, PN_stdfloat *vz,
    PN_stdfloat *fx, PN_stdfloat *fy, PN_stdfloat *fz,
    PN_stdfloat *ux, PN_stdfloat *uy, PN_stdfloat *uz
  );

  virtual void audio_3d_set_distance_factor(PN_stdfloat factor);
  virtual PN_stdfloat audio_3d_get_distance_factor() const;

  virtual void audio_3d_set_doppler_factor(PN_stdfloat factor);
  virtual PN_stdfloat audio_3d_get_doppler_factor() const;

  virtual void audio_3d_set_drop_off_factor(PN_stdfloat factor);
  virtual PN_stdfloat audio_3d_get_drop_off_factor() const;

  // For Panda3D's pointer system
 public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    AudioManager::init_type();
    register_type(_type_handle, "MaAudioManager", AudioManager::get_class_type());
  }
  virtual TypeHandle get_type() const {
    return get_class_type();
  }
  virtual TypeHandle force_init_type() {
    init_type();
    return get_class_type();
  }

 private:
  static TypeHandle _type_handle;

};

EXPCL_MA_AUDIO AudioManager *Create_MaAudioManager();

#endif /* MINIAUDIOMANAGER_H */
