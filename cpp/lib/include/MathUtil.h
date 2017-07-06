/** \file   MathUtil.h
 *  \brief  Declarations for functions used in our classifiers.
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

#ifndef MATH_UTIL_H
#define MATH_UTIL_H


#include <algorithm>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <cfloat>
#include <cstddef>
#include <Real.h>
#include <SparseMatrixAndVector.h>
#include <SparseVector.h>
#include <StlHelpers.h>
#include <StringUtil.h>
#include <VariableStats.h>
#include <VectorOfReals.h>
#include <WallClockTimer.h>


#ifdef min
#      undef min
#endif
#ifdef max
#      undef max
#endif
#ifdef Abs
#      undef Abs
#endif


/** \brief  Helper function used when adding numbers in a numerically safe way. Refer to class
 *          MathUtil::NumericallySafeSum for details.
 */
inline bool SortedByIncreasingMagnitudeHelper(const float value1, const float value2)
    { return ::fabsf(value1) < ::fabsf(value2); }


/** \brief  Helper function used when adding numbers in a numerically safe way. Refer to class
 *          MathUtil::NumericallySafeSum for details.
 */
inline bool SortedByIncreasingMagnitudeHelper(const double value1, const double value2)
    { return std::fabs(value1) < std::fabs(value2); }


/** \brief  Helper function used when adding numbers in a numerically safe way. Refer to class
 *          MathUtil::NumericallySafeSum for details.
 */
inline bool SortedByIncreasingMagnitudeHelper(const long double value1, const long double value2)
    { return ::fabsl(value1) < ::fabsl(value2); }


/** \brief  Helper function used when adding numbers in a numerically safe way. Refer to class
 *          MathUtil::NumericallySafeSum for details.
 */
template<typename FloatingPointType> inline bool SortedByIncreasingMagnitude(const FloatingPointType value1,
                                                                             const FloatingPointType value2)
    { return SortedByIncreasingMagnitudeHelper(value1, value2); }


/** \brief  Returns the dot product between a sparse vector and a vector of doubles. */
real operator*(const SparseVector &u, const VectorOfReals &v);


/** \brief  Returns the dot product between a vector of doubles and a sparse vector. */
inline real operator*(const VectorOfReals &u, const SparseVector &v) { return v * u; }


/** \namespace  MathUtil
 *  \brief      Functions used by our classifiers.
 */
