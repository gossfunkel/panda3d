/* @author Katie
 * @date 2026-08-05
 */

#include "catch_amalgamated.hpp"

#include "config_miniaudio.h"

TEST_CASE("p3mini_audio initialises a new MiniAudioManager correctly", "[audio]") {
  init_libMiniAudio();
  AudioManager *test_man = get_audio_manager();
  REQUIRE(test_man->is_valid());
}
