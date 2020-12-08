# fastsync
Sync utility between single device and parallel filesystem.

## Disclaimer
Please note that fastsync is provided as-is. No responsibility is taken for eventual data loss or any other problems which might occur when running the software.

## Intended Use
fastsync is intended to sync directories between two filesystems from which one or both might be parallel filesystems. Therefore, fastsync is able to read the source filesystem (which might be on a single disk or a RAID0 with high sequential but low random read speed) from a single thread while writing to the target filesystem (which might be a massively parallel filesystem like GlusterFS on many devices and disks) with multiple threads.

## What you can expect
fastsync will always expect at least two arguments: A source and a destination. Fastsync aims on making the destination similar to the source.
These are the things that fastsync can do:
* Copy single files
* Copy directories recursively
* Detect changes based on mtime (second precision), size, mode, uid and gid and copy only changed files
* Copy the file's timestamps, owner and mode
* Copy symlinks (as they are, i.e. character by character without any interpretation of the target)
* Remove filesystem objects from the destination which are not in the source

# What you cannot expect
fastsync does not do this:
* Detect changes based on checksums to decide that files must be copied. If you modify a file, keeping the same size and you reset the mtime, it won't be copied.
* Copying hardlinks
* Copy ACLs, extended file attributes or anything else that goes beyond old-fashioned UNIX attributes

# Building
First clone this repository and cd into the cloned folder. Then run:
```bash
mkdir build
cd build
cmake ../
make
```

# Using
The compiled binary can be used with
```./fastsync SOURCE DEST [#READERS [#WRITERS [CHUNK_SIZE_MB]]]```
* SOURCE is the source directory or file
* DEST is the destination directory or file which should be made similar to source
* #READERS is the number of reader threads
* #WRITERS is the number of writer threads
* CHUNK_SIZE_MB is the chunk size in MBs. If the used filesystem use sharding, set this to a multiple of the shard block size of all of them for best performance.
Note that the amount of memory needed is in the order of 2 * max(#READERS, #WRITERS) * CHUNK_SIZE_MB.

# Trying it out
You may use the test.sh file to create a test folder in the current working directory which has some simple test cases in it.
Run
```bash
./fastsync test/filein test/fileout
./fastsync test/linkin test/linkout
./fastsync test/dirin test/dirout
./fastsync test/dirfilledin test/dirfilledout
```
and check if a diff tool of your choice if the \*in and \*out elements are similar.

# Internals
fastsync creates a Job for every filesystem entity (file, directory, link) and splits it up into several tasks: Creating the entity, copying a chunk of data and writing the attributes. A user defined number of reader and writer modules can be spawned in separate threads which execute the tasks. The main thread schedules Tasks to the readers and then to the writers, recursively creates new Jobs and Tasks for directory contents and tracks dependencies such that directories are only finished (unnecessary files removed, attributes set) after all content has been copied.
