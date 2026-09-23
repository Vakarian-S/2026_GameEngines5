#ifndef MATH2D_H
#define MATH2D_H
#include "Line2d.h"
#include "Point2d.h"
#include "Motor2d.h"
#include "GeometricProduct2d.h"
#include "Dot2d.h"
#include "Dual2d.h"
#include "Meet2d.h"
#include "Join2d.h"

namespace MATHEX {

	struct Math2d {

		// As a line is just grade 1, there is nothing to swap
		// This is a no-op, so there is no point in doing this in case you are!
		static const Line2d reverse(const Line2d& line) {
			return line;
		}

		// This is a no-op if the line is normalized, so there is no point in doing this in case you are!
		static const Line2d inverse(const Line2d& line) {
			float magnitudeSquared = magSquared(line); // could also dot the line with itself (as it is its own reverse), but that doesnt work for motors
			return line / magnitudeSquared;
		}

		// Flip the sign on each bivector part
		static const Point2d reverse(const Point2d& p) {
			Point2d result;
			result.e20 = -p.e20;
			result.e01 = -p.e01;
			result.e12 = -p.e12;
			return result;
		}

		// Ensure p * p_inverse = 1
		static const Point2d inverse(const Point2d& p) {
			Point2d result = reverse(p);
			float w = p.e12; // magnitude means different things to a Vec3 versus a Point2d so I wont use p dot p
			result = result / (w * w);
			return result;
		}

		// Flip the sign on each bivector part
		static const Motor2d reverse(const Motor2d& m) {
			Motor2d result;
			// Don't flip the sign on the real part, just the others
			result.real =  m.real;
			result.e01  = -m.e01;
			result.e02  = -m.e02;
			result.e12  = -m.e12;
			return result;
		}

		// Ensure m * m_inverse = 1
		static const Motor2d inverse(const Motor2d& m) {
			Motor2d result = reverse(m);
			float magnitudeSquared = magSquared(m); // dotting the motor with itself wont work
			result = result / magnitudeSquared;
			return result;
		}

		// Return a pure rotation Motor2d, built from an angle in degrees
		// Isn't it cool how similar this is to the 3D version QMath::angleAxisRotation
		static const Motor2d rotate(float angleDeg) {
			float theta = angleDeg * DEGREES_TO_RADIANS;
			float cosVal = cos(theta / 2.0f);
			float sinVal = sin(theta / 2.0f);
			// A rotation is cos(theta/2) - sin(theta/2) * e12
			Motor2d result;
			result.real = cosVal;
			result.e01 = 0.0f;
			result.e02 = 0.0f;
			result.e12 = -sinVal;
			return result;
		}

		// Return a pure rotation motor from inside a general motor (that could have translation too)
		static const Motor2d rotate(const Motor2d& m) {
			// Rotation is always the first four elements
			// even if the original transform is, for example, T * R or R * T
			// Not so easy for translations as you'll see in getTranslation
			Motor2d result;
			result.real = m.real;
			result.e01 = 0.0f; // zero out the translation parts
			result.e02 = 0.0f;
			result.e12 = m.e12;
			return result;
		}
		// Return a pure translation Motor2d, built from a translation
		static const Motor2d translate(float x, float y) {
			// A translation is 1 - x/2 * e01 - y/2 * e02
			Motor2d result;
			result.real = 1.0f;
			result.e01 = -x / 2.0f;
			result.e02 = -y / 2.0f;
			result.e12 = 0.0f;
			return result;
		}

		// Return a pure translation Motor2d, built from a Point2d translation
		static const Motor2d translate(const Point2d& translation) {
			// A translation is 1 - x/2 * e01 - y/2 * e02
			Motor2d result;
			result.real = 1.0f;
			result.e01 = -translation.x / 2.0f;
			result.e02 = -translation.y / 2.0f;
			result.e12 = 0.0f;
			return result;
		}

		// Return just the translation parts of a motor
		static const Motor2d translate(const Motor2d& m) {
			Motor2d result;
			result.real = 1.0f;
			result.e01  = m.e01;
			result.e02  = m.e02;
			result.e12  = 0.0f;
			return result;
		}

		// TODO (UN): Why do the eZero and !eZero do the magic?
		//	Reference: https://www.youtube.com/watch?v=2DgxeizE3E8	New Hope I
		// Return a pure translation motor using a distance and a line
		// Isn't it cool how similar this is to the 3D version DQMath::translateAlongLine
		static const Motor2d translateAlongLine(float dist, const Line2d& line) {
			Line2d eZero(0.0f, 0.0f, 1.0f);
			// !eZero is eOneTwo
			// Motor2d() is just the number one
			// UN - I needed a plus here rather than a minus in the video
			return Motor2d() + eZero * (normalize(line) * dist / 2.0f) * !eZero;
		}

	
		// A rigid transform is a rotate, a translate, or a combination of both.
		// REFERENCE: https://bivector.net/PROJECTIVE_GEOMETRIC_ALGEBRA.pdf
		static const Point2d rigidTransformation(const Motor2d& m, const Point2d& p) {
			// Weirdly enough, the sandwich with a point returns a motor but I think the real part is always zero
			// so I've just grabbed the point part of the motor
			Point2d result = getPoint(m * p * reverse(m));
			return result;
		}