namespace MathUtil {


/** \namespace  LineMinimization
 *  \brief      Functions used by the line minimizer algorithm.
 */
namespace LineMinimization {


enum OutputVerbosity { NO_OUTPUT, SUMMARY_AT_END_OF_EXECUTION, SUMMARY_AT_EACH_ITERATION };


enum ReturnCode { SUCCESS, INTERVAL_OF_UNCERTAINTY_BELOW_TOLERANCE, MAX_ITERS_REACHED, MIN_STEP_LENGTH_REACHED,
                  MAX_STEP_LENGTH_REACHED, TOLERANCE_TOO_SMALL, NO_REDUCTION, POSITIVE_INITIAL_SLOPE,
                  TOO_MANY_FUNCTION_EVALUATIONS };


/** \brief  Defines a number of optional parameters used by the line minimizer algorithm.
  *
  * \param  min_step			A non-negative optional parameter with default value 1.0e-20, specifying the
  *					minimum acceptable step value.
  *
  * \param  max_step			A non-negative optional parameter with default value 1.0e+20, specifying the
  *					maximum acceptable step value.
  *
  * \param  min_interval_length		The smallest length admissible for the interval of uncertainty; the line
  *                                     search will halt if the length of the interval of uncertainty falls below
  *                                     this value.  Its default value is the square root of the machine-dependent
  *                                     epsilon.
  *
  * \param  max_function_evals		A positive optional parameter with default value 20, specifying the maximum
  *					number of function evaluations.
  *
  * \param  bracket_increase_factor	A positive optional parameter with default value 4.0, specifying how much to
  *                                     increase the bracketing interval when backtracking.
  *
  * \param  function_decrease_tol	The function decrease tolerance, a non-negative optional parameter with
  *                                     default	value 1.0e-4, controlling the accuracy of the line search algorithm.
  *
  * \param  slope_decrease_tol		The slope decrease tolerance, a non-negative optional parameter with default
  *					value 9.0e-1, controlling the accuracy of the line search algorithm. If the
  *					function and gradient evaluations are inexpensive with respect to the cost of
  *					the iteration (which is sometimes the case when solving very large problems)
  *                                     it may be advantageous to set slope_decrease_tol to a smaller value, typically
  *                                     1.0e-1. Restriction: slope_decrease_tol should be greater than
  *                                     function_decrease_tol.
  *
  * \param  zero_division_tol		The tolerance for dealing with divisions by zero, ie, when to consider a
  *                                     division by a small number to be equivalent to a division by zero. Its default
  *                                     values is REAL_MIN.
  *
  * \param  display_verbosity		The level of verbosity to use when displaying results on screen.
  *
  * \param  display_precision		The number of digits of precision to use when displaying results on screen.
  *
  * \param  dump_stream			The stream to which results should be dumped.
  *
  * \param  stream_dump_verbosity	The level of verbosity to use when dumping results to a stream.
  *
  * \param  stream_dump_precision	The number of digits of precision to use when dumping results to a stream.
  */
struct Options {
    real min_step_;
    real max_step_;
    real min_interval_length_;
    unsigned max_function_evals_;
    real bracket_increase_factor_;
    real function_decrease_tol_;
    real slope_decrease_tol_;
    real zero_division_tol_;
    std::auto_ptr<std::ostream> dump_stream_;
    OutputVerbosity stream_dump_verbosity_;
    int stream_dump_precision_;
public:
    Options()
        : min_step_(static_cast<real>(1.0e-20)), max_step_(static_cast<real>(1.0e+20)),
          min_interval_length_(SQRT(REAL_EPSILON)), max_function_evals_(10),
          bracket_increase_factor_(static_cast<real>(4.0)), function_decrease_tol_(static_cast<real>(1.0e-4)),
          slope_decrease_tol_(static_cast<real>(9.0e-1)), zero_division_tol_(REAL_MIN),
          stream_dump_verbosity_(NO_OUTPUT), stream_dump_precision_(6) {}
};


} // namespace LineMinimization


typedef std::deque<VectorOfReals> DequeOfVecsOfDoubles;
typedef std::deque<real> DequeOfReals;


/** \brief  Uses an efficient recursive algorithm to compute the product of the inverse Hessian matrix
  *         and the gradient vector, using the L-BFGS method. In the description below, k' has the value
  *         k when k <= m, but the value m when k >= m.
  *
  * IN arguments:
  *
  * \param  k		The current iteration index.
  * \param  m	  	The storage factor.
  * \param  dx_deque	The most recent k' dx vectors, stored into a deque.
  * \param  dg_deque	The most recent k' dg vectors, stored into a deque.
  * \param  rho_deque	The most recent k' rho values, stored into a deque.
  *
  * OUT arguments:
  *
  * \param  _Hg		The next value of Hg.
  */
void Lbfgs_ComputeNextHg(const unsigned k, const unsigned m, const DequeOfVecsOfDoubles &dx_deque, const DequeOfVecsOfDoubles &dg_deque,
			 const DequeOfReals &rho_deque, VectorOfReals * const _Hg);


/** \brief  Computes the quotient a/b, being careful to trap division-by-zero (overflow) and NaN conditions.
  *         Returns true when the quotient is a proper result (not a NaN) and not overflown, false otherwise.
  *
  * \param  a		The numerator.
  * \param  b		The denominator.
  * \param  tolerance	A value specifying when to consider the division to have failed.
  * \param  quotient	The result of the division of a by b.
  */
bool LineMin_DivisionSucceeded(const real a, const real b, real tolerance, real * const quotient);


void LineMin_DisplayResults(const LineMinimization::ReturnCode return_code, const LineMinimization::Options &options);


void LineMin_ComputeNextStep(const real &fv, const real &dg, const real &mu_min, const real &mu_max,
                             bool * const _solution_is_bracketed, real * const _mux, real * const _fx,
                             real * const _dgx, real * const _muy, real * const _fy, real * const _dgy,
                             real * const _mu, const LineMinimization::Options &options);


/** \brief  Performs a line minimization using the algorithm of More' and Thuente. This implementation is
  *	    based on that of the Hilbert Class Library (http://www.trip.caam.rice.edu/txt/hcldoc/html/).
  *
  *	    The purpose of LineMinimizer is to find a step which satisfies a sufficient decrease condition and a
  *         curvature condition. At each stage the subroutine updates an interval of uncertainty with endpoints
  *         stx and sty. The interval of uncertainty is initially chosen so that it contains a minimizer of the
  *         modified function
  *
  *		f(x + step * dir) - f(x) - function_decrease_tol * step * <gradf(x), dir>
  *
  *	    If a step is obtained for which the modified function has a nonpositive function value and nonnegative
  *	    derivative, then the interval of uncertainty is chosen so that it contains a minimizer of
  *
  *		f(x + step * dir).
  *
  *	    The algorithm is designed to find a step which satisfies the sufficient decrease condition
  *
  *		f(x + step * dir) <= f(x) + function_decrease_tol * step * <gradf(x), dir>
  *
  *	    and the curvature condition
  *
  *		|<gradf(x + step * dir), dir>| <= slope_decrease_tol * |<gradf(x), dir>|.
  *
  *	    If function_decrease_tol is less than slope_decrease_tol and if, for example, the function is bounded
  *         from below, then there is always a step which satisfies both conditions. If no step can be found which
  *         satisfies both conditions, then the algorithm usually stops when rounding errors prevent further progress.
  *         In this case, step satisfies only the sufficient decrease condition.
  *
  * TERMINATION:
  *
  *	    Termination occurs when any of the following conditions applies:
  *
  *	(1) on success: a finite step is found that minimizes the function along the search direction.
  *
  *	(2) when the length of the interval of uncertainty is smaller than the tolerance.
  *
  *	(3) when the number of iterations exceeds the maximum allowed.
  *
  *	(4) when the minimum step length is reached.
  *
  *	(5) when the maximum step length is reached.
  *
  *	(6) when the tolerance is too small to allow for progress to be made.
  *
  *	(7) when no reduction in the function value is achieved.
  *
  *	(8) when the initial slope along the search direction is positive.
  *
  *	(9) when the maximum number of function evaluations is exceeded.
  *
  * IN arguments:
  *
  * \param  _f		A functor used to compute the value of the function to be minimized.
  *
  * \param  _g		A functor used to compute the gradient of the function to be minimized.
  *
  * IN/OUT arguments:
  *
  * \param  _x		On input it must contain the base point for the line search. On output it contains
  *                     x + step * dir.
  *
  * \param  _dir	A vector specifying what direction to perform the line minimization along.
  *
  * \param  _step	A non-negative value specifying a satisfactory step along the search direction. On input
  *			it must contain an initial estimate; on output it contains the final estimate.
  *
  * OPTIONAL arguments:
  *
  * \param  options	An object encapsulating the various options for the line minimization algorithm.
  */
template<typename FunctionFunc, typename GradientFunc>
void LineMinimizer(FunctionFunc * const _f, GradientFunc * const _g, VectorOfReals * const _x,
		   VectorOfReals * const _dir, real * const _step, const LineMinimization::Options &options)
{
    FunctionFunc &f = *_f;
    GradientFunc &g = *_g;

    VectorOfReals &dir = *_dir;
    VectorOfReals &x = *_x;
    real &step = *_step;

    if (unlikely(options.stream_dump_verbosity_ != LineMinimization::NO_OUTPUT)
        and options.dump_stream_.get() != NULL)
        *options.dump_stream_ << "============= Line Minimizer =============" << '\n';

    // Compute the length of the Newton step then scale the search direction, if necessary.
    real newton_length = SQRT(dir * dir);

    if (newton_length > options.max_step_) {
        real scale;
        if (not LineMin_DivisionSucceeded(options.max_step_, newton_length, options.zero_division_tol_,
                                          &scale))
        {
            if (unlikely(options.stream_dump_verbosity_ != LineMinimization::NO_OUTPUT)
                and options.dump_stream_.get() != NULL)
            {
                *options.dump_stream_ << "Error in LineMin_DivisionSucceeded: newton_length too small. "
                                      << "Terminated with no reduction in function value." << '\n';
            }

            LineMin_DisplayResults(LineMinimization::TOLERANCE_TOO_SMALL, options);
            throw std::runtime_error("in MathUtil::LineMinimizer: newton_length too small. Terminated with no "
                                     "reduction in function value.");
        }

        dir *= scale;
        newton_length = options.max_step_;

        if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
            and options.dump_stream_.get() != NULL)
        {
            *options.dump_stream_ << "Search direction vector is too long and will be multiplied by "
                                  << scale << "." << '\n';
        }
    }

    // Compute initial slope; exit if not negative.

    VectorOfReals grad_at_cur_x(x.size());
    g(x, &grad_at_cur_x);
    real initial_directional_gradient = grad_at_cur_x * dir;

    if (initial_directional_gradient >= 0.0) {
        if (unlikely(options.stream_dump_verbosity_ != LineMinimization::NO_OUTPUT)
            and options.dump_stream_.get() != NULL)
        {
            *options.dump_stream_ << "Non-negative initial directional gradient in line search. "
                                  << "initial_directional_gradient: "
                                  << initial_directional_gradient << '\n';
        }

        LineMin_DisplayResults(LineMinimization::POSITIVE_INITIAL_SLOPE, options);
        throw std::runtime_error("in MathUtil::LineMinimizer: Non-negative initial directional gradient in line "
                                 "search.  Terminated with no reduction in function value.");
    }

