/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file maAudioSound.h
 * @author Katie <katherineegoss@gmail.com> & J0y
 * @date 2026-06-02
 */

#ifndef MAAUDIOSOUND_H
#define MAAUDIOSOUND_H

#include "pandabase.h"

#include "audioSound.h"
#include "maAudioManager.h"

#include "miniaudio.h"

class EXPCL_MA_AUDIO MaAudioSound final : public AudioSound {
  friend class MaAudioManager;

  MaAudioSound(MaAudioManager *manager,
               ma_data_source *data_src,
               Filename &file_name,
               bool positional,
               int mode);
  MaAudioSound(const MaAudioSound &copy_sound);
  void cleanup();

  bool            _valid;

  MaAudioManager *_manager;
  ma_sound        _ma_sound;
  int             _ma_flags;

  // iterator holding the manager's reference to the sound
  pdeque<WPT(AudioSound)>::iterator _manager_it;

  PN_stdfloat     _volume; // 0..1.0
  PN_stdfloat     _balance; // -1..1
  PN_stdfloat     _play_rate; // 0..1.0

  LVector3        _location;
  LVector3        _velocity;
  LVector3        _direction;

  PN_stdfloat     _min_dist;
  PN_stdfloat     _max_dist;
  PN_stdfloat     _drop_off_factor;

  double          _length;
  bool            _loop;
  int             _loop_count;
  PN_stdfloat     _loop_start;
  int             _loops_completed;

  // MiniAudio callback at end of play
  auto _end_cb;

  int             _desired_mode;

  // The start_time field affects the next call to play.
  double          _start_time;

  // This is the string that throw_event() will throw when the sound finishes
  //  playing.  It is not triggered when the sound is stopped with stop().
  std::string     _finished_event;

  Filename        _basename;

  // _active is for things like a 'turn off sound effects' in a preferences
  //  panel.  _active is not about whether a sound is currently playing.  Use
  //  status() for info on whether the sound is playing.
  bool            _active;
  bool            _paused;

  /* These settings are used to define a directional sound source. The
   *  inner angle defines a cone wherein the sound can be heard at normal
   *  volume. _cone_outer_angle defines a second cone. Between the inner
   *  and the outer cone the volume is attenuated.  _cone_outer_gain is a
   *  factor applied to the volume setting to define the volume in the
   *  zone outside of the outer cone.
   */
  PN_stdfloat     _cone_inner_angle;
  PN_stdfloat     _cone_outer_angle;
  PN_stdfloat     _cone_outer_gain;

  vector_string   _comment;

  INLINE bool is_valid() const;

public:
  ~MaAudioSound();

  void play();
  void stop();

  void uncache();

  // loop: false = play once; true = play forever.  inits to false.
  void set_loop(bool loop=true);
  bool get_loop() const;

  // loop_count: 0 = forever; 1 = play once; n = play n times.  inits to 1.
  void set_loop_count(unsigned long loop_count=1);
  unsigned long get_loop_count() const;

  // loop_start: 0 = beginning.  expressed in seconds.  inits to 0.
  void set_loop_start(PN_stdfloat loop_start=0);
  PN_stdfloat get_loop_start() const;

  // increment the _loops_completed counter and check if limit reached
  bool loop_completed();

  // 0 = beginning; length() = end.  inits to 0.0.
  void set_time(PN_stdfloat time=0.0);
  PN_stdfloat get_time() const;

  // 0 = minimum; 1.0 = maximum.  inits to 1.0.
  void set_volume(PN_stdfloat volume=1.0);
  PN_stdfloat get_volume() const;

  // -1.0 is hard left 0.0 is centered 1.0 is hard right inits to 0.0.
  void set_balance(PN_stdfloat balance_right=0.0);
  PN_stdfloat get_balance() const;

  // play_rate is any positive float value.  inits to 1.0.
  void set_play_rate(PN_stdfloat play_rate=1.0f);
  PN_stdfloat get_play_rate() const;

  // Inits to manager's state.
  void set_active(bool active=true);
  bool get_active() const;

  // This is the string that throw_event() will throw when the sound finishes
  //  playing.  It is not triggered when the sound is stopped with stop().
  void set_finished_event(std::string event);
  const std::string &get_finished_event() const;

  const std::string &get_name() const;

  // return: playing time in seconds.
  PN_stdfloat length() const;

  // Controls the position of this sound's emitter.  pos is a pointer to an
  //  xyz triplet of the emitter's position.  vel is a pointer to an xyz
  //  triplet of the emitter's velocity.
  void set_3d_attributes(
      PN_stdfloat px, PN_stdfloat py, PN_stdfloat pz,
      PN_stdfloat vx, PN_stdfloat vy, PN_stdfloat vz);
  void get_3d_attributes(
      PN_stdfloat *px, PN_stdfloat *py, PN_stdfloat *pz,
      PN_stdfloat *vx, PN_stdfloat *vy, PN_stdfloat *vz);

  // Controls the direction of this sound emitter.
  void set_3d_direction(LVector3 d);
  LVector3 get_3d_direction() const;

  void set_3d_min_distance(PN_stdfloat dist);
  PN_stdfloat get_3d_min_distance() const;

  void set_3d_max_distance(PN_stdfloat dist);
  PN_stdfloat get_3d_max_distance() const;

  void set_3d_drop_off_factor(PN_stdfloat factor);
  PN_stdfloat get_3d_drop_off_factor() const;

  void set_3d_cone_inner_angle(PN_stdfloat angle);
  PN_stdfloat get_3d_cone_inner_angle() const;

  void set_3d_cone_outer_angle(PN_stdfloat angle);
  PN_stdfloat get_3d_cone_outer_angle() const;

  void set_3d_cone_outer_gain(PN_stdfloat gain);
  PN_stdfloat get_3d_cone_outer_gain() const;

  // Construct a near-identical copy of this object on the heap and return
  //  a refcounted pointer to the new copy
  virtual PT(AudioSound) make_copy() const;

  AudioSound::SoundStatus status() const;

  void finished();

  const vector_string& get_raw_comment() const;

  static TypeHandle get_class_type() {
      return _type_handle;
  }
  static void init_type() {
      AudioSound::init_type();
      register_type(_type_handle, "MaAudioSound",
          AudioSound::get_class_type());
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

#include "maAudioSound.I"

#endif /* __MAAUDIOSOUND_H__ */
