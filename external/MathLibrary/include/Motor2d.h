#ifndef MOTOR2D_H
#define MOTOR2D_H
#include <cmath>    // Used for fabs
#include <string>   // Used for stringing exception messages together
#include "ConstantsConversions.h" // Used for VERY_SMALL

// UN 2026-07-03
// We can rotate and translate points and lines
// Instead of calling this a rotator or translator, 
// I'll call it a motor as it can do one, the other, or both

namespace MATHEX {
	struct Motor2d {
	public:
		float real; // the scalar part
		float e01;  // the part used to translate along x
		float e02;  // the part used to translate along y
		float e12;  // the rotation part
		// Interestingly the three e values are used to make Point2d's too
		// I've kept it as e02 here but e20 in Point2d
		// so it matches how we do this in 3D too in DualQuat.h
		// REFERENCE: https://bivector.net/PROJECTIVE_GEOMETRIC_ALGEBRA.pdf

		inline void set(float real_, float e01_, float e02_, float e12_) {
			real = real_; 
			e01 = e01_; 
			e02 = e02_; 
			e12 = e12_;
		}

		// Our default constructor builds the identity motor (i.e. the number 1)
		inline Motor2d() {
			set(1.0f, 0.0f, 0.0f, 0.0f);
		}

		inline Motor2d(float real_, float e01_, float e02_, float e12_) {
			set(real_, e01_, e02_, e12_);
		}

		/// A copy constructor
		inline Motor2d(const Motor2d& m) {
			set(m.real, m.e01, m.e02, m.e12);
		}

		/// An assignment operator   
		inline Motor2d& operator = (const Motor2d& m) {
			set(m.real, m.e01, m.e02, m.e12);
			return *this;
		}

		// Multiply a motor by a scalar
		inline const Motor2d operator * (const float c) const {
			Motor2d result;
			result.real = real * c;
			result.e01  = e01 * c;
			result.e02  = e02 * c;
			result.e12  = e12 * c;
			return result;
		}

		// Multiply the other way around too
		friend inline const Motor2d operator * (const float c, const Motor2d& m) {
			return m * c;
		}

		// Multiply itself by a scalar
		inline Motor2d& operator *= (const float c) {
			real *= c;
			e01  *= c;
			e02  *= c;
			e12  *= c;
			return *this;
		}

		// Divide a motor by a scalar
		inline const Motor2d operator / (const float c) const {
#ifdef _DEBUG 
			if (std::fabs(c) < VERY_SMALL) {
				std::string errorMsg = std::string(__FILE__) + ":" + std::to_string(__LINE__);
				throw errorMsg.append(": Divide by nearly zero! ");
			}
#endif
			float r = 1.0f / c;
			return *this * r;
		}

		// Divide itself by a scalar
		inline Motor2d& operator /= (const float c) {
#ifdef _DEBUG 
			if (std::fabs(c) < VERY_SMALL) {
				std::string errorMsg = std::string(__FILE__) + ":" + std::to_string(__LINE__);
				throw errorMsg.append(": Divide by nearly zero! ");
			}
#endif
			real /= c;
			e01  /= c;
			e02  /= c;
			e12  /= c;
			return *this;
		}

		// Add two motors
		inline const Motor2d operator + (const Motor2d& m) const {
			return Motor2d(real + m.real, 
				            e01 + m.e01, 
				            e02 + m.e02, 
				            e12 + m.e12);
		}

		// Add a motor to itself
		inline Motor2d& operator += (const Motor2d& m) {
			real += m.real;
			e01  += m.e01;
			e02  += m.e02;
			e12  += m.e12;
			return *this;
		}

		// Take the negative of a motor
		inline const Motor2d operator - () const {
			return Motor2d(-real, -e01, -e02, -e12);
		}

		// Subract two motors
		inline const Motor2d operator - (const Motor2d& m) const {
			return Motor2d(real - m.real,
				            e01 - m.e01,
				            e02 - m.e02,
				            e12 - m.e12);
		}

		// Subtract a motor from itself
		inline Motor2d& operator -= (const Motor2d& m) {
			real -= m.real;
			e01  -= m.e01;
			e02  -= m.e02;
			e12  -= m.e12;
			return *this;
		}

		// Add to the real part of the motor
		inline const Motor2d operator + (float w_) const {
			Motor2d result;
			result.real = real + w_;
			result.e01 = e01;
			result.e02 = e02;
			result.e12 = e12;
			return result;
		}

		// Add the other way around too
		friend const Motor2d operator + (const float w_, const Motor2d& m) {
			return m + w_;
		}

		void print(const char* comment = nullptr) const {
			if (comment) printf("%s\n", comment);
			printf("%1.8f %1.8f %1.8f %1.8f\n", real, e01, e02, e12);
		}

	};
}

#endif // !MOTOR2D_H

