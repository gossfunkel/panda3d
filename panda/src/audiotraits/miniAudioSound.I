INLINE bool MiniAudioSound::is_valid() const {
  if (_manager == nullptr) return null;
  return _valid;
}
