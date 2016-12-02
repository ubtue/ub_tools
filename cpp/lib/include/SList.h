/** \file     SList.h
 *  \brief    Declaration of template class SList.
 *  \author   Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2006-2008 Project iVia.
 *  Copyright 2006-2008 The Regents of The University of California.
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

#ifndef S_LIST_H
#define S_LIST_H


#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cstddef>
#include "Compiler.h"


template<typename EntryType> class SList {
    struct Node {
        EntryType data_;
        Node *next_;
    public:
        explicit Node(const EntryType &data): data_(data) { }
    };
    Node *head_, *tail_;
    size_t size_;
public:
    typedef const EntryType &const_reference;

    class const_iterator;

    class iterator: public std::iterator<std::forward_iterator_tag, EntryType> {
        friend class SList<EntryType>;
        friend class SList<EntryType>::const_iterator;
        Node *previous_, *current_;
    public:
        iterator(const iterator &rhs): previous_(rhs.previous_), current_(rhs.current_) { }
        iterator(): previous_(nullptr), current_(nullptr) { }
        iterator(const const_iterator &rhs): previous_(rhs.previous_), current_(rhs.current_) { }
        EntryType *operator->() { return &(current_->data_); }
        EntryType &operator*() { return current_->data_; }
        iterator operator++() { previous_ = current_; current_ = current_->next_; return *this; }
        iterator operator++(int) { iterator last(*this); previous_ = current_; current_ = current_->next_; return last; }
        bool operator==(const iterator &rhs) const { return rhs.current_ == current_; }
        bool operator!=(const iterator &rhs) const { return not operator==(rhs); }
    private:
        iterator(Node * const previous, Node * const current): previous_(previous), current_(current) { }
    };

    class const_iterator: public std::iterator<std::forward_iterator_tag, EntryType> {
        friend class SList;
        Node *previous_, *current_;
    public:
        const_iterator(const const_iterator &rhs): previous_(rhs.previous_), current_(rhs.current_) { }
        const_iterator(iterator rhs): previous_(rhs.previous_), current_(rhs.current_) { }
        const EntryType *operator->() const { return &(current_->data_); }
        const EntryType &operator*() const { return current_->data_; }
        const_iterator operator++() { previous_ = current_; current_ = current_->next_; return *this; }
        const_iterator operator++(int) {iterator last(*this); previous_ = current_; current_ = current_->next_; return last; }
        bool operator==(const const_iterator &rhs) const { return rhs.current_ == current_; }
        bool operator!=(const const_iterator &rhs) const { return not operator==(rhs); }
    private:
        const_iterator(Node * const previous, Node * const current): previous_(previous), current_(current) { }
    };
public:
    SList(): head_(nullptr), tail_(nullptr), size_(0) { }
    SList(const SList &rhs);
    virtual ~SList();
    SList &operator=(const SList &rhs);
    bool operator==(const SList &rhs) const;
    void clear();
    bool empty() const { return size_ == 0; }
    size_t size() const { return size_; }
    void swap(SList &other);
    void push_front(const EntryType &new_entry);
    void push_back(const EntryType &new_entry);
    void pop_front();
    iterator begin() { return iterator(nullptr, head_); }
    iterator end() { return iterator(tail_, nullptr); }
    const_iterator begin() const { return const_iterator(nullptr, head_); }
    const_iterator end() const { return const_iterator(tail_, nullptr); }
    iterator insert(iterator where, const EntryType &new_entry);
    iterator erase(iterator where);
    iterator erase(iterator first, iterator last);
    EntryType &front() { return head_->data_; }
    EntryType &back() { return tail_->data_; }
    const EntryType &front() const { return head_->data_; }
    const EntryType &back() const { return tail_->data_; }

    /** \brief  Append the contents of another list to this list.
     *  \param  other_list  The list whose contents we want to append to the current list.
     *  \note   "other_list" will be empty after a call to this member function!
     */
    void append(SList * const other_list);

    /** Uniform randomly shuffles the data in "this" list. */
    void shuffle() { RandomShuffle(begin(), end()); }

    /** Uniform randomly shuffles a range of an SList. */
    static void RandomShuffle(const iterator &first, const iterator &last);
};


template<typename EntryType> SList<EntryType>::SList(const SList &rhs) {
    // Avoid self assignment:
    if (&rhs != this) {
        if (rhs.head_ == nullptr)
            head_ = tail_ = nullptr;
        else {
            Node **last(&head_);
            Node *new_node(nullptr);
            for (Node *node(rhs.head_); node != nullptr; node = node->next_) {
                new_node = new Node(node->data_);
                *last = new_node;
                last = &new_node->next_;
            }
            tail_ = new_node;
            tail_->next_ = nullptr;
        }
        size_ = rhs.size_;
    }
}


template<typename EntryType> SList<EntryType>::~SList() {
    Node *node(head_);
    while (node != nullptr) {
        Node *temp(node);
        node = node->next_;
        delete temp;
    }
}