		static const Line2d rigidTransformation(const Motor2d& m, const Line2d& l) {
			// Weirdly enough, the sandwich with a line returns a weird e012 plus a line thing
			// so I've just grabbed the line part
			Line2d result = (m * l * reverse(m)).line;
			return result;
		}

		// I like the name sandwich, but maybe it's not for everyone
		static const Point2d sandwich(const Motor2d& m, const Point2d& p) {
			return rigidTransformation(m, p);
		}

		static const Line2d sandwich(const Motor2d& m, const Line2d& l) {
			return rigidTransformation(m, l);
		}

		static const float getAngleDeg(const Motor2d& m) {
			// Find the angle using the real part of the motor
			// R = cos(theta/2) - sin(theta/2) * e12
			float cosThetaOver2 = m.real;
			float sinThetaOver2 = -m.e12;
			// atan2 is best to get the angle as it handles all four quadrants
			// and returns an angle between -180 and 180
			float thetaOver2 = atan2(sinThetaOver2, cosThetaOver2);
			float angleRad = thetaOver2 * 2.0f;
			float result = angleRad * RADIANS_TO_DEGREES;
			return result;
		}

		// Return just the point part of the motor
		static const Point2d getPoint(const Motor2d& m) {
			// Just ignore the real part of the motor and flip the e02 part
			Point2d result;
			result.e01 =  m.e01;
			result.e20 = -m.e02;
			result.e12 =  m.e12;
			return result;
		}

		// Return just the rotation part of the motor
		static const Motor2d getRotation(const Motor2d& m) {
			// Find the rotation using just the real and e12 parts of the motor
			Motor2d result;
			result.real = m.real;
			result.e01 = 0.0f;
			result.e02 = 0.0f;
			result.e12 = m.e12;
			return result;
		}

		// Return a pure translation motor from inside a general motor (that could have rotation too)
		static const Motor2d getTranslationMotor(const Motor2d& m) {
			return translate(getTranslation(m));
		}

		// Return the translation vector from the motor
		static const Point2d getTranslation(const Motor2d& m)
		{
			// NOTE (UN): I've stolen this from DQMath
			// We are doing old school dual quaternion math here
			// Where a dual quaternion is literally made up of two quaternions
			// A real one (that holds the rotation), and a dual one (that encodes translation)
			// So dual quaternion = q_rot + dualBasis * q_t * q_rot
			// Find translation from the last bit of the dual quaternion
			// First make sure its a valid motor coming in
			Motor2d normalizedMotor = normalize(m);
			Motor2d dualPart = translate(normalizedMotor);
			Motor2d realPart = rotate(normalizedMotor);
			// Rebuild the translation using t * r.conjugate * 2
			// To conjugate our dual quaternion, we flip the sign on the axis of the real part 
			realPart.e12 *= -1.0f;
			Motor2d transformed = dualPart * realPart * 2.0f;
			// Remember a translation dual quat is 1 - deltaX/2 e01 - deltaY/2 e02 - deltaZ/2 e03
			// So we need minus signs for the next step
			Point2d translation = Point2d(-transformed.e01, -transformed.e02, 0.0f);
			return translation;
		}


		// Slerp from one translation and orientation to another translation and orientation
		// Just like the regular quaternion slerp, but now we can include position too!
		// EXAMPLE:   https://github.com/ScottFielder/MathLibrary/blob/master/Notes/Dual_quat_slerp.pdf
		// IN ACTION: https://youtu.be/deX-1AAbifA
		static const Motor2d slerp(const Motor2d& start, const Motor2d& end, float t) {
			// The slerp is written as 
			// exp(t * log(end/start)) * start
			// Let's turn this into something human readable
			Motor2d endOverStart = end * inverse(start);

			// The log brings out the rotation axis & angle/2 
			// and the translation axis and displacement/2
			Point2d translation = getTranslation(endOverStart);
			float angleDeg      = getAngleDeg(endOverStart);

			// Now multiply the angle and translation by t
			translation *= t;
			angleDeg    *= t;

			// The exp means turn it all back into a dual quaternion
			Motor2d transformedDqRot = rotate(angleDeg);
			Motor2d transformedDqTra = translate(translation);
			Motor2d transformedDq    = transformedDqTra * transformedDqRot;

			// Lastly multiply it with the start
			Motor2d result = transformedDq * start;
			return result;
		}

		static const float magSquared(const Motor2d& m) {
			return (m * reverse(m)).real;
		}

