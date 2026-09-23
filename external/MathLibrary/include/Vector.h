#ifndef VECTOR_H
#define VECTOR_H
#include <iostream>
#include <string> /// Used for passing exceptions 
#include "ConstantsConversions.h"


/// Vec3 definitions followed by Vec4 then Vec2
/// There are notes at the bottom of this file you might want to read

namespace  MATH {

/// I will need to forward declare the union Vec4 for the "Vec3(const Vec4& v)" 
/// and "Vec3& operator = (const Vec4& v);" prototypes.  See the Vec3 code and the end of the 
/// Vec4 for the body of the code
union Vec4;

	union Vec3 {
		struct {
			float x, y, z;
		};

		struct {
			float e032, e013, e021;
		};

		/// Just a little utility to populate a vector
		constexpr void set(float x_, float y_, float z_) {
			x = x_; y = y_; z = z_;
		}

		/// Here's a set of constructors
		constexpr Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
		constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
		constexpr Vec3(const Vec3& v) : x(v.x), y(v.y), z(v.z) {}
		/// Create a Vec3 from a Vec4 - This is a bit of a bother. 
		/// The Vec4 definition has not been read yet so the compiler has no idea
		/// about Vec4. Just above the Vec3 definition, I do a forward declaration of 
		/// union Vec4. This allows the prototype to exist. The actual code for this 
		/// constructor is listed just after the end of the Vec4 definition. 
		
		/// Forward declaration 
		constexpr Vec3(const Vec4& v);
		Vec3& operator = (const Vec4& v); /// An assignment operator from a Vec4 

		/// Operator overloads (see note 1 at the end of this file)
		Vec3& operator = (const Vec3& v) {
			///if (this != &v) ///protection against self-assignment - should I bother?
			set(v.x, v.y, v.z);
			return *this;
		}

		/// Now we can use the Vec3 like an array but we'll need two overloads
		float operator [] (int index) const {  /// This one is for reading the Vec3 as if where an array
			return *(&x + index);
		}

		float& operator [] (int index) {	/// This one is for writing to the Vec3 as if where an array.  
			return *(&x + index);			/// See note 2 at the end of this file about lvalues and rvalues
		}

		Vec3 operator + (const Vec3& v) const {
			return Vec3(x + v.x, y + v.y, z + v.z);
		}

		Vec3& operator += (const Vec3& v) {
			x += v.x;
			y += v.y;
			z += v.z;
			return *this;
		}

		Vec3 operator - () const {
			return Vec3(-x, -y, -z);
		}

		Vec3 operator - (const Vec3& v) const {
			return Vec3(x - v.x, y - v.y, z - v.z);
		}

		Vec3& operator -= (const Vec3& v) {
			x -= v.x;
			y -= v.y;
			z -= v.z;
			return *this;
		}

		Vec3 operator * (const float s) const {
			return Vec3(s * x, s * y, s * z);
		}

		/// Multiply a scaler by a Vec3  It's the scalar first then the Vec3
		/// Overloaded and a friend, ouch! It's the only way to make it work with a scalar first.
		/// Friends are tricky, look them up. 
		friend Vec3 operator * (const float s, const Vec3& v) {
			return v * s;
		}

		Vec3& operator *= (const float s) {
			x *= s;
			y *= s;
			z *= s;
			return *this;
		}


		/// Divide by a scalar - Watch for divide by zero issues
		Vec3 operator / (const float s) const {
#ifdef _DEBUG  /// If in debug mode let's worry about divide by zero or nearly zero 
			if (fabs(s) < VERY_SMALL) {
				std::string errorMsg = "Divide by nearly zero ";
				throw errorMsg;
			}
#endif
			float r = 1.0f / s;
			return *this * r;
		}


		Vec3& operator /= (const float s) {
#ifdef _DEBUG  /// If in debug mode let's worry about divide by zero or nearly zero. 
			if (std::fabs(s) < VERY_SMALL) {
				std::string errorMsg = "Divide by nearly zero ";
				throw errorMsg;

			}
#endif 
			float r = 1.0f / s;
			*this *= r;
			return *this;
		}

