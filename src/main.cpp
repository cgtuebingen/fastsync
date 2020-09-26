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

using namespace std;

size_t chunkSize = 50 * 1024 * 1024;

bool operator==(const timespec &t1, const timespec &t2) {
	if (t1.tv_sec != t2.tv_sec)
		return false;
	if (t1.tv_nsec != t2.tv_nsec)
		return false;
	return true;
}

bool operator!=(const timespec &t1, const timespec &t2) {
	return !(t1 == t2);
}

/**
 * Does the first step of file copying: Creating the target file or dir or
 * symlink. For regular files, content has to be copied explicitely later.
 * @param pathIn Path to input.
 * @param sin File stats for input.
 * @param pathOut Path to output.
 * @param sout In/out parameter. File stats for output. Has to be set if
 * output file exists. Must be set if output file was successfully created.
 * @param outputExists If true, the output file exists and stats could be read.
 */
bool copyFileContent(const char *pathIn, const struct stat &sin,
		const char *pathOut, struct stat &sout, bool outputExists) {
	// Check if an input of a wrong type exists -> has to be removed!
	if (outputExists && sin.st_mode & S_IFMT != sout.st_mode & S_IFMT) {
		filesystem::remove_all(pathOut);
		outputExists = lstat(pathOut, &sout);
	}

	// Check for file type
	if (S_ISREG(sin.st_mode)) {
		// Check if output must be removed

		// Check if output is already there and looks correct
		if (outputExists && sin.st_mtim == sout.st_mtim
				&& sin.st_size == sout.st_size) {
			// Done!
			return true;
		}

		// Create an sparse empty output file
		{
			ofstream fout(pathOut, ios_base::binary);
			if (sin.st_size > 0) {
				fout.seekp(sin.st_size - 1);
				fout.write("", 1);
				if (!fout.good()) {
					cerr << "Error creating file " << pathOut << endl;
				}
			}
			fout.close();
		}

		// Reflect the new state of the file
		return lstat(pathOut, &sout) == 0;
	} else if (S_ISDIR(sin.st_mode)) {
		// Check if output is already there
		if (outputExists) {
			// Done
			return true;
		}

		if (mkdir(pathOut, sin.st_mode) != 0) {
			cerr << "Error creating directory " << pathOut << endl;
		}

		// Reflect the new state of the directory
		return lstat(pathOut, &sout) == 0;
	} else if (S_ISLNK(sin.st_mode)) {
		// Check if output is already there and looks correct
		if (outputExists) {
			if (sin.st_mtim == sout.st_mtim && sin.st_size == sout.st_size)
				// Done!
				return true;
			// If it does not look correct, remove it
			filesystem::remove(pathOut);
		}

		char linkTgt[4097];
		size_t linkTgtSize = readlinkat(AT_FDCWD, pathIn, linkTgt, 4096);
		if (linkTgtSize == -1) {
			cerr << "Could not read link target from " << pathIn << endl;
			return lstat(pathOut, &sout) == 0;
		}
		linkTgt[linkTgtSize] = 0;
		if (symlinkat(linkTgt, AT_FDCWD, pathOut) != 0) {
			cerr << "Could not create symlink " << pathOut << ": "
					<< strerror(errno) << endl;
			return lstat(pathOut, &sout) == 0;
		}

		return lstat(pathOut, &sout) == 0;
	}

	cerr << "Unsupported file type (not reg, dir or lnk)" << endl;
	return false;
}

bool copyFileChunk(const char *pathIn, const char *pathOut, const size_t chunk,
		const size_t size) {
	// Copy file content in chunks
	size_t startPos = chunk * chunkSize;
	size_t currentChunkSize = min(chunkSize, size - startPos);

	ifstream fin(pathIn, ios_base::binary);
	fin.seekg(startPos);
	vector<char> data(chunkSize);
	fin.read(&data[0], currentChunkSize);
	if (!fin.good()) {
		cerr << "Error while reading " << pathIn << " chunk "
				<< (startPos / chunkSize) << endl;
		return false;
	}
	fin.close();

	ofstream fout(pathOut, ios_base::binary | ios_base::in);
	fout.seekp(startPos);
	fout.write(&data[0], currentChunkSize);
	if (!fout.good()) {
		cerr << "Error while writing " << pathOut << " chunk "
				<< (startPos / chunkSize) << endl;
		return false;
	}
	fout.close();
	return true;
}

