#ifndef SRC_MODWRITER_H_
#define SRC_MODWRITER_H_

#include "ThreadedModule.h"

template<typename Type>
class ThreadsafeBuffer;
struct Task;

struct ModWriter : public ThreadedModule {
	ThreadsafeBuffer<Task>* In;
	ThreadsafeBuffer<Task>* Out;
protected:
	virtual void run() override;
};

#endif /* SRC_MODWRITER_H_ */