		void print(const char* comment = nullptr) const {
			if (comment) printf("%s\n", comment);
			printf("%1.8f %1.8f %1.8f\n", x, y, z);
		}

		
		/// Type conversion operators 
		operator const float* () const {
			return static_cast<const float*>(&x);
		}

		operator float* () {
			return static_cast<float*>(&x);
		}


		

	};

	/// Vec4 definitions		
	union Vec4 {
		struct {
			float x, y, z, w;
		};
		struct {
			float  e032, e013, e021, e123;
		};

		constexpr void set(float x_, float y_, float z_, float w_) {
			x = x_; y = y_; z = z_; w = w_;
		}

		/// Here's a set of constructors
		constexpr Vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
		constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
		constexpr Vec4(const Vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}
		constexpr Vec4(const Vec3& v, const float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
		constexpr Vec4(const Vec3& v) : x(v.x), y(v.y), z(v.z), w(1.0f) {}

		/// An assignment operator
		Vec4& operator = (const Vec4& v) {
			x = v.x;
			y = v.y;
			z = v.z;
			w = v.w;
			return *this;
		}

		/// See how I did it in the Vec3 definition 
		float& operator [] (int index) {
			return *(&x + index);
		}
		float operator [] (int i) const {
			return *(&x + i);
		}

		/// See the Vec3 definition 
		Vec4 operator + (const Vec4& v) const {
			return Vec4(x + v.x, y + v.y, z + v.z, w + v.w);
		}

		Vec4& operator += (const Vec4& v) {
			x += v.x;
			y += v.y;
			z += v.z;
			w += v.w;
			return *this;
		}

		Vec4 operator - () const {
			return Vec4(-x, -y, -z, -w);
		}

		Vec4 operator - (const Vec4& v) const {
			return Vec4(x - v.x, y - v.y, z - v.z, w - v.w);
		}

		Vec4& operator -= (const Vec4& v) {
			x -= v.x;
			y -= v.y;
			z -= v.z;
			w -= v.w;
			return *this;
		}

		Vec4 operator * (const float s) const {
			return Vec4(s * x, s * y, s * z, s * w);
		}

		Vec4& operator *= (const float s) {
			x *= s;
			y *= s;
			z *= s;
			w *= s;
			return *this;
		}

		friend Vec4 operator * (const float s, const Vec4& v) {
			return v * s;
		}

		Vec4 operator / (const float s) const {
#ifdef _DEBUG  /// If in debug mode let's worry about divide by zero or nearly zero!!! 
			if (std::fabs(s) < VERY_SMALL) {
				std::string errorMsg = "Divide by nearly zero ";
				throw errorMsg;
			}
#endif
			float r = 1.0f / s;
			return *this * r;
		}

		Vec4& operator /= (const float s) {
#ifdef _DEBUG  /// If in debug mode let's worry about divide by zero or nearly zero!!! 
			if (std::fabs(s) < VERY_SMALL) {
				
			}
#endif 
			float r = 1.0f / s;
			*this *= r;
			return *this;
		}

		void print(const char* comment = nullptr) const {
			if (comment) printf("%s\n", comment);
			printf("%1.8f %1.8f %1.8f %1.8f\n", x, y, z, w);
		}


		/// Type conversion operators 
		operator const float* () const {
			return static_cast<const float*>(&x);
		}

		operator float* () {
			return static_cast<float*>(&x);
		}

	};


	/// These are defined in the Vec3 definition but because they have a Vec4 in them 
	/// I can't code them until after the Vec4 definition
	inline constexpr Vec3::Vec3(const Vec4& v) : x(v.x), y(v.y), z(v.z) {}
	inline Vec3& Vec3::operator = (const Vec4& v) {
		set(v.x, v.y, v.z);
		return *this;
	}



	struct Vec2 {
		float  x, y;
		/// Just a little utility to populate a vector
		constexpr void set(float x_, float y_) {
			x = x_; y = y_;
		}
		/// Here's a set of constructors
		constexpr Vec2(): x(0.0f),y(.0f) {}
		constexpr Vec2(float x_, float y_): x(x_),y(y_) {}
		

