#ifndef FLECTOR2D_H
#define FLECTOR2D_H
#include "Line2d.h"

// Yeah, this is a weird one. If I mulitply a Line2d with a Point2d or a Motor2d
// I get this weird thing thats a Line2d along with the pseudoscalar e012
// I tried transforming points with it, using the sandwich, and it turns out this thing does two thing
// 1. It reflects across the line
// 2. Then it translates 2 * coeff on e012 along the line
// My scribbles suggest that e1 points up and e2 points left!
// It also seems to be the dual of a Motor2d, which is cool

// Ok, so it reflects and translates. That sounds like a transflector
// So I'll use the name Flector2d to mirror the name I use in PGA3D

namespace MATHEX {
	struct Flector2d {
	public:
		float e012;
		Line2d line;

		inline void set(float e012_, Line2d line_) {
			e012 = e012_;
			line = line_;
		}

		// Our default constructor builds a line x = 0
		inline Flector2d() {
			e012 = 0.0f;
			line.set(1.0f, 0.0f, 0.0f);
		}

		inline Flector2d(float e012_, Line2d line_) {
			set(e012_, line_);
		}

		/// An assignment operator   
		inline Flector2d& operator = (const Flector2d& p) {
			set(p.e012, p.line);
			return *this;
		}

		// Multiply a line by a scalar
		inline const Flector2d operator * (const float scalar) const {
			Flector2d result;
			result.e012 = e012 * scalar;
			result.line = line * scalar;
			return result;
		}

		// Multiply the other way around too
		friend inline const Flector2d operator * (const float scalar, const Flector2d& l) {
			return l * scalar;
		}

		// Multiply itself by a scalar
		inline Flector2d& operator *= (const float scalar) {
			e012 *= scalar;
			line *= scalar;
			return *this;
		}

		// Divide a line by a scalar
		inline const Flector2d operator / (const float scalar) const {
#ifdef _DEBUG 
			if (std::fabs(scalar) < VERY_SMALL) {
				std::string errorMsg = std::string(__FILE__) + ":" + std::to_string(__LINE__);
				throw errorMsg.append(": Divide by nearly zero! ");
			}
#endif
			float r = 1.0f / scalar;
			return *this * r;
		}

		// Divide itself by a scalar
		inline Flector2d& operator /= (const float scalar) {
#ifdef _DEBUG 
			if (std::fabs(scalar) < VERY_SMALL) {
				std::string errorMsg = std::string(__FILE__) + ":" + std::to_string(__LINE__);
				throw errorMsg.append(": Divide by nearly zero! ");
			}
#endif
			e012 /= scalar;
			line /= scalar;
			return *this;
		}

		// Add two lines to get a weighted average
		inline const Flector2d operator + (const Flector2d& l) const {
			return Flector2d(e012 + l.e012, line + l.line);
		}

		// Add a line to itself
		inline Flector2d& operator += (const Flector2d& l) {
			e012 += l.e012;
			line += l.line;
			return *this;
		}

		// Take the negative of a line
		inline const Flector2d operator - () const {
			return Flector2d(-e012, -line);
		}

		// Subract two lines
		inline const Flector2d operator - (const Flector2d& l) const {
			return Flector2d(e012 - l.e012, line - l.line);
		}

		// Subtract a line from itself
		inline Flector2d& operator -= (const Flector2d& l) {
			e012 -= l.e012;
			line -= l.line;
			return *this;
		}

		void print(const char* comment = nullptr) const {
			if (comment) printf("%s\n", comment);
			printf("%1.8f %1.8f %1.8f %1.8f\n", e012, line.x, line.y, line.c);
		}

	};
}


#endif

