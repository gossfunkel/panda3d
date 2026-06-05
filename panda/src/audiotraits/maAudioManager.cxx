#include "maAudioManager.h"

#define check_ma(result, failcond, outstr) if ((result) != MA_SUCCESS) {  \
  (failcond); audio_error(outstr); return NULL; }

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
  //int flags = 0; load to ram without decoding
  // TODO put data_src in MaAudioSound for refcount and destructor -
  //      pass the constructor a PT(_resource_mgr)?
  ma_resource_manager_data_source data_src;
  //int flags = (loop_sound) ? MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING : 0;
  int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM; // decode in 1s pages
  //int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE; // decode to ram
  //int flags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC; load to ram later
  check_ma(ma_resource_manager_data_source_init(_resource_mgr,
        file_name.get_fullpath(), flags, &data_src));
  // TODO put in ~MaAudioSound()
  ma_resource_manager_data_source_uninit(&data_src);
}

~MaAudioManager() {
  ma_device_uninit(&_device);
  ma_engine_uninit(&_audio_engine);
}
