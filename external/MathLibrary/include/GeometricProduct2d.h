#ifndef GEOMETRICPRODUCT2D_H
#define GEOMETRICPRODUCT2D_H
#include "Point2d.h"
#include "Flector2d.h"
#include "Motor2d.h"

namespace MATHEX {
	// TODO (UN): For this whole file I am just using my PGA3D GeometricProduct.h code 
	// and deleting the parts not needed. Gotta derive these by hand to make sure they are correct for 2D

	// I wrote this out on paper. My wrist is still hurting.
	// You don't need the brackets, it just helps my eyes
	// DERIVATION: https://github.com/ScottFielder/MathLibrary/blob/master/Notes/Multiplying_dual_quaternions.pdf
	inline const Motor2d operator * (const Motor2d& a, const Motor2d& b) {
		Motor2d result;
		result.real = (a.real * b.real) - (a.e12 * b.e12);
		result.e12 = (a.real * b.e12) + (a.e12 * b.real);
		result.e01 = (a.real * b.e01) + (a.e12 * b.e02)
			+ (a.e01 * b.real) - (a.e02 * b.e12);
		result.e02 = (a.real * b.e02) - (a.e12 * b.e01)
			+ (a.e01 * b.e12) + (a.e02 * b.real);
		return result;
	}

	// I'll overload the division operator as well
	// Such that A / B = A * inverse(B)		in that order
	inline const Motor2d operator / (const Motor2d& a, const Motor2d& b) {
		Motor2d inverseB = b;
		// First reverse it
		inverseB.e12 *= -1.0f;
		inverseB.e01 *= -1.0f;
		inverseB.e02 *= -1.0f;
		// Then divide by mag^2
		float magSquared = inverseB.real * inverseB.real + inverseB.e12 * inverseB.e12;
		// The next line will blow up if b is infinitely far way, there is no inverse in that case
		inverseB = inverseB / magSquared;
		return a * inverseB;
	}

	// A motor pops out when you multiply a motor with a point 
	// DERIVATION: https://github.com/ScottFielder/MathLibrary/blob/master/Notes/Multiplying_dq_with_point.pdf
	inline const Motor2d operator * (const Motor2d& a, const Point2d& b) {
		Motor2d result;
		result.real = -(a.e12  * b.e12);
		result.e12  =  (a.real * b.e12);
		result.e01  =  (a.real * b.e01) - (a.e12 * b.e20) - (a.e02 * b.e12);
		result.e02  = -(a.real * b.e20) - (a.e12 * b.e01) + (a.e01 * b.e12);
		return result;
	}

	// A few changes when you multiply the other way round
	// DERIVATION: https://github.com/ScottFielder/MathLibrary/blob/master/Notes/Multiplying_point_with_dq.pdf
	inline const Motor2d operator * (const Point2d& a, const Motor2d& b) {
		Motor2d result;
		result.real = -(a.e12 * b.e12);
		result.e12  =  (a.e12 * b.real);
		result.e01  =  (a.e12 * b.e02) + (a.e01 * b.real) + (a.e20 * b.e12);
		result.e02  = -(a.e12 * b.e01) + (a.e01 * b.e12)  - (a.e20 * b.real);
		return result;
	}

	// TODO (UN): Ooh, this one is tricky. A Line2d * Motor2d = a Line2d with a e012 part potentially
	// I don't have a struct that keeps hold of e012 naturally, 
	// So... I made a weird beast called a Flector2d that reflects across a line then translates along the same line
	// Turns out e1 points up and e2 points left for the translation 
	// DERIVATION: ON MA FONE
	inline const Flector2d operator * (const Line2d& l, const Motor2d& m) {
		Flector2d result;
		result.e012    = -l.e1 * m.e02 + l.e2 * m.e01 + l.e0 * m.e12;
		result.line.e1 = l.e1 * m.real - l.e2 * m.e12;
		result.line.e2 = l.e1 * m.e12 + l.e2 * m.real;
		result.line.e0 = -l.e1 * m.e01 - l.e2 * m.e02 + l.e0 * m.real;
		return result;
	}

	// Some sign flips if you multiply the other way round
	inline const Flector2d operator * (const Motor2d& m, const Line2d& l) {
		Flector2d result;
		result.e012    = -m.e02 * l.e1 + m.e12 * l.e0 + m.e01 * l.e2;
		result.line.e1 = m.real * l.e1 + m.e12 * l.e2;
		result.line.e2 = m.real * l.e2 - m.e12 * l.e1;
		result.line.e0 = m.real * l.e0 + m.e01 * l.e1 + m.e02 * l.e2;
		return result;
	}

