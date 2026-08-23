/* @author Katie
 * @date 2026-08-05
 */

#include "catch_amalgamated.hpp"

#include "miniAudioManager.h"

TEST_CASE("p3mini_audio creates a valid MiniAudioManager", "[audio]") {
  PT(AudioManager) test_man = AudioManager::create_AudioManager();
  REQUIRE(test_man->is_valid());
}

TEST_CASE("MiniAudioManager creates a valid MiniAudiosound", "[audio]") {
  PT(AudioManager) test_man = AudioManager::create_AudioManager();
  test_man->set_cache_limit(1);
  REQUIRE(test_man->get_cache_limit() == 1);
  test_man->set_concurrent_sound_limit(1);
  REQUIRE(test_man->get_concurrent_sound_limit() == 1);
  Filename sound_path = Filename::from_os_specific(
    std::string(__FILE__).substr(0, std::string(__FILE__).find_last_of("/"))
    + "/wav_test.wav");
  PT(AudioSound) test_sound = test_man->get_sound(sound_path, 0, 0);
  REQUIRE(test_sound->status() == AudioSound::READY);
}