    real min_mu, max_mu;
    if (not LineMin_DivisionSucceeded(options.min_step_, newton_length, options.zero_division_tol_, &min_mu) or
        not LineMin_DivisionSucceeded(options.max_step_, newton_length, options.zero_division_tol_, &max_mu)) {
        if (unlikely(options.stream_dump_verbosity_ != LineMinimization::NO_OUTPUT)
            and options.dump_stream_.get() != NULL)
        {
            *options.dump_stream_ << "Protected division failed while computing either min_mu or max_mu. "
                                  << options.min_step_ << "/" << newton_length << ", "
                                  << options.max_step_ << "/" << newton_length << '\n';
        }

        LineMin_DisplayResults(LineMinimization::NO_REDUCTION, options);
        throw std::runtime_error("in LineMinimizer: Protected division failed while computing either min_mu or "
                                 "max_mu.");
    }

    real initial_function_value = f(x);

    if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
        and options.dump_stream_.get() != NULL)
    {
        *options.dump_stream_ << "||x|| = " << SQRT(x * x) << '\n';
        *options.dump_stream_ << " f(x) = " << initial_function_value << '\n';
        *options.dump_stream_ << "||dir|| = " << newton_length << '\n';
        *options.dump_stream_ << "initial directional gradient = " << initial_directional_gradient << '\n';
        *options.dump_stream_ << "maximum step to boundary = " << REAL_MAX << '\n';
        *options.dump_stream_ << "minimum mu = " << min_mu << '\n';
        *options.dump_stream_ << "maximum mu = " << max_mu << '\n';
    }

    unsigned num_function_evals = 0;
    bool solution_is_bracketed = false;
    bool algorithm_in_stage_one = true;
    real directional_gradient_test = options.function_decrease_tol_ * initial_directional_gradient;
    real interval_width = options.max_step_ - options.min_step_;
    real twice_the_interval_width = 2.0 * interval_width;

    real mux = 0.0;
    real fx = initial_function_value;
    real dgx = initial_directional_gradient;

    real muy = 0.0;
    real fy = initial_function_value;
    real dgy = initial_directional_gradient;

    real mu = 1.0;
    mu = std::min(mu, max_mu);
    mu = std::max(mu, min_mu);
    real mu_min, mu_max;

    real fv, dg;

    for (unsigned iter(0); iter < 10000; ++iter) {
        // Set the min and max steps to correspond to the present interval of uncertainty.
        if (solution_is_bracketed) {
            mu_min = std::min(mux, muy);
            mu_max = std::max(mux, muy);
        }
        else {
            mu_min = mux;
            mu_max = mu + (mu - mux) * options.bracket_increase_factor_;
        }

        // Force the step to be within bounds.
        mu = std::max(mu, min_mu);
        mu = std::min(mu, max_mu);

        // In case of failure, let mu correspond to the best point.
        if ((solution_is_bracketed and (mu <= mu_min or mu >= mu_max))
            or num_function_evals >= options.max_function_evals_ - 1)
            mu = mux;

        // Evaluate the function and gradient at mu.

        VectorOfReals next_x(x);
        next_x += mu * dir;

        fv = f(next_x);
        ++num_function_evals;

        VectorOfReals grad_at_next_x(x.size());
        g(next_x, &grad_at_next_x);
        dg = grad_at_next_x * dir;

        real function_test_one = initial_function_value + mu * directional_gradient_test;

        if (unlikely(options.stream_dump_verbosity_ > LineMinimization::NO_OUTPUT)
            and options.dump_stream_.get() != NULL)
        {
            *options.dump_stream_ << "num of function evals: " << num_function_evals << '\n';
            *options.dump_stream_ << "                   mu: " << mu << '\n';
            *options.dump_stream_ << "       function value: " << fv << '\n';
            *options.dump_stream_ << "                slope: " << dg << '\n';
        }

        // Check for convergence.

        LineMinimization::ReturnCode return_code = LineMinimization::SUCCESS;
        if (solution_is_bracketed and (mu <= mu_min or mu >= mu_max))
            return_code = LineMinimization::TOLERANCE_TOO_SMALL;
        else if (mu == max_mu and fv <= function_test_one and dg <= directional_gradient_test)
            return_code = LineMinimization::MAX_STEP_LENGTH_REACHED;
        else if (mu == min_mu and (fv > function_test_one or dg >= directional_gradient_test))
            return_code = LineMinimization::MIN_STEP_LENGTH_REACHED;
        else if (num_function_evals >= options.max_function_evals_)
            return_code = LineMinimization::MAX_ITERS_REACHED;
        else if (solution_is_bracketed and mu_max - mu_min <= options.min_interval_length_ * mu_max)
            return_code = LineMinimization::INTERVAL_OF_UNCERTAINTY_BELOW_TOLERANCE;

        if (return_code != LineMinimization::SUCCESS) {
            LineMin_DisplayResults(return_code, options);
            if (fv >= initial_function_value)
                throw std::runtime_error("in MathUtil::LineMinimizer: No reduction in function value.");
        }

        if (fv <= function_test_one and
            ABS(dg) <= options.slope_decrease_tol_ * (- initial_directional_gradient))
        {
            LineMin_DisplayResults(LineMinimization::SUCCESS, options);
            x = next_x;
            step = mu;
            return;
        }

        // In the first stage, we seek a step for which the modified function has a non-positive
        // value and non-negative derivative.

        if (algorithm_in_stage_one and fv <= function_test_one and
            dg >= std::min(options.function_decrease_tol_, options.slope_decrease_tol_) *
            initial_directional_gradient)
            algorithm_in_stage_one = false;

        // We use the modified function to predict the step only if we do not have a step for which
        // the modified function has a non-positive function value and non-negative derivative, and if
        // a lower function value has been obtained but the decrease is not sufficient.

        if (algorithm_in_stage_one and fv <= fx and fv >= function_test_one) {
            // Define the modified function and derivative values.
            real fm   = fv -  mu * directional_gradient_test;
            real fxm  = fx - mux * directional_gradient_test;
            real fym  = fy - muy * directional_gradient_test;
            real dgm  = dg  - directional_gradient_test;
            real dgxm = dgx - directional_gradient_test;
            real dgym = dgy - directional_gradient_test;

            // Update the interval of uncertainty and compute the next step.
            LineMin_ComputeNextStep(fm, dgm, mu_min, mu_max, &solution_is_bracketed, &mux, &fxm, &dgxm,
                                    &muy, &fym, &dgym, &mu, options);

            // Reset the function and derivative values.
            fx  = fxm + mux * directional_gradient_test;
            fy  = fym + muy * directional_gradient_test;
            dgx = dgxm + directional_gradient_test;
            dgy = dgym + directional_gradient_test;
        } else // Update the interval of uncertainty and compute the next step.
            LineMin_ComputeNextStep(fv, dg, mu_min, mu_max, &solution_is_bracketed, &mux, &fx, &dgx,
                                    &muy, &fy, &dgy, &mu, options);

        // Force a sufficient decrease in the size of the interval of uncertainty.
        if (solution_is_bracketed) {
            if (ABS(muy - mux) >= 0.66 * twice_the_interval_width)
                mu = mux + 0.5 * (muy - mux);
            twice_the_interval_width = interval_width;
            interval_width = ABS(muy - mux);
        }
    }
}


