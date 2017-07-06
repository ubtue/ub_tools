/** \file   Real.h
 *  \brief  Allows easy switching between floats and doubles.
 *  \author Wagner Truppel
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
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

#ifndef REAL_H
#define REAL_H


#include <cmath>
#include <math.h>
#include <limits>


#define USE_FLOAT       1               // Set this to 0 to use doubles.


#ifdef min
#	undef min
#endif
#ifdef max
#	undef max
#endif


#if USE_FLOAT

        typedef float real;

        const real REAL_MIN             = std::numeric_limits<float>::min();
        const real REAL_MAX             = std::numeric_limits<float>::max();
        const real REAL_EPSILON         = std::numeric_limits<float>::epsilon();

        #define TO_REAL_1(s)            StringUtil::ToFloat(s)
        #define TO_REAL_2(s,n)          StringUtil::ToFloat(s,n)

        #define ABS(x)			fabsf(x)
        #define ACOS(x)			acosf(x)		/* Arc cosine of x. */
        #define ASIN(x)			asinf(x)		/* Arc sine of x. */
        #define ATAN(x)			atanf(x)		/* Arc tangent of x. */
        #define ATAN2(y,x)		atan2f(y,x)		/* Arc tangent of y/x. */
        #define COS(x)			cosf(x)                 /* Cosine of x. */
        #define SIN(x)			sinf(x)		        /* Sine of x. */
        #define TAN(x)			tanf(x)		        /* Tangent of x. */
        #define COSH(x)			coshf(x)		/* Hyperbolic cosine of x. */
        #define SINH(x)			sinhf(x)		/* Hyperbolic sine of x. */
        #define TANH(x)			tanhf(x)		/* Hyperbolic tangent of x. */
        #define ACOSH(x)		acoshf(x)		/* Hyperbolic arc cosine of x. */
        #define ASINH(x)		asinhf(x)		/* Hyperbolic arc sine of x. */
        #define ATANH(x)		atanhf(x)		/* Hyperbolic arc tangent of x. */
        #define EXP(x)			expf(x)                 /* Exponential function of x. */
        #define LOG(x)			logf(x)                 /* Natural logarithm of x. */
        #define LOG10(x)		log10f(x)		/* Base-ten logarithm of x. */
        #define EXP10(x)		exp10f(x)		/* 10^x. */
        #define EXPM1(x)		expm1f(x)		/* Return exp(x) - 1. */
        #define LOG1P(x)		log1pf(x)		/* Return log(1 + x). */
        #define EXP2(x)			exp2f(x)		/* 2^x. */
        #define LOG2(x)			log2f(x)		/* Compute base-2 logarithm of x. */
        #define POW(x,y)		powf(x,y)		/* Return x to the y power. */
        #define DREM(x,y)		dremf(x,y)		/* Return the remainder of x/y. */
        #define HYPOT(x,y)		hypotf(x,y)		/* Return `sqrt(x*x + y*y)'. */
        #define SQRT(x)			sqrtf(x)		/* Return the square root of x. */
        #define CBRT(x)			cbrtf(x)		/* Return the cube root of x. */
        #define CEIL(x)			ceilf(x)		/* Smallest integral value not less than x. */
        #define FABS(x)			fabsf(x)		/* Absolute value of x. */
        #define FLOOR(x)		floorf(x)		/* Largest integer not greater than x. */
        #define DREM(x,y)		dremf(x,y)		/* Return the remainder of x/y. */
        #define COPYSIGN(x,y)		copysignf(x,y)          /* Return x with its signed changed to y's. */
        #define RDOUBLE(x)		rdoublef(x)		/* Return the integer nearest x in the direction of the
        							 * prevailing rounding mode. */
        #define REMAINDER(x,y)		remainderf(x,y)         /* Return the remainder of integer divison x / y with
        							 * infinite precision. */
        #define NEARBYDOUBLE(x)		nearbydoublef(x)	/* Round x to integral value in floating-point format
        							 * using current rounding direction, but do not raise
        							 * inexact exception. */
        #define ROUND(x)		roundf(x)		/* Round x to nearest integral value, rounding halfway
        							 * cases away from zero. */
        #define TRUNC(x)		truncf(x)		/* Round x to the integral value in floating-point
        							 * format nearest but not larger in magnitude. */
        #define FDIM(x,y)		fdimf(x,y)		/* Return positive difference between x and y. */

