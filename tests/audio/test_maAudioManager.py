import asyncio
import os
import pytest

<<<<<<< HEAD
from panda3d.core import AudioManager
=======
def test_create_shutdown():
    test_mgr = AudioManager.create_AudioManager()
    assert !(str(test_mgr).startswith("NullAudioManager"))
    test_mgr.shutdown()
    # TODO does this work with python objects - will test_mgr be None after destruction?
    assert test_mgr == None
>>>>>>> origin/ma_backend


async def test_create_shutdown(mgr):
    # give the manager a moment to run its update method a few times
    await asyncio.sleep(1)
    assert not str(mgr).startswith("NullAudioManager")
    mgr.shutdown()
    # TODO does this work with python objects - will mgr be None after destruction?
    assert mgr == None

def test_audio_manager_creation():
    test_mgr = AudioManager.create_AudioManager()
    asyncio.run(test_create_shutdown(test_mgr))