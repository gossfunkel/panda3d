/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file maAudioManager.h
 * @author Katie <katherineegoss@gmail.com> & J0y
 * @date 2026-06-02
 */


#ifndef MINIAUDIOMANAGER_H
#define MINIAUDIOMANAGER_H

#include "pandabase.h"

#include "audioManager.h"
#include "pdeque.h"
#include "pmap.h"
#include "pset.h"
//#include "reMutex.h"
#include "vector_string.h"
#include "config_audio.h"
#include "config_putil.h"
#include "config_express.h"
#include "config_openalAudio.h"

#include "miniaudio.h"

class MaAudioSound;

class EXPCL_MA_AUDIO MaAudioManager final : public AudioManager {
  friend class MaAudioSound;
  // TODO benchmarks with and without mutexes
  //static ReMutex _lock;

  int _active_managers;
  bool _active;
  bool _is_valid;
  int _cache_limit;
  PN_stdfloat _volume;
  PN_stdfloat _play_rate;

  // MiniAudio high-level interface objects
  ma_device _device;
  ma_resource_manager_config _resource_mgr_conf;
  ma_resource_manager _resource_mgr;
  ma_engine _engine;

  // TODO if we don't use mutexes, these should probably all
  //  be made atomic/thread-safe in some other way (smart queues)

  // set of all managers
  static pset<MaAudioManager *> _managers;

  // deque of cached AudioSounds in this manager
  pdeque<WPT(AudioSound)> _all_sounds;
  // counting number of sounds referencing cached sources
  pmap<Filename, int>_cache_counts;
  // MiniAudio node containing all sounds
  ma_sound _all_sounds_grp;
  // maximum playing sounds
  unsigned int _concurrent_sound_limit;
  // Counter for playing sounds
  unsigned int _num_concurrent_sounds;

  PN_stdfloat _distance_factor;
  PN_stdfloat _doppler_factor;
  PN_stdfloat _drop_off_factor;

  LVector3 l_pos;
  LVector3 l_vel;
  LVector3 l_fwd;
  LVector3 l_up;

public:
  MaAudioManager();
  virtual ~MaAudioManager();

  virtual int get_speaker_setup();
  void set_speaker_setup(SpeakerModeCategory cat);
  bool configure_filters(FilterProperties *config);

  virtual void shutdown();

  virtual bool is_valid();

  virtual PT(AudioSound) get_sound(const Filename &file_name, bool positional = false, int mode=SM_heuristic);
  virtual PT(AudioSound) get_sound(MovieAudio &source, bool positional = false, int mode=SM_heuristic);

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