/** \brief  Uses the Limited-memory Broyden-Fletcher-Goldfarb-Shanno (L-BFGS) method to find the minimum of
  *         a function of n variables, given some derivative information and an initial guess for
  *         the location of the minimum.
  *
  * IN arguments:
  *
  * \param  m			The storage index, ie, the maximum number of vector pairs to store.
  *
  * \param  initial_step	A non-negative value specifying a satisfactory inirial step along the search direction.
  *
  * \param  _f			A functor used to compute the value of the function to be minimized.
  *
  * \param  _grad_f		A functor used to compute the gradient of the function to be minimized.
  *
  * IN/OUT arguments:
  *
  * \param  _x			The n-dimensional position of the minimum. On input, it holds an initial guess. On
  *                             output, it holds the final estimate of the position of the minimum.
  *
  * OUT arguments:
  *
  * \param  line_minimizer_time_stats           On exit, this variable contains statistical information on the time
  *                                             spent in the LineMinimizer function.
  *
  * \param  lbfgs_compute_next_hg_time_stats    On exit, this variable contains statistical information on the time
  *                                             spent in the Lbfgs_ComputeNextHg function.
  *
  * OPTIONAL arguments:
  *
  * \param  line_min_options	An object encapsulating the various options for the line minimization algorithm
  *                             used by L-BFGS.
  *
  * \param  solution_accuracy	Used as a termination condition: the algorithm is considered done when
  *                             ||g|| / max(1, ||x||) <= solution_accuracy where||...|| is the Euclidean norm.
  *                             Its default value is 1.0e-6.
  *
  * \param  max_iters		The maximum number of iterations to attemp while performing the minimization. Its
  *                             default value is 1000.
  *
  * TERMINATION:
  *
  *	    Termination occurs when one of the following conditions applies:
  *
  *	(1) The number of iterations exceeds max_iters.
  *
  *	(2) The abolute value of the Euclidean norm of the gradient at position x divided by the absolute value of the
  *         Euclidean norm of x is less than or equal to the value of solution_accuracy. If the absolute value of the
  *         norm of x is less than 1.0, then 1.0 is used in its place.
  */
template<typename FunctionFunc, typename GradientFunc>
void Lbfgs_Minimizer(const unsigned m, const real initial_step, FunctionFunc * const _f,
		     GradientFunc * const _grad_f, VectorOfReals * const _x,
		     TimeInMillisecsStats * const line_minimizer_time_stats,
		     TimeInMillisecsStats * const lbfgs_compute_next_hg_time_stats,
		     const LineMinimization::Options &line_min_options,
                     const real solution_accuracy = 1.0e-6, const unsigned max_iters = 1000)
{
    FunctionFunc &f = *_f;
    GradientFunc &grad_f = *_grad_f;
    VectorOfReals &x = *_x;

    real step = initial_step;
    if (step < 0.0)
        throw std::runtime_error("in MathUtil::Lbfgs_Minimizer: 'initial_step' must be non-negative!");

    DequeOfVecsOfDoubles dx_deque, dg_deque;
    DequeOfReals rho_deque;

    VectorOfReals x_old(x);
    VectorOfReals g_old(x.size());

    grad_f(x_old, &g_old);
    VectorOfReals Hg(-1.0 * g_old);

    WallClockTimer line_minimizer_timer(WallClockTimer::NON_CUMULATIVE_WITH_AUTO_STOP, "line_minimizer_timer");
    WallClockTimer lbfgs_compute_next_hg_timer(WallClockTimer::NON_CUMULATIVE_WITH_AUTO_STOP,
                                               "lbfgs_compute_next_hg_timer");

    bool done = false;
    unsigned k;
    for (k = 0; not done; ++k) {
        line_minimizer_timer.start();
        LineMinimizer<FunctionFunc, GradientFunc>(&f, &grad_f, &x, &Hg, &step, line_min_options);
        line_minimizer_timer.stop();
        line_minimizer_time_stats->accrue(line_minimizer_timer.getTimeInMilliseconds());

        VectorOfReals g(x.size());
        grad_f(x, &g);

        VectorOfReals dx(x - x_old);
        VectorOfReals dg(g - g_old);

        x_old = x;
        g_old = g;

        real rho = dx * dg;
        if (rho < 0.0) {
            throw std::runtime_error("in MathUtil::Lbfgs_Minimizer: negative rho = dx * dg = "
                                     + StringUtil::ToString(rho) + " encountered on the "
                                     + StringUtil::ToString(k) + "-th iteration.");
        } else if (rho == 0.0) {
            throw std::runtime_error("zero rho = dx * dg encountered on the " + StringUtil::ToString(k) +
                                     "-th iteration.");
        } else if (0.0 < rho and rho < REAL_EPSILON) {
            throw std::runtime_error("in MathUtil::Lbfgs_Minimizer:rho = dx * dg = " + StringUtil::ToString(rho) +
                                     " value below machine precision encountered on the " +
                                     StringUtil::ToString(k) + "-th iteration.");
        } else // rho >= REAL_EPSILON
            rho = 1.0 / rho;

        if (k >= m) { // remove the oldest entry
            dx_deque.pop_front();
            dg_deque.pop_front();
            rho_deque.pop_front();
        }

        // Add the latest entries.
        dx_deque.push_back(dx);
        dg_deque.push_back(dg);
        rho_deque.push_back(rho);

        Hg = -1.0 * g;
        lbfgs_compute_next_hg_timer.start();
        Lbfgs_ComputeNextHg(k, m, dx_deque, dg_deque, rho_deque, &Hg);
        lbfgs_compute_next_hg_timer.stop();
        lbfgs_compute_next_hg_time_stats->accrue(lbfgs_compute_next_hg_timer.getTimeInMilliseconds());

        // Termination conditions.

        if (k >= max_iters) {
            done = true;
            break;
        }

        const real g_norm = SQRT(g * g);
        real x_norm = SQRT(x * x);
        if (x_norm < 1.0)
            x_norm = 1.0;

        done = (g_norm <= x_norm * solution_accuracy);
    }
}


double Factorial(const unsigned n);


/** Returns the natural log of n!. */
double LogFactorial(const unsigned n);


inline float Abs(const float x) { return ::fabsf(x); }
inline double Abs(const double x) { return std::fabs(x); }
inline long double Abs(const long double x) { return ::fabsl(x); }


inline float Sqrt(const float x) { return ::sqrtf(x); }
inline double Sqrt(const double x) { return std::sqrt(x); }
inline long double Sqrt(const long double x) { return ::sqrtl(x); }


inline float Exp(const float x) { return ::expf(x); }
inline double Exp(const double x) { return std::exp(x); }
inline long double Exp(const long double x) { return ::expl(x); }


/** \brief An utility class to be used when one wants to add several numbers in a way that minimizes both
 *         round-off error and the chance of getting an overflow error.
 */
template<typename FloatingPointType> class NumericallySafeSum: private std::vector<FloatingPointType> {
public:
    NumericallySafeSum(const typename std::vector<FloatingPointType>::size_type initial_size = 0) {
        if (initial_size != 0)
            this->reserve(initial_size);
    }

    const NumericallySafeSum<FloatingPointType> &operator+=(const FloatingPointType &value) {
        if (value != 0.0)
            this->push_back(value);

        return *this;
    }

    void resetToZero() {
        const typename std::vector<FloatingPointType>::size_type current_size = this->size();
        this->clear();
        if (current_size != 0)
            this->reserve(current_size);
    }

    FloatingPointType sum();

    FloatingPointType average();
};


