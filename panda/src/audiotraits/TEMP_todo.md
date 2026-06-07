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

## General notes
The 'cache' generally refers to the cache of all loaded AudioSounds, but some
methods (such as `uncache_sound`) refer to the expiration queue. We have both
`_cache_size` and `_concurrent_sound_limit`, the former of which refers to
AudioSounds in memory, the latter of which refers to active/playing sounds.

I'm not sure why we're `typedef`ing all the data structures in the OpenAL
implementation, and I'm trying out changing the header so that we just create
member variables for the data structures as the structure type. We can always
use the `auto` type if there's particularly unweildy types, and since we now
use C++17, we have class template argument deduction too.

Also, rdb said that the `plist` data type isn't very good and should be
avoided, so I'm just going to try using STL arrays. For this, we'll need to
make sure that AudioSounds (or the AudioManager) do not construct sounds if
the `_cache_size` has been reached, since they cannot be stored in the array.
We should instead return a null sound, to allow applications to hit the limit
without crashing, and we should provide debug output to indicate to developers
when they're maxing out their cache.

The expiration queue works as follows: when an AudioSound becomes inactive, it
downcounts the data source's refcount (TODO). If the refcount reaches 0, the
data source is added (by whom?) to the `_expiration_queue`. The update method
does periodic checks on the expiration queue: if a source has been there for x
time (how long? how do we check, sorting? sounds slow, isn't EBR better here?),
it is uncached from memory. Also, if the queue size is decreased to smaller than
the number of sources loaded, or the limit is reached, the oldest sound[s]
is/are uncached from memory.

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