#else

        typedef double real;

        const real REAL_MIN             = std::numeric_limits<double>::min();
        const real REAL_MAX             = std::numeric_limits<double>::max();
        const real REAL_EPSILON         = std::numeric_limits<double>::epsilon();

        #define TO_REAL_1(s)            StringUtil::ToDouble(s)
        #define TO_REAL_2(s,n)          StringUtil::ToDouble(s,n)

        #define ABS(x)			std::fabs(x)

        #define ACOS(x)			std::acos(x)		/* Arc cosine of x. */
        #define ASIN(x)			std::asin(x)		/* Arc sine of x. */
        #define ATAN(x)			std::atan(x)		/* Arc tangent of x. */
        #define ATAN2(y,x)		std::atan2(y,x)		/* Arc tangent of y/x. */
        #define COS(x)			std::cos(x)		/* Cosine of x. */
        #define SIN(x)			std::sin(x)		/* Sine of x. */
        #define TAN(x)			std::tan(x)		/* Tangent of x. */
        #define COSH(x)			std::cosh(x)		/* Hyperbolic cosine of x. */
        #define SINH(x)			std::sinh(x)		/* Hyperbolic sine of x. */
        #define TANH(x)			std::tanh(x)		/* Hyperbolic tangent of x. */
        #define ACOSH(x)		std::acosh(x)		/* Hyperbolic arc cosine of x. */
        #define ASINH(x)		std::asinh(x)		/* Hyperbolic arc sine of x. */
        #define ATANH(x)		std::atanh(x)		/* Hyperbolic arc tangent of x. */
        #define EXP(x)			std::exp(x)		/* Exponential function of x. */
        #define LOG(x)			std::log(x)		/* Natural logarithm of x. */
        #define LOG10(x)		std::log10(x)		/* Base-ten logarithm of x. */
        #define EXP10(x)		std::exp10(x)		/* 10^x. */
        #define EXPM1(x)		std::expm1(x)		/* Return exp(x) - 1. */
        #define LOG1P(x)		std::log1p(x)		/* Return log(1 + x). */
        #define EXP2(x)			std::exp2(x)		/* 2^x. */
        #define LOG2(x)			std::log2(x)		/* Compute base-2 logarithm of x. */
        #define POW(x,y)		std::pow(x,y)		/* Return x to the y power. */
        #define DREM(x,y)		std::drem(x,y)		/* Return the remainder of x/y. */
        #define HYPOT(x,y)		std::hypot(x,y)		/* Return `sqrt(x*x + y*y)'. */
        #define CBRT(x)			std::cbrt(x)		/* Return the cube root of x. */
        #define SQRT(x)			std::sqrt(x)	        /* Return the square root of x. */
        #define CEIL(x)			std::ceil(x)		/* Smallest integral value not less than x. */
        #define FABS(x)			std::fabs(x)		/* Absolute value of x. */
        #define FLOOR(x)		std::floor(x)		/* Largest integer not greater than x. */
        #define DREM(x,y)		std::drem(x,y)		/* Return the remainder of x/y. */
        #define COPYSIGN(x,y)		std::copysign(x,y)	/* Return x with its signed changed to y's. */
        #define RDOUBLE(x)		std::rdouble(x)		/* Return the integer nearest x in the direction of the
        							 * prevailing rounding mode. */
        #define REMAINDER(x,y)		std::remainder(x,y)	/* Return the remainder of integer divison x / y with
        							 * infinite precision. */
        #define NEARBYDOUBLE(x)		std::nearbydouble(x)	/* Round x to integral value in floating-point format
        							 * using current rounding direction, but do not raise
        							 * inexact exception. */
        #define ROUND(x)		std::round(x)		/* Round x to nearest integral value, rounding halfway
        							 * cases away from zero. */
        #define TRUNC(x)		std::trunc(x)		/* Round x to the integral value in floating-point
        							 * format nearest but not larger in magnitude. */
        #define FDIM(x,y)		std::fdim(x,y)		/* Return positive difference between x and y. */

#endif


#endif // ifndef REAL_H
