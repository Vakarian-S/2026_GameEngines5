#ifndef JOIN2D_H
#define JOIN2D_H
#include "Line2d.h"
#include "Point2d.h"
#include "Dual2d.h"
#include "Meet2d.h"
namespace MATHEX {

	// The join represents when you "join" two things together. Get it?
	// For example if you "join" two point, it gives you the line that goes through them
	// WHY?     https://github.com/ScottFielder/MathLibrary/blob/master/Notes/Why_combine_things.pdf
	// More thoughts at the end of this file

	// Two points join to make a line
	// Look how easy that is now that we have the Poincare Duality (!) and the Meet (^)
	// EXAMPLE: https://github.com/ScottFielder/MathLibrary/blob/master/Notes/Example_joining_points.pdf
	inline const Line2d join(const Point2d& p1, const Point2d& p2) {
		// The mess below means: dual(meet(dual(p1), dual(p2)))
		return !(!p1 ^ !p2);
	}
	inline const Line2d operator & (const Point2d& p1, const Point2d& p2) {
		return join(p1, p2);
	}

	// A line and a point join to make just the real term of a Motor2d
	// Turns out this is the oriented distance between the point and the line if they are both normalized!
	// REFERENCE: https://bivector.net/2DPGA.pdf
	inline const Motor2d join(const Line2d& l, const Point2d& p) {
		return !(!l ^ !p);
	}

	inline const Motor2d operator & (const Line2d& l, const Point2d& p) {
		return join(l, p);
	}

	// TODO (UN):
	// Messing around with it suggests that order does not matter here either
	// Best to double check though
	inline const Motor2d join(const Point2d& p, const Line2d& l) {
		return join(l, p);
	}

	inline const Motor2d operator & (const Point2d& p, const Line2d& l) {
		return join(l, p);
	}

}
#endif

// Joining two points is handy as it creates a line from two points
// It's written as an upside down "^" in  math, but unfortunately that is not available on my keyboard
// Consensus seems to be to go with overloading "&". I guess because you are joining this "AND" that. Get it?
// The join is also known as the regressive product
// I like to think of it as a complement to the meet. That's why there are so many duals.
