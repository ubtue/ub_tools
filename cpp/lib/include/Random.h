/** \file    Random.h
 *  \brief   Declarations of random variable related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Wagner Truppel
 *  \author  Anthony Moralez
 */

/*
 *  Copyright 2003-2009 Project iVia.
 *  Copyright 2003-2009 The Regents of The University of California.
 *  Copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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

#ifndef RANDOM_H
#define RANDOM_H


#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <sys/time.h>


namespace Random {


class Uniform {
    double min_;
    double max_;
public:
    /** \brief  Constructs an object representing a pseudo-random uniform distribution over the interval
     *          [0.0, 1.0), with an initial seed.
     *  \param  seed  A random number generator seed.
     */
    explicit Uniform(const unsigned seed) { init(0.0, 1.0, &seed); }
    explicit Uniform() { init(0.0, 1.0, NULL); }

    /** \brief  Constructs an object representing a pseudo-random uniform distribution over the interval
     *          [min, max), with an initial seed.
     *  \param  min   The lower end of the distribution.
     *  \param  max   The upper end of the distribution.
     */
    Uniform(const double min, const double max, const unsigned seed) { init(min, max, &seed); }
    Uniform(const double min, const double max) { init(min, max, NULL); }

    /** Returns a pseudo-random deviate uniformly distributed over the interval [min, max). */
    double operator()() { return min_ + (max_ - min_) * ::random() / static_cast<double>(RAND_MAX); }
private:
    Uniform(const Uniform &rhs);		      // Intentionally unimplemented!
    const Uniform &operator=(const Uniform &rhs); // Intentionally unimplemented!
    void init(const double min, const double max, const unsigned * const seed);
};


class Exponential {
    double mean_;
public:
    /** \brief  Constructs an object representing a pseudo-random exponential distribution with a given mean
     *          and an initial seed.
     *  \param  mean  The mean of the exponential distribution.
     *  \param  seed  A random number generator seed.
     */
    Exponential(const double mean, const unsigned seed) { init(mean, &seed); }
    explicit Exponential(const double mean) { init(mean, NULL); }

    /** Returns a pseudo-random deviate exponentially distributed with mean 'mean'. */
    double operator()() { return -mean_ * std::log(::random() / static_cast<double>(RAND_MAX)); }
private:
    Exponential(const Exponential &rhs);		      // Intentionally unimplemented!
    const Exponential &operator=(const Exponential &rhs); // Intentionally unimplemented!
    void init(const double mean, const unsigned * const seed);
};


class Rand {
public:
    /** \brief  Constructs an object representing a pseudo-random uniform distribution with an initial seed, used
     *          to select "uniformly" distributed unsigned integers between 0 and a given integer.
     *  \param  seed  A random number generator seed.
     */
    explicit Rand(const unsigned seed) { init(&seed); }
    Rand() { init(NULL); }

    /** Returns a pseudo-random UNSIGNED INTEGER deviate "uniformly" distributed over the interval [0, n). */
    unsigned operator()(const unsigned n)
        { return static_cast<unsigned>(static_cast<double>(n) * (::random() / static_cast<double>(RAND_MAX))); }
private:
    Rand(const Rand &rhs);                  // Intentionally unimplemented!
    const Rand &operator=(const Rand &rhs); // Intentionally unimplemented!
    void init(const unsigned * const seed);
};


/** \brief   Choose a random sample of items from the range [first, last) and place them at the end of the range.
 *  \param   size   The number of items in the sample.
 *  \param   first  The start of the range to sample from.
 *  \param   last   The end of the range to sample from.
 *  \return  An iterator to the beginning of the random sample, like std::remove().
 *
 *  The container is modified. The random sample at the end of the original container from the returned iterator to the end of the container.
 *  Similar to std::remove.
 */
template <typename RandomAccessIterator> RandomAccessIterator RandomSample(
    const size_t size, RandomAccessIterator first, RandomAccessIterator last)
{
    static Rand rand;
    for (size_t count(size); count > 0; --count)
        std::iter_swap(first + rand(last - first), --last);
    return last;
}


/** \brief   Move a random sample of items in the container to the end of the container.
 *  \param   size       The number of items in the sample.
 *  \param   container  The container to sample from.
 *  \return  An iterator to the beginning of the random sample, like std::remove().
 */
template <typename RandomAccessContainer> typename RandomAccessContainer::iterator RandomSample(
    size_t size, RandomAccessContainer * const container)
{
    return RandomSample(size, container->begin(), container->end());
}


