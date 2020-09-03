#pragma once

#include "math/vector.hpp"
#include "math/functions.hpp"

namespace math {

template<class Vec>
class CubicBezierSolver
{
public:
    using vector_type = Vec;
    using scalar = scalar_type<Vec>;

    constexpr CubicBezierSolver(Vec p0, Vec p1, Vec p2, Vec p3) noexcept
    : points_{p0, p1, p2, p3} {}


    /**
     * \brief Finds the point along the bezier curve
     * \param factor between 0 and 1
     */
    constexpr Vec solve(scalar factor) const noexcept
    {
        Vec v;
        for ( int i = 0; i < detail::VecSize<Vec>::value; i++ )
            detail::get(v, i) = solve_component(factor, i);
        return v;
    }

    constexpr scalar solve_component(scalar factor, int component) const noexcept
    {
        return fast_cubic(
            factor,
            detail::get(points_[0], component),
            detail::get(points_[1], component),
            detail::get(points_[2], component),
            detail::get(points_[3], component)
        );
    }

    constexpr scalar derivative(scalar factor, int component) const noexcept
    {
        return (
            3 * a(detail::get(points_[0], component), detail::get(points_[1], component), detail::get(points_[2], component), detail::get(points_[3], component)) * factor +
            2 * b(detail::get(points_[0], component), detail::get(points_[1], component), detail::get(points_[2], component))
        ) * factor + c(detail::get(points_[0], component), detail::get(points_[1], component));
    }

    /**
     * \brief Finds the tangent of the point on the bezier
     * \param factor between 0 and 1
     * \return Angle in radians
     */
    scalar tangent_angle(scalar factor) const
    {
        return std::atan2(derivative(factor, 1), derivative(factor, 0));
    }

    constexpr const std::array<Vec, 4>& points() const noexcept
    {
        return points_;
    }

    constexpr std::array<Vec, 4>& points() noexcept
    {
        return points_;
    }

    /**
     * \brief Splits a bezier
     * \param factor value between 0 and 1 determining the split position
     * \return Two vectors for the two resulting cubic beziers
     */
    std::pair<std::vector<Vec>, std::vector<Vec>> split(scalar factor) const
    {
        Vec p01 = lerp(points_[0], points_[1], factor);
        Vec p12 = lerp(points_[1], points_[2], factor);
        Vec p23 = lerp(points_[2], points_[3], factor);

        Vec p012 = lerp(p01, p12, factor);
        Vec p123 = lerp(p12, p23, factor);

        Vec p0123 = lerp(p012, p123, factor);

        return {
            {points_[0], p01, p012, p0123},
            {p0123, p123, p23, points_[3]}
        };
    }

    std::pair<Vec, Vec> bounds()
    {
        std::vector<scalar> solutions;
        for ( int i = 0; i < detail::VecSize<Vec>::value; i++ )
        {
            scalar p0 = detail::get(points_[0], i);
            scalar p1 = detail::get(points_[1], i);
            scalar p2 = detail::get(points_[2], i);
            scalar p3 = detail::get(points_[3], i);
            scalar c_a = 3 * p3 - 9 * p2 + 9 * p1 - 3 * p0;
            scalar c_b = 6 * p2 - 12 * p1 + 6 * p0;
            scalar c_c = 3 * p1 - 3 * p0;
            bounds_solve(c_a, c_b, c_c, solutions);
        }

        std::vector<Vec> boundary_points;
        boundary_points.push_back(points_[0]); //Add Begin and end point not the control points!
        boundary_points.push_back(points_[3]);

        for ( scalar e : solutions )
            boundary_points.push_back(solve(e));

        Vec min;
        Vec max;
        for ( int i = 0; i < detail::VecSize<Vec>::value; i++ )
        {
            scalar cmin = std::numeric_limits<scalar>::max();
            scalar cmax = std::numeric_limits<scalar>::lowest();
            for ( const Vec& p : boundary_points )
            {
               if ( detail::get(p, i) < cmin )
                   cmin = detail::get(p, i);
               if ( detail::get(p, i) > cmax )
                   cmax = detail::get(p, i);
            }
            detail::get(max, i) = cmax;
            detail::get(min, i) = cmin;
        }

        return {min, max};
    }

private:
    // You get these terms by expanding the Bezier definition and re-arranging them as a polynomial in t
    // B(t) = a t^3 + b t^2 + c t + d
    static constexpr scalar a(const scalar& k0, const scalar& k1, const scalar& k2, const scalar& k3) noexcept
    {
        return -k0 + k1*3 + k2 * -3 + k3;
    }
    static constexpr scalar b(const scalar& k0, const scalar& k1, const scalar& k2) noexcept
    {
        return k0 * 3 + k1 * -6 + k2 * 3;
    }
    static constexpr scalar c(const scalar& k0, const scalar& k1) noexcept
    {
        return k0 * -3 + k1 * 3;
    }
    static constexpr scalar d(const scalar& k0) noexcept
    {
        return k0;
    }
    static constexpr scalar fast_cubic(scalar t, const scalar& k0, const scalar& k1, const scalar& k2, const scalar& k3) noexcept
    {
        return ((a(k0, k1, k2, k3) * t + b(k0, k1, k2) ) * t +  c(k0, k1) ) * t + d(k0);
    }

