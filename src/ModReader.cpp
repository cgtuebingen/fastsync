#include "ModReader.h"

#include "Job.h"
#include "Task.h"
#include "ThreadsafeBuffer.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

extern size_t chunkSize;

void ModReader::run() {
	// Read in elements from the input and put them to the output
	while (!stop) {
		Task *task = In->PopFront();
		if (task == nullptr)
			continue;

		if (task->Type == Task::TaskType::INIT) {
			// Read stat
			task->ItsJob->Log.ErrorStatSource = lstat(
					task->ItsJob->SourcePath.c_str(), &task->ItsJob->SourceStat)
					!= 0;
			// Check type
			if (!S_ISREG(task->ItsJob->SourceStat.st_mode) &&
			!S_ISDIR(task->ItsJob->SourceStat.st_mode) &&
			!S_ISLNK(task->ItsJob->SourceStat.st_mode))
				task->ItsJob->Log.ErrorSourceType = true;

			// If type is regular file, resize chunks state vector of job
			if (S_ISREG(task->ItsJob->SourceStat.st_mode)) {
				size_t numChunks = task->ItsJob->SourceStat.st_size / chunkSize
						+ ((task->ItsJob->SourceStat.st_size % chunkSize == 0) ?
								0 : 1);
				task->ItsJob->ChunkState.resize(numChunks,
						Job::CopyState::OPEN);
				task->ItsJob->Log.ErrorReadChunk.resize(numChunks, false);
				task->ItsJob->Log.ErrorWriteChunk.resize(numChunks, false);
			}

			// If type is link, copy content
			if (S_ISLNK(task->ItsJob->SourceStat.st_mode)) {
				task->data.resize(4097);
				size_t linkTgtSize = readlinkat(AT_FDCWD,
						task->ItsJob->SourcePath.c_str(), &task->data[0], 4096);
				if (linkTgtSize == -1) {
					task->data.resize(0);
					task->ItsJob->Log.ErrorReadLink = true;
				}
				task->data[linkTgtSize] = 0;
				task->data.resize(linkTgtSize + 1);
			}
		} else if (task->Type == Task::TaskType::CHUNK) {
			size_t startPos = task->ChunkIdx * chunkSize;
			size_t currentChunkSize = min(chunkSize,
					task->ItsJob->SourceStat.st_size - startPos);

			int fd = open(task->ItsJob->SourcePath.c_str(),
					O_RDONLY | O_NOFOLLOW);
			lseek(fd, startPos, SEEK_SET);
			task->data.resize(currentChunkSize);
			task->ItsJob->Log.ErrorReadChunk[task->ChunkIdx] = read(fd,
					&task->data[0], currentChunkSize) == 0;
			close(fd);
		} else if (task->Type == Task::TaskType::ATTRIBUTES) {
			// Attributes were already read during init stat
			// -> Nothing to do
		}

		Out->PushBack(task);
	}
}
