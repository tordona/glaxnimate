#include "transform_graphics_item.hpp"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include "model/document.hpp"
#include "command/animation_commands.hpp"
#include "math/math.hpp"

class graphics::TransformGraphicsItem::Private
{
public:
    enum Index
    {
        TL,
        TR,
        BR,
        BL,
        Top,
        Bottom,
        Left,
        Right,

        Anchor,
        Rot,

        Count
    };

    struct Handle
    {
        MoveHandle* handle;
        QPointF (Private::* get_p)()const;
        void (TransformGraphicsItem::* signal)(const QPointF&);
    };

    model::Transform* transform;
    model::DocumentNode* target;
    std::array<Handle, Count> handles;
    QRectF cache;
    QTransform transform_matrix;
    QTransform transform_matrix_inv;

    QPointF get_tl() const { return cache.topLeft(); }
    QPointF get_tr() const { return cache.topRight(); }
    QPointF get_br() const { return cache.bottomRight(); }
    QPointF get_bl() const { return cache.bottomLeft(); }
    QPointF get_t() const { return {cache.center().x(), cache.top()}; }
    QPointF get_b() const { return {cache.center().x(), cache.bottom()}; }
    QPointF get_l() const { return {cache.left(), cache.center().y()}; }
    QPointF get_r() const { return {cache.right(), cache.center().y()}; }
    QPointF get_a() const
    {
        return transform->anchor_point.get();
    }
    QPointF get_rot() const
    {
        return {
            cache.center().x(),
            cache.top() + rot_top()
        };
    }
    qreal rot_top() const
    {
        return -32 / transform->scale.get().y();
    }

    void set_pos(const Handle& h) const
    {
        h.handle->setPos((this->*h.get_p)());
    }

    Private(TransformGraphicsItem* parent, model::Transform* transform, model::DocumentNode* target)
    : transform(transform),
        target(target),
        handles{
            Handle{
                new MoveHandle(parent, MoveHandle::DiagonalDown, MoveHandle::Square, 6, true),
                &TransformGraphicsItem::Private::get_tl,
                &TransformGraphicsItem::drag_tl
            },
            Handle{
                new MoveHandle(parent, MoveHandle::DiagonalUp, MoveHandle::Square, 6, true),
                &TransformGraphicsItem::Private::get_tr,
                &TransformGraphicsItem::drag_tr
            },
            Handle{
                new MoveHandle(parent, MoveHandle::DiagonalDown, MoveHandle::Square, 6, true),
                &TransformGraphicsItem::Private::get_br,
                &TransformGraphicsItem::drag_br
            },
            Handle{
                new MoveHandle(parent, MoveHandle::DiagonalUp, MoveHandle::Square, 6, true),
                &TransformGraphicsItem::Private::get_bl,
                &TransformGraphicsItem::drag_bl
            },
            Handle{
                new MoveHandle(parent, MoveHandle::Vertical, MoveHandle::Diamond, 8, true),
                &TransformGraphicsItem::Private::get_t,
                &TransformGraphicsItem::drag_t
            },
            Handle{
                new MoveHandle(parent, MoveHandle::Vertical, MoveHandle::Diamond, 8, true),
                &TransformGraphicsItem::Private::get_b,
                &TransformGraphicsItem::drag_b
            },
            Handle{
                new MoveHandle(parent, MoveHandle::Horizontal, MoveHandle::Diamond, 8, true),
                &TransformGraphicsItem::Private::get_l,
                &TransformGraphicsItem::drag_l
            },
            Handle{
                new MoveHandle(parent, MoveHandle::Horizontal, MoveHandle::Diamond, 8, true),
                &TransformGraphicsItem::Private::get_r,
                &TransformGraphicsItem::drag_r
            },
            Handle{
                new MoveHandle(parent, MoveHandle::Any, MoveHandle::Square, 6, true),
                &TransformGraphicsItem::Private::get_a,
                &TransformGraphicsItem::drag_a
            },
            Handle{
                new MoveHandle(parent, MoveHandle::Rotate, MoveHandle::Circle, 6, true),
                &TransformGraphicsItem::Private::get_rot,
                &TransformGraphicsItem::drag_rot
            },
        }
    {
    }

