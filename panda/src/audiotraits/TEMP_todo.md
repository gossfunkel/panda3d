MiniAudio library files:
- might need to make an empty header for interrogate?
- might need to change the ma implementation file to a .cxx/.cpp

MaAudioManager:
- cache: playing sounds and expiring sounds
- inline error checking function (fn ptr as parameter)
- remaining virtual methods: update, speaker_setup, get/set concurrent_sound_limit
- distance attenuation?

MaAudioSound:
- all property methods (active, volume, time, length, play rate, balance, loop count/pos)
- play and stop methods
- fix/finish copy constructor
- 3d audio properties
- Filename
- TypeHandle

### Crucial engine tools
- `MovieAudio` objects have refcounts.
- `SoundsPlaying::iterator` to help allocate and deallocate sounds safely.
- `ExpirationQueue::iterator` to cache recently stopped sounds (don't use `plist`; `phash_map` is good)

#### Other tools to note / concepts involved
- `nassertv` - assert a condition
- `ReMutexHolder` - mutex
- `phash_map<std::string, SoundData *>` - fast table (hashmap) of sample data
- `phash_set<PT(MaAudioSound)>` - fast set of sounds
- `pset<MaAudioManager *>` - fast set of audio managers
- `plist<void *>` - fast list of pointers
- `vector_string` -
- `friend` classes
- `iterator`s
- inheritance (`protected`, `virtual`, and `static` methods/members, and the `final` keyword)
- heap allocated garbage collection, memory management in general

