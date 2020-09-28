#include "ThreadedModule.h"

#include <cassert>

using namespace std;

void ThreadedModule::onStop() {
}

ThreadedModule::ThreadedModule() :
		itsThread(nullptr), stop(false) {
}

ThreadedModule::~ThreadedModule() {
	itsThread->join();
	delete itsThread;
}

void ThreadedModule::Start() {
	assert(itsThread == nullptr);
	itsThread = new thread(&ThreadedModule::run, this);
}

void ThreadedModule::Stop() {
	assert(itsThread != nullptr);
	assert(stop == false);
	stop = true;
	onStop();
}
