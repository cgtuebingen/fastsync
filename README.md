# fastsync
Sync utility between single device and parallel filesystem.

## Intended Use
fastsync is intended to sync directories between two filesystems from which one or both might be parallel filesystems. Therefore, fastsync is able to read the source filesystem (which might be on a RAID0 with high sequential but low random read speed) from a single thread while writing to the target filesystem (which might be a massively parallel filesystem like GlusterFS on many devices and disks) with multiple threads.

## What you can expect
fastsync will always expect at least two arguments: A source and a destination. Fastsync aims on making the destination similar to the source.
These are the things that fastsync can do:
* Copy single files
* Copy directories recursively
* Detect changes based on mtime (second precision), size, mode, uid and gid and copy only changed files
* Copy the file's timestamps, owner and mode
* Copy symlinks (as they are, i.e. character by character without any interpretation of the target)

# What you cannot expect
fastsync does not do this:
* Detect changes based on checksums to decide that files must be copied. If you modify a file, keeping the same size and you reset the mtime, it won't be copied.
* Copying hardlinks
* Copy ACLs, extended file attributes or anything else that goes beyond old-fashioned UNIX attributes

# Compilation
First clone this repository and cd into the cloned folder. Then run:
```bash
mkdir build
cd build
cmake ../
make
```

# Usage
The compiled binary can be used with
```./fastsync SOURCE DEST [#READERS [#WRITERS [CHUNK_SIZE_MB]]]```
* SOURCE is the source directory or file
* DEST is the destination directory or file which should be made similar to source
* CHUNK_SIZE_MB is the chunk size in MBs. If the used filesystem use sharding, set this to a multiple of the shard block size of all of them for best performance.
Note that the amount of memory needed is in the order of 2 * max(#READERS, #WRITERS) * CHUNK_SIZE_MB.