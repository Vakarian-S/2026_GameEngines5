#pragma once
#include <cstdint>

namespace PHYSICS
{
    /**
     * Struct containing the results of a Quadratic equation
     */
    struct QuadraticRoots
    {
        /** How many solutions exist */
        uint8_t numSolutions = 0;
        /** The Smaller of the two solutions */
        float smallerSolution = 0.f;
        /** The larger of the two solutions */
        float largerSolution = 0.f;
    };

    /**
     * Solves the quadratic equation ax^2 + bx + c = 0.
     * Determines the number and values of real solutions based on the coefficients.
     *
     * @param a Coefficient for the quadratic term (x^2). If zero, the equation is not quadratic.
     * @param b Coefficient for the linear term (x).
     * @param c Constant term.
     *
     * @return A QuadraticRoots struct containing the number of real solutions, as well as the
     *         values of the solutions (if they exist). The smaller solution is placed in
     *         `smallerSolution`, and the larger solution in `largerSolution`.
     *         If no real solutions exist, the `numSolutions` field is set to zero.
     */
    QuadraticRoots SolveQuadratic(float a, float b, float c);
}