		/// A copy constructor
		Vec2(const Vec2& v) {
			set(v.x, v.y);
		}
		
		/// An assignment operator   
		Vec2& operator = (const Vec2& v) {
			set(v.x, v.y);
			return *this;
		}

		/// Now we can use the Vec2 like an array but we'll need two overloads
		float operator [] (int index) const {  /// This one is for reading the Vec2 as if where an array
			return *(&x + index);
		}

		float& operator [] (int index) {	/// This one is for writing to the Vec3 as if where an array.  
			return *(&x + index);			/// See note 2 at the end of this file about lvalues and rvalues
		}

		/// Add two Vec2s
		Vec2 operator + (const Vec2 v) const {
			return Vec2(x + v.x, y + v.y);
		}

		/// Add a Vec3 to itself
		Vec2& operator += (const Vec2& v) {
			x += v.x;
			y += v.y;
			return *this;
		}

		/// Take the negative of a Vec2
		Vec2 operator - () const {
			return Vec2(-x, -y);
		}

		/// Subtract two Vec3s
		Vec2 operator - (const Vec2& v) const {
			return Vec2(x - v.x, y - v.y);
		}

		/// Subtract a Vec 3 from itself
		Vec2& operator -= (const Vec2& v) {
			x -= v.x;
			y -= v.y;
			return *this;
		}

		/// Multiply a Vec2 by a scalar
		Vec2 operator * (const float s) const {
			return Vec2(s * x, s * y);
		}


		/// Multiply a scaler by a Vec2  It's the scalar first then the Vec2
		/// Overloaded and a friend, It's the only way to make it work with a scalar first. 
		friend Vec2 operator * (const float s, const Vec2& v) {
			return v * s;
		}

		/// Multiply a Vec2 by a scalar and assign it to itself
		Vec2& operator *= (const float s) {
			x *= s;
			y *= s;
			return *this;
		}


		/// Divide by a scalar - Watch for divide by zero issues
		Vec2 operator / (const float s) const {
#ifdef _DEBUG  /// If in debug mode let's worry about divide by zero or nearly zero 
			if (fabs(s) < VERY_SMALL) {
				std::string errorMsg = "Divide by nearly zero ";
				throw errorMsg;
			}
#endif
			float r = 1.0f / s;
			return *this * r;
		}


		Vec2& operator /= (const float s) {
#ifdef _DEBUG  /// If in debug mode let's worry about divide by zero or nearly zero!!! 
			if (std::fabs(s) < VERY_SMALL) {
				std::string errorMsg = "Divide by nearly zero ";
				throw errorMsg;
			}
#endif 
			float r = 1.0f / s;
			*this *= r;
			return *this;
		}

		void print(const char* comment = nullptr) const {
			if (comment) printf("%s\n", comment);
			printf("%1.8f %1.8f\n", x, y);
		}
	};
}

#endif


/*** Note 1.
I hate operator overloading as a general rule but a few make sense!! Just be careful and
be consistent. In this code, I will overload many operators. I don't believe in
overloading when the operator's job is less than obvious.
For example, in this class, the relational operators (== != < > <= >=) might mean "in relation
to their magnitude or direction" I'm just not sure. Just write a function to do that and don't make
me guess what the operator might mean. Use the idea of "Least Astonishment" don't surprise me,
or shock me or anything else that will piss me or others off -SSF
***/

/*** Note 2.
In straight C programming, lvalues and rvalues literally means left and right assignments.  For example,
int x = 5; x is the lvalue and 5 is the rvalue. Easy. In C++ it get a bit more tricky. The modern idea is
rvalues are temporary values residing in the registers of the CPU. lvalues are actual memory locations.
In the code:
	float& operator [] ( int index ) {
		return *(&x + index);
	}
To read this precisely, &x is the address of the x variable (the first in the list of x,y,z) add to that
address the index as if it were an array. Then *(&x + index) dereferences that address back into the
object, the float& reference returns it as an address and thus an lvalue.
***/