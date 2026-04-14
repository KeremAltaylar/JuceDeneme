# Audio Safety Rules (Always Apply)

## Realtime Audio Thread: `processBlock`

Inside `processBlock` (and anything called by it on the audio thread):

- Do not allocate or free memory: no `new`, `delete`, `malloc`, `free`, `std::make_*`, `std::vector::push_back` growth, `juce::String` building, or any operation that can allocate.
- Do not do any file I/O: no `juce::File`, `FileInputStream`, `FileOutputStream`, `std::fstream`, `std::filesystem`, or reading/writing files.
- Do not do any console I/O or logging: no `printf`, `std::cout`, `DBG`, `Logger`, or similar.
- Do not block: no locks that can block, no waiting, no thread joins.

All buffers and scratch memory must be pre-allocated in `prepareToPlay`.

