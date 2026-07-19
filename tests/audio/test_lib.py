import pytest
from panda3d.core import loadPrcFileData, AudioManager

def test_factory():
    mgr = AudioManager.create_AudioManager()
    assert "null" not in str(mgr).lower()