    static void bounds_solve(scalar a, scalar b, scalar c, std::vector<scalar>& solutions)
    {
        scalar d = b * b - 4. * a * c;

        // no real solution
        if ( d < 0 )
            return;

        if ( a == 0 )
        {
            // linear case
            add_bounds_solution(-c / b, solutions);
        }
        else
        {
            add_bounds_solution((-b + std::sqrt(d)) / (2 * a), solutions);

            if ( d != 0 )
                add_bounds_solution((-b - std::sqrt(d)) / (2 * a), solutions);
        }
    }

    static void add_bounds_solution(scalar solution, std::vector<scalar>& solutions)
    {
        if ( solution >= 0 && solution <= 1 )
            solutions.push_back(solution);
    }

    std::array<Vec, 4> points_;
};

#if 0
/**
 * \brief Bezier solver up to degree 3
 */
template<class Vec>
class BezierSolver
{
public:
    using vector_type = Vec;
    using scalar = scalar_type<Vec>;

    /**
     * \param points Bezier points (absolute values)
     */
    BezierSolver(std::vector<Vec> points) : points_(std::move(points)) {}

    BezierSolver(std::initializer_list<Vec> points) : points_(std::move(points)) {}

    /**
     * \brief Reduces the order in case of duplicate points
     */
    void optimize()
    {
        if ( points_.size() <= 2 )
            return;

        std::vector<Vec> new_points;
        new_points.reserve(points_.size());
        new_points.push_back(points_[0]);
        for ( std::size_t i = 1; i < points_.size(); i++ )
            if ( points_[i] != new_points.back() )
                new_points.push_back(points_[i]);

        if ( new_points.size() == 1 )
            new_points.push_back(points_.back());

        points_ = std::move(new_points);
    }

    /**
     * \brief Bezier order (eg: 1 for linear etc)
     */
    int order() const
    {
        return points_.size() - 1;
    }

    /**
     * \brief Finds the point along the bezier curve
     * \param factor between 0 and 1
     */
    Vec solve(scalar factor) const
    {
        Vec v;
        for ( int i = 0; i < detail::VecSize<Vec>::value; i++ )
            detail::get(v, i) = solve_component(factor, i);
        return v;
    }

    scalar solve_component(scalar factor, int component) const
    {
        switch ( order() )
        {
            case 3:
                return fast_cubic(
                    factor,
                    detail::get(points_[0], component),
                    detail::get(points_[1], component),
                    detail::get(points_[2], component),
                    detail::get(points_[3], component)
                );
            case 2:
                return fast_quadratic(
                    factor,
                    detail::get(points_[0], component),
                    detail::get(points_[1], component),
                    detail::get(points_[2], component)
                );
            case 1:
                return math::lerp(
                    detail::get(points_[0], component),
                    detail::get(points_[1], component),
                    factor
                );
            case 0:
                return detail::get(points_[0], component);
            // Slow but more general algorithm
            default:
            {
                scalar p = 0;
                for ( int i = 0; i < int(points_.size()); i++ )
                    p += detail::get(points_[i], component) * coefficient(i, order(), factor);
                return p;
            }
        }
    }

    /**
     * \brief Finds the tangent of the point on the bezier
     * \param factor between 0 and 1
     * \return Angle in radians
     */
    scalar tangent_angle(scalar factor) const
    {
        if ( order() <= 0 )
            return 0;

        Vec p;
        for ( int i = 0; i < int(points_.size()) - 1; i++ )
            p += (points_[i+1] - points_[i]) * order() * coefficient(i, order() - 1, factor);
        return std::atan2(p.y(), p.x());
    }

