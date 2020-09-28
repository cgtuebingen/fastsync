#ifndef SRC_TOOLS_THREADEDMODULE_H_
#define SRC_TOOLS_THREADEDMODULE_H_

#include <thread>

/**
 * Class that encapsulates a module which runs in a separate thread.
 */
class ThreadedModule {
	std::thread* itsThread;
protected:
	/// When this variable becomes true, the run method must stop.
	volatile bool stop;
	/**
	 * To be implemented by derieved classes.
	 */
	virtual void run() = 0;
	/**
	 * Is called when Stop was called. Can be used to trigger stop actions
	 */
	virtual void onStop();
public:
	/**
	 * Creates a module.
	 */
	ThreadedModule();
	/**
	 * Blocks until the thread of the module is not running (anymore).
	 */
	virtual ~ThreadedModule();
	/**
	 * Starts the module's thread.
	 */
	void Start();
	/**
	 * Signals the module's thread to stop and returns immediatelly.
	 */
	void Stop();
};

#endif /* SRC_TOOLS_THREADEDMODULE_H_ */
