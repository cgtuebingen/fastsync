#ifndef SRC_MODREADER_H_
#define SRC_MODREADER_H_

#include "ThreadedModule.h"

template<typename Type>
class ThreadsafeBuffer;
struct Task;

struct ModReader: ThreadedModule {
	ThreadsafeBuffer<Task> *In;
	ThreadsafeBuffer<Task> *Out;
protected:
	virtual void run() override;
};

#endif /* SRC_MODREADER_H_ */
