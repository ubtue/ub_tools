/** \file    SimpleDB.h
 *  \brief   Declaration of class SimpleDB
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2007 Project iVia.
 *  Copyright 2002-2007 The Regents of The University of California.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#pragma once


#include <string>
#include <cstdio>
#include <tcbdb.h>


/** \class  SimpleDB
 *  \brief  A wrapper class for the tokyocabinet database.
 */
class SimpleDB {
        const char *db_name_;
        TCBDB *db_;
        mutable int last_error_;
        mutable void *last_data_;

        /** The number of currently open databases */
        static unsigned open_count_;

        class Cursor {
                BDBCUR *cursor_;
                std::string db_name_; // Used for error messages.
                bool at_end_;
        public:
                Cursor(): cursor_(nullptr), at_end_(true) { }

                /** \brief  Initializes a new Cursor object.
                 *  \param  simple_db    The database for which we'd like to create a cursor.
                 *  \param  initial_key  If not empty, the new cursor will be positioned at this key, or if no such key exists at the next closest key.
                 */
                explicit Cursor(SimpleDB * const simple_db, const std::string &initial_key = "");

                ~Cursor();
                bool advance();
                bool getCurrentKey(std::string * const current_key) const;
                bool atEnd() const { return at_end_; }

                /** \warning  Keys and data returned from this will eventually have to be free'd! */
                bool getKeyAndData(void ** const new_key, unsigned *new_key_size, void ** const new_data, unsigned *new_data_size);
        };
public:
        struct Data {
                void *data_;
                unsigned size_;
        public:
                Data(): data_(nullptr), size_(0) { }
                Data(void * const data, const unsigned size): data_(data), size_(size) { }
                ~Data(); // Calls std::free(3) on _data
                void clear();
        };
        struct KeyDataPair {
                friend class const_iterator;
                Data key_, data_;
        public:
                KeyDataPair() { }

                /** Return the key as a string. */
                const char *key() const { return reinterpret_cast<const char *>(key_.data_); }

                const void *data() const { return data_.data_; }

                void clear();
        private:
                KeyDataPair(const KeyDataPair &rhs);            // Intentionally unimplemented!
                KeyDataPair &operator=(const KeyDataPair &rhs); // Intentionally unimplemented!
                KeyDataPair(void * const new_key, const unsigned new_key_size, void * const new_data, const unsigned new_data_size)
                        : key_(new_key, new_key_size), data_(new_data, new_data_size) { }
        };
public:
        class const_iterator {
                friend class SimpleDB;
                mutable Cursor *cursor_;
                KeyDataPair key_data_pair_;
        public:
                const_iterator(const const_iterator &rhs): cursor_(rhs.cursor_) { rhs.cursor_ = nullptr; }
                ~const_iterator() { delete cursor_; }
                const const_iterator &operator++();

                /** Bogus hack! */
                bool operator==(const const_iterator &rhs) const { return cursor_->atEnd() == rhs.cursor_->atEnd(); }
                bool operator!=(const const_iterator &rhs) const { return !operator==(rhs); }

                const KeyDataPair &operator*() const { return key_data_pair_; }
                const KeyDataPair *operator->() const { return &key_data_pair_; }
        private:
                explicit const_iterator(Cursor * const cursor);
        };


        friend class SimpleDB::Cursor;
        friend class SimpleDB::const_iterator;

        enum OpenMode { OPEN_CREATE, OPEN_CREATE_READ_WRITE, OPEN_RDONLY, OPEN_READ_WRITE };
        enum Type { BTREE, HASH };
private:
        mutable KeyDataPair key_data_pair_;
        const Type type_;
public:
        /** \brief  The constructor is used to create or open a SimpleDB.
         *  \param  db_file_name  The file that contains the Berkeley DB database.
         *  \param  open_mode     The types of operations allowed on the database.
         *  \param  mode
         *  \param  type          The type of database (b-tree or hash).
         */
        SimpleDB(const std::string &db_file_name, const OpenMode open_mode, const int mode = 0664, const Type type = BTREE);
        SimpleDB(const char * const db_file_name, const OpenMode open_mode, const int mode = 0664, const Type type = BTREE);
        ~SimpleDB();

        /** \brief    Deletes all entries from a SimpleDB.
         *  \warning  It is an error to call this function on a database with open cursors!
         */
        void clear();

        void close();
        void flush();

        /** Get the name of this database file. */
        std::string getFileName() const { return db_name_; }

        /** \brief Add or update a record in a Bekeley DB database
         *  \param key         The key of the item to add or update.
         *  \param key_size    The size (in bytes) of the key.
         *  \param data        The new data value.
         *  \param data_size   The size of the full data value (???).
         */
        void putData(const void * const key, const size_t key_size, const void * const data, const size_t data_size);

        void putData(const std::string &key, const void * const data, const size_t data_size) { putData(key.c_str(), key.length(), data, data_size); }

        void putData(const std::string &key, const std::string &data) { putData(key.c_str(), key.length(), data.c_str(), data.length()); }

        /** Makes no assumptions about "key" or "binary_data" being zero-terminated. */
        void binaryPutData(const std::string &key, const std::string &binary_data)
                { putData(key.data(), key.size(), binary_data.data(), binary_data.size()); }

        /** \brief    Retrieve all or part of a record from a Berkeley DB table
         *  \param    key        The key of the item to retrieve.
         *  \param    key_size   The size (in bytes) of the key.
         *  \param    data_size  If non-nullptr, the returned data size will be stored here.
         *  \return   A pointer to the returned data.  Do not free(3) or delete[] this!
         *  \warning  Caution: you must *not* "free" or "delete" the returned memory when you are done with it!
         */
        void *getData(const void * const key, const size_t key_size, size_t * const data_size = nullptr) const;

        void *getData(const std::string &key, size_t * const data_size = nullptr) const { return getData(key.c_str(), key.length(), data_size); }

        /** Makes no assumptions about "key" being zero-terminated. */
        void *binaryGetData(const std::string &key, size_t * const data_size = nullptr) const { return getData(key.c_str(), key.length(), data_size); }

        /** \brief  Delete a record from a Berkeley DB table
         *  \param  key       The key of the item to be deleted.
         *  \param  key_size  The size, in bytes, of key.
         *  \return True if the key was in the database and was deleted, false if the key was not in the database.  If an error occurs, an exception is
         *          thrown.
         */
        bool deleteData(const void * const key, const size_t key_size);

        /** \brief  Delete a record from a Berkeley DB table
         *  \param  key  The key of the item to be deleted.
         *  \return True if the key was in the database and was deleted, false if the key was not in the database.  If an error occurs, an exception is
         *          thrown.
         */
        bool deleteData(const std::string &key) { return deleteData(key.data(), key.size()); }

        /** \brief  Verifies that a key exists in the database.
         *  \param  key  The key of the item to be found.
         *  \return True if the key was in the database, false if the key was not in the database.
         */
        bool find(const std::string &key) const;

        const_iterator begin(const std::string &initial_key = "") const { return const_iterator(new Cursor(const_cast<SimpleDB *>(this), initial_key)); }
        const_iterator end() const { return const_iterator(new Cursor); }

        std::string getLastError() const { return ::tcbdberrmsg(last_error_); }

        static unsigned GetOpenCount() { return open_count_; }
private:
        void throwError(const char * const function_name, const char * const msg = nullptr) const;
        SimpleDB(const SimpleDB &rhs);                  // Intentionally unimplemented!
        const SimpleDB &operator=(const SimpleDB &rhs); // Intentionally unimplemented!
        void init(const OpenMode open_mode, int mode);
};
