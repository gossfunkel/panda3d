# TODO list:
### features and functionality:
- config option to set device (declared in the config files) should be defined somewhere?
- implement `MiniAudioManager::audio_3d_set_distance_factor()`
- fade in/out support
- exhaustively go through the supported DSP effects in FilterConfig and implement a mapping from config settings to MiniAudio nodes with equivalent effects applied.
- map out flow of `AudioSound`s to ensure none are orphaned
- establish whether re-activating sounds should unpause them (NOTE: is this required for looping? see Darktohka comment on github)
- windows build: does miniaudio require that we add some newer libs (like `winmm`) to `makepanda`?
### debugging, logging, and testing:
- debug macro guard in `MaAudioSound`?
- test: resource manager is shared across `MiniAudioManager`s
- test: cache works as intended for buffered `MiniAudioSound`s
- test: cache limit works as intended for buffered `MiniAudioSound`s
- test: 3D sound works to spec
- test: shutdown/cleanup works to spec
- test: fx
- double check over miniaudio.h 6.2.3. Data Streams to make sure it fits with expected behaviour
- test: `MiniAudioSound`s set to stream mode do not enter/leave the cache, or observe its limit
- set up config or logging for MiniAudio
- more error checking with `nassert`
- use pstats/gdb/vs to check for memory leaks, unexpected behaviour, or other runtime side effects and issues
- builds passing tests on ~windows~, mac, ~linux~, android, emscripten
- benchmarks with and without the `ReMutex`es (or other multithreading approach?) against single-threaded
### Documentation
- manual page for miniaudio (sphinx)
- reference pages, comments, inline documentation

# For a second PR (P3MiniAudio expanded):
- resampling support
- sound generation support (note: this is currently disabled via defines in `panda/src/audiotraits/miniaudio.c`)

# For a general audio update PR:
- keep a `const static PT(NullAudioSound)` instead of creating many objects

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
- miniaudio has a built-in clipping function that can be _disabled_ with the device `noClip` option

## General notes
In the OpenAL implementation, each AudioSound is a 'client' of a SoundData
source. SoundDatas that have no clients are moved to an expiration queue. This
queue is what `uncache_sound()`, `clear_cache()`, and the `cache_limit`
methods act on, so this is our 'cache' in those cases. The
`_concurrent_sound_limit` refers to active/playing sounds, not actual audio
data cached in memory.

I don't like `typedef`ing all the data structures in the OpenAL implementation;
the MaAudioManager cache is significantly simplified. As we now use C++17, we
have class template argument deduction and the `auto` keyword for easy
iterator procurement.

Also, rdb said that the `plist` data type isn't very good and should be
avoided, so I'm using `pset`s, `pdeque`s, and `pmap`s. For this, we'll need to
make sure that AudioSounds (or the AudioManager) do not construct sounds if
the `_cache_size` has been reached, since they cannot be stored in the array.
We should instead return a null sound, to allow applications to hit the limit
without crashing, and we should provide debug output to indicate to developers
when they're maxing out their cache.

Giving users pointers to sounds means that our cache could invalidate pointers
to users' sounds when clearing (e.g. per a user calling `set_cache_size()`
lower than the number of cached sounds, or calling `uncache_sound`). This is
why `MaAudioSounds` now have a `cache` and `uncache` method in order to
manually call `init()` and `uninit()` on their `ma_sound`s. This incurs a
runtime cost to check if a sound is initialised at runtime, but it prevents
Python objects from breaking after using `uncache_sound` in some situations.
Adjusting the pointer format for null sounds could have wider effects and
will have to go in a future PR.

We have elected to format the internal backend names in the style
`miniAudio*`/`[*_]mini_audio[_*]`. This optimises for internal consistency
along with clarity, and has a minimal character cost (2 more than `ma`).

#### Other tools to note / concepts involved
- cmake
- makepanda
- Catch2
- Pytest
- `nassertv` - assert a condition
- `ReMutexHolder` - mutex
- `pmap` - fast lookup table (hashmap)
- `pset` - fast set
- `friend` classes
- `iterator`s and C++ containers
- inheritance (`protected`, `virtual`, and `static` methods/members, and the `final` keyword)
- heap allocated garbage collection using `PT()`s; memory management in general

