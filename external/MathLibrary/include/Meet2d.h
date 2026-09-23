#ifndef MEET2D_H
#define MEET2D_H
#include "Flector2d.h"
#include "Motor2d.h"
#include "Point2d.h"
#include "GeometricProduct2d.h" // TODO (UN): Check this doesnt create a circular dependency
namespace MATHEX {

	// The meet represents where two things "meet". Get it?
	// For example if you "meet" two lines, it gives you the point of intersection
	// It's written as a "^" in  math, so let's override that
	// WHY? https://github.com/ScottFielder/MathLibrary/blob/master/Notes/Why_combine_things.pdf
	// More thoughts at the end of this file

	// A line and a line meet at a point
	// TODO (UN): Just had a thought. Why don't I just multiply the objects together 
	// and just return the higher grade part? Best to check this at some point (get it!?)
	// This will return a point infinitely far away (e12 = 0) if lines are parallel
	inline const Point2d meet(const Line2d& l1, const Line2d& l2) {
		Point2d result;
		Motor2d m = l1 * l2;
		result.e20 = -m.e02;
		result.e01 =  m.e01;
		result.e12 =  m.e12;
		return result;
	}
	inline const Point2d operator ^ (const Line2d& l1, const Line2d& l2) {
		return meet(l1, l2);
	}

	// A point and a line meet at a flector (only the e012 part survives)
	// TODO (UN): I vaguely remember this being useful for something, but need to check
	// 3D DERIVATION: https://github.com/ScottFielder/MathLibrary/blob/master/Notes/Meet_point_with_plane.pdf
	inline const Flector2d meet(const Point2d& p, const Line2d& l) {
		Flector2d result;
		Flector2d f = p * l;
		// Only return the higher grade part
		result.e012 = f.e012;
		result.line.set(0.0f, 0.0f, 0.0f);
		return result;
	}

	inline const Flector2d operator ^ (const Point2d& p, const Line2d& l) {
		return meet(p, l);
	}

	// TODO (UN): I think the order does not matter here either, but best to double check
	inline const Flector2d meet(const Line2d& l, const Point2d& p) {
		return meet(p, l);
	}

	inline const Flector2d operator ^ (const Line2d& l, const Point2d& p) {
		return meet(p, l);
	}

}
#endif

/// The "meet" is the cross product on steroids. Remember A x A = 0?
/// The meet (also known as the exterior product, the wedge, and the outer product) does the same thing
/// It's the part of the geometric product where there are no repeating terms in the e's
