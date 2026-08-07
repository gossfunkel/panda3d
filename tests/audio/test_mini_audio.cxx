/* @author Katie
 * @date 2026-08-05
 */

#include "catch_amalgamated.hpp"

#include "miniAudioManager.h"

TEST_CASE("p3mini_audio initialises a new MiniAudioManager correctly", "[audio]") {
  PT(AudioManager) test_man = AudioManager::create_AudioManager();
  REQUIRE(test_man->is_valid());
}
