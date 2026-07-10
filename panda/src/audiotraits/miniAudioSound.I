INLINE bool MiniAudioSound::is_valid() const {
  if (_manager == nullptr) return false;
  return _valid;
}
