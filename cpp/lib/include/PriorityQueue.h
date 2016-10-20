/** \file    PriorityQueue.h
 *  \brief   Implements an enhanced, relative to std::priority_queue, priority queue.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2005-2007 Project iVia.
 *  Copyright 2005-2007 The Regents of The University of California.
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


#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H


#include <algorithm>
#include <vector>


/** \class  PriorityQueue
 *  \brief  Implements a priority queue much like the std::priority_queue STL class.
 *  \note   If you have no need to use the additional functionality, in particular the capability to change queue
 *          members priorities and then call adjust() you should use the STL std::priority_queue class instead.
 */
template<typename ElementType, typename RandomAccessContainer = std::vector<ElementType>,
	 typename CmpFunc = std::less<typename RandomAccessContainer::value_type> > class PriorityQueue
{
    RandomAccessContainer container_;
    CmpFunc cmp_functor_;
public:
    typedef typename RandomAccessContainer::size_type size_type;
    typedef typename RandomAccessContainer::value_type value_type;
    typedef typename RandomAccessContainer::const_reference const_reference;
public:
    explicit PriorityQueue(const RandomAccessContainer &container = RandomAccessContainer(),
                           const CmpFunc &cmp_functor = CmpFunc())
        : container_(container), cmp_functor_(cmp_functor)
        { std::make_heap(container_.begin(), container_.end(), cmp_functor_); }

    template<typename InputIterator> PriorityQueue(InputIterator begin_iter, InputIterator end_iter,
                                                   const CmpFunc &cmp_functor = CmpFunc(),
                                                   const RandomAccessContainer &container = RandomAccessContainer())
        : cmp_functor_(cmp_functor), container_(container)
    {
        container_.insert(container_.end(), begin_iter, end_iter);
        std::make_heap(container_.begin(), container_.end(), cmp_functor_);
    }

    size_type size() const { return container_.size(); }
    bool empty() const { return container_.empty(); }

    void push(const value_type new_value)
    {
        container_.push_back(new_value);
        std::push_heap(container_.begin(), container_.end(), cmp_functor_);
    }

    void pop()
    {
        std::pop_heap(container_.begin(), container_.end(), cmp_functor_);
        container_.pop_back();
    }

    const_reference top() const { return container_.front(); }

    /** Re-adjust the queue after you have altered one or more of the queue member's priority. */
    void adjust() { std::make_heap(container_.begin(), container_.end(), cmp_functor_); }

    RandomAccessContainer &getContainer() { return container_; }
    const RandomAccessContainer &getContainer() const { return container_; }
};


#endif // ifndef PRIORITY_QUEUE_H
