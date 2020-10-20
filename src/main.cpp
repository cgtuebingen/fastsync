#include "ThreadsafeBuffer.h"
#include "Task.h"
#include "Job.h"
#include "ModReader.h"
#include "ModWriter.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <unordered_set>

using namespace std;

size_t chunkSize = 64 * 1024 * 1024;
size_t readerThreads = 1;
size_t writerThreads = 8;

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

void copyTree(const char *pathIn, const char *pathOut) {
	// == Initialize Pipeline ==

	// Buffers
	ThreadsafeBuffer<Task> TasksOpen(max(readerThreads, writerThreads) * 2);
	ThreadsafeBuffer<Task> TasksRead(max(readerThreads, writerThreads) * 2);
	ThreadsafeBuffer<Task> TasksWritten(max(readerThreads, writerThreads) * 2);

	// Readers
	vector<ModReader*> readers;
	for (size_t r = 0; r < readerThreads; r++) {
		ModReader *modReader = new ModReader();
		modReader->In = &TasksOpen;
		modReader->Out = &TasksRead;
		modReader->Start();
		readers.push_back(modReader);
	}

	// Writers
	vector<ModWriter*> writers;
	for (size_t w = 0; w < writerThreads; w++) {
		ModWriter *modWriter = new ModWriter();
		modWriter->In = &TasksRead;
		modWriter->Out = &TasksWritten;
		modWriter->Start();
		writers.push_back(modWriter);
	}

	// == Processing loop ==

	struct JobPtrCompare {
		bool operator() (const Job* lhs, const Job* rhs) const {
			// Special case: If nullptrs are involved, they are always smaller
			if(lhs == nullptr && rhs == nullptr)
				return false;
			if(lhs == nullptr)
				return true;
			if(rhs == nullptr)
				return false;

			// Both are not nullptr -> sequence id is most significant
			if(lhs->SourcePath < rhs->SourcePath)
				return true;
			if(lhs->SourcePath >= rhs->SourcePath)
				return false;

			if(lhs < rhs)
				return true;
			return false;
		}
	};

	// All jobs that are currently in flight
	std::set<Job*, JobPtrCompare> jobsOpen;

	// Insert root as first open job
	Job *rootJob = new Job();
	rootJob->SourcePath = pathIn;
	rootJob->DestPath = pathOut;
	jobsOpen.insert(rootJob);

	while (jobsOpen.size() > 0) {
		// Try to create new jobs from finished tasks
		if (TasksWritten.Size() > 0) {
			Task *task = TasksWritten.PopFront();
			if (task->Type == Task::TaskType::INIT) {
				cout << jobsOpen.size() << " I " << task->ItsJob->SourcePath
						<< endl;

				// If this was a directory task
				if (S_ISDIR(task->ItsJob->SourceStat.st_mode)) {
					// Start jobs for subdirectories and create dependencies
					for (const auto &entry : filesystem::directory_iterator(
							task->ItsJob->SourcePath)) {
						Job *subJob = new Job();
						subJob->SourcePath = task->ItsJob->SourcePath
								/ entry.path().filename();
						subJob->DestPath = task->ItsJob->DestPath
								/ entry.path().filename();
						createDependency(task->ItsJob, subJob);
						jobsOpen.insert(subJob);
					}
				}
				task->ItsJob->InitState = Job::CopyState::DONE;

				// For links and files, check if copy has to continue at all
				if ((S_ISREG(task->ItsJob->DestStat.st_mode)
						|| S_ISLNK(task->ItsJob->DestStat.st_mode))
						&& task->ItsJob->DestStat.st_size
								== task->ItsJob->SourceStat.st_size
						&& task->ItsJob->DestStat.st_mtim.tv_sec
								== task->ItsJob->SourceStat.st_mtim.tv_sec
						&& task->ItsJob->DestStat.st_uid
								== task->ItsJob->SourceStat.st_uid
						&& task->ItsJob->DestStat.st_gid
								== task->ItsJob->SourceStat.st_gid) {
					// Remove this job's dependencies
					while (task->ItsJob->Dependents.size() > 0) {
						removeDependency(*task->ItsJob->Dependents.begin(),
								task->ItsJob);
					}

					jobsOpen.erase(task->ItsJob);
					delete task->ItsJob;
				}
			}
			if (task->Type == Task::TaskType::CHUNK) {
				cout << jobsOpen.size() << " C" << task->ChunkIdx << " "
						<< task->ItsJob->SourcePath << endl;
				task->ItsJob->ChunkState[task->ChunkIdx] = Job::CopyState::DONE;
			}
			if (task->Type == Task::TaskType::ATTRIBUTES) {
				cout << jobsOpen.size() << " A " << task->ItsJob->SourcePath
						<< endl;
				// Remove this job's dependencies
				while (task->ItsJob->Dependents.size() > 0) {
					removeDependency(*task->ItsJob->Dependents.begin(),
							task->ItsJob);
				}
				//Mark attributes as finished (not really necessary because job will be deleted immediatelly)
				task->ItsJob->AttribState = Job::CopyState::DONE;
				// Delete the job
				jobsOpen.erase(task->ItsJob);
				delete task->ItsJob;
			}

			delete task;
			continue;
		}

		// Try to continue on an open job
		for (Job *job : jobsOpen) {
			// Check for init - can always be done
			if (job->InitState == Job::CopyState::OPEN) {
				job->InitState = Job::CopyState::SCHEDULED;
				TasksOpen.PushBack(new Task(Task::TaskType::INIT, job));
				break;
			}

			// Check for chunk - can be done if init is finished
			bool allChunksWritten = true;
			if (job->InitState == Job::CopyState::DONE) {
				bool openChunkFound = false;
				for (size_t c = 0; c < job->ChunkState.size(); c++) {
					bool prevChunksWritten = allChunksWritten;
					if (job->ChunkState[c] != Job::CopyState::DONE)
						allChunksWritten = false;
					if (prevChunksWritten
							&& job->ChunkState[c] == Job::CopyState::OPEN) {
						job->ChunkState[c] = Job::CopyState::SCHEDULED;
						TasksOpen.PushBack(
								new Task(Task::TaskType::CHUNK, job, c));
						openChunkFound = true;
						break;
					}
				}
				if (openChunkFound)
					break;
			}

			// Check for attributes - can be done if all chonks are written and there are no dependencies
			if (job->InitState == Job::CopyState::DONE && allChunksWritten
					&& job->AttribState == Job::CopyState::OPEN
					&& job->FinishDirDependencies.size() == 0) {
				job->AttribState = Job::CopyState::SCHEDULED;
				TasksOpen.PushBack(new Task(Task::TaskType::ATTRIBUTES, job));
				break;
			}
		}
	}

	// == Cleanup ==

	assert(TasksOpen.Size() == 0);
	assert(TasksRead.Size() == 0);
	assert(TasksWritten.Size() == 0);

	// Readers
	for (ModReader *reader : readers)
		reader->Stop();
	for (ModReader *reader : readers)
		TasksOpen.PushBack(nullptr);
	for (ModReader *reader : readers)
		delete reader;

	// Writers
	for (ModWriter *writer : writers)
		writer->Stop();
	for (ModWriter *writer : writers)
		TasksRead.PushBack(nullptr);
	for (ModWriter *writer : writers)
		delete writer;

}

int main(int argc, char **argv) {
	if (argc < 3) {
		cerr
				<< "Usage: ./fastsync SOURCE DEST [#READERS [#WRITERS [CHUNK_SIZE_MB]]]"
				<< endl;
		return -1;
	}

	if (argc >= 4)
		readerThreads = atoi(argv[3]);
	if (argc >= 5)
		writerThreads = atoi(argv[4]);
	if (argc >= 6)
		chunkSize = atoi(argv[5]) * 1024 * 1024;

	copyTree(argv[1], argv[2]);
}
