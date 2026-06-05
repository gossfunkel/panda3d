#include "maAudioSound.h"

MaAudioSound::
MaAudioSound(MaAudioManager *manager,
             Filename file_name,
             bool positional,
             int mode) :
  AudioSound(positional),
  //_movie(movie),
  _sd(nullptr),
  _playing_loops(0),
  _playing_rate(0.0),
  _loops_completed(0),
  _source(0),
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
  //_basename(movie->get_filename().get_basename()),
  _basename(file_name.get_basename()),
  _active(manager->get_active()),
  _paused(false),
  _cone_inner_angle(360.0f),
  _cone_outer_angle(360.0f),
  _cone_outer_gain(0.0f)
{

}