template<typename EntryType> typename SList<EntryType>::SList &SList<EntryType>::operator=(const SList<EntryType> &rhs) {
    // Avoid self-assignment:
    if (&rhs != this) {
        this->~SList();
        if (rhs.head_ == nullptr)
            head_ = tail_ = nullptr;
        else {
            Node **last(&head_);
            Node *new_node;
            for (Node *node(rhs.head_); node != nullptr; node = node->next_) {
                new_node = new Node(node->data_);
                *last = new_node;
                tail_ = *last;
                last = &new_node->next_;
            }
            tail_ = new_node;
            tail_->next_ = nullptr;
        }
        size_ = rhs.size_;
    }

    return *this;
}


template<typename EntryType> bool SList<EntryType>::operator==(const SList<EntryType> &rhs) const {
    if (head_ == nullptr)
        return rhs.head_ == nullptr;
    else if (size_ != rhs.size_)
        return false;

    Node *rhs_node(rhs.head_), *node(head_);
    for (/* Empty. */; node != nullptr; node = node->next_, rhs_node = rhs_node->next_) {
        if (node->data_ != rhs_node->data_)
            return false;
    }

    return true;
}


template<typename EntryType> void SList<EntryType>::clear() {
    this->~SList();
    head_ = tail_ = nullptr;
    size_ = 0;
}


template<typename EntryType> void SList<EntryType>::swap(SList<EntryType> &other) {
    if (likely(this != &other)) {
        Node *temp(head_);
        head_ = other.head_;
        other.head_ = temp;

        temp  = tail_;
        tail_ = other.tail_;
        other.tail_ = temp;

        size_t temp_size(size_);
        size_ = other.size_;
        other.size_ = temp_size;
    }
}


template<typename EntryType> void SList<EntryType>::push_front(const EntryType &new_entry) {
    Node *new_node(new Node(new_entry));

    // Empty list?
    if (head_ == nullptr)
        tail_ = new_node;

    new_node->next_ = head_;
    head_ = new_node;
    ++size_;
}


template<typename EntryType> void SList<EntryType>::push_back(const EntryType &new_entry) {
    Node *new_node(new Node(new_entry));
    new_node->next_ = nullptr;

    // Empty list?
    if (tail_ == nullptr)
        head_ = new_node;
    else
        tail_->next_ = new_node;

    tail_ = new_node;
    ++size_;
}


template<typename EntryType> void SList<EntryType>::pop_front() {
    if (unlikely(head_ == nullptr))
        throw std::runtime_error("in SList<EntryType>::pop_front: can't pop an empty list!");

    Node *temp(head_);
    head_ = head_->next_;

    // Do we have an empty list now?
    if (head_ == nullptr)
        tail_ = nullptr;

    delete temp;
    --size_;
}


template<typename EntryType> typename SList<EntryType>::iterator SList<EntryType>::insert(SList<EntryType>::iterator where,
                                                                                          const EntryType &new_entry)
{
    if (where == begin()) {
        push_front(new_entry);
        return begin();
    }
    else if (where == end()) {
        Node *temp(tail_);
        push_back(new_entry);
        return iterator(temp, tail_);
    }
    else {
        Node *new_node(new Node(new_entry));
        where.previous_->next_ = new_node;
        new_node->next_ = where.current_;
        ++size_;
        return iterator(where.previous_, new_node);
    }
}


template<typename EntryType> typename SList<EntryType>::iterator SList<EntryType>::erase(SList<EntryType>::iterator where) {
    if (unlikely(head_ == nullptr))
        throw std::runtime_error("in SList<EntryType>::erase: can't erase an element from an empty list!");

    if (where == begin()) {
        pop_front();
        return iterator(nullptr, head_);
    }
    else if (unlikely(where == end()))
        throw std::runtime_error("in SList<EntryType>::erase: can't erase end()!");
    else {
        Node *temp(where.current_);
        where.previous_->next_ = temp->next_;
        if (temp == tail_)
            tail_ = where.previous_;
        delete temp;
        --size_;
        return iterator(where.previous_, where.previous_->next_);
    }
}


template<typename EntryType> typename SList<EntryType>::iterator SList<EntryType>::erase(
    SList<EntryType>::iterator first, SList<EntryType>::iterator last)
{
    while (first != last)
        first = erase(first);

    return first;
}


template<typename EntryType> void SList<EntryType>::append(SList<EntryType> * const other_list) {
    if (other_list->empty())
        return;

    if (head_ == nullptr)
        head_ = other_list->head_;
    else
        tail_->next_ = other_list->head_;
    tail_ = other_list->tail_;
    size_ += other_list->size_;

    other_list->head_ = other_list->tail_ = nullptr;
    other_list->size_ = 0;
}


template<typename EntryType> void SList<EntryType>::RandomShuffle(const SList<EntryType>::iterator &first,
                                                                  const SList<EntryType>::iterator &last)
{
    // 1. Copy data to be shuffled into a random-access container:
    std::vector<EntryType> temp;
    for (iterator i(first); i != last; ++i)
        temp.push_back(i.current_->data_);

    // 2. Perform an O(N) shuffle:
    std::random_shuffle(temp.begin(), temp.end());

    // 3. Overwrite the original data with the shuffled data:
    typename std::vector<EntryType>::const_iterator randomized_i(temp.begin());
    for (iterator i(first); i != last; ++i, ++randomized_i)
        (i.current_)->data_ = *randomized_i;
}


#endif // ifndef S_LIST_H
