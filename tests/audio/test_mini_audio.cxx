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
  PT(AudioSound) test_sound = test_man->get_sound("wav_test.wav", 0, 0);
  REQUIRE(test_sound->status() == AudioSound::READY);
}
