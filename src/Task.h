#ifndef TASK_H_
#define TASK_H_

#include <cstddef>
#include <vector>

struct Job;

/**
 * Represents the stuff that is copied by one thread in one step.
 */
struct Task {
	enum struct TaskType {
		INIT, CHUNK, ATTRIBUTES
	} Type;

	size_t ChunkIdx;

	std::vector<char> data;

	Job *ItsJob;

public:
	Task(const TaskType &type, Job *job, const size_t chunkIdx = -1) :
			Type(type), ItsJob(job), ChunkIdx(chunkIdx) {
	}
};

#endif /* TASK_H_ */
