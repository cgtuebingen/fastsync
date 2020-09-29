#ifndef SRC_JOB_H_
#define SRC_JOB_H_

#include <filesystem>
#include <vector>
#include <sys/stat.h>
#include <set>
#include <cstring>
#include <cassert>

/**
 * Represents a filesystem item that should be copied.
 * File, directory or symlink.
 */
struct Job {
	/// Path to the input.
	std::filesystem::path SourcePath;
	/// Path to the destination.
	std::filesystem::path DestPath;

	/// Current stat of the source.
	struct stat SourceStat;
	/// Stat of the destination. Must be updated whenever dest is changed.
	struct stat DestStat;

	/// Copy state only reflects position in the pipeline, not errors.
	enum struct CopyState {
		OPEN, SCHEDULED, DONE
	};

	/// State of the initialization
	CopyState InitState;
	/// State of the individual chunks for regular files
	std::vector<CopyState> ChunkState;
	/// State of the attributes
	CopyState AttribState;

	/// Lists all jobs that have to be finished before a directory can be finalized
	/// (deleting content that is not in the source dir and setting attributes).
	std::set<Job*> FinishDirDependencies;

	/// Lists all jobs that can only be executed when this job is finished.
	std::set<Job*> Dependents;

	/**
	 * Log only reflects what to reflect to the user and should not be used
	 * as input for later pipeline stages.
	 */
	struct Log {
		bool ErrorStatSource;
		bool ErrorSourceType;
		bool ErrorReadLink;
		bool ErrorDeleteOld;
		bool ErrorCreateDest;
		std::vector<bool> ErrorReadChunk;
		std::vector<bool> ErrorWriteChunk;
		bool ErrorDeleteDirContents;
		bool ErrorSetTimes;
		bool ErrorSetOwner;
		bool ErrorSetMode;

		Log() :
				ErrorStatSource(false), ErrorSourceType(false), ErrorReadLink(
						false), ErrorDeleteOld(false), ErrorCreateDest(false), ErrorDeleteDirContents(
						false), ErrorSetTimes(false), ErrorSetOwner(false), ErrorSetMode(
						false) {
		}
	} Log;

	Job() :
			InitState(CopyState::OPEN), AttribState(CopyState::OPEN) {
		memset(&SourceStat, 0, sizeof(SourceStat));
		memset(&DestStat, 0, sizeof(DestStat));
	}
};

inline void createDependency(Job *dependent, Job *independent) {
	assert(
			dependent->FinishDirDependencies.find(independent)
					== dependent->FinishDirDependencies.end());
	assert(
			independent->Dependents.find(dependent)
					== independent->Dependents.end());

	dependent->FinishDirDependencies.insert(independent);
	independent->Dependents.insert(dependent);
}

inline void removeDependency(Job *dependent, Job *independent) {
	assert(
			dependent->FinishDirDependencies.find(independent)
					!= dependent->FinishDirDependencies.end());
	assert(
			independent->Dependents.find(dependent)
					!= independent->Dependents.end());

	dependent->FinishDirDependencies.erase(independent);
	independent->Dependents.erase(dependent);
}

#endif /* SRC_JOB_H_ */
