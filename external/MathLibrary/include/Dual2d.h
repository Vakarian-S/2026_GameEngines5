#ifndef DUAL2D_H
#define DUAL2D_H
#include "Point2d.h"
#include "Motor2d.h"
#include "Flector2d.h"
namespace MATHEX {

	// The dual returns whatever e basis vectors are missing
	// So basically whatever it is not. The not operator ! seems like a good option here
	inline const Flector2d dual(const Motor2d& m) {
		Flector2d result;
		result.e012     = m.real;
		result.line.e1  = m.e02;
		result.line.e2  = m.e01;
		result.line.e0  = m.e12;
		return result;
	}

	inline const Flector2d operator ! (const Motor2d& m) {
		return dual(m);
	}

	inline const Motor2d dual(const Flector2d& f) {
		Motor2d result;
		result.real = f.e012;
		result.e01 = f.line.e2;
		result.e02 = f.line.e1;
		result.e12 = f.line.e0;
		return result;
	}

	inline const Motor2d operator ! (const Flector2d& f) {
		return dual(f);
	}

	// Its crazy, the dual of a point returns a line
	inline const Line2d dual(const Point2d& p) {
		Line2d result;
		result.e1 = p.e20;
		result.e2 = p.e01;
		result.e0 = p.e12;
		return result;
	}

	inline const Line2d operator ! (const Point2d& p) {
		return dual(p);
	}

	// The dual of a line is a point. The world makes sense
	inline const Point2d dual(const Line2d& l) {
		Point2d result;
		result.e20 = l.e1;
		result.e01 = l.e2;
		result.e12 = l.e0;
		return result;
	}
	inline const Point2d operator ! (const Line2d& l) {
		return dual(l);
	}
}
#endif