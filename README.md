# zarch

a linux file archiver with lzss + huffman compression and posix metadata support.

## build

```bash
# clone
git clone https://github.com/kroown/zarch.git
cd zarch

# configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# build
cmake --build build
```

dependencies: `zlib`, `pthreads` (system libraries on any linux distribution).

## usage

```bash
# compress files/directories
./build/zarch -c -f archive.zarch file1.txt dir/

# compress with a pre-filter
./build/zarch -c -f archive.zarch --filter delta largefile.bin
./build/zarch -c -f archive.zarch --filter bcj program.exe

# extract
./build/zarch -x -f archive.zarch

# stdin pipe mode
cat data.bin | ./build/zarch -c > out.zarch

# verbose / thread control
./build/zarch -c -v -t 4 -f out.zarch bigfile.dat

# help
./build/zarch --help
```

## options

| flag | description |
|------|-------------|
| `-c`, `--create` | create archive |
| `-x`, `--extract` | extract archive |
| `-f`, `--file` | target .zarch file |
| `-v`, `--verbose` | verbose output |
| `-t`, `--threads` | worker threads (default: all cores) |
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
