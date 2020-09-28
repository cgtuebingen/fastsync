#include "ModWriter.h"

#include "Job.h"
#include "Task.h"
#include "ThreadsafeBuffer.h"

#include <sys/stat.h>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>

using namespace std;

extern size_t chunkSize;

inline bool operator==(const timespec &t1, const timespec &t2) {
	if (t1.tv_sec != t2.tv_sec)
		return false;
	if (t1.tv_nsec != t2.tv_nsec)
		return false;
	return true;
}

inline bool operator!=(const timespec &t1, const timespec &t2) {
	return !(t1 == t2);
}

void ModWriter::run() {
	// Read elements from the input and put them to the output
	while (!stop) {
		Task *task = In->PopFront();

		if (task->Type == Task::TaskType::INIT) {
			// Get stat of what is already there
			lstat(task->ItsJob->DestPath.c_str(), &task->ItsJob->DestStat);
			if (S_ISREG(task->ItsJob->SourceStat.st_mode)) {
				// Check if wrong output has to be deleted
				if (task->ItsJob->DestStat.st_ino
						!= 0&& !S_ISREG(task->ItsJob->DestStat.st_mode)) {
					std::error_code ec;
					filesystem::remove_all(task->ItsJob->DestPath, ec);
					if (ec.value() != 0)
						task->ItsJob->Log.ErrorDeleteOld = true;
					// Update stat
					lstat(task->ItsJob->DestPath.c_str(),
							&task->ItsJob->DestStat);
				}
				// Check if output has to be updated
				if (S_ISREG(task->ItsJob->DestStat.st_mode)
						&& (task->ItsJob->DestStat.st_size
								!= task->ItsJob->SourceStat.st_size
								|| task->ItsJob->DestStat.st_mtim
										!= task->ItsJob->SourceStat.st_mtim)) {
					ofstream fout(task->ItsJob->DestPath.c_str(),
							ios_base::binary);
					if (task->ItsJob->SourceStat.st_size > 0) {
						fout.seekp(task->ItsJob->SourceStat.st_size - 1);
						fout.write("", 1);
					}
					task->ItsJob->Log.ErrorCreateDest = !fout.good();
					fout.close();
				}
			} else if (S_ISDIR(task->ItsJob->SourceStat.st_mode)) {
				// Check if wrong output has to be deleted
				if (task->ItsJob->DestStat.st_ino != 0
						&& (!S_ISDIR(
								task->ItsJob->DestStat.st_mode
										|| task->ItsJob->SourcePath.filename()
												!= task->ItsJob->DestPath.filename()))) {
					std::error_code ec;
					filesystem::remove_all(task->ItsJob->DestPath, ec);
					if (ec.value() != 0)
						task->ItsJob->Log.ErrorDeleteOld = true;
					// Update stat
					lstat(task->ItsJob->DestPath.c_str(),
							&task->ItsJob->DestStat);
				}
				if (!S_ISDIR(task->ItsJob->DestStat.st_mode)
						|| task->ItsJob->SourcePath.filename()
								!= task->ItsJob->DestPath.filename()) {
					task->ItsJob->Log.ErrorCreateDest = mkdir(
							task->ItsJob->DestPath.c_str(),
							task->ItsJob->SourceStat.st_mode) != 0;
				}
			} else if (S_ISLNK(task->ItsJob->SourceStat.st_mode)) {
				// Check if wrong output has to be deleted
				if (task->ItsJob->DestStat.st_ino != 0
						&& (!S_ISLNK(
								task->ItsJob->DestStat.st_mode
										|| task->ItsJob->SourcePath.filename()
												!= task->ItsJob->DestPath.filename()
										|| task->ItsJob->SourceStat.st_size
												!= task->ItsJob->DestStat.st_size
										|| task->ItsJob->SourceStat.st_mtim
												!= task->ItsJob->DestStat.st_mtim))) {
					std::error_code ec;
					filesystem::remove_all(task->ItsJob->DestPath, ec);
					if (ec.value() != 0)
						task->ItsJob->Log.ErrorDeleteOld = true;
					// Update stat
					lstat(task->ItsJob->DestPath.c_str(),
							&task->ItsJob->DestStat);
				}

				// Check if link has to be created
				if (!S_ISLNK(task->ItsJob->DestStat.st_mode)
						|| task->ItsJob->DestStat.st_size
								!= task->ItsJob->SourceStat.st_size
						|| task->ItsJob->DestStat.st_mtim
								!= task->ItsJob->SourceStat.st_mtim) {
					if (task->data.size() > 0) {
						task->ItsJob->Log.ErrorCreateDest = symlinkat(
								&task->data[0], AT_FDCWD,
								task->ItsJob->DestPath.c_str()) != 0;
					}
				}
			}
			task->ItsJob->InitState = Job::CopyState::WRITTEN;
		} else if (task->Type == Task::TaskType::CHUNK) {
			if (!task->data.empty()) {
				size_t startPos = task->ChunkIdx * chunkSize;
				size_t currentChunkSize = task->data.size();
				ofstream fout(task->ItsJob->DestPath,
						ios_base::binary | ios_base::in);
				fout.seekp(startPos);
				fout.write(&task->data[0], currentChunkSize);
				task->ItsJob->Log.ErrorWriteChunk[task->ChunkIdx] =
						!fout.good();
				fout.close();
			}
			task->ItsJob->ChunkState[task->ChunkIdx] = Job::CopyState::WRITTEN;
		} else if (task->Type == Task::TaskType::ATTRIBUTES) {
			// Check if there is a valid input stat
			if (task->ItsJob->SourceStat.st_ino != 0) {
				// Check if there is an output object
				lstat(task->ItsJob->DestPath.c_str(), &task->ItsJob->DestStat);
				if (task->ItsJob->DestStat.st_ino != 0) {
					// If directory, delete content which is not in the input
					if (S_ISDIR(task->ItsJob->DestStat.st_mode)) {
						for (const auto &entry : filesystem::directory_iterator(
								task->ItsJob->DestPath)) {
							struct stat sin;
							bool inputExists = lstat(
									(task->ItsJob->SourcePath
											/ entry.path().filename()).c_str(),
									&sin) == 0;
							if (!inputExists) {
								std::error_code ec;
								filesystem::remove_all(
										task->ItsJob->DestPath
												/ entry.path().filename(), ec);
								task->ItsJob->Log.ErrorDeleteDirContents |=
										ec.value() != 0;
							}
						}
					}

					// Preserve timestamps
					if (task->ItsJob->SourceStat.st_mtim
							!= task->ItsJob->DestStat.st_mtim
							|| task->ItsJob->SourceStat.st_atim
									!= task->ItsJob->DestStat.st_atim) {
						struct timespec times[2];
						times[0] = task->ItsJob->SourceStat.st_atim;
						times[1] = task->ItsJob->SourceStat.st_mtim;
						task->ItsJob->Log.ErrorSetTimes = utimensat(AT_FDCWD,
								task->ItsJob->DestPath.c_str(), times,
								AT_SYMLINK_NOFOLLOW) != 0;
					}

					// Preserve owner
					if (task->ItsJob->SourceStat.st_uid
							!= task->ItsJob->DestStat.st_uid
							|| task->ItsJob->SourceStat.st_gid
									!= task->ItsJob->DestStat.st_gid) {
						task->ItsJob->Log.ErrorSetOwner = lchown(
								task->ItsJob->DestPath.c_str(),
								task->ItsJob->SourceStat.st_uid,
								task->ItsJob->SourceStat.st_gid) != 0;
					}

					// Preserve mode
					if (!S_ISLNK(task->ItsJob->SourceStat.st_mode)
							&& task->ItsJob->SourceStat.st_mode
									!= task->ItsJob->DestStat.st_mode) {
						task->ItsJob->Log.ErrorSetMode = chmod(
								task->ItsJob->DestPath.c_str(),
								task->ItsJob->SourceStat.st_mode) != 0;
					}
				}
			}
			task->ItsJob->AttribState = Job::CopyState::WRITTEN;
		}

		Out->PushBack(task);
	}
}
