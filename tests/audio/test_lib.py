import pytest
from panda3d.core import loadPrcFileData, AudioManager

def test_null():
    mgr = AudioManager.create_AudioManager()
    assert "null" in str(mgr).lower()

def test_miniaudio():
    loadPrcFileData('', 'audio-library-name p3mini_audio')
    mgr = AudioManager.create_AudioManager()
    assert "mini" in str(mgr).lower()

def test_openal():
    loadPrcFileData('', 'audio-library-name p3openal_audio')
    mgr = AudioManager.create_AudioManager()
    assert "openal" in str(mgr).lower()

def test_fmod():
    loadPrcFileData('', 'audio-library-name p3fmod_audio')
    mgr = AudioManager.create_AudioManager()
    assert "fmod" in str(mgr).lower()

