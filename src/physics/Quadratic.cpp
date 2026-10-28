#include "physics/Quadratic.h"

#include <algorithm>
#include <cmath>

#include "ConstantsConversions.h"

PHYSICS::QuadraticRoots PHYSICS::SolveQuadratic(const float a, const float b, const float c)
{
    QuadraticRoots solution{};
    /* With a = 0 the equation is not quadratic, and we would divide by zero below */
    if (std::abs(a) < MATH::kVerySmall)
    {
        solution.numSolutions = 0;
        return solution;
    }

    /* Calculate discriminant: b2 - 4ac */
    const float discriminant = b * b - 4.0f * a * c;
    
    /* One solution if discriminant is zero */
    if (std::abs(discriminant) < MATH::kVerySmall)
    {
        solution.numSolutions = 1;
        solution.smallerSolution = -b / (2.0f * a);
        solution.largerSolution = solution.smallerSolution;
        return solution;
    }

    /* No real solutions if discriminant is negative */
    if (discriminant < 0.0f)
    {
        solution.numSolutions = 0;
        return solution;
    }

    /* Two solutions if discriminant is positive */
    solution.numSolutions = 2;
    const float sqrtDiscriminant = std::sqrt(discriminant);
    const float solution1 = (-b - sqrtDiscriminant) / (2.0f * a);
    const float solution2 = (-b + sqrtDiscriminant) / (2.0f * a);

    /* Store the smaller solution first */
    solution.smallerSolution = std::min(solution1, solution2);
    solution.largerSolution = std::max(solution1, solution2);

    return solution;
}