		static const float magSquared(const Line2d& line) {
			return (line * reverse(line)).real;
		}

		// Return magnitude of the rotator (1 + e12 terms) or translator (1 + e01 + e2 terms)
		// Ignore the e01 & e02 bits
		static const float mag(const Motor2d& m) {
			return sqrt(m.real * m.real + m.e12 * m.e12);
			// could also return sqrt(magSquared(m));
		}

		// Return the magnitude of the bits that aren't infinitely far away
		// ie ignore e0
		static const float mag(const Line2d& l) {
			return sqrt(l.e1 * l.e1 + l.e2 * l.e2);
			// could also return sqrt(magSquared(line));
		}

		// NOTE (UN): I was thinking to make a mag(Point2d) function but I think that will get confusing
		// In PGA2D, the magnitude squared of a point = e12 * e12 or w * w
		// In regular 2D vectors, it would be x^2 + y^2
		 
		static const Motor2d normalize(const Motor2d& m)
		{
			return m / mag(m);
		}

		static const Line2d normalize(const Line2d& line)
		{
			return line / mag(line);
		}

		/// Return a Point2d with the w component divided out
		static const Point2d perspectiveDivide(const Point2d& v) {
#ifdef _DEBUG  /// If in debug mode let's worry about divide by zero or nearly zero!!! 	
			if (fabs(v.w) < VERY_SMALL) {
				std::string errorMsg = std::string(__FILE__) + ":" + std::to_string(__LINE__);
				throw errorMsg.append(": Divide by nearly zero! ");
			}
#endif
			return Point2d(v.x / v.w, v.y / v.w, 1.0f);
		}

		// Oriented distance between a point and a line (sign tells you which side of the line)
		// EXAMPLE: https://github.com/ScottFielder/MathLibrary/blob/master/Notes/Oriented_distance_point_and_line.pdf
		static const float orientedDist(const Point2d& v, const Line2d& m) {
			// First normalize the point and plane 
			Point2d vNormalized = perspectiveDivide(v);
			Line2d  mNormalized = normalize(m);
			// Then use the formula for the oriented distance from https://bivector.net/3DPGA.pdf
			float dist = join(vNormalized, mNormalized).real; // join a point and a line to get the scalar part of a motor

			// TODO (UN): I'm completely making this up by looking at my PGA3D code 
			// will need to check if the real part of the motor is actually the oriented distance
			return dist;
		}

	
		// The squareRoot is defined as an even mixture with the number 1, then normalized
		// That gives you a transform halfway from start to end
		// I named it squareRoot to avoid any clashes with std::sqrt in user code
		static const Motor2d squareRoot(const Motor2d& m) {
			// Do a similar shortest-path trick as QMath::slerp
			// It also solves the problem of everything blowing up if m = -1
			float cosThetaOverTwo = m.real;
			if (cosThetaOverTwo < 0.0f) {		// if cosTheta is negative, the angle is oblique. The shortest path 
				return normalize(1.0f + (-m));  // would be the other representation of the same motor = -m 
			}
			return normalize(1.0f + m);
		}

		// A motor that takes a point/line/motor "a" to a point/line/motor "b" is:
		// motor = sqrt(b/a)
		// REFERENCE: https://observablehq.com/@enkimute/glu-lookat-in-3d-pga
		static const Motor2d motorAtoB(const Point2d& a, const Point2d& b) {
			return squareRoot(b / a);
		}

		static const Motor2d motorAtoB(const Line2d& a, const Line2d& b) {
			return squareRoot(b / a);
		}

		static const Motor2d motorAtoB(const Motor2d& a, const Motor2d& b) {
			return squareRoot(b / a);
		}

		// Projecting a point onto a line
		// REFERENCE: Equations from the 58:51 min mark here https://www.gdcvault.com/play/1029237/
		// UN - Tested 2025-02-24 for Sphere-Triangle collision assignment in Game Physics 2
		static const Point2d project(const Point2d& point, const Line2d& line) {
			return (line | point) ^ line;
		}

		// Projecting a line onto a point
		// UN - Tested 3D version on 2025-03-10 for my Voronoi madness in QuadMath::closestPointOnQuad
		static const Line2d project(const Line2d& line, const Point2d& point) {
			// Ooh, looks like we might actually need the geometric product here compared to the project point onto line
			return ((point | line) * point).line;
		}

		// Just the distance between two points, but done in a PGA way to test out my other functions
		// You could just use VMath::distance
		// REFERENCE: https://bivector.net/2DPGA.pdf
		static const float distance(const Point2d& p1, const Point2d& p2) {
			float result;
			Point2d p1_normalized = perspectiveDivide(p1);
			Point2d p2_normalized = perspectiveDivide(p2);
			Line2d line = p1_normalized & p2_normalized;
			result = mag(line);
			return result;
		}
	};
}
#endif