/** \class NonUniformRandom
 *  \brief Used to select numbers from 0 to N-1 using a non-uniform distribution.
 */
class NonUniformRandom {
    Uniform uniform_;
    std::vector<double> cumulative_distribution_;
public:
    /** \brief  Creates a new NonUniformRandom.
     *  \param  distribution  Specifies the probability for each index.  Please note that the sum over all indices
     *                        must add up to 1.0.
     *  \param  seed          A random number generator seed.
     */
    NonUniformRandom(const std::vector<double> &distribution, const unsigned seed): uniform_(seed)
        { init(distribution); }
    explicit NonUniformRandom(const std::vector<double> &distribution) { init(distribution); }

    unsigned operator()(size_t n = 0);
private:
    void init(const std::vector<double> &distribution);
};


/** \brief   Choose a random sample of items from the range [first, last) and place them at the end of the range.
     *  \param   size          The number of items in the sample.
     *  \param   distribution  The probability distribution for the random sample.
     *  \param   first         The start of the range to sample from.
     *  \param   last          The end of the range to sample from.
     *  \param   seed          The seed for the random function.
     *  \return  An iterator to the beginning of the random sample, like std::remove().
     *
     *  The container is modified. The random sample at the end of the original container from the returned
     *  iterator to the end of the container.  Similar to std::remove.
     */
template <typename RandomAccessIterator> RandomAccessIterator RandomSample(
    const size_t size, RandomAccessIterator first, RandomAccessIterator last,
    const std::vector<double> &distribution, const unsigned seed)
{
    NonUniformRandom rand(distribution, seed);
    for (size_t count(size); count > 0; --count)
        std::iter_swap(first + rand(last - first), --last);

    return last;
}


/** \brief   Choose a random sample of items from the range [first, last) and place them at the end of the range.
 *  \param   size          The number of items in the sample.
 *  \param   distribution  The probability distribution for the random sample.
 *  \param   first         The start of the range to sample from.
 *  \param   last          The end of the range to sample from.
 *  \return  An iterator to the beginning of the random sample, like std::remove().
 *
 *  The container is modified. The random sample at the end of the original container from the returned iterator
 *  to the end of the container.  Similar to std::remove.
 */
template <typename RandomAccessIterator> RandomAccessIterator RandomSample(
    const size_t size, RandomAccessIterator first, RandomAccessIterator last,
    const std::vector<double> &distribution)
{
    NonUniformRandom rand(distribution);
    for (size_t count(size); count > 0; --count)
        std::iter_swap(first + rand(last - first), --last);

    return last;
}


/** \brief   Move a random sample of items in the container to the end of the container.
 *  \param   size          The number of items in the sample.
 *  \param   distribution  The probability distribution for the random sample.
 *  \param   container     The container to sample from.
 *  \param   seed          The seed for the random function.
 *  \return  An iterator to the beginning of the random sample, like std::remove().
 */
template <typename RandomAccessContainer> typename RandomAccessContainer::iterator RandomSample(
    const size_t size, RandomAccessContainer * const container, const std::vector<double> &distribution,
    const unsigned seed)
{
    return RandomSample(size, container->begin(), container->end(), distribution, seed);
}


/** \brief  Moves a specified number of elements from one set to another.
 *  \param  size    Number of entries to be moved from "source" to "taget."
 *  \param  source  Container to remove entries from.
 *  \param  target  Container to remove entries to.
 *  \note   Does not clear "target" before adding elements to it.
 */
    template <typename Entry> void RemoveRandomSample(const size_t size, std::unordered_set<Entry> * const source,
                                                      std::unordered_set<Entry> * const target)
{
    if (unlikely(size > source->size()))
        throw std::runtime_error("in Random::RandomSample: source container is too small for request sample size!");

    // Create a random vector of "source->size()" booleans that have "size" entries that are true:
    std::vector<bool> selections(source->size());
    for (unsigned i(0); i < size; ++i)
        selections[i] = true;
    std::random_shuffle(selections.begin(), selections.end());

    // Now move those entries of "source" to "target" that coincide with "true" entries of "selections":
    std::vector<bool>::const_iterator selected(selections.begin());
    for (typename std::unordered_set<Entry>::iterator entry(source->begin()); entry != source->end(); ++selected) {
        if (*selected) {
            target->insert(*entry);
            source->erase(entry++);
        } else
            ++entry;
    }
}


} // namespace Random


#endif // define RANDOM_H
