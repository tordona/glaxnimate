#include "geom.hpp"

#include <QVector3D>

using namespace glaxnimate;


QPointF math::line_closest_point(const QPointF& line_a, const QPointF& line_b, const QPointF& p)
{
    QPointF a_to_p = p - line_a;
    QPointF a_to_b = line_b - line_a;

    qreal atb2 = length_squared(a_to_b);
    qreal atp_dot_atb = QPointF::dotProduct(a_to_p, a_to_b);
    qreal t = atp_dot_atb / atb2;

    return line_a + a_to_b * t;
}

// Algorithm from http://ambrsoft.com/TrigoCalc/Circle3D.htm
QPointF math::circle_center(const QPointF& p1, const QPointF& p2, const QPointF& p3)
{
    qreal x1 = p1.x();
    qreal x2 = p2.x();
    qreal x3 = p3.x();
    qreal y1 = p1.y();
    qreal y2 = p2.y();
    qreal y3 = p3.y();
    qreal A = 2 * (x1 * (y2 - y3) - y1 * (x2 - x3) + x2 * y3 - x3 * y2);
    qreal p12 = x1*x1 + y1*y1;
    qreal p22 = x2*x2 + y2*y2;
    qreal p32 = x3*x3 + y3*y3;
    qreal B = p12 * (y3 - y2) + p22 * (y1 - y3) + p32 * (y2 - y1);
    qreal C = p12 * (x2 - x3) + p22 * (x3 - x1) + p32 * (x1 - x2);

    return {
        - B / A,
        - C / A
    };
}



std::optional<QPointF> math::line_intersection(const QPointF& start1, const QPointF& end1, const QPointF& start2, const QPointF& end2)
{
    QVector3D v1(start1.x(), start1.y(), 1);
    QVector3D v2(end1.x(), end1.y(), 1);
    QVector3D v3(start2.x(), start2.y(), 1);
    QVector3D v4(end2.x(), end2.y(), 1);

    QVector3D cp = QVector3D::crossProduct(
        QVector3D::crossProduct(v1, v2),
        QVector3D::crossProduct(v3, v4)
    );

    if ( qFuzzyIsNull(cp.z()) )
        return {};

    return QPointF(cp.x() / cp.z(), cp.y() / cp.z());
}