    /**
     * \brief Performs a bezier step, reducing the order of the points
     * \returns Bezier points of a smaller order
     */
    std::vector<Vec> solve_step(scalar factor) const
    {
        if ( order() < 1 )
            return points_;

        std::vector<Vec> next;
        Vec p1 = points_[0];
        for ( auto it = points_.begin() + 1; it != points_.end(); ++it )
        {
            next.push_back(lerp(p1, *it, factor));
            p1 = *it;
        }
        return next;
    }

    /**
     * \brief Splits a bezier
     * \param factor value between 0 and 1 determining the split position
     * \return Two vectors for the two resulting cubic beziers
     */
    std::pair<std::vector<Vec>, std::vector<Vec>> split_cubic(scalar factor) const
    {
        if ( order() == 1 )
        {
            auto k = lerp(points_[0], points_[1], factor);
            return {
                {points_[0], points_[0], k, k},
                {k, k, points_[1], points_[1]},
            };
        }

        BezierSolver quad_solv(order() == 2 ? points_ : solve_step(factor));
        auto lin = quad_solv.solve_step(factor);
        auto k = lerp(lin[0], lin[1], factor);
        return {
            {points_.front(), quad_solv.points_.front(), lin.front(), k},
            {k, lin.back(), quad_solv.points_.back(), points_.back()}
        };
    }

    scalar derivative(scalar factor, int component) const
    {
        switch ( order() )
        {
            case 3:
                return (
                    3 * a(detail::get(points_[0], component), detail::get(points_[1], component), detail::get(points_[2], component), detail::get(points_[3], component)) * factor +
                    2 * b(detail::get(points_[0], component), detail::get(points_[1], component), detail::get(points_[2], component))
                ) * factor + c(detail::get(points_[0], component), detail::get(points_[1], component));
            case 2:
                return 2 * q2(detail::get(points_[0], component), detail::get(points_[1], component), detail::get(points_[2], component)) * factor +
                           q1(detail::get(points_[0], component), detail::get(points_[1], component));
            case 1:
                return detail::get(points_[1], component) - detail::get(points_[0], component);
            default:
                return 0;
        }
    }

    const std::vector<Vec>& points() const
    {
        return points_;
    }

    std::vector<Vec>& points()
    {
        return points_;
    }

private:
    // You get these terms by expanding the Bezier definition and re-arranging them as a polynomial in t
    // B(t) = a t^3 + b t^2 + c t + d
    static constexpr scalar a(const scalar& k0, const scalar& k1, const scalar& k2, const scalar& k3) noexcept
    {
        return -k0 + k1*3 + k2 * -3 + k3;
    }
    static constexpr scalar b(const scalar& k0, const scalar& k1, const scalar& k2) noexcept
    {
        return k0 * 3 + k1 * -6 + k2 * 3;
    }
    static constexpr scalar c(const scalar& k0, const scalar& k1) noexcept
    {
        return k0 * -3 + k1 * 3;
    }
    static constexpr scalar d(const scalar& k0) noexcept
    {
        return k0;
    }
    static constexpr scalar fast_cubic(scalar t, const scalar& k0, const scalar& k1, const scalar& k2, const scalar& k3) noexcept
    {
        return ((a(k0, k1, k2, k3) * t + b(k0, k1, k2) ) * t +  c(k0, k1) ) * t + d(k0);
    }

    static constexpr scalar q2(const scalar& k0, const scalar& k1, const scalar& k2) noexcept
    {
        return k0 * 1 + k1 * -2 + k2;
    }
    static constexpr scalar q1(const scalar& k0, const scalar& k1) noexcept
    {
        return k0 * -2 + k1 * 2;
    }
    static constexpr scalar fast_quadratic(scalar t, const scalar& k0, const scalar& k1, const scalar& k2) noexcept
    {
        return k0 + (q1(k0, k1) + q2(k0, k1, k2) * t) * t;
    }

    scalar coefficient(int point_index, int order, scalar factor) const
    {
        return binom(order, point_index)
            * std::pow(factor, point_index)
            * std::pow(1-factor, order - point_index)
        ;
    }

    std::vector<Vec> points_;
};
#endif

} // namespace math