    qreal find_scale(QPointF target_local, qreal size_to_anchor, qreal anchor_lin, qreal target_local_lin, qreal old_scale)
    {

        QPointF ap = transform_matrix.map(transform->anchor_point.get());
        QPointF target = transform_matrix.map(target_local);
        qreal target_length = math::length(target - ap);
        qreal new_scale = target_length / size_to_anchor;
        auto sign = math::sign(target_local_lin - anchor_lin);
        if ( sign != math::sign(old_scale) )
            new_scale *= -1;
        if ( qFuzzyCompare(new_scale, 0) )
            new_scale = 0.01 * sign;

        return new_scale;
    }

    void push_command(model::AnimatableBase& prop, const QVariant& value, bool commit)
    {
        target->document()->undo_stack().push(new command::SetMultipleAnimated(
            prop.name(), {&prop}, {prop.value()}, {value}, commit
        ));
    }

};

graphics::TransformGraphicsItem::TransformGraphicsItem(
    model::Transform* transform, model::DocumentNode* target, QGraphicsItem* parent
)
    : QGraphicsObject(parent), d(std::make_unique<Private>(this, transform, target))
{
    connect(target, &model::DocumentNode::bounding_rect_changed, this, &TransformGraphicsItem::update_handles);
    connect(transform, &model::Object::property_changed, this, &TransformGraphicsItem::update_transform);
    update_transform();
    update_handles();
    for ( const auto& h : d->handles )
    {
        connect(h.handle, &MoveHandle::dragged, this, h.signal);
        if ( &h == &d->handles[Private::Anchor] )
            connect(h.handle, &MoveHandle::drag_finished, this, &TransformGraphicsItem::commit_anchor);
        else if ( &h == &d->handles[Private::Rot] )
            connect(h.handle, &MoveHandle::drag_finished, this, &TransformGraphicsItem::commit_rot);
        else
            connect(h.handle, &MoveHandle::drag_finished, this, &TransformGraphicsItem::commit_scale);
    }
}

graphics::TransformGraphicsItem::~TransformGraphicsItem() = default;

void graphics::TransformGraphicsItem::update_handles()
{
    prepareGeometryChange();
    d->cache = d->target->local_bounding_rect(d->target->document()->current_time());
    for ( const auto& h : d->handles )
    {
        d->set_pos(h);
    }
}

void graphics::TransformGraphicsItem::update_transform()
{
    d->transform_matrix = d->transform->transform_matrix();
    d->transform_matrix_inv = d->transform_matrix.inverted();
    d->set_pos(d->handles[Private::Rot]);
    d->set_pos(d->handles[Private::Anchor]);
}


void graphics::TransformGraphicsItem::drag_tl(const QPointF& p)
{
    auto scale = d->transform->scale.get();
    qreal size_to_anchor_y = d->cache.top() - d->transform->anchor_point.get().y();
    qreal size_to_anchor_x = d->cache.left() - d->transform->anchor_point.get().x();

    if ( size_to_anchor_y != 0 )
    {
        scale.setY(d->find_scale(
            QPointF(d->transform->anchor_point.get().x(), p.y()),
            size_to_anchor_y,
            d->transform->anchor_point.get().y(),
            p.y(),
            scale.y()
        ));
    }

    if ( size_to_anchor_x != 0 )
    {
        scale.setX(d->find_scale(
            QPointF(p.x(), d->transform->anchor_point.get().y()),
            size_to_anchor_x,
            d->transform->anchor_point.get().x(),
            p.x(),
            scale.x()
        ));
    }

    if ( size_to_anchor_x || size_to_anchor_y )
        d->push_command(d->transform->scale, scale, false);
}

