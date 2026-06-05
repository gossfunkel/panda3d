#include "maAudioManager.h"

#define check_ma(result, failcond, outstr) if ((result) != MA_SUCCESS) {  \
  (failcond);                                                             \
  std::cerr << (outstr) << std::endl;                                   \
  return NULL;                                                            \
}

MaAudioManager::
MaAudioManager() {
  // lock mutex

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

    //ma_resource_manager_config _resource_mgr_conf; TODO member variable
    //ma_resource_manager _resource_mgr; TODO member variable
    resource_mgr_conf = ma_resource_manager_config_init();
    resource_mgr_conf.decodedFormat = _device.playback.format;
    resource_mgr_conf.decodedChannels = _device.playback.channels;
    resource_mgr_conf.decodedSampleRate = _device.sampleRate;
    //resource_mgr_conf.jobThreadCount = 2;
    //resource_mgr_conf.pVFS = _vfs;
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

~MaAudioManager() {
  ma_device_uninit(&_device);
  ma_engine_uninit(&_audio_engine);
}
