#ifndef DOT2D_H
#define DOT2D_H
#include "GeometricProduct2d.h"
namespace MATHEX {

	// TODO (UN): I need to derive these by hand to make sure they are correct for 2D

	// A line and a point dot to make a line
	// This new line is orthogonal to the original line and through the point!
	inline const Line2d dot(const Line2d& l, const Point2d& p) {
		return (l * p).line;
	}
	inline const Line2d operator | (const Line2d& l, const Point2d& p) {
		return dot(l, p);
	}

	// You should get the same line if you swap the order, just all signs are flipped
	// Means that the line is oriented the other way
	inline const Line2d dot(const Point2d& p, const Line2d& l) {
		return (p * l).line;
	}
	inline const Line2d operator | (const Point2d& p, const Line2d& l) {
		return dot(p, l);
	}

	// Now we are dotting the same type of things together, it returns just a float
	// If the two arguments are normalized, returns cos(theta)
	inline float dot(const Line2d& l1, const Line2d& l2) {
		return (l1 * l2).real;
	}
	inline float operator | (const Line2d& l1, const Line2d& l2) {
		return dot(l1, l2);
	}

	// TODO (UN) Not sure what dotting a motor with another motor means geometrically, 
	inline float dot(const Motor2d& m1, const Motor2d& m2) {
		return (m1 * m2).real;
	}

	inline float operator | (const Motor2d& m1, const Motor2d& m2) {
		return dot(m1, m2);
	}

	// NOTE (UN): 
	// Dotting a point with another point always returns the negative of the the two e12 coefficients multiplied together
	// So if the two points are normalized, you always get -1
	// This will be confusing for those of us that use the VMath::dot, so I wont code it here
}
#endif

/// For example A dot B = |A|*|B|*cos(theta) which gives us information about the angle between two vectors
/// But now we can dot lines and points together too! This is going to be fun
/// Usually a dot product returns just a number right? Well, in GA its a bit different
/// If the two things you are dotting are the same type (line or point) you do get a scalar
/// But if the things are different, it reduces the highest grade by 1
/// Amazingly the new lines are orthogonal & through the original point we were dotting