bool copyFileAttributes(const char *pathIn, const struct stat &sin,
		const char *pathOut, const struct stat &sout, const bool outputExists) {
	bool allGood = true;

	// Preserve timestamps
	if (sin.st_mtim != sout.st_mtim || sin.st_atim != sout.st_atim) {
		struct timespec times[2];
		times[0] = sin.st_atim;
		times[1] = sin.st_mtim;
		if (utimensat(AT_FDCWD, pathOut, times, AT_SYMLINK_NOFOLLOW) != 0) {
			cerr << "Error on setting times on " << pathOut << endl;
			allGood = false;
		}
	}

	// Preserve owner
	if (sin.st_uid != sout.st_uid || sin.st_gid != sout.st_gid) {
		if (lchown(pathOut, sin.st_uid, sin.st_gid) != 0) {
			cerr << "Error on setting owner on " << pathOut << endl;
			allGood = false;
		}
	}

	// Preserve mode
	if (sin.st_mode != sout.st_mode) {
		if (chmod(pathOut, sin.st_mode) != 0) {
			cerr << "Error on setting mode on " << pathOut << endl;
			allGood = false;
		}
	}

	return allGood;
}

bool copyItem(const char *pathIn, const char *pathOut) {
	// Read input stats
	struct stat sin;
	if (lstat(pathIn, &sin) != 0) {
		cerr << "Input " << pathIn << " could not be opened" << endl;
		return false;
	}

	// Get current output state
	struct stat sout;
	bool outputExists = lstat(pathOut, &sout) == 0;

	// Initialize the output
	outputExists = copyFileContent(pathIn, sin, pathOut, sout, outputExists);

	// If it is a regular, new file
	if (outputExists && S_ISREG(sout.st_mode) && sin.st_mtim != sout.st_mtim) {
		// Copy chunks
		size_t numChunks = sin.st_size / chunkSize
				+ ((sin.st_size % chunkSize) ? 1 : 0);
		for (size_t chunk = 0; chunk < numChunks; chunk++)
			copyFileChunk(pathIn, pathOut, chunk, sin.st_size);
	}

	if (outputExists && S_ISDIR(sin.st_mode))
		return true;
	return false;
}

void copyTree(const char *pathIn, const char *pathOut) {
	// First copy the root
	bool isDir = copyItem(pathIn, pathOut);

	if (isDir) {

		// Then copy all items in the directory
		filesystem::path pIn(pathIn);
		filesystem::path pOut(pathOut);
		for (const auto &entry : filesystem::directory_iterator(pIn)) {
			copyTree((pIn / entry.path().filename()).c_str(),
					(pOut / entry.path().filename()).c_str());
		}

		// Then check if there is any element in the output which is not contained in the input
		// and remove it
		for (const auto &entry : filesystem::directory_iterator(pOut)) {
			struct stat sin;
			bool inputExists = lstat((pIn / entry.path().filename()).c_str(),
					&sin) == 0;
			if (!inputExists)
				filesystem::remove_all(pOut / entry.path().filename());
		}
	}

	// Read input stats
	struct stat sin;
	if (lstat(pathIn, &sin) != 0) {
		cerr << "Input " << pathIn << " could not be opened" << endl;
		return;
	}
	struct stat sout;
	bool outputExists = lstat(pathOut, &sout) == 0;
	if (outputExists)
		copyFileAttributes(pathIn, sin, pathOut, sout, outputExists);
}

int main(int argc, char **argv) {
	copyTree("test/filein", "test/fileout");
	copyTree("test/dirin", "test/dirout");
	copyTree("test/linkin", "test/linkout");

	copyTree("test/dirfilledin", "test/dirfilledout");
}
