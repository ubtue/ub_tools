/** \file    Random.cc
 *  \brief   Implementations of random variable related utility functions.
 *  \author  Wagner Truppel
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2009 Project iVia.
 *  Copyright 2004-2009 The Regents of The University of California.
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

#include <Random.h>
#include <algorithm>
#include <stdexcept>
#include <MathUtil.h>
#include <StringUtil.h>
#include <TimeUtil.h>


namespace Random {


void Uniform::init(const double min, const double max, const unsigned * const seed) {
    min_ = min,  max_= max;

    if (seed == NULL)
        ::srandom(static_cast<unsigned>(TimeUtil::GetCurrentTimeInMicroseconds()));
    else
        ::srandom(*seed);
}


void Exponential::init(const double mean, const unsigned * const seed) {
    mean_ = mean;
    if (seed == NULL)
        ::srandom(static_cast<unsigned>(TimeUtil::GetCurrentTimeInMicroseconds()));
    else
        ::srandom(*seed);
}


void Rand::init(const unsigned * const seed) {
    if (seed == NULL)
        ::srandom(static_cast<unsigned>(TimeUtil::GetCurrentTimeInMicroseconds()));
    else
        ::srandom(*seed);
}


void NonUniformRandom::init(const std::vector<double> &distribution) {
    cumulative_distribution_.reserve(distribution.size());
    double cdf(0.0);
    for (std::vector<double>::const_iterator probability(distribution.begin()); probability != distribution.end();
         ++probability)
    {
        if (unlikely(*probability < 0.0 or *probability > 1.0))
            throw std::runtime_error("in Random::NonUniformRandom::NonUniformRandom: invalid probability \""
                                     + StringUtil::ToString(*probability) + "\"!");
        cdf += *probability;
        cumulative_distribution_.push_back(cdf);
    }

    if (not MathUtil::ApproximatelyEqual(cdf, 1.0))
        throw std::runtime_error("in Random::NonUniformRandom::NonUniformRandom: invalid distribution argument "
                                 "(probabilities must add up to 1.0).(cdf: " + StringUtil::ToString(cdf) + ")!");

    cumulative_distribution_[cumulative_distribution_.size() - 1] = 1.0;
}


unsigned NonUniformRandom::operator()(size_t max) {
    const size_t size(cumulative_distribution_.size());
    const std::vector<double>::iterator last(
        ((max == 0) or (max > size)) ? cumulative_distribution_.end() : cumulative_distribution_.begin() + max);
    const std::vector<double>::const_iterator iter(std::lower_bound(cumulative_distribution_.begin(), last,
                                                                    uniform_()));
    return static_cast<unsigned>(iter - cumulative_distribution_.begin());
}


} // namespace Random