	// TODO (UN): I'm just resuing my line * motor and motor * line algos here
	// for a line * point or a point * line and just skipping the real part and flipping sign on e02
	// Haven't scribbled it out tho
	inline const Flector2d operator * (const Line2d& l, const Point2d& p) {
		Flector2d result;
		result.e012    = l.e1 * p.e20 + l.e2 * p.e01 + l.e0 * p.e12;
		result.line.e1 = - l.e2 * p.e12;
		result.line.e2 = l.e1 * p.e12;
		result.line.e0 = -l.e1 * p.e01 + l.e2 * p.e20;
		return result;
	}

	inline const Flector2d operator * (const Point2d& p, const Line2d& l) {
		Flector2d result;
		result.e012    = p.e20 * l.e1 + p.e12 * l.e0 + p.e01 * l.e2;
		result.line.e1 = p.e12 * l.e2;
		result.line.e2 = -p.e12 * l.e1;
		result.line.e0 = p.e01 * l.e1 - p.e20 * l.e2;
		return result;
	}

	// Line2d 1 * Line2d 2 = a Motor2d
	// kinda like
	// Plane 1 * Plane 2 = a DualQuat
	// DERIVATION: https://github.com/ScottFielder/MathLibrary/blob/master/Notes/Multiplying_planes.pdf
	inline const Motor2d operator * (const Line2d& l1, const Line2d& l2)
	{
		Motor2d result;
		result.real = l1.e1 * l2.e1 + l1.e2 * l2.e2;
		result.e12  = l1.e1 * l2.e2 - l1.e2 * l2.e1;
		result.e01  = l1.e0 * l2.e1 - l1.e1 * l2.e0;
		result.e02  = l1.e0 * l2.e2 - l1.e2 * l2.e0;
		return result;
	}

	// Turns out the inverse of a Line2d is the exact same Line2d, so A / B = A * B if they are normalized
	// Just remember to divide out by magSquared for the inversed line
	inline const Motor2d operator / (const Line2d& a, const Line2d& b) {
		Line2d inverseB = b;
		float magSquared = inverseB.e1 * inverseB.e1 + inverseB.e2 * inverseB.e2;
		// The next line will blow up if b is infinitely far way, there is no inverse in that case
		inverseB = inverseB / magSquared;
		return a * inverseB;
	}

	// Point 1 * Point 2 = a Motor2d
	// I''m just reusing my Motor2d * Motor2d code here, 
	// but ignoring the real part and flipping the sign on e02
	// TODO (UN): I should scribble this out to make sure it's correct
	inline const Motor2d operator * (const Point2d& a, const Point2d& b) {
		Motor2d result;
		result.real = -(a.e12 * b.e12);
		result.e12 =  0.0f;
		result.e01 = -(a.e12 * b.e20) + (a.e20 * b.e12);
		result.e02 =  -(a.e12 * b.e01) + (a.e01 * b.e12);
		return result;
	}

	// I'll overload the division operator as well
	// Such that A / B = A * inverse(B)		in that order
	inline const Motor2d operator / (const Point2d& a, const Point2d& b) {
		Point2d inverseB = b;
		// First reverse it
		inverseB.e12 *= -1.0f;
		inverseB.e01 *= -1.0f;
		inverseB.e20 *= -1.0f;
		// Then divide by mag^2
		float magSquared = inverseB.e12 * inverseB.e12;
		// The next line will blow up if b is infinitely far way, there is no inverse in that case
		inverseB = inverseB / magSquared;
		return a * inverseB;
	}

	// If I multiply that weird Flector2d thing with a motor, 
	// I get another Flector2d thing
	inline const Flector2d operator * (const Flector2d& w, const Motor2d& m) {
		Flector2d result;
		Flector2d linePartMultipliedByMotor = w.line * m;

		result.e012    = linePartMultipliedByMotor.e012 + w.e012 * m.real;
		result.line.e1 = linePartMultipliedByMotor.line.e1;
		result.line.e2 = linePartMultipliedByMotor.line.e2;
		result.line.e0 = linePartMultipliedByMotor.line.e0 - w.e012 * m.e12;
		return result;
	}

}
#endif