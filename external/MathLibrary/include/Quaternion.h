#ifndef QUATERTNION_H
#define QUATERTNION_H
#include <iostream>
#include "ConstantsConversions.h"
#include "VMath.h"
#include "Matrix.h"

/// A quaternion can be written as a scalar plus a 3-vector (Vec3 ijk) component or


namespace  MATH {
union Quaternion {
public:
	struct {
		float i;
		float j;
		float k;
		float w;
	};

	struct {
		float e32;  // i is -e23 
		float e13;  // j is -e31
		float e21;  // k is -e12
		float real;
	};

	/// Just a little utility to populate a quaternion
	void set(float w_, float x_, float y_, float z_) {
		w = w_; i = x_; j = y_; k = z_;
	}

	/// Another little utility to populate a quaternion
	void set(float w_, Vec3 ijk_) {
		w = w_; i = ijk_.x; j = ijk_.y; k = ijk_.z;
	}

	/// This is the unit quaterion by definition 
	Quaternion() {
		set(1.0f, 0.0f, 0.0f, 0.0f);
	}

	Quaternion(float w_, const Vec3& ijk_) {
		set(w_, ijk_.x, ijk_.y, ijk_.z);
	}

	/// A copy constructor
	Quaternion(const Quaternion& q) {
		set(q.w, q.i, q.j, q.k);
	}

	/// An assignment operator   
	Quaternion& operator = (const Quaternion& q) {
		set(q.w, q.i, q.j, q.k);
		return *this;
	}

	/// Take the negative of a Quaternion
	Quaternion operator - () const {
		return Quaternion(-w, Vec3(-i, -j, -k));
	}

	/// Multiply a two quaternions - using the right-hand rule 
	/// 2022-02-12 Scott edit. Worried that Umer uncovered a bug in my code,
	/// I derived the multiply over again (this time less sexy) and it seems to work 
	/// correctly.
	Quaternion operator * (const Quaternion& q) const {
		Quaternion result;
		result.w = (w * q.w) - (i * q.i) - (j * q.j) - (k * q.k);
		result.i = (w * q.i) + (i * q.w) - (k * q.j) + (j * q.k);
		result.j = (w * q.j) + (j * q.w) - (i * q.k) + (k * q.i);
		result.k = (w * q.k) + (k * q.w) - (j * q.i) + (i * q.j);
		return result;		
	}

	Quaternion& operator *= (const Quaternion& q) {
		*this = q * *this;
		return *this;
	}

	Quaternion operator + (const Quaternion q) const {
		return Quaternion(w + q.w, Vec3(i + q.i, j + q.j, k + q.k));
	}

	Quaternion operator - (const Quaternion q) const {
		return Quaternion(w - q.w, Vec3(i - q.i, j - q.j, k - q.k));
	}

	Quaternion operator * (const float scalar) const {
		return Quaternion(w * scalar, Vec3(i * scalar, j * scalar, k * scalar));
	}

	Quaternion operator / (const float scalar) const {
		return Quaternion(w / scalar, Vec3(i / scalar, j / scalar, k / scalar));
	}

	/// Now we can use the Quaternion like an array but we'll need two overloads
	float operator [] (int index) const {  /// This one is for reading the Quaternion as if where an array
		return *((float*)this + index);
	}

	float& operator [] (int index) {	/// This one is for writing to the Quaternion as if where an array.  
		return *((float*)this + index);
	}

	void print(const char* comment = nullptr) const {
		if (comment) printf("%s\n", comment);
		printf("%1.4f %1.4f %1.4f %1.4f\n", i, j, k, w);
	}


	/////////////////////////////////////////////////////////////////////////
	/// This is just for teaching purposes - Caution, I'm getting out of control
	/////////////////////////////////////////////////////////////////////////
	/// Multiply a quaternion by a Vec3 (Quaternion * Vec3) 
	const Vec3 operator * (const Vec3& v_) const {
		/// Promote the Vec3 to a Quaternion and set w to be 0.0
		Quaternion p(0.0, v_);
		/// Now just call the Quaternion * Quaternion operator
		Quaternion result = *this * p;
		return Vec3(result.i, result.j, result.k);
	}
	/// Multiply a Vec3 by a Quaternion (Vec3 * Quaternion) 
	friend Vec3 operator * (const Vec3 v, const Quaternion& q) {
		Quaternion qv(0.0f, v);
		Quaternion result = qv * q;
		return Vec3(result.i, result.j, result.k);
	}

	/// Seriously, the tilde ~ is the complement operator not the 
	///  conjugate - but it was for fun. 
	Quaternion operator~() { return Quaternion(w, Vec3(-i, -j, -k)); }
	/////////////////////////////////////////////////////////////////////////

	};
}
#endif
