#ifndef RMATH_H
#define RMATH_H
#include "Sphere.h"
#include "Ray.h"
#include "Plane.h"
#include "Quadratic.h"
namespace MATHEX {
    using namespace MATH;
    class RMath {
    public:
          /// Done but not tested
        static bool doesIntersect(const Ray &ray, const Plane &plane){
            /// If the dot product of the ray's direction and the plane's normal are nearly zero,
            /// then the plane are ray are perpendicular therefore there is no intersection 
            Vec3 n = Vec3(plane.x, plane.y, plane.z);
            if (abs(VMath::dot(ray.direction, n) < VERY_SMALL)) {
                return false;
            }
            return true;
        }

        /// Done but not tested
        static Vec3 intersection(const Ray &ray, const Plane &plane){
            Vec3 n = Vec3(plane.x, plane.y, plane.z);
            float denominator = VMath::dot(ray.direction,n);
#ifdef _DEBUG  /// If in debug mode let's worry about no intersection at all 	
			if (abs(denominator) < VERY_SMALL) {
				std::string errorMsg = ": The ray and Plane are parallel - no intersection";
				throw errorMsg;
			}
#endif
            float t = -(VMath::dot(ray.start, n) - plane.d) / denominator;
            return ray.start + t * ray.direction;
        }

        /// Done but not tested
        static Roots intersection(const Ray& ray, const Sphere& sphere) {
            Vec3 CenterOfSphereToStart =  ray.start - sphere.center; 
            /// Setup the quadratic equation 
            float a = VMath::dot(ray.direction, ray.direction);
            float b = 2.0f * VMath::dot(CenterOfSphereToStart, ray.direction);
            float c = VMath::dot(CenterOfSphereToStart, CenterOfSphereToStart) - sphere.r * sphere.r;
            return Quadratic::findRoots(a, b, c);
        }
    };
}
#endif
