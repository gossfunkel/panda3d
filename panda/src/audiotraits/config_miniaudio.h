/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file config_miniAudio.h
 * @author Katie & J0y
 */

#ifndef CONFIG_MINIAUDIO_H
#define CONFIG_MINIAUDIO_H

#include "pandabase.h"

#include "notifyCategoryProxy.h"
#include "dconfig.h"
#include "audioManager.h"
#include "miniaudio.h"

ConfigureDecl(config_miniaudio, EXPCL_MINI_AUDIO, EXPTP_MINI_AUDIO);
NotifyCategoryDecl(miniaudio, EXPCL_MINI_AUDIO, EXPTP_MINI_AUDIO);

extern "C" EXPCL_MINI_AUDIO void init_libMiniAudio();
extern "C" EXPCL_MINI_AUDIO Create_AudioManager_proc *get_audio_manager_func_mini_audio();

extern ConfigVariableString miniaudio_device;

#endif // CONFIG_OPENALAUDIO_H
