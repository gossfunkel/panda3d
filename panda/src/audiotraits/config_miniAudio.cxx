/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file config_miniAudio.cxx
 * @author kate & joy
 */

#include "pandabase.h"

#include "config_miniAudio.h"
#include "miniAudioManager.h"
#include "miniAudioSound.h"
#include "pandaSystem.h"
#include "dconfig.h"

#if !defined(CPPPARSER) && !defined(LINK_ALL_STATIC) \
                        && !defined(BUILDING_MINI_AUDIO)
  #error Buildsystem error: BUILDING_MINI_AUDIO not defined
#endif

ConfigureDef(config_miniAudio);
NotifyCategoryDef(miniAudio, ":audio");

ConfigureFn(config_miniAudio) {
  init_libMiniAudio();
}

ConfigVariableBool disable_miniaudio
("disable_miniaudio", 0,
 PRC_DESC("Disable the MiniAudio backend. If OpenAL or FMOD are not enabled, "
   "the audio engine will be disabled."));

ConfigVariableString ma_device
("ma-device", "",
 PRC_DESC("Specify the MiniAudio device string for audio playback (no quotes). If this "
          "is not specified, the MiniAudio default device is used."));

/**
 * Initializes the library.  This must be called at least once before any of
 * the functions or classes in this library can be used.  Normally it will be
 * called by the static initializers and need not be called explicitly, but
 * special cases exist.
 */
void
init_libMiniAudio() {
  static bool initialized = false;
  if (initialized) {
    return;
  }

  initialized = true;
  MiniAudioManager::init_type();
  MiniAudioSound::init_type();

  AudioManager::register_AudioManager_creator(&Create_MiniAudioManager);

  PandaSystem *ps = PandaSystem::get_global_ptr();
  ps->add_system("MiniAudio");
  ps->add_system("audio");
  ps->set_system_tag("audio", "implementation", "MiniAudio");
}

/**
 * This function is called when the dynamic library is loaded; it should
 * return the Create_AudioManager function appropriate to create a
 * MiniAudioManager.
 */
Create_AudioManager_proc *
get_audio_manager_func_mini_audio() {
  init_libMiniAudio();
  return &Create_MiniAudioManager;
}