void graphics::TransformGraphicsItem::drag_tr(const QPointF& p)
{
    auto scale = d->transform->scale.get();
    qreal size_to_anchor_y = d->cache.top() - d->transform->anchor_point.get().y();
    qreal size_to_anchor_x = d->cache.right() - d->transform->anchor_point.get().x();

    if ( size_to_anchor_y != 0 )
    {
        scale.setY(d->find_scale(
            QPointF(d->transform->anchor_point.get().x(), p.y()),
            size_to_anchor_y,
            d->transform->anchor_point.get().y(),
            p.y(),
            scale.y()
        ));
    }

    if ( size_to_anchor_x != 0 )
    {
        scale.setX(d->find_scale(
            QPointF(p.x(), d->transform->anchor_point.get().y()),
            size_to_anchor_x,
            d->transform->anchor_point.get().x(),
            p.x(),
            scale.x()
        ));
    }

    if ( size_to_anchor_x || size_to_anchor_y )
        d->push_command(d->transform->scale, scale, false);
}

void graphics::TransformGraphicsItem::drag_br(const QPointF& p)
{
    auto scale = d->transform->scale.get();
    qreal size_to_anchor_y = d->cache.bottom() - d->transform->anchor_point.get().y();
    qreal size_to_anchor_x = d->cache.right() - d->transform->anchor_point.get().x();

    if ( size_to_anchor_y != 0 )
    {
        scale.setY(d->find_scale(
            QPointF(d->transform->anchor_point.get().x(), p.y()),
            size_to_anchor_y,
            d->transform->anchor_point.get().y(),
            p.y(),
            scale.y()
        ));
    }

    if ( size_to_anchor_x != 0 )
    {
        scale.setX(d->find_scale(
            QPointF(p.x(), d->transform->anchor_point.get().y()),
            size_to_anchor_x,
            d->transform->anchor_point.get().x(),
            p.x(),
            scale.x()
        ));
    }

    if ( size_to_anchor_x || size_to_anchor_y )
        d->push_command(d->transform->scale, scale, false);
}

void graphics::TransformGraphicsItem::drag_bl(const QPointF& p)
{
    auto scale = d->transform->scale.get();
    qreal size_to_anchor_y = d->cache.bottom() - d->transform->anchor_point.get().y();
    qreal size_to_anchor_x = d->cache.left() - d->transform->anchor_point.get().x();

    if ( size_to_anchor_y != 0 )
    {
        scale.setY(d->find_scale(
            QPointF(d->transform->anchor_point.get().x(), p.y()),
            size_to_anchor_y,
            d->transform->anchor_point.get().y(),
            p.y(),
            scale.y()
        ));
    }

    if ( size_to_anchor_x != 0 )
    {
        scale.setX(d->find_scale(
            QPointF(p.x(), d->transform->anchor_point.get().y()),
            size_to_anchor_x,
            d->transform->anchor_point.get().x(),
            p.x(),
            scale.x()
        ));
    }

    if ( size_to_anchor_x || size_to_anchor_y )
        d->push_command(d->transform->scale, scale, false);
}

void graphics::TransformGraphicsItem::drag_t(const QPointF& p)
{
    qreal size_to_anchor = d->cache.top() - d->transform->anchor_point.get().y();
    if ( size_to_anchor != 0 )
    {
        auto old = d->transform->scale.get();
        qreal new_scale = d->find_scale(
            QPointF(d->transform->anchor_point.get().x(), p.y()),
            size_to_anchor,
            d->transform->anchor_point.get().y(),
            p.y(),
            old.y()
        );

        QVector2D scale(old.x(), new_scale);
        d->push_command(d->transform->scale, scale, false);
    }
}

void graphics::TransformGraphicsItem::drag_b(const QPointF& p)
{
    qreal size_to_anchor = d->cache.bottom() - d->transform->anchor_point.get().y();
    if ( size_to_anchor != 0 )
    {
        auto old = d->transform->scale.get();
        qreal new_scale = d->find_scale(
            QPointF(d->transform->anchor_point.get().x(), p.y()),
            size_to_anchor,
            d->transform->anchor_point.get().y(),
            p.y(),
            old.y()
        );

        QVector2D scale(old.x(), new_scale);
        d->push_command(d->transform->scale, scale, false);
    }
}

