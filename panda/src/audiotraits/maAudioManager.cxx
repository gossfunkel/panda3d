#include "maAudioManager.h"

using std::string;

TypeHandle MaAudioManager::_type_handle;

ReMutex MaAudioManager::_lock;
int MaAudioManager::_active_managers = 0;
bool MaAudioManager::_ma_active = false;
ma_device *MaAudioManager::_device = nullptr;
ma_resource_manager_config MaAudioManager::_resource_mgr_conf;
ma_resource_manager MaAudioManager::_resource_mgr;

#define check_ma(result, failcond, outstr) if ((result) != MA_SUCCESS) {  \
  (failcond); audio_error(outstr); return NULL; }

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

  //ma_engine _audio_engine; TODO member variable
  ma_engine_config audio_engine_conf;
  audio_engine_conf = ma_engine_config_init();
  audio_engine_conf.pResourceManager = &_resource_mgr;
  audio_engine_conf.noAutoStart = MA_TRUE;
  check_ma(
    ma_engine_init(&audio_engine_conf, &_audio_engine),
    ma_device_uninit(&_device),
    "Failed to initialise MiniAudio engine."
  );

  ma_engine_listener_set_position(&audio_engine, 0, 0, 0, 0);
  ma_engine_listener_set_direction(&audio_engine, 0, 0, 0, 0);

  // we'll do this when p3d is ready for it, or remove the noAutoStart line
  ma_engine_start(&audio_engine);
}

/**
 * This is what creates a sound instance.
 */
PT(AudioSound) MaAudioManager::
get_sound(const Filename &file_name, bool positional, int mode) {
  return new MaAudioSound(this, file_name, positional, mode);
}

PT(ma_resource_manager) MaAudioManager::
get_resource_manager() {
  return PT(_resource_mgr);
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

void MaAudioManager::
cleanup() {
  // TODO do we need to uninit all AudioSounds?
  ma_device_uninit(&_device);
  ma_engine_uninit(&_audio_engine);
}
