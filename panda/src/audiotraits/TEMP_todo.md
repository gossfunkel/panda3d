# notes from MiniAudio header
- resource manager uses refcounts to keep sources in memory until all sounds `uninit()`ed. Expiring sounds: what if we just keep a reference to a sound? checking this list seems more expensive than just sometimes reloading a sound, honestly. I think we should ditch it.
- resource manager handles data sources itself; `init()`ing a sound with a source skips the manager.
- `ma_sound_init_ex` offers most flexibility; `ma_sound` has a config with a `pFileName` and `pDataSource`.
- `ma_sound_init_from_file_internal` mallocs out a data source on the heap.
- buffered sounds (i.e. sounds not streamed) can be cloned with `ma_sound_init_copy`.
- implements a BST with a hashmap like I was going to do with `MaAudioManager::_data_sources`.
- MiniAudio uses a fixed-size MPMC job queue, optionally multithreaded. If threads > 1, uses a spinlock.
- streams loop over the source in 2s chunks (two 'pages'), reading PCM frames to a buffer. Better for large files (e.g. soundtracks)
- both streams and buffered/async sounds use the job queue.
- engine wraps a node graph (as well as the resource manager). Multiple features of nodes (like loading fx, mixing, setting start and stop times) could be useful, but could incur locking on the audio thread. Node chains are linked lists.
- wav, mp3, and flac decoding built in. vorbis decoder extensibility described (requires vtable override).
- rich format, data, and channel conversion/mapping options; see also the `ma_format` enum.
- `ma_audio_buffer` API for interfacing with `MovieAudioCursor`s?
- disable pitch and doppler by default until set as non-default value for the first time for performance improvement
- set engine & resource manager sample rate to match first sound and leave unchanged to prevent conversions as sounds are added until a sound is added that doesn't have a matching rate; then disable engine rate.
- `ma_data_source` is a *very* open API (it's just a `typedef void`)!

# general notes
MiniAudio library files:
- might need to make an empty header for interrogate?
- might need to change the ma implementation file to a .cxx/.cpp

MaAudioManager:
- inline error checking function (fn ptr as parameter)
- remaining virtual methods: update, speaker_setup, get/set concurrent_sound_limit
- distance attenuation?

MaAudioSound:
- all property methods (active, volume, time, length, play rate, balance, loop count/pos)
- play and stop methods (buffering and updates, cache)
- 3d audio properties

Reference counting:
Since MiniAudio handles its own refcounts and garbage collection of data
sources, our options for manually keeping 'expiring sources' loaded are
limited. We could:
1) ditch the expiring sources feature
1) override or sidestep the high-level MiniAudio API (resource management, nodes, engine)
1) hide the `ma_sound`s and `ma_data_source`s behind getters which dispense them based on a cache status. The getter would access a list of cached sounds with open references (i.e. `init()`ed `ma_data_source` objects) which is trimmed by the update method. Update pops the oldest member with no other refs, getter never pops and only refreshes counts.

## Cacheing sounds
Load new sound with source file:
1) check if the file is cached and sets `_cached` appropriately in `get_sound()`
1) if it is to be loaded active, `init()`s the `ma_data_source`
1) update the cache
1) construct an `AudioSound` with the new `DataSource`
1) return a `PT()` to the caller of `get_sound()`

Load new sound with new MovieAudio:
1) check if the `MovieAudio` is cached and sets `_cached` appropriately in `get_sound()`
1) if it is to be loaded active, `init()`s the `ma_movie_audio_data_source`
1) update the cache
1) construct an `AudioSound` with the new `DataSource`
1) return a `PT()` to the caller of `get_sound()`

Load sound with already-used source file:
1) check if the file is cached and sets `_cached` appropriately in `get_sound()`
1) finding the `DataSound` for the file, update its refcount and active/expiring status
1) construct an `AudioSound` with the existing `DataSource`
1) return a `PT()` to the caller of `get_sound()`

Delete sound using unique source:
TODO

Call uncache_sound on valid target:
TODO

Call uncache_sound on invalid target:
TODO

## General notes
I finally read the docs on PointerTo()! PT() does garbage collection, i.e. it
establishes a refcount and frees an object when the refcount reaches 0. These
can help us out a little, but we also need to be careful not to use them when
we should be using regular pointers, as they could result in something being
removed from memory when we don't expect it to in odd edge cases.

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

Once I've got the caches figured out, I can write up a basic constructor/
destructor test for the AudioManager. The first test will be the hardest to
write (because I've got to get my head around PyTest and the P3D test
infrastructure again heh), but then it should be easier to add new tests for
methods and specific use-cases/edge-cases/error-cases.

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

