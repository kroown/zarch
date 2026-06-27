# zarch

file archiver · linux · c++

<p align="center">
  <img src="https://skillicons.dev/icons?i=linux,cpp">
</p>

a linux file archiver with lzss + huffman compression, posix metadata preservation, and multi-threading.

## features

- lzss + canonical huffman compression pipeline
- bcj (x86 call/jump) and delta pre-filters
- posix metadata preservation (permissions, ownership, timestamps)
- multi-threaded compression and decompression
- stdin/stdout pipe mode
- custom binary .zarch format

## usage

```
zarch -c -f archive.zarch file1.txt dir/
zarch -c -f archive.zarch --filter delta largefile.bin
zarch -c -f archive.zarch --filter bcj program.exe
zarch -x -f archive.zarch
cat data.bin | zarch -c > out.zarch
zarch -c -v -t 4 -f out.zarch bigfile.dat
```

## build

```bash
git clone https://github.com/kroown/zarch.git
cd zarch
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

dependencies: zlib, pthreads (system libraries on any linux distribution).

## options

| flag | description |
|------|-------------|
| `-c`, `--create` | create archive |
| `-x`, `--extract` | extract archive |
| `-f`, `--file` | target .zarch file |
| `-v`, `--verbose` | verbose output |
| `-t`, `--threads` | worker threads |
| `--filter` | pre-processing filter: `bcj` or `delta` |
| `--help` | show help |

## format

the .zarch format is a custom binary layout:

```
+------------------+
| archive header   |  32 bytes (magic, version, filter type, etc.)
+------------------+
| block 0 prefix   |  12 bytes (compressed/uncompressed sizes, crc32)
| block 0 data     |  huffman table + huffman-coded lz stream
+------------------+
| block 1 prefix   |
| block 1 data     |
+------------------+
| ...              |
+------------------+
| footer           |  entry count + file entries (paths, metadata, block mapping)
+------------------+
```

compression pipeline: input → optional filter (bcj/delta) → lzss → huffman → bitstream.

## architecture

```
src/
  main.cpp           entry point, stdin mode
  cli.cpp/hpp        argument parsing
  archive.cpp/hpp    create/extract orchestration
  lz.cpp/hpp         lzss compressor/decompressor with hash-chain matching
  huffman.cpp/hpp    canonical huffman codec with serialized tables
  bitwriter.cpp/hpp  m-bit bit writer (msb-first)
  bitreader.cpp/hpp  bit reader with lazy refill
  filters.cpp/hpp    bcj (x86 call/jump) and delta pre-filters
  io.cpp/hpp         mmap-based file i/o
  metadata.cpp/hpp   posix metadata (permissions, ownership, timestamps)
  thread_pool.cpp/hpp  worker pool with ordered output
  format.hpp         binary struct definitions
```