void graphics::TransformGraphicsItem::drag_l(const QPointF& p)
{
    qreal size_to_anchor = d->cache.left() - d->transform->anchor_point.get().x();
    if ( size_to_anchor != 0 )
    {
        auto old = d->transform->scale.get();
        qreal new_scale = d->find_scale(
            QPointF(p.x(), d->transform->anchor_point.get().y()),
            size_to_anchor,
            d->transform->anchor_point.get().x(),
            p.x(),
            old.x()
        );

        QVector2D scale(new_scale, old.y());
        d->push_command(d->transform->scale, scale, false);
    }
}

void graphics::TransformGraphicsItem::drag_r(const QPointF& p)
{
    qreal size_to_anchor = d->cache.right() - d->transform->anchor_point.get().x();
    if ( size_to_anchor != 0 )
    {
        auto old = d->transform->scale.get();
        qreal new_scale = d->find_scale(
            QPointF(p.x(), d->transform->anchor_point.get().y()),
            size_to_anchor,
            d->transform->anchor_point.get().x(),
            p.x(),
            old.x()
        );

        QVector2D scale(new_scale, old.y());
        d->push_command(d->transform->scale, scale, false);
    }
}

void graphics::TransformGraphicsItem::drag_a(const QPointF& p)
{
    QPointF anchor = p;
    QPointF anchor_old = d->transform->anchor_point.get();

    QPointF p1 = sceneTransform().map(QPointF(0, 0));
    d->transform->anchor_point.set(anchor);
    QPointF p2 = sceneTransform().map(QPointF(0, 0));
    QPointF pos = d->transform->position.get() - p2 + p1;
    d->transform->anchor_point.set(anchor_old);
    d->target->document()->undo_stack().push(new command::SetMultipleAnimated(
        tr("Drag anchor point"),
        false,
        {&d->transform->anchor_point, &d->transform->position},
        anchor,
        pos
    ));
}

void graphics::TransformGraphicsItem::drag_rot(const QPointF& p)
{
    QPointF diff_old = d->get_rot() - d->transform->anchor_point.get();
    QVector2D scale = d->transform->scale.get();
    qreal angle_to_rot_handle = std::atan2(diff_old.y() * scale.y(), diff_old.x() * scale.x());

    QPointF p_new = d->transform_matrix.map(p);
    QPointF ap = d->transform_matrix.map(d->transform->anchor_point.get());
    QPointF diff_new = p_new - ap;
    qreal angle_new = std::atan2(diff_new.y(), diff_new.x());
    qreal angle = angle_new - angle_to_rot_handle;

    d->push_command(d->transform->rotation, qRadiansToDegrees(angle), false);
}

void graphics::TransformGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* opt, QWidget*)
{
    painter->save();
    QPen pen(opt->palette.color(QPalette::Highlight), 1);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->drawRect(d->cache);
    painter->drawLine(d->handles[Private::Top].handle->pos(), d->handles[Private::Rot].handle->pos());
    painter->restore();
}

QRectF graphics::TransformGraphicsItem::boundingRect() const
{
    return d->cache.adjusted(0, d->rot_top(), 0, 0);
}

void graphics::TransformGraphicsItem::commit_anchor()
{
    d->target->document()->undo_stack().push(new command::SetMultipleAnimated(
        tr("Drag anchor point"),
        true,
        {&d->transform->anchor_point, &d->transform->position},
        d->transform->anchor_point.get(),
        d->transform->position.get()
    ));
}

void graphics::TransformGraphicsItem::commit_rot()
{
    d->push_command(d->transform->rotation, d->transform->rotation.value(), true);
}

void graphics::TransformGraphicsItem::commit_scale()
{
    d->push_command(d->transform->scale, d->transform->scale.value(), true);
}


void graphics::TransformGraphicsItem::set_transform_matrix(const QTransform& t)
{
    setTransform(t);
}