template<typename FloatingPointType> FloatingPointType NumericallySafeSum<FloatingPointType>::sum() {
    if (unlikely(this->empty()))
        return 0.0;

    // We need to sort the summands to add them in increasing order of their absolute values, so as
    // to guarantee robustness against round-off errors.
    std::sort(this->begin(), this->end(), SortedByIncreasingMagnitude<FloatingPointType>);

    // The largest summand, in absolute value, is at the end of the sorted vector.
    const FloatingPointType abs_max = Abs(this->back());

    if (unlikely(abs_max == 0.0))
        return 0.0;

    // Add the summands. We must not use an iterator because we want to guarantee that elements
    // are accessed in increasing order of their absolute values.
    FloatingPointType sum_over_max = 0.0;
    for (unsigned i = 0; i < this->size(); ++i)
        sum_over_max += (this->operator[](i) / abs_max);

    return abs_max * sum_over_max;
}


template<typename FloatingPointType> FloatingPointType NumericallySafeSum<FloatingPointType>::average() {
    FloatingPointType numerator(sum());
    FloatingPointType denominator(this->size());

    return numerator / denominator;
}


/** Returns the inverse of the CDF of the standard Normal distribution. */
double InverseNormalCDF(const double p);


/** \brief  Splits a single-precision floating-point number into an exponent and a mantissa.
 *  \param  f         The number to split.
 *  \param  exponent  The exponent of the split number.
 *  \param  mantissa  The biased mantissa of the number.  (The bias is FLT_RADIX ^ FLT_MANT_DIG).
 */
void ExtractExponentAndMantissa(const float f, int * const exponent, int32_t * const mantissa);


/** \brief  Splits a double-precision floating-point number into an exponent and a mantissa.
 *  \param  d         The number to split.
 *  \param  exponent  The exponent of the split number.
 *  \param  mantissa  The biased mantissa of the number.  (The bias is DBL_RADIX ^ DBL_MANT_DIG).
 */
void ExtractExponentAndMantissa(const double d, int * const exponent, int64_t * const mantissa);


/** Reconstructs a single-precision floating-point number from the exponent and "mantissa" as returned by
    ExtractExponentAndMantissa(). */
void ExponentAndMantissaToFloat(const int exponent, const int32_t mantissa, float * const f);


/** Reconstructs a double-precision floating-point number from the exponent and "mantissa" as returned by
    ExtractExponentAndMantissa(). */
void ExponentAndMantissaToDouble(const int exponent, const int64_t mantissa, double * const d);


inline float SafeLog(const float x) {
    if (unlikely(x < 0.0f))
        throw std::runtime_error("in MathUtil::SafeLog: can't take the log of a negative number (1)!");
    else if (x == 0.0)
        return -FLT_MAX; // Cheating, I know!
    else
        return ::logf(x);
}


inline double SafeLog(const double x) {
    if (unlikely(x < 0.0))
        throw std::runtime_error("in MathUtil::SafeLog: can't take the log of a negative number (2)!");
    else if (x == 0.0)
        return -DBL_MAX; // Cheating, I know!
    else
        return std::log(x);
}


inline long double SafeLog(const long double x) {
    if (unlikely(x < 0.0))
        throw std::runtime_error("in MathUtil::SafeLog: can't take the log of a negative number (3)!");
    else if (x == 0.0L)
        return -LDBL_MAX; // Cheating, I know!
    else
        return ::logl(x);
}


/** \brief  Calculates a root of a function using the Newton-Raphson method.
 *  \param  initial_guess  The initial guess for the root.
 *  \param  max_err        Return when consecutive estimates of the root differ by no more than this quantity.
 *  \param  f              The function for which we'd like to find a root.
 *  \param  f_prime        The derivative of the function for which we'd like to find a root.
 *  \param  root           If we return true, an estimate of a root.
 *  \param  max_iter       Maximum number of iterations after which we'll bail.
 *  \return In general the convergence is quadratic: the error is essentially squared at each step (that is, the number
 *          of accurate digits doubles in each step).  If the initial value is too far from the true zero, Newton's
 *          method can fail to converge.  If the root being sought has multiplicity greater than one, the convergence
 *          rate is merely linear (errors reduced by a constant factor at each step).
 */
#if __GNUC__ > 3 || __GNUC_MINOR__ > 3
template<typename FloatingPointType, typename FuncType> bool NewtonRaphson(
	const FloatingPointType initial_guess, const FloatingPointType max_err, const FuncType &f,
	const FuncType &f_prime, FloatingPointType * const root, const unsigned max_iter = 100)
#else
template<typename FloatingPointType, typename FuncType> bool NewtonRaphson(
	const FloatingPointType initial_guess, const FloatingPointType max_err, const FuncType f,
	const FuncType f_prime, FloatingPointType * const root, const unsigned max_iter = 100)
#endif
{
	FloatingPointType new_root_estimate = initial_guess;
	FloatingPointType f_value(0.0);
	for (unsigned iter(0); iter < max_iter; ++iter) {
		const FloatingPointType root_estimate(new_root_estimate);
		const FloatingPointType f_prime_value = f_prime(root_estimate);
		if (unlikely(isnan(f_prime_value)))
			return false;
		f_value = f(root_estimate);
		if (f_prime_value == 0.0) {
			*root = 0.0;
			return true;
		}
		new_root_estimate = root_estimate - f_value / f_prime_value;
		if (unlikely(isnan(new_root_estimate)))
			return false;
		if (Abs(root_estimate - new_root_estimate) < max_err) {
			*root = new_root_estimate;
			return true;
		}
	}

	if (Abs(f_value) > 1.0)
		return false;

	*root = new_root_estimate;
	return true;
}


template<typename NumericType> class NormalDistribution {
	const NumericType mean_;
	const NumericType standard_deviation_;
public:
	explicit NormalDistribution(const NumericType &mean = 0.0, const NumericType &standard_deviation = 1.0)
		: mean_(mean), standard_deviation_(standard_deviation) { }
	NumericType getMean() const { return mean_; }
	NumericType getStandardDeviation() const { return standard_deviation_; }
	NumericType getPdf(const NumericType x) const;
	NumericType getCdf(const NumericType x) const;
private:
	NumericType getZValue(const NumericType x) const { return (x - mean_) / standard_deviation_; }
};


template<typename NumericType> NumericType NormalDistribution<NumericType>::getPdf(const NumericType x) const
{
	const NumericType z_value(getZValue(x));
	const NumericType one_over_sqrt_of_2pi(1.0 * M_2_SQRTPIl / (2 * M_SQRT2l));
	return one_over_sqrt_of_2pi * Exp(-z_value * z_value / 2);
}


/** The implementation is based on approximation 26.2.19 in the 1964 ed. of the "Handbook of Mathematical Functions" by
    Abramowitz and Stegun.  The max. absolute error for this function is 1.5e-7.  (It may be slightly larger if
    NumericType is a float.) */
template<typename NumericType> NumericType NormalDistribution<NumericType>::getCdf(const NumericType x) const
{
	NumericType z_value(getZValue(x));
	const bool arg_is_negative(z_value < 0);
	if (arg_is_negative)
		z_value = -z_value;
	const NumericType sum(1 + z_value * (0.0498673470
					     + z_value * (0.0211410061
							  + z_value * (0.0032776263
								       + z_value * (0.0000380036
										    + z_value * (0.0000488906
												 + z_value
												 * 0.0000053830))))));
	NumericType power(1 / sum);
	power *= power;
	power *= power;
	power *= power;
	power *= power;

	return arg_is_negative ? (power / 2) : (1 - power / 2);
}


/** \brief Represents a set of statistical operations that can be performed on a data set (an iterable container
 *         of some sort of number) -- this class will only do the bare minimum of calculations upon construction
 *         (mean, median, variance) and derive everything else as-needed later. The two types are the template
 *         you wish to use internally for calculation (most likely double) and the type of container being passed
 *         in, such as a list<double>. You need to provide an const_iterable forward container like a list or vector,
 *         as the data will be run though linearly exactly twice. If you do not pass a const container, the
 *         constructor will assume the data is unsorted (it needs to be) and will then call the sort method before
 *         continuing. This class assumes a Standard Normal Distribution of the data provided, with exactly 50% of
 *         the data lying at or below and 50% at or above the median value and a bell-curve shape.
 */
template<typename NumberType, class ContainerType> class DataSetStatistics
{
	NumberType mean_;
	NumberType median_;
	NumberType standard_deviation_;
        NumberType variance_;
	std::auto_ptr< NormalDistribution<NumberType> > normal_dist_;
public:
	/** \brief Create a new dataset representing the container provided. The DataSetStatistics instance will NOT
	 *         contain a copy of the data used to create it.
	 *  \param numerical_container The container to read from. Use the non-const constructor if the
	 *         data need to be sorted; the method will call ->sort().
	 */
	explicit DataSetStatistics(ContainerType * const numerical_container)
		: median_(0.0), variance_(0.0)
	{
		numerical_container->sort();
		this->initialize(const_cast<const ContainerType *>(numerical_container));
	}

        /** \brief Create a new dataset representing the container provided. The DataSet instance will NOT
	 *         contain a copy of the data used to construct it.
	 *  \param numerical_container The container to read from. The const constructor assumes the
	 *         data are sorted. If they are not, the median and related numbers will be incorrect.
	 */
        explicit DataSetStatistics(const ContainerType &numerical_container)
		: median_(0.0), variance_(0.0)
	{
		this->initialize(&numerical_container);
	}

	explicit DataSetStatistics(const DataSetStatistics<NumberType, ContainerType> &dss)
		: median_(dss.median_), variance_(dss.variance_), standard_deviation_(dss.standard_deviation_),
		mean_(dss.mean_), normal_dist_(dss.normal_dist_)
	{
	}

	explicit DataSetStatistics() {}

	/** \brief Actually initialize all the instance members
	 */
	void initialize(ContainerType const * numerical_container)
	{
		computeMean(numerical_container->begin(), numerical_container->end(), numerical_container->size());
		computeMedianAndVariance(numerical_container->begin(), numerical_container->end(), numerical_container->size());
		normal_dist_.reset(new NormalDistribution<NumberType>(mean_, standard_deviation_));
	}

	/** \brief Return a number between zero and .38 of that percentage of the normal distribution is
	 *         approximately this value
	 */
	NumberType getNormalDistribution(const NumberType value) const
	{
		return normal_dist_->getCdf(value);
	}

	/** \brief Return a number between zero and one of what proportion of the probability distribution
	 *         is less than the provided value. This will give an approximation of the precision of the
	 *         integral performed in a certain number of iterations per standard deviation.
	 *  \param value      The value to test against the dataset
	 */
	NumberType getCumulativeDistribution(const NumberType value)
	{
		return normal_dist_->getCdf(value);
	}

	/** \brief Returns a number between zero and one of what proportion of the probability distribution
	 *         lies between the provided values. This will give an approximation of the precision of the
	 *         integral performed in a certain number of iterations per standard deviation.
	 *  \param value   The first value of the range. Order does not matter.
	 *  \param value2  The second value of the range. Order does not matter.
	 */
        NumberType getCumulativeRange(const NumberType value, const NumberType value2)
	{
		if (value > value2)
			return normal_dist_->getCdf(value) - normal_dist_->getCdf(value2);
		else
			return normal_dist_->getCdf(value2) - normal_dist_->getCdf(value);
	}

	/** \brief Compute the mean of the data set
	 */
	NumberType getMean() const { return normal_dist_->getMean(); }

	/** \brief Compute the median of the data set, the middle value of oddly-numbered containers and the
	 *         avarage of the two middle values of even-numbered ones
	 */
	NumberType getMedian() { return median_; }

	/** \brief Compute the standard deviation (square root of variance) of the data set
	 */
	NumberType getStandardDeviation() { return normal_dist_->getStandardDeviation(); }

	/** \brief Compute the standard score ((value - mean)/stddev) of the provided value, assuming
	 *         the distribution is adequately represented by the data provided in the container passed
	 *         at construction.
	 */
	NumberType getStandardScore(NumberType value) const
		{ return (value - normal_dist_->getMean()) / normal_dist_->getStandardDeviation(); }

	/** \brief Compute the variance (ie sigma-squared) by ((1/n) * SUM([(x - mean)**2 for x in dataset]))
	 */
	NumberType getVariance() const { return variance_; }
private:
	/** Trick the compiler into not inferring the iterator type of what we're going to use
	 *  over the templatized container until the last minute
	 */
	template<class IteratorType> void computeMean(const IteratorType &begin, const IteratorType &end, size_t dataset_size)
	{
		NumberType total(0.0);
		NumberType denominator(dataset_size);
		for (IteratorType mean_getter(begin); mean_getter != end; ++mean_getter)
			total += (*mean_getter);
		total /= denominator;
		mean_ = total;
	}

	/** Trick the compiler into not inferring the iterator type of what we're going to use
	 *  over the templatized container until the last minute
	 */
	template<class IteratorType> void computeMedianAndVariance(const IteratorType &begin, const IteratorType &end,
								   size_t dataset_size)
	{
		size_t current_subscript(0);
                size_t middle_element(dataset_size / 2);
		bool is_oddly_sized(dataset_size % 2 == 0);
		NumberType denominator(dataset_size);
		if (not is_oddly_sized)
			middle_element = ((dataset_size - 1) / 2);
		for (IteratorType variance_and_median_getter(begin); variance_and_median_getter != end;
		     ++variance_and_median_getter)
		{
			NumberType current_value(*variance_and_median_getter);
			if (current_subscript == middle_element)
				median_ = current_value;
			else if ((current_subscript == (middle_element + 1)) and (not is_oddly_sized)) {
				median_ += current_value;
				median_ /= 2;
			}
			variance_ += (current_value - mean_) * (current_value - mean_);
			++current_subscript;
		}
                variance_ /= denominator;
		standard_deviation_ = Sqrt(variance_);
	}
};


