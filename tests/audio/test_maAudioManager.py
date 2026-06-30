import os

import pytest

def test_create_shutdown():
    test_mgr = AudioManager.create_AudioManager()
    assert !(str(test_mgr).startswith("NullAudioManager"))
    test_mgr.shutdown()
    # TODO does this work with python objects - will test_mgr be None after destruction?
    assert test_mgr == None

