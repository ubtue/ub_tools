/** \file    SimpleDB.cc
 *  \brief   Implementation of class SimpleDB.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "SimpleDB.h"
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include "StringUtil.h"


unsigned SimpleDB::open_count_;


SimpleDB::Cursor::Cursor(SimpleDB * const simple_db, const std::string &initial_key)
	: cursor_(::tcbdbcurnew(simple_db->db_)), db_name_(simple_db->getFileName())
{
	// Sanity check:
	if (unlikely(cursor_ == NULL)) {
		const std::string error(::tcbdberrmsg(::tcbdbecode(simple_db->db_)));
		throw std::runtime_error("in SimpleDB::Cursor::Cursor: tcbdbcurnew() failed (" + error + ")!");
	}

	// Start of the beginning of the database?
	if (initial_key.empty()) // Yes!
		at_end_ = not ::tcbdbcurfirst(cursor_);
	else // Start at "key."
		at_end_ = not ::tcbdbcurjump(cursor_, initial_key.data(), initial_key.size());
}


SimpleDB::Cursor::~Cursor()
{
	if (cursor_ != NULL)
		::tcbdbcurdel(cursor_);
}


bool SimpleDB::Cursor::advance()
{
	if (at_end_)
		return false;

	if (not ::tcbdbcurnext(cursor_)) {
		at_end_ = true;
		return false;
	}

	return true;
}


bool SimpleDB::Cursor::getCurrentKey(std::string * const current_key) const
{
	if (at_end_) {
		current_key->clear();
		return false;
	}

	int size;
	char * const key(reinterpret_cast<char *>(::tcbdbcurkey(cursor_, &size)));
	if (unlikely(key == NULL))
		throw std::runtime_error("in SimpleDB::Cursor::getCurrentKey: tcbdbcurkey() failed!");

	*current_key = std::string(key, size);
	std::free(key);

	return true;
}


bool SimpleDB::Cursor::getKeyAndData(void ** const new_key, unsigned *new_key_size, void ** const new_data, unsigned *new_data_size)
{
	if (unlikely(at_end_))
		return false;

	*new_key = ::tcbdbcurkey(cursor_, reinterpret_cast<int * const>(new_key_size));
	if (unlikely(*new_key == NULL))
		throw std::runtime_error("in SimpleDB::Cursor::getKeyAndData: tcbdbcurkey() failed!");

	*new_data = ::tcbdbcurval(cursor_, reinterpret_cast<int * const>(new_data_size));
	if (unlikely(*new_data == NULL))
		throw std::runtime_error("in SimpleDB::Cursor::getKeyAndData: tcbdbcurval() failed!");

	return true;
}


SimpleDB::Data::~Data()
{
	std::free(data_);
}


void SimpleDB::Data::clear()
{
	std::free(data_);
	data_ = NULL;
	size_ = 0;
}


void SimpleDB::KeyDataPair::clear()
{
	key_.clear();
	data_.clear();
}


SimpleDB::SimpleDB(const char * const db_name, const OpenMode open_mode, int mode, const Type type)
	: db_name_(StringUtil::strnewdup(db_name)), type_(type)
{
	init(open_mode, mode);
}


SimpleDB::SimpleDB(const std::string &db_name, const OpenMode open_mode, int mode, const Type type)
	: db_name_(StringUtil::strnewdup(db_name.c_str())), type_(type)
{
	init(open_mode, mode);
}


void SimpleDB::init(const OpenMode open_mode, const int mode)
{
	last_error_ = TCESUCCESS;
	last_data_  = NULL;

	db_ = ::tcbdbnew();
	if (unlikely(db_ == NULL))
		throw std::runtime_error("in SimpleDB::init: call to tcbdbnew() failed!");

	if (unlikely(not ::tcbdbsetmutex(db_))) {
		last_error_ = ::tcbdbecode(db_);
		throwError("init", "tcbdbsetmutex() failed");
	}

	// Set the Berkeley DB open flags for the requested open mode:
	int omode;
	switch (open_mode) {
	case OPEN_CREATE:
		omode = HDBOWRITER | HDBOCREAT | HDBOTRUNC | HDBOLCKNB;
		break;
	case OPEN_CREATE_READ_WRITE:
		omode = HDBOREADER | HDBOWRITER | HDBOCREAT | HDBOLCKNB;
		break;
	case OPEN_RDONLY:
		omode = HDBOREADER | HDBOLCKNB;
		break;
	case OPEN_READ_WRITE:
		omode = HDBOREADER | HDBOWRITER | HDBOLCKNB;
		break;
	default:
	    throw std::runtime_error("in SimpleDB::init: unknown open mode (" + std::to_string(static_cast<int>(open_mode)) + ")!");
		omode = 0; // <- keep the compiler happy
	}

	if (unlikely(not ::tcbdbopen(db_, db_name_, omode))) {
		last_error_ = ::tcbdbecode(db_);
		throwError("init", "tcbdbopen() failed");
	}

	if (unlikely(::chmod(db_name_, mode)))
	    throw std::runtime_error("in SimpleDB::init: chmod(2) failed (errno: " + std::to_string(errno) + ")!");

	++open_count_;
}


SimpleDB::~SimpleDB()
{
	close();
	delete [] db_name_;
}


void SimpleDB::clear()
{
	if (unlikely(not ::tcbdbvanish(db_))) {
		last_error_ = ::tcbdbecode(db_);
		throwError("putData", "tcbdbvanish() failed");
	}
}


void SimpleDB::close()
{
	if (db_ != NULL) {
		if (last_data_ != NULL) {
			std::free(last_data_);
			last_data_ = NULL;
		}

		::tcbdbclose(db_);
		::tcbdbdel(db_);
		db_ = NULL;

		--open_count_;
	}
}


void SimpleDB::flush()
{
	if (unlikely(not ::tcbdbsync(db_))) {
		last_error_ = ::tcbdbecode(db_);
		throwError("putData", "tcbdbsync() failed");
	}
}


void SimpleDB::throwError(const char * const function_name, const char * const msg) const
{
	std::string err_msg("SimpleDB:");
	err_msg += function_name;
	err_msg += ": database \"";
	err_msg += db_name_;
	err_msg += "\": ";
	if (msg != NULL) {
		err_msg += ' ';
		err_msg += msg;
	}
	err_msg += " (";
	err_msg += ::tchdberrmsg(last_error_);
	err_msg += ')';
	throw std::runtime_error(err_msg.c_str());
}


// SimpleDB::putData -- Add a record to the database.
//
void SimpleDB::putData(const void * const key, const size_t key_size, const void * const data, const size_t data_size)
{
	if (not ::tcbdbput(db_, key, key_size, data, data_size)) {
		last_error_ = ::tcbdbecode(db_);
		throwError("putData", "tcbdbput() failed");
	}
}


void *SimpleDB::getData(const void * const key, const size_t key_size, size_t * const data_size) const
{
	if (last_data_ != NULL) {
		::free(last_data_);
		last_data_ = NULL;
	}

	int size;
	last_data_ = ::tcbdbget(db_, key, key_size, &size);
	if (last_data_ == NULL) {
		if (data_size != NULL)
			*data_size = 0;
		return NULL;
	}

	if (data_size != NULL)
		*data_size = size;

	return last_data_;
}


// SimpleDB::deleteData -- deletes a record from the database.
//
bool SimpleDB::deleteData(const void * const key, const size_t key_size)
{
	return ::tcbdbout(db_, key, key_size);
}


// SimpleDB::find -- determines whether a key exists or not
//
bool SimpleDB::find(const std::string &key) const
{
	Cursor cursor(const_cast<SimpleDB *>(this), key);
	std::string current_key;
	return cursor.getCurrentKey(&current_key) and current_key == key;
}


SimpleDB::const_iterator::const_iterator(Cursor * const cursor)
	: cursor_(cursor)
{
	void *new_key, *new_data;
	unsigned new_key_size, new_data_size;
	if (not cursor_->getKeyAndData(&new_key, &new_key_size, &new_data, &new_data_size))
		key_data_pair_.clear();
	else {
		key_data_pair_.key_.data_  = new_key;
		key_data_pair_.key_.size_  = new_key_size;
		key_data_pair_.data_.data_ = new_data;
		key_data_pair_.data_.size_ = new_data_size;
	}
}


const SimpleDB::const_iterator &SimpleDB::const_iterator::operator++()
{
	if (cursor_->atEnd())
		throw std::runtime_error("SimpleDB::::const_iterator::operator++(int): tried to iterate past end()!");

	if (not cursor_->advance())
		key_data_pair_.clear();
	else {
		void *new_key, *new_data;
		unsigned new_key_size, new_data_size;
		if (unlikely(not cursor_->getKeyAndData(&new_key, &new_key_size, &new_data, &new_data_size)))
			throw std::runtime_error(
			    "in SimpleDB::const_iterator::operator++: this should *never* happen!");

		key_data_pair_.key_.data_  = new_key;
		key_data_pair_.key_.size_  = new_key_size;
		key_data_pair_.data_.data_ = new_data;
		key_data_pair_.data_.size_ = new_data_size;
	}

	return *this;
}
