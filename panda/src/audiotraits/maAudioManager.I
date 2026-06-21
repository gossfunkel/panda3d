
/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file maAudioManager.I
 * @author Kate Goss <katherineegoss@gmail.com>
 */

// original macro design
//#define check_ma(result, failcond, outstr) if ((result) != MA_SUCCESS) {  \
//  (failcond); audio_error(outstr); return nullptr; }

/*
 * General error-checker for MiniAudio with callback on failure.
 */
INLINE bool check_ma(
    bool result,
    void (*func)(void)fail_cb,
    void *params,
    std::string out_str) {
  bool failed = (result != MA_SUCCESS);
  if (failed) {
    if (params != nullptr) fail_cb(*params);
    else fail_cb();
    audio_error(out_str);
  }
  return failed;
}

/*
 * General error-checker for MiniAudio.
 */
INLINE bool check_ma(
    bool result,
    std::string out_str) {
  if (result != MA_SUCCESS) {
    audio_error(out_str);
    return true;
  }
  return false;
}
