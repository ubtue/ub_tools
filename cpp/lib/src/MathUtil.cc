/** \file   MathUtil.cc
 *  \brief  Implementation for functions used in our classifiers.
 *  \author Wagner Truppel
 *  \author Dr. Johannes Ruscheinski
 *  \author Jason Scheirer
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
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

#include <MathUtil.h>
#include <cfloat>
#include <Compiler.h>


#ifndef DIM
#       define DIM(array)  (sizeof(array)/sizeof(array[0]))
#endif


namespace MathUtil {


void Lbfgs_ComputeNextHg(const unsigned k, const unsigned m, const DequeOfVecsOfDoubles &dx_deque,
                         const DequeOfVecsOfDoubles &dg_deque, const DequeOfReals &rho_deque,
                         VectorOfReals * const _Hg)
{
    VectorOfReals &Hg = *_Hg;
    unsigned bound;

    if (unlikely(k <= m))
        bound = k;
    else
        bound = m;

    VectorOfReals alpha(bound);
    for (unsigned s = 0; s < bound; ++s) {
        const unsigned i = bound - 1 - s;
        alpha[i] = rho_deque[i] * (dx_deque[i] * Hg);
        Hg -= alpha[i] * dg_deque[i];
    }

    VectorOfReals beta(bound);
    for (unsigned i = 0; i < bound; ++i) {
        beta[i] = rho_deque[i] * (dg_deque[i] * Hg);
        Hg += (alpha[i] - beta[i]) * dx_deque[i];
    }
}


bool LineMin_DivisionSucceeded(const real a, const real b, real tolerance, real * const quotient) {
    if (unlikely(tolerance <= 0.0))
        tolerance = REAL_MIN;

    if (likely(ABS(b) > ABS(a) * tolerance)) {
        *quotient = a / b;
        return true;
    } else
        return false;
}


void LineMin_DisplayResults(const LineMinimization::ReturnCode return_code, const LineMinimization::Options &options)
{
    if (unlikely(options.stream_dump_verbosity_ != LineMinimization::NO_OUTPUT)
        and options.dump_stream_.get() != nullptr)
    {
        switch (return_code) {
        case LineMinimization::SUCCESS:
            *options.dump_stream_ << "Line Minimizer succeeded." << '\n';
            break;
        case LineMinimization::INTERVAL_OF_UNCERTAINTY_BELOW_TOLERANCE:
            *options.dump_stream_ << "Line Minimizer halted: interval of uncertainty is below "
                                  << "tolerance." << '\n';
            break;
        case LineMinimization::MAX_ITERS_REACHED:
            *options.dump_stream_ << "Line Minimizer halted: maximum number of iterations "
                                  << "reached." << '\n';
            break;
        case LineMinimization::MIN_STEP_LENGTH_REACHED:
            *options.dump_stream_ << "Line Minimizer halted: minimum step length reached." << '\n';
            break;
        case LineMinimization::MAX_STEP_LENGTH_REACHED:
            *options.dump_stream_ << "Line Minimizer halted: maximum step length reached." << '\n';
            break;
        case LineMinimization::TOLERANCE_TOO_SMALL:
            *options.dump_stream_ << "Line Minimizer halted: failure to make progress; tolerance "
                                  << "may be too small." << '\n';
            break;
        case LineMinimization::NO_REDUCTION:
            *options.dump_stream_ << "Line Minimizer halted: function value not reduced." << '\n';
            break;
        case LineMinimization::POSITIVE_INITIAL_SLOPE:
            *options.dump_stream_ << "Line Minimizer halted: initial slope along search direction "
                                  << "is positive." << '\n';
            break;
        case LineMinimization::TOO_MANY_FUNCTION_EVALUATIONS:
            *options.dump_stream_ << "Line Minimizer halted: too many function evaluations."
                                  << '\n';
            break;
        default:
            *options.dump_stream_ << "Line Minimizer unknown error: this case should never have "
                                  << "happened." << '\n';
        }
    }
}


void LineMin_ComputeNextStep(const real &fv, const real &dg, const real &mu_min, const real &mu_max,
                             bool * const _solution_is_bracketed, real * const _mux, real * const _fx,
                             real * const _dgx, real * const _muy, real * const _fy, real * const _dgy,
                             real * const _mu, const LineMinimization::Options &options)
{
    bool &solution_is_bracketed = *_solution_is_bracketed;
    real &mux = *_mux;
    real  &fx = *_fx;
    real &dgx = *_dgx;
    real &muy = *_muy;
    real  &fy = *_fy;
    real &dgy = *_dgy;
    real  &mu = *_mu;

    // Check input for errors.
    bool on_error = false;
    std::string error_msg("Invalid input(s):\n");

    if (solution_is_bracketed and (mu <= std::min(mux, muy) or mu >= std::max(mux, muy))) {
        on_error = true;
        error_msg += "(1) solution_is_bracketed and (mu <= min(mux, muy) or mu >= max(mux, muy))\n";
    }

    if (dgx * (mu - mux) >= 0.0) {
        on_error = true;
        error_msg += "(2) dgx * (mu - mux) >= 0.0\n";
    }

    if (mu_max < mu_min) {
        on_error = true;
        error_msg += "(3) mu_max < mu_min\n";
    }

    if (on_error) {
        throw std::runtime_error("in MathUtil::LineMin_ComputeNextStep: " + error_msg +
                                 "   mux = " + StringUtil::ToString(mux)      +
                                 "\n    fx = " + StringUtil::ToString(fx)     +
                                 "\n   dgx = " + StringUtil::ToString(dgx)    +
                                 "\n   muy = " + StringUtil::ToString(muy)    +
                                 "\n    fy = " + StringUtil::ToString(fy)     +
                                 "\n   dgy = " + StringUtil::ToString(dgy)    +
                                 "\n    mu = " + StringUtil::ToString(mu)     +
                                 "\n    fv = " + StringUtil::ToString(fv)     +
                                 "\n    dg = " + StringUtil::ToString(dg)     +
                                 "\nmu_min = " + StringUtil::ToString(mu_min) +
                                 "\nmu_max = " + StringUtil::ToString(mu_max) +
                                 "\nsolution_is_bracketed = " + StringUtil::ToString(solution_is_bracketed) + '\n');
    }

    real muf = 0.0;
    bool bounded, division_failed = false;

    // Determine if derivatives have opposite signs.
    real sgnd = dg;
    if (dgx < 0.0)
        sgnd = -dg;

    if (fv > fx) {
        // First case: a higher function value. The solution is bracketed.
        // If the cubic step is closer to mux than the quadratic step, the cubic step
        // is taken. Otherwise, the average of the cubic and quadratic steps is taken.

        if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
            and options.dump_stream_.get() != nullptr)
            *options.dump_stream_ << "LineMin_ComputeNextStep: case 1." << '\n';

        solution_is_bracketed = true;
        bounded = true;
        real theta;
        if ((division_failed = not LineMin_DivisionSucceeded(static_cast<real>(3.0 * (fx - fv)),
                                                             mu - mux, options.zero_division_tol_, &theta)))
        {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                and options.dump_stream_.get() != nullptr)
            {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "1:\ntheta = 3.0 * (fx - fv) / (mu - mux) = "
                                      << 3.0 * (fx - fv) << "/" << mu - mux << '\n';
            }

            goto final_processing;
        }

        theta += dgx + dg;
        real s = std::max(std::max(ABS(theta), ABS(dgx)), ABS(dg));

        real t1;
        if ((division_failed = not LineMin_DivisionSucceeded(theta, s, options.zero_division_tol_, &t1))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "1:\ngamma (t1) = theta / s = "
                                      << theta << "/" << s << '\n';
            }

            goto final_processing;
        }

        real t2;
        if ((division_failed = not LineMin_DivisionSucceeded(dgx, s, options.zero_division_tol_, &t2))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "1:\ngamma (t2) = dgx / s = "
                                      << dgx << "/" << s << '\n';
            }

            goto final_processing;
        }

        real t3;
        if ((division_failed = not LineMin_DivisionSucceeded(dg, s, options.zero_division_tol_, &t3))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "1:\ngamma (t3) = dg / s = "
                                      << dg << "/" << s << '\n';
            }

            goto final_processing;
        }

        real gamma = s * SQRT(t1 * t1 - t2 * t3);
        if (mu < mux)
            gamma = -gamma;
        real p = (gamma - dgx) + theta;
        real q = ((gamma - dgx) + gamma) + dg; // weird... why not 2.0 * gamma + dg - dgx ?
        real r;
        if ((division_failed = not LineMin_DivisionSucceeded(p, q, options.zero_division_tol_, &r))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "1:\nr = p / q = "
                                      << p << "/" << q << '\n';
            }

            goto final_processing;
        }

        real muc = mux + r * (mu - mux);
        if ((division_failed = not LineMin_DivisionSucceeded(fx - fv, mu - mux, options.zero_division_tol_, &t1))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "1:\nmuq (t1) = (fx - fv) / (mu - mux) = "
                                      << fx - fv << "/" << mu - mux << '\n';
            }

            goto final_processing;
        }

        t1 += dgx;
        if ((division_failed = not LineMin_DivisionSucceeded(dgx, t1, options.zero_division_tol_, &t2))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "1:\nmuq (t2) = dgx / t1 = "
                                      << dgx << "/" << t1 << '\n';
            }

            goto final_processing;
        }

        real muq = static_cast<real>(mux + (t2 / 2.0) * (mu - mux));
        if (ABS(muc - mux) < ABS(muq - mux))
            muf = muc;
        else
            muf = static_cast<real>(muc + (muq - muc) / 2.0);
    }
    else if (sgnd < 0.0) {
        // The second case: a lower function value and derivatives of opposite signs.
        // The solution is bracketed. If the cubic step is closer to mux than the
        // quadratic step, the cubic step is taken. Otherwise, the quadratic step is
        // taken.

        if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr)
            *options.dump_stream_ << "LineMin_ComputeNextStep: case 2." << '\n';

        solution_is_bracketed = true;
        bounded = false;
        real theta;
        if ((division_failed = not LineMin_DivisionSucceeded(static_cast<real>(3.0 * (fx - fv)), mu - mux, options.zero_division_tol_, &theta))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "2:\ntheta = 3.0 * (fx - fv) / (mu - mux) = "
                                      << 3.0 * (fx - fv) << "/" << mu - mux << '\n';
            }

            goto final_processing;
        }

        theta += dgx + dg;
        real s = std::max(std::max(ABS(theta), ABS(dgx)), ABS(dg));

        real t1;
        if ((division_failed = not LineMin_DivisionSucceeded(theta, s, options.zero_division_tol_, &t1))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "2:\ngamma (t1) = theta / s = "
                                      << theta << "/" << s << '\n';
            }

            goto final_processing;
        }
        real t2;
        if ((division_failed = not LineMin_DivisionSucceeded(dgx, s, options.zero_division_tol_, &t2))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "2:\ngamma (t2) = dgx / s = "
                                      << dgx << "/" << s << '\n';
            }

            goto final_processing;
        }

        real t3;
        if ((division_failed = not LineMin_DivisionSucceeded(dg, s, options.zero_division_tol_, &t3))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                and options.dump_stream_.get() != nullptr)
            {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "2:\ngamma (t3) = dg / s = "
                                      << dg << "/" << s << '\n';
            }

            goto final_processing;
        }

        real gamma = s * SQRT(t1 * t1 - t2 * t3);
        if (mu > mux)
            gamma = -gamma;
        real p = (gamma - dg) + theta;
        real q = ((gamma - dg) + gamma) + dgx; // weird... why not 2.0 * gamma + dgx - dg ?
        real r;
        if ((division_failed = not LineMin_DivisionSucceeded(p, q, options.zero_division_tol_, &r))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                and options.dump_stream_.get() != nullptr)
            {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "2:\nr = p / q = "
                                      << p << "/" << q << '\n';
            }

            goto final_processing;
        }

        real muc = mu + r * (mux - mu);
        if ((division_failed = not LineMin_DivisionSucceeded(dg, dg - dgx, options.zero_division_tol_, &t1))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                and options.dump_stream_.get() != nullptr)
            {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "2:\nmuq (t1): t1 = dg / (dg - dgx) = "
                                      << dg << "/" << dg - dgx << '\n';
            }

            goto final_processing;
        }

        real muq = mu + t1 * (mux - mu);
        if (ABS(muc - mu) > ABS(muq - mu))
            muf = muc;
        else
            muf = muq;
    } else if (Abs(dg) < Abs(dgx)) {
        // Third case: a lower function value, derivatives of the same sign, and the
        // magnitude of the derivative decreases. The cubic step is used only if the
        // cubic tends to infinity in the direction of the step or it the minimum of
        // the cubic is beyond mu. Otherwise, the cubic step is defined to be either
        // mu_min or mu_max. The quadratic step is also computed and if the solution
        // is bracketed, then the step closest to mu x is taken, otherwise the step
        // farthest away is taken.

        if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
            and options.dump_stream_.get() != nullptr)
            *options.dump_stream_ << "LineMin_ComputeNextStep: case 3." << '\n';

        bounded = true;
        real theta;
        if ((division_failed = not LineMin_DivisionSucceeded(static_cast<real>(3.0 * (fx - fv)), mu - mux, options.zero_division_tol_, &theta))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "3:\ntheta = 3.0 * (fx - fv) / (mu - mux) = "
                                      << 3.0 * (fx - fv) << "/" << mu - mux << '\n';
            }

            goto final_processing;
        }

        theta += dgx + dg;
        real s = std::max(std::max(ABS(theta), ABS(dgx)), ABS(dg));

        real t1;
        if ((division_failed = not LineMin_DivisionSucceeded(theta, s, options.zero_division_tol_, &t1))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                and options.dump_stream_.get() != nullptr)
            {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "3:\ngamma (t1) = theta / s = "
                                      << theta << "/" << s << '\n';
            }

            goto final_processing;
        }

        real t2;
        if ((division_failed = not LineMin_DivisionSucceeded(dgx, s, options.zero_division_tol_, &t2))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                and options.dump_stream_.get() != nullptr)
                {
                    *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                          << "3:\ngamma (t2) = dgx / s = "
                                          << dgx << "/" << s << '\n';
                }

            goto final_processing;
        }

        real t3;
        if ((division_failed = not LineMin_DivisionSucceeded(dg, s, options.zero_division_tol_, &t3))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT) and options.dump_stream_.get() != nullptr) {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "3:\ngamma (t3) = dg / s = "
                                      << dg << "/" << s << '\n';
            }

            goto final_processing;
        }

        real gamma = s * SQRT(std::max(static_cast<real>(0.0), t1 * t1 - t2 * t3));
        if (mu > mux)
            gamma = -gamma;
        real p = (gamma - dg) + theta;
        real q = (gamma + (dgx - dg)) + gamma; // weird... why not 2.0 * gamma + dgx - dg ?
        real r;
        if ((division_failed = not LineMin_DivisionSucceeded(p, q, options.zero_division_tol_, &r))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                and options.dump_stream_.get() != nullptr)
            {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "3:\nr = p / q = "
                                      << p << "/" << q << '\n';
            }

            goto final_processing;
        }

        real muc;
        if (r < 0.0 and gamma != 0.0)
            muc = mu + r * (mux - mu);
        else if (mu > mux)
            muc = mu_max;
        else
            muc = mu_min;

        if ((division_failed = not LineMin_DivisionSucceeded(dg, dg - dgx, options.zero_division_tol_, &t1))) {
            if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                and options.dump_stream_.get() != nullptr)
            {
                *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep case "
                                      << "3:\nmuq (t1) = dg / (dg - dgx) = "
                                      << dg << "/" << dg - dgx << '\n';
            }

            goto final_processing;
        }

        real muq = mu + t1 * (mux - mu);
        if (solution_is_bracketed) {
            if (ABS(muc - mu) < ABS(muq - mu))
                muf = muc;
            else
                muf = muq;
        } else if (ABS(mu - muc) > ABS(mu - muq))
            muf = muc;
        else
            muf = muq;
    } else {
        // Fourth case: a lower function value, derivatives of the same sign, and the
        // magnitude of the derivative does not decrease. If the solution is not
        // bracketed, the step is either mu_min or mu_max. Otherwise, the cubic step
        // is taken.

        if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
            and options.dump_stream_.get() != nullptr)
            *options.dump_stream_ << "LineMin_ComputeNextStep: case 4." << '\n';

        bounded = false;
        if (solution_is_bracketed) {
            real theta;
            if ((division_failed = not LineMin_DivisionSucceeded(static_cast<real>(3.0 * (fv - fy)),
                                                                 muy - mu, options.zero_division_tol_, &theta)))
            {
                if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                    and options.dump_stream_.get() != nullptr)
                {
                    *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep"
                                          << " case 4:\ntheta = 3.0 * (fv - fy) / (muy - mu) = "
                                          << 3.0 * (fv - fy) << "/" << muy - mu << '\n';
                }

                goto final_processing;
            }

            theta += dgy + dg;
            real s = std::max(std::max(ABS(theta), ABS(dgy)), ABS(dg));

            real t1;
            if ((division_failed = not LineMin_DivisionSucceeded(theta, s, options.zero_division_tol_, &t1))) {
                if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                    and options.dump_stream_.get() != nullptr)
                {
                    *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep"
                                          << " case 4:\ngamma (t1) = theta / s = "
                                          << theta << "/" << s << '\n';
                }

                goto final_processing;
            }
            real t2;
            if ((division_failed = not LineMin_DivisionSucceeded(dgy, s, options.zero_division_tol_, &t2))) {
                if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                    and options.dump_stream_.get() != nullptr)
                 {
                     *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep"
                                           << " case 4:\ngamma (t2) = dgy / s = "
                                           << dgy << "/" << s << '\n';
                 }

                goto final_processing;
            }
            real t3;
            if ((division_failed = not LineMin_DivisionSucceeded(dg, s, options.zero_division_tol_, &t3))) {
                if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                    and options.dump_stream_.get() != nullptr)
                {
                    *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep"
                                          << " case 4:\ngamma (t3) = dg / s = "
                                          << dg << "/" << s << '\n';
                }

                goto final_processing;
            }

            real gamma = s * SQRT(t1 * t1 - t2 * t3);
            if (mu > muy)
                gamma = -gamma;
            real p = (gamma - dg) + theta;
            real q = ((gamma - dg) + gamma) + dgy; // weird... why not 2.0 * gamma + dgy - dg ?
            real r;
            if ((division_failed = not LineMin_DivisionSucceeded(p, q, options.zero_division_tol_, &r))) {
                if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
                    and options.dump_stream_.get() != nullptr)
                {
                    *options.dump_stream_ << "Protected division failed in LineMin_ComputeNextStep"
                                          << " case 4:\nr = p / q = "
                                          << p << "/" << q << '\n';
                }

                goto final_processing;
            }

            real muc = mu + r * (muy - mu);
            muf = muc;
        } else if (mu > mux)
            muf = mu_max;
        else
            muf = mu_min;
    }

final_processing:

    // Update the interval of uncertainty.

    if (fv > fx) {
        muy = mu;
        fy = fv;
        dgy = dg;
    } else {
        if (sgnd < 0.0) {
            muy = mux;
            fy = fx;
            dgy = dgx;
        }

        mux = mu;
        fx = fv;
        dgx = dg;
    }

    // Bisect the interval if there was a floating point error in the calculation
    // of muf.

    if (division_failed and solution_is_bracketed)
        muf = static_cast<real>(0.5 * (mux + muy));
    else if (division_failed)
        muf = mu_max;

    // Compute the new step and safeguard it.

    muf = std::min(mu_max, muf);
    muf = std::max(mu_min, muf);
    mu = muf;

    if (solution_is_bracketed and bounded) {
        if (muy > mux)
            mu = std::min(mux + static_cast<real>(0.66) * (muy - mux), mu);
        else
            mu = std::max(mux + static_cast<real>(0.66) * (muy - mux), mu);
    }
}


namespace {


double factorials[] = {
    1.0, // 0!
    1.0, // 1!
    2.0, // 2!
    6.0, // 3!
    24.0, // 4!
    120.0, // 5!
    720.0, // 6!
    5040.0, // 7!
    40320.0, // 8!
    362880.0, // 9!
    3628800.0, // 10!
    39916800.0, // 11!
    479001600.0, // 12!
    6227020800.0, // 13!
    87178291200.0, // 14!
    1307674368000.0, // 15!
    20922789888000.0, // 16!
    355687428096000.0, // 17!
    6402373705728000.0, // 18!
    121645100408832000.0, // 19!
    2432902008176640000.0, // 20!
    51090942171709440000.0, // 21!
    1124000727777607780000.0, // 22!
};


double factorial_logs[] = {
    std::log(1.0), // log(0!)
    std::log(1.0), // log(1!)
    std::log(2.0), // log(2!)
    std::log(6.0), // log(3!)
    std::log(24.0), // log(4!)
    std::log(120.0), // log(5!)
    std::log(720.0), // log(6!)
    std::log(5040.0), // log(7!)
    std::log(40320.0), // log(8!)
    std::log(362880.0), // log(9!)
    std::log(3628800.0), // log(10!)
    std::log(39916800.0), // log(11!)
    std::log(479001600.0), // log(12!)
    std::log(6227020800.0), // log(13!)
    std::log(87178291200.0), // log(14!)
    std::log(1307674368000.0), // log(15!)
    std::log(20922789888000.0), // log(16!)
    std::log(355687428096000.0), // log(17!)
    std::log(6402373705728000.0), // log(18!)
    std::log(121645100408832000.0), // log(19!)
    std::log(2432902008176640000.0), // log(20!)
    std::log(51090942171709440000.0), // log(21!)
    std::log(1124000727777607780000.0), // log(22!)
};


} // unnamed namespace


double Factorial(const unsigned n) {
    if (n < DIM(factorials))
        return factorials[n];

    // Use Stirling's approximation:
    return std::pow(n / M_E, static_cast<double>(n)) * std::sqrt(2.0 * n * M_PI);
}


double LogFactorial(const unsigned n) {
    if (n < DIM(factorials))
        return factorial_logs[n];

    // Use Stirling's approximation:
    static const double LN_PI(std::log(M_PI));
    return (n + 0.5) * std::log(static_cast<double>(n)) - n + 0.5 * (M_LN2 + LN_PI);
}


// InverseNormalCDF -- for implementation details see http://home.online.no/~pjacklam/notes/invnorm/
//
double InverseNormalCDF(const double p) {
    if (unlikely(p < 0.0 or p > 1.0))
        throw std::range_error("in MathUtil::InverseNormalCDF: argument out of range (not in [0..1])!");

    const double A1 = -3.969683028665376e+01;
    const double A2 =  2.209460984245205e+02;
    const double A3 = -2.759285104469687e+02;
    const double A4 =  1.383577518672690e+02;
    const double A5 = -3.066479806614716e+01;
    const double A6 =  2.506628277459239e+00;

    const double B1 = -5.447609879822406e+01;
    const double B2 =  1.615858368580409e+02;
    const double B3 = -1.556989798598866e+02;
    const double B4 =  6.680131188771972e+01;
    const double B5 = -1.328068155288572e+01;

    const double C1 = -7.784894002430293e-03;
    const double C2 = -3.223964580411365e-01;
    const double C3 = -2.400758277161838e+00;
    const double C4 = -2.549732539343734e+00;
    const double C5 =  4.374664141464968e+00;
    const double C6 =  2.938163982698783e+00;

    const double D1 =  7.784695709041462e-03;
    const double D2 =  3.224671290700398e-01;
    const double D3 =  2.445134137142996e+00;
    const double D4 =  3.754408661907416e+00;

    const double P_LOW = 0.02425;
    const double P_HIGH = 1 - P_LOW;


    double result = 0.0, q, r, u, e;
    if (0.0 < p  and p < P_LOW){
        q = std::sqrt(-2*log(p));
        result = (((((C1 * q + C2) * q + C3) * q + C4) * q + C5) * q + C6)
            / ((((D1 * q + D2) * q + D3) * q + D4) * q + 1.0);
    } else{
        if (P_LOW <= p and p <= P_HIGH){
            q = p - 0.5;
            r = q * q;
            result = (((((A1 * r + A2) * r + A3) * r + A4) * r + A5) * r + A6) * q
                / (((((B1 * r + B2) * r + B3) * r + B4) * r + B5) * r + 1.0);
        }
        else if (P_HIGH < p and p < 1.0) {
            q = std::sqrt(-2.0 * std::log(1.0 - p));
            result = -(((((C1 * q + C2) * q + C3) * q + C4) * q + C5) * q + C6)
                / ((((D1 * q + D2) * q + D3) * q + D4) * q + 1.0);
        }
    }

    // Use Halley's rational method (third order) to give full machine precision:
    if (0.0 < p and p < 1.0) {
        e = 0.5 * ::erfc(-result / M_SQRT2) - p;
        u = e * std::sqrt(2.0 * M_PI) * std::exp(result * result / 2.0);
        result = result - u / (1.0 + result * u / 2.0);
    }

    return result;
}


void ExtractExponentAndMantissa(const float f, int * const exponent, int32_t * const mantissa) {
    float f1 = ::frexpf(f, exponent);
    *mantissa = static_cast<int32_t>(::scalbnf(f1, FLT_MANT_DIG));
}


void ExtractExponentAndMantissa(const double d, int * const exponent, int64_t * const mantissa) {
    double d1 = ::frexp(d, exponent);
    *mantissa = static_cast<int64_t>(::scalbn(d1, DBL_MANT_DIG));
}


void ExponentAndMantissaToFloat(const int exponent, const int32_t mantissa, float * const f) {
    float f1 = ::scalbnf(mantissa, -FLT_MANT_DIG);
    *f = ::ldexpf(f1, exponent);
}


void ExponentAndMantissaToDouble(const int exponent, const int64_t mantissa, double * const d) {
    double d1 = ::scalbn(mantissa, -DBL_MANT_DIG);
    *d = ::ldexp(d1, exponent);
}


unsigned GetMinNumberOfBits(uint64_t n) {
    return static_cast<unsigned>(::log2(static_cast<double>(n))) + 1;
}


namespace {


const unsigned long long bits[] = {
    1ull << 0u,
    1ull << 1u,
    1ull << 2u,
    1ull << 3u,
    1ull << 4u,
    1ull << 5u,
    1ull << 6u,
    1ull << 7u,
    1ull << 8u,
    1ull << 9u,
    1ull << 10u,
    1ull << 11u,
    1ull << 12u,
    1ull << 13u,
    1ull << 14u,
    1ull << 15u,
    1ull << 16u,
    1ull << 17u,
    1ull << 18u,
    1ull << 19u,
    1ull << 20u,
    1ull << 21u,
    1ull << 22u,
    1ull << 23u,
    1ull << 24u,
    1ull << 25u,
    1ull << 26u,
    1ull << 27u,
    1ull << 28u,
    1ull << 29u,
    1ull << 30u,
    1ull << 31u,
    1ull << 32u,
    1ull << 33u,
    1ull << 34u,
    1ull << 35u,
    1ull << 36u,
    1ull << 37u,
    1ull << 38u,
    1ull << 39u,
    1ull << 40u,
    1ull << 41u,
    1ull << 42u,
    1ull << 43u,
    1ull << 44u,
    1ull << 45u,
    1ull << 46u,
    1ull << 47u,
    1ull << 48u,
    1ull << 49u,
    1ull << 50u,
    1ull << 51u,
    1ull << 52u,
    1ull << 53u,
    1ull << 54u,
    1ull << 55u,
    1ull << 56u,
    1ull << 57u,
    1ull << 58u,
    1ull << 59u,
    1ull << 50u,
    1ull << 61u,
    1ull << 62u,
    1ull << 63u,
};


} // unnamed namespace


unsigned Log2(const unsigned long long n) {
    for (int leading_bit(63); leading_bit >= 0; --leading_bit) {
        if (n & bits[leading_bit])
            return leading_bit;
    }

    return 0;
}


double Log2(const double n) {
    return std::log(n) / M_LN2;
}


CombinationGenerator::CombinationGenerator(const unsigned n, const unsigned r)
    : n_(n), r_(r), next_combination_(r)

{
    if (unlikely(n == 0))
        throw std::runtime_error("in MathUtil::CombinationGenerator::CombinationGenerator: n cannot be 0!");
    if (unlikely(r > n))
        throw std::runtime_error("in MathUtil::CombinationGenerator::CombinationGenerator: r must be greater than n!");

    for (unsigned i(1); i <= r; ++i)
        next_combination_[i - 1] = i;
}


void CombinationGenerator::advance() {
    if (unlikely(next_combination_.empty())) // This should never happen!
        throw std::runtime_error("in MathUtil::CombinationGenerator::advance: can't advance.  No more combinations!");

    if (unlikely(next_combination_[0] == n_ + 1 - r_)) // // Current combination was the last.
        next_combination_.clear();
    else { // Typical case.
        for (int index_to_advance(r_ - 1); index_to_advance >= 0; --index_to_advance) {
            const unsigned max_value_for_index(n_ - (r_ - 1 - index_to_advance));
            if (next_combination_[index_to_advance] < max_value_for_index) {
                ++next_combination_[index_to_advance];
                for (int index(index_to_advance + 1); index < static_cast<int>(r_); ++index)
                    next_combination_[index] = next_combination_[index - 1] + 1;
                return;
            }
        }
    }
}


void CombinationGenerator::PrintCombination(std::ostream &output, const std::vector<unsigned> &combination) {
    for (std::vector<unsigned>::const_iterator number(combination.begin()); number != combination.end(); ++number)
        output << *number << ' ';
}


unsigned long long Combinations(const unsigned n, const unsigned r) {
    if (r > n)
        return 0;

    unsigned long long c_n_r(1);
    for (unsigned i(n); i > r; --i)
        c_n_r *= i;
    for (unsigned i(n - r); i > 1; --i)
        c_n_r /= i;

    return c_n_r;
}


// KahanSummation -- see http://en.wikipedia.org/wiki/Kahan_summation_algorithm for an explanation.
//
double KahanSummation(const std::vector<double> &x) {
    double sum(0.0), compensation(0.0);
    for (std::vector<double>::const_iterator element(x.begin()); element != x.end(); ++element) {
        volatile const double difference(*element - compensation);
        volatile const double temp(sum + difference);
        compensation = (temp - sum) - difference;
        sum = temp;
    }

    return sum;
}


double RelativeError(const double x, const double y, const double threshold) {
    if (unlikely(threshold <= 0.0))
        throw std::runtime_error("in MathUtil::RelativeError: \"threshold\" parameter must be positive!");

    const double abs_x(std::fabs(x));
    const double abs_y(std::fabs(y));

    if (abs_x < threshold or abs_y < threshold)
        return std::fabs(x - y); // Return an absolute error.

    return std::fabs(x - y) / std::min(abs_x, abs_y);
}


} // namespace MathUtil


real operator*(const SparseVector &u, const VectorOfReals &v) {
    // No point in wasting cycles multiplying against zero!
    if (unlikely(u.getNumOfNonZeroElements() == 0))
        return 0.0;

    // Keep intermediate non-zero summands of the form u[i] * v[i].
    // The number of non-zero summands is at most the number of non-zero elements of u.
    MathUtil::NumericallySafeSum<real> dot_prod_elems(u.getNumOfNonZeroElements());

    for (SparseVector::const_iterator idx_value_pair(u.begin()); idx_value_pair != u.end(); ++idx_value_pair) {
        const real v_value = v[idx_value_pair.getIndex()];
        if (likely(v_value != 0.0))
            dot_prod_elems += v_value * idx_value_pair.getValue();
    }

    return dot_prod_elems.sum();
}