/** \brief  Implements a minimum finder using a combination of the golden-section search and successive parabolic
 *          interpolation.
 *  \param  a         One endpoint of the interval that will be searched.
 *  \param  b         The other endpoint of the interval that will be searched.
 *  \param  f         The function that will be minimised.
 *  \param  tolerance The tolerance for the minimum that will be calculated.
 *  \note   epsilon is used as a measure of the accuracy of the minimum that will be calculated.
 *  \return An estimate of the minimum.
 *  \note   This implementation is based on FMINBR() in "Numerical Methods in Software" pub. by Prentice Hall.
 */
template<typename FloatType, typename FloatFunc> FloatType BrentMinimizer(
	FloatType a, FloatType b, const FloatFunc &f,
	const FloatType &tolerance = 2.0 * std::numeric_limits<FloatType>::epsilon())
{
	if (unlikely(tolerance < 0.0))
            throw std::runtime_error("in MathUtil::BrentMinimizer: \"tolerance\" must be non-negative!");

	// Normalise the interval:
	if (b < a)
		std::swap(a, b);

	const FloatType golden_ratio((3.0 - Sqrt(FloatType(5.0))) / 2.0);
	const FloatType sqrt_of_epsilon(Sqrt(std::numeric_limits<FloatType>::epsilon()));

	// First step.  Always use the golden section:
	FloatType v(a + golden_ratio * (b - a));
	FloatType f_v(f(v));
	FloatType x(v);
	FloatType w(v);
	FloatType f_x(f_v);
	FloatType f_w(f_v);

	for (;;) {
		const FloatType range(b - a);
		const FloatType mid_range((a + b) / 2.0);
		const FloatType actual_tolerance(sqrt_of_epsilon * Abs(x) + tolerance / 3.0);

		// Acceptable approximation found?
		if (Abs(x - mid_range) + range / 2.0 <= 2.0 * actual_tolerance)
			return x;

		// Obtain the golden section step size:
		FloatType new_step(golden_ratio * (x < mid_range ? b - x : a - x));

		// Attempt interpolation if "x" and "w" are sufficiently distinct:
		if (Abs(x - w) >= actual_tolerance) {
			const FloatType t((x - w) * (f_x - f_v));
			FloatType q((x - v) * (f_x - f_w));
			FloatType p((x - v) * q - (x - w) * t);
			q = 2.0 * (q - t);

			if (q > 0.0)
				p = -p;
			else
				q = -q;

			// If x + p/q lies in [a,b], is not too close to a or b, and isn't too large, we use p/q as the
			// new step size:
			if (Abs(p) < Abs(new_step * q) and p > q * (a - x + 2.0 * actual_tolerance)
			    and p < q * (b - x - 2.0 * actual_tolerance))
				new_step = p / q;
		}

		// Make sure that the new step size is no less than the tolerance:
		if (Abs(new_step) < actual_tolerance) {
			if (new_step > 0.0)
				new_step = actual_tolerance;
			else
				new_step = -actual_tolerance;
		}

		//
		// Calculate the new approx. for the minimum and reduce the enclosing range:
		//

		FloatType t(x + new_step); // Possible better approx. for the minimum.
		FloatType f_t(f(t));

		if (f_t <= f_x) {
			// Reduce the range:
			if (t < x)
				b = x;
			else
				a = x;

			v   = w;
			w   = x;
			x   = t;
			f_v = f_w;
			f_w = f_x;
			f_x = f_t;
		}
		else { // "x" is a better approx. than "t".
			if (t < x)
				a = t;
			else
				b = t;

			if (f_t <= f_w or w == x) {
				v   = w;
				w   = t;
				f_v = f_w;
				f_w = f_t;
			}
			else if (f_t <= f_v or v == x or v == w) {
				v   = t;
				f_v = f_t;
			}
		}
	}
}


template <typename Real> inline bool ApproximatelyEqual(const Real &x1, const Real &x2, const Real &epsilon = 1e-3)
{
	if (x1 != 0.0 and x2 != 0.0)
		return Abs(1.0 - Abs(x1 / x2)) < epsilon;
	else
		return Abs(x1 - x2) < epsilon;
}


template <typename NumericType> inline int Sign(const NumericType &x)
{
	if (x == 0.0)
		return 0;
	return (x > 0.0) ? 1 : -1;
}


template <typename Real> bool DiffersByMoreThan(const Real &x1, const Real &x2, const Real &factor)
{
	if (Sign(x1) != Sign(x2))
		return true;

	const Real abs_x1(std::fabs(x1));
	const Real abs_x2(std::fabs(x2));
	const Real min_x(std::min(abs_x1, abs_x2));
	const Real max_x(std::max(abs_x1, abs_x2));

	MSG_UTIL_ASSERT(factor > 0.0);

	if (factor > 1.0)
		return min_x * factor < max_x;
	else // Assume factor < 1.0
		return min_x < max_x * factor;
}


/**
 * \brief  Calculate the information theory entropy on a set of sample proportions. see
 *         http://en.wikipedia.org/wiki/Information_entropy.
 * \param  first  The iterators spanning the items to be used in the calculation.
 * \param  last   The iterators spanning the items to be used in the calculation.
 * \note   This calculates the entropy of a set given a list of the count of subclasses that make up the
 *         set. For example 6 oranges, 7 apples, 10 watermelons make up the set of 23 items. The first and
 *         last iterators would point to numbers 6/23, 7/23 and 10/23.
 *         If the iterators point into a map style collection, pair::second is used as the value in the
 *         calculations.
 */
template <typename IteratorType> double GetEntropy(const IteratorType &first, const IteratorType &last)
{
	// Accumulate the total number of training examples into this.
	double total_entropy(0.0);
	double check_total(0.0);

	for (IteratorType fraction(first); fraction != last; ++fraction) {
		typename IteratorType::value_type value(*fraction);
		total_entropy += (-StlHelpers::Value(value) * ::log2(StlHelpers::Value(value)));
		check_total += StlHelpers::Value(value);
	}

	// Make sure the list of fractions was complete
	if (1.0 - check_total > .01)
            throw std::runtime_error(StringUtil::Format("in GetEntropy: Entropy calculation %g doesn\'t add up to "
                                                        "1.0: ", check_total));

	return total_entropy;
}


/**
 * \brief  Calculate the information theory entropy on a set of sample proportions.  See
 *         http://en.wikipedia.org/wiki/Information_entropy.
 * \param  first          The iterator pointing to the first data element.
 * \param  last           The iterator pointing to the last data element.
 * \param  total_set_size The total size of the set.
 * \note   This calculates the entropy of a set given a list of the count of subclasses that make up the set.
 *         For example 6 oranges, 7 apples, 10 watermelons make up the set of 23 items. The begin and end
 *         iterators would point to numbers 6, 7 and 10 and total_set_size would be 23.
 *         IMPORTANT: This works on the proportions of samples, not a raw set of samples. Use the
 *         GetProportionsFromSamples if you have the raw_samples (which must be discrete values for the function to
 *         have any meaning. Sets of real number values would rarely have bunches of equal value)
 *         Example: If you have the raw samples you can do
 *         GetEntropy(Values(GetCountsForEachSamples(ContainerOfDiscreteItems)));
 */
