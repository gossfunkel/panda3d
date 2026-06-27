import os
import pytest
from time import sleep

from panda3d.core import AudioManager

def test_create_shutdown():
    test_mgr = AudioManager.create_AudioManager()
    # give the manager a moment to run its update method a few times
<<<<<<< HEAD
    # await(1) # i don't think that functions exist in python
    sleep(1)
    assert not str(test_mgr).startswith("NullAudioManager")
=======
    # TODO asyncio or sequence/interval?
    await(1)
    assert !(str(test_mgr).startswith("NullAudioManager"))
>>>>>>> origin/ma_backend
    test_mgr.shutdown()
    # TODO does this work with python objects - will test_mgr be None after destruction?
    assert test_mgr == None

