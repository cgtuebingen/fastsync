/*
 * (C)opyright Uni Tuebingen
 */
#ifndef THREADSAFEBUFFER_H_
#define THREADSAFEBUFFER_H_

#include <pthread.h>
#include <list>

/**
 * Buffer that can be accessed from multiple threads and blocks
 * in cases of over- oder underflows.
 */
template<typename Type>
class ThreadsafeBuffer {
	std::list<Type*> buffer;
	unsigned int maxSize;

	pthread_mutex_t bufferModified;
	pthread_cond_t bufferModificationDone;

public:
	/**
	 * Initializes the threadsafe buffer with a certain size.
	 * @param maxSize of the buffer. This size does not change during
	 * the buffer's lifetime.
	 */
	explicit ThreadsafeBuffer(unsigned int maxSize);

	/**
	 * Adds a pointer to a new element to the buffer.
	 * @remarks This method blocks until PopFront is called from another thread
	 * if the buffer is currently full.
	 * @param value Pointer to the object that should be enqueued in the buffer.
	 */
	inline void PushBack(Type*const& value);
	/**
	 * Removes the oldest pointer from the buffer.
	 * @remarks This method blocks until PushBack() is called from another
	 * thread if the buffer is currently empty.
	 */
	inline Type* PopFront();
	/**
	 * Returns the number of elements currently in the buffer.
	 * @returns Number of elements in the buffer.
	 */
	inline unsigned int Size();
	/**
	 * Fills the buffer with nullptr elements until it is full
	 * (to keep Poppers spinning).
	 */
	inline void Fill();
	/**
	 * Deletes everything that is still in the buffer (to clean up or keep
	 * Pushers spinning).
	 */
	inline void Clear();
};

template<typename Type> ThreadsafeBuffer<Type>::ThreadsafeBuffer(
		unsigned int maxSize) :
		maxSize(maxSize) {
	pthread_mutex_init(&bufferModified, NULL);
	pthread_cond_init(&bufferModificationDone, NULL);
}

template<typename Type> void ThreadsafeBuffer<Type>::PushBack(
		Type*const& value) {
	pthread_mutex_lock(&bufferModified);

	while (buffer.size() == maxSize)
		pthread_cond_wait(&bufferModificationDone, &bufferModified);

	buffer.push_back(value);

	pthread_cond_signal(&bufferModificationDone);

	pthread_mutex_unlock(&bufferModified);
}

template<typename Type> Type* ThreadsafeBuffer<Type>::PopFront() {
	pthread_mutex_lock(&bufferModified);

	while (buffer.empty())
		pthread_cond_wait(&bufferModificationDone, &bufferModified);

	Type* result = buffer.front();
	buffer.pop_front();

	pthread_cond_signal(&bufferModificationDone);

	pthread_mutex_unlock(&bufferModified);

	return result;
}

template<typename Type> unsigned int ThreadsafeBuffer<Type>::Size() {
	pthread_mutex_lock(&bufferModified);

	unsigned int result = buffer.size();

	pthread_mutex_unlock(&bufferModified);

	return result;
}

template<typename Type>
inline void ThreadsafeBuffer<Type>::Fill() {
	pthread_mutex_lock(&bufferModified);

	while (buffer.size() < maxSize)
		buffer.push_back(nullptr);

	pthread_cond_signal(&bufferModificationDone);

	pthread_mutex_unlock(&bufferModified);
}

template<typename Type>
inline void ThreadsafeBuffer<Type>::Clear() {
	pthread_mutex_lock(&bufferModified);

	while (buffer.size() > 0) {
		delete buffer.front();
		buffer.pop_front();
	}

	pthread_cond_signal(&bufferModificationDone);

	pthread_mutex_unlock(&bufferModified);
}

#endif /* THREADSAFEBUFFER_H_ */