template <typename IteratorType> double GetEntropyOfProportions(const IteratorType &first, const IteratorType &last,
                                                                const unsigned total_set_size)
{
    // Accumulate the total number of training examples into this.
    double total_entropy(0.0);
    double check_total(0.0);

    for (IteratorType amount(first); amount != last; ++amount) {
        const double proportion(StlHelpers::Value(*amount)/ total_set_size);
        total_entropy += (-proportion * ::log2(proportion));
        check_total += proportion;
    }

    // Make sure the list of fractions was complete
    if (1.0 - check_total > .01)
        throw std::runtime_error(StringUtil::Format("in MathUtil::GetEntropyOfProportions: Entropy calculation %g "
                                                    "doesn\'t add up to 1.0: ", check_total));

    return total_entropy;
}


/**
 *  \brief  Calculate the information theory entropy on a set of sample proportions. see
 *          http://en.wikipedia.org/wiki/Information_entropy
 *  \param  subsets_sizes  A container containing a list of iterators to the numbers. The numbers represent how many items of
 *                         the same subset are in this set. For example, 10 oranges is a subset of a basket full of mixed
 *                         fruit.
 *  \note   This calculates the entropy of a set given a list of the count of subclasses that make up the
 *          set. For example 6 oranges, 7 apples, 10 watermelons make up the set of 23 items. The container must
 *          contain the numbers 6, 7, and 10. WARNING: If the container is a container of pairs, the second member
 *          of the pair is what is used in the calculations.
 */
template <typename Container> double GetEntropyOfProportions(const Container &subsets_sizes) {
    return GetEntropyOfProportions(subsets_sizes.begin(), subsets_sizes.end(), StlHelpers::Total(subsets_sizes));
}


/** Returns the minimum number of bits required to encode "n." */
unsigned GetMinNumberOfBits(uint64_t n);


/**
 * \brief  Calculate a list of members and their counts from a collection of samples
 *         http://en.wikipedia.org/wiki/Information_entropy.
 * \param  first  The iterator pointing to the first data element.
 * \param  last   The iterator pointing to the last data element.
 * \note   This calculates the divisions of elements. For if you have a set of numbers like
 *         1,1,2,2,2,2,5,5,5 this would return a map giving you the count of each unique
 *         value in the set, i.e. map[1] = 2, map[2] = 4, map[5] = 3. There are 2 ones,
 *         4 2's and 3 5's. The items counted can be any class that can be a key in an stl::map.
 */
template <typename IteratorType> std::map<typename IteratorType::value_type, unsigned>
GetCountsForEachSamples(const IteratorType &first, const IteratorType &last) {
    // Accumulate the total number of training examples into this.
    std::map<typename IteratorType::value_type, unsigned> return_map;

    for (IteratorType sample(first); sample != last; ++sample)
        ++return_map[*sample];

    return return_map;
}


/**
 *  \brief  Calculate the information theory entropy on a set of samples
 *          http://en.wikipedia.org/wiki/Information_entropy
 *  \param  container  A container containing a list of iterators to the items.
 *  \note   This calculates the entropy of the container
 */
template <typename Container> double GetEntropyOfContainer(const Container &container)
{
	return GetEntropyOfProportions(GetCountsForEachSamples(container.begin(), container.end()));
}


/**\brief Calculate the value of a sigmoid value calibrated to 1/( 1 + ln(A*t + B)) where t is the function output value and A and B are
 *        multiplier/modifier parameters for the sigmoid (as trained by something like a Levenberg-Marquardt algorithm).
 * \param multiplier          The multiplier for the value to analyze on the sigmoid.
 * \param modifier            The shift left/right for the value.
 * \param value_ton_ormalize  The value to normalize between 0 and 1 from whatever the original values were.
 */
inline double GetValueForCalibratedSigmoid(const double multiplier, const double modifier,
                                           const double value_to_normalize)
{
    return 1.0 / (1.0 + std::exp((value_to_normalize * multiplier) + modifier));
}


/** \brief  Integer log with base 2.
 *  \param  The number for which we desire the log base 2.
 *  \return 0 for 0 or 1, 1 for numbers 2 and 3, 2 for numbers between 4 and 7, 3 for numbers between 8 and 15, etc.
 */
unsigned Log2(const unsigned long long n);


/** \brief  Double-precision floating point log with base 2. */
double Log2(const double n);


/** \class  CombinationGenerator
 *  \brief  Generates all possible combinations of the integers 1 .. n, taken r at a time.
 */
class CombinationGenerator {
    const unsigned n_, r_;
    std::vector<unsigned> next_combination_;
public:
    /** \brief  Constructs a new object of type CombinationGenerator and seeds it with the first combination.
     *  \param  n  The range of intergers as in 1 .. n that the combinations will be chosen from.
     *  \param  r  The size of a sample.  As in "n choose r."
     */
    CombinationGenerator(const unsigned n, const unsigned r);

    /** \return  True if no more new combinations can be retrieved via a call to getNextCombination(), else false. */
    bool done() const { return next_combination_.empty(); }

    /** \brief   Retrieves the next combination.
     *  \param   combination  Contains the next combination after a call to this member function.
     *  \warning You must not call this function if done() return true!
     */
    void getNextCombination(std::vector<unsigned> * const combination)
        { *combination = next_combination_; advance(); }

    static void PrintCombination(std::ostream &output, const std::vector<unsigned> &combination);
private:
    void advance();
};


/** \return  C(n, r). (Number of combinations with no repetitions.  Order does *not* matter!) */
unsigned long long Combinations(const unsigned n, const unsigned r);


/** \brief   Returns true if "i" is odd, otherwise returns false.
 *  \warning Do \em{not} use with floating point numbers.
 */
template<typename IntType> inline bool IsOdd(const IntType i) { return i & 1; }


/** \brief   Returns true if "i" is even, otherwise returns false.
 *  \warning Do \em{not} use with floating point numbers.
 */
template<typename IntType> inline bool IsEven(const IntType i) { return not IsOdd(i); }


/** \brief  Implements Kahan's summation algorithm a.k.a. "compensated summation".
 *  \note   This reduces the numerical error when the summands are not all of the same order of magitude while still retaining O(N) behaviour.
 */
double KahanSummation(const std::vector<double> &x);


/** \brief  Returns the relative error for two numbers.
 *  \return Unless |x| < threshold or |y| < threshold, this function returns |x-y|/min(|x|,|y|), otherwise it
 *          returns |x-y|.
 */
double RelativeError(const double x, const double y, const double threshold = 1.0e-10);


/** \brief  Tests whether a floating point number "d" has no fractional part or not. */
inline bool IsInteger(const double d) { return d == static_cast<long>(d); }


} // namespace MathUtil


#endif // ifndef MATH_UTIL_H
