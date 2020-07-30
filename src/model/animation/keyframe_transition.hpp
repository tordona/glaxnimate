#pragma once

#include "math/bezier_solver.hpp"

#include <QObject>


namespace model {

namespace detail {
    struct SampleCache
    {
        std::array<double, 11> sample_values;
        bool clean = false;
    };
} // namespace detail

/**
 * \brief Describes the easing between two keyframes
 */
class KeyframeTransition: public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hold READ hold WRITE set_hold)
    Q_PROPERTY(Descriptive before READ before WRITE set_before NOTIFY before_changed STORED false)
    Q_PROPERTY(Descriptive after READ after WRITE set_after NOTIFY after_changed STORED false)
    Q_PROPERTY(math::Vec2 before_handle READ before_handle WRITE set_before_handle)
    Q_PROPERTY(math::Vec2 after_handle READ after_handle WRITE set_after_handle)

public:
    enum Descriptive
    {
        Constant,
        Linear,
        Ease,
        Custom,
    };
    Q_ENUM(Descriptive)

    const math::BezierSolver<math::Vec2>& bezier() const { return bezier_; }
    bool hold() const { return hold_; }

    Descriptive before() const;
    Descriptive after() const;
    math::Vec2 before_handle() const { return bezier_.points()[1]; }
    math::Vec2 after_handle() const  { return bezier_.points()[2]; }

    /**
     * \brief Get interpolation factor
     * \param ratio in [0, 1]. Determines the time ratio (0 = before, 1 = after)
     * \return A value in [0, 1]: the corresponding interpolation factor
     *
     * If the bezier is defined as B(t) = (x,y). This gives y given x.
     */
    Q_INVOKABLE double lerp_factor(double ratio) const;

    /**
     * \brief Get the bezier parameter at the given time
     * \param ratio in [0, 1]. Determines the time ratio (0 = before, 1 = after)
     * \return A value in [0, 1]: the corresponding bezier parameter
     *
     * If the bezier is defined as B(t) = (x,y). This gives t given x.
     */
    Q_INVOKABLE double bezier_parameter(double ratio) const;

public slots:
    void set_hold(bool hold);
    void set_before(Descriptive d);
    void set_after(Descriptive d);
    void set_handles(const math::Vec2& before, const math::Vec2& after);
    void set_before_handle(const math::Vec2& before);
    void set_after_handle(const math::Vec2& after);

signals:
    void before_changed(Descriptive d);
    void after_changed(Descriptive d);

private:
    math::BezierSolver<math::Vec2> bezier_ { math::Vec2(0, 0), math::Vec2(0, 0), math::Vec2(1, 1), math::Vec2(1, 1) };
    bool hold_ = false;
    mutable detail::SampleCache sample_cache_;
};

} // namespace model