/* eos - A reimplementation of BioWare's Aurora engine
 * Copyright (c) 2010 Sven Hesse (DrMcCoy)
 *
 * The Infinity, Aurora, Odyssey and Eclipse engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and Bioware corp.
 *
 * This file is part of eos and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

#include "boost/algorithm/string.hpp"
#include "boost/regex.hpp"

#include "filelist.h"
#include "file.h"
#include "stream.h"

// boost-filesystem stuff
using boost::filesystem::path;
using boost::filesystem::exists;
using boost::filesystem::is_directory;
using boost::filesystem::directory_iterator;

// boost-string_algo
using boost::to_lower_copy;
using boost::equals;
using boost::iequals;

namespace Common {

FileList::FilePath::FilePath(std::string b, path p) : baseDir(b), filePath(p) {
}

FileList::FilePath::FilePath(const FilePath &p) : baseDir(p.baseDir), filePath(p.filePath) {
}


FileList::FileList() {
}

FileList::FileList(const FileList &list) {
	*this = list;
}

FileList::~FileList() {
}

FileList &FileList::operator=(const FileList &list) {
	_files = list._files;
	_fileMap = list._fileMap;

	return *this;
}

FileList &FileList::operator+=(const FileList &list) {
	_files.insert(_files.end(), list._files.begin(), list._files.end());
	_fileMap.insert(list._fileMap.begin(), list._fileMap.end());

	return *this;
}

void FileList::clear() {
	_files.clear();
	_fileMap.clear();
}

bool FileList::isEmpty() const {
	return _files.empty();
}

uint32 FileList::size() const {
	return _files.size();
}

void FileList::getFileNames(std::list<std::string> &list) const {
	for (std::list<FilePath>::const_iterator it = _files.begin(); it != _files.end(); ++it)
		list.push_back(it->filePath.string());
}

bool FileList::addDirectory(const std::string &directory, int recurseDepth) {
	return addDirectory(directory, path(directory), recurseDepth);
}

bool FileList::addDirectory(const std::string &base, const path &directory, int recurseDepth) {
	if (!exists(directory) || !is_directory(directory))
		// Path is either no directory or doesn't exist
		return false;

	try {
		// Iterator over the directory's contents
		directory_iterator itEnd;
		for (directory_iterator itDir(directory); itDir != itEnd; ++itDir) {
			if (is_directory(itDir->status())) {
				// It's a directory. Recurse into it if the depth limit wasn't yet reached

				if (recurseDepth != 0)
					if (!addDirectory(base, itDir->path(), (recurseDepth == -1) ? -1 : (recurseDepth - 1)))
						return false;

			} else
				// It's a path, add it to the list
				addPath(base, itDir->path());
		}
	} catch (...) {
		return false;
	}

	return true;
}

void FileList::addPath(const FilePath &p) {
	_files.push_back(p);
	_fileMap.insert(std::make_pair(to_lower_copy(p.filePath.stem()), --_files.end()));
}

void FileList::addPath(const std::string &base, const path &p) {
	addPath(FilePath(base, p));
}

bool FileList::getSubList(const std::string &glob, FileList &subList, bool caseInsensitive) const {
	boost::regex::flag_type type = boost::regex::perl;
	if (caseInsensitive)
		type |= boost::regex::icase;
	boost::regex expression(glob, type);

	bool foundMatch = false;

	// Iterate through the whole list, adding the matches to the sub list
	for (std::list<FilePath>::const_iterator it = _files.begin(); it != _files.end(); ++it)
		if (boost::regex_match(it->filePath.string(), expression)) {
			subList.addPath(*it);
			foundMatch = true;
		}

	return foundMatch;
}

bool FileList::getSubList(const std::string &glob, std::list<std::string> &list, bool caseInsensitive) const {
	boost::regex::flag_type type = boost::regex::perl;
	if (caseInsensitive)
		type |= boost::regex::icase;
	boost::regex expression(glob, type);

	bool foundMatch = false;

	// Iterate through the whole list, adding the matches to the sub list
	for (std::list<FilePath>::const_iterator it = _files.begin(); it != _files.end(); ++it)
		if (boost::regex_match(it->filePath.string(), expression)) {
			list.push_back(it->filePath.string());
			foundMatch = true;
		}

	return foundMatch;
}

bool FileList::contains(const std::string &fileName, bool caseInsensitive) const {
	if (getPath(fileName, caseInsensitive))
		return true;

	return false;
}

SeekableReadStream *FileList::openFile(const std::string &fileName, bool caseInsensitive) const {
	const FilePath *p = getPath(fileName, caseInsensitive);
	if (!p)
		return 0;

	File *file = new File;
	if (!file->open(p->filePath.string())) {
		delete file;
		return 0;
	}

	return file;
}

const FileList::FilePath *FileList::getPath(const std::string &fileName, bool caseInsensitive) const {
	// Find the files matching the lowercase stem
	std::pair<FileMap::const_iterator, FileMap::const_iterator> files;
	files = _fileMap.equal_range(to_lower_copy(path(fileName).stem()));

	// Iterate through those files, looking for a real match
	for (; files.first != files.second; ++files.first) {
		if (caseInsensitive) {
			// Does the complete path match?
			if (iequals(files.first->second->filePath.string(), fileName))
				return &*files.first->second;
			// Does the relative path match?
			if (iequals(files.first->second->filePath.string(), (path(files.first->second->baseDir) / fileName).string()))
				return &*files.first->second;
			// Does the file name match?
			if (iequals(files.first->second->filePath.filename(), fileName))
				return &*files.first->second;
		} else {
			// Does the complete path match?
			if (equals(files.first->second->filePath.string(), fileName))
				return &*files.first->second;
			// Does the relative path match?
			if (equals(files.first->second->filePath.string(), (path(files.first->second->baseDir) / fileName).string()))
				return &*files.first->second;
			// Does the file name match?
			if (equals(files.first->second->filePath.filename(), fileName))
				return &*files.first->second;
		}
	}

	return 0;
}

std::string FileList::findSubDirectory(const std::string &directory, const std::string &subDirectory,
		bool caseInsensitive) {

	if (!exists(directory) || !is_directory(directory))
		// Path is either no directory or doesn't exist
		return "";

	try {
		path dirPath(directory);
		path subDirPath(subDirectory);

		// Iterator over the directory's contents
		directory_iterator itEnd;
		for (directory_iterator itDir(dirPath); itDir != itEnd; ++itDir) {
			if (is_directory(itDir->status())) {
				// It's a directory. Recurse into it if the depth limit wasn't yet reached

				if (caseInsensitive) {
					if (iequals(itDir->path().filename(), subDirectory))
						return itDir->path().string();
				} else {
					if (equals(itDir->path().filename(), subDirectory))
						return itDir->path().string();
				}
			}
		}
	} catch (...) {
	}

	return "";
}

} // End of namespace Common
