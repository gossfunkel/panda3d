import os, sys
import pytest
from panda3d.core import Filename, MovieAudio

def test_get_sound(audiomgr):
    if "mini" not in str(audiomgr).lower():
        pytest.skip("Testing MiniAudio")
    sound_path = os.path.join(os.path.dirname(__file__), "wav_test.wav")
    sound_path = Filename.from_os_specific(sound_path)
    test_sound = MovieAudio.get(sound_path)
    assert str(test_sound) != 'Load-Failure Stub'
    test_sound = audiomgr.get_sound(test_sound)
    assert (str(test_sound).startswith("NullAudioSound") != True)
