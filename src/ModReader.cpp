#include "ModReader.h"

#include "Job.h"
#include "Task.h"
#include "ThreadsafeBuffer.h"

#include <sys/stat.h>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

extern size_t chunkSize;

void ModReader::run() {
	// Read in elements from the input and put them to the output
	while (!stop) {
		Task *task = In->PopFront();

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

			// If type is link, copy content
			if (S_ISLNK(task->ItsJob->SourceStat.st_mode)) {
				task->data.resize(4097);
				size_t linkTgtSize = readlinkat(AT_FDCWD,
						task->ItsJob->SourcePath.c_str(), &task->data[0], 4096);
				if(linkTgtSize == -1) {
					task->data.resize(0);
					task->ItsJob->Log.ErrorReadLink = true;
				}
				task->data[linkTgtSize] = 0;
				task->data.resize(linkTgtSize + 1);
			}

			// Mark init as read
			task->ItsJob->InitState = Job::CopyState::READ;
		} else if (task->Type == Task::TaskType::CHUNK) {
			size_t startPos = task->ChunkIdx * chunkSize;
			size_t currentChunkSize = min(chunkSize,
					task->ItsJob->DestStat.st_size - startPos);

			ifstream fin(task->ItsJob->SourcePath.c_str(), ios_base::binary);
			fin.seekg(startPos);
			task->data.resize(currentChunkSize);
			fin.read(&task->data[0], currentChunkSize);
			task->ItsJob->Log.ErrorReadChunk[task->ChunkIdx] = !fin.good();
			fin.close();

			// Mark chunk as read
			task->ItsJob->ChunkState[task->ChunkIdx] = Job::CopyState::READ;
		} else if (task->Type == Task::TaskType::ATTRIBUTES) {
			// Attributes were already read during init stat
			// -> Nothing to do

			// Mark attribs as read
			task->ItsJob->AttribState = Job::CopyState::READ;
		}

		Out->PushBack(task);
	}
}
