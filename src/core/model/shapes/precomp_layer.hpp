#pragma once

#include "model/property/reference_property.hpp"
#include "model/animation_container.hpp"
#include "model/shapes/shape.hpp"
#include "model/defs/precomposition.hpp"


namespace model {

class PreCompLayer : public ShapeElement
{
    GLAXNIMATE_OBJECT(PreCompLayer)

    GLAXNIMATE_SUBOBJECT(model::StretchableAnimation, animation)
    GLAXNIMATE_PROPERTY_REFERENCE(model::Precomposition, composition, &PreCompLayer::valid_precomps, &PreCompLayer::is_valid_precomp)
    GLAXNIMATE_PROPERTY(QSizeF, size, {})
    GLAXNIMATE_SUBOBJECT(model::Transform, transform)
    GLAXNIMATE_ANIMATABLE(float, opacity, 1, &PreCompLayer::opacity_changed, 0, 1)

public:
    PreCompLayer(Document* document);


    QIcon docnode_icon() const override;
    QString type_name_human() const override;
    void set_time(FrameTime t) override;

    /**
     * \brief Returns the (frame) time relative to this layer
     *
     * Useful for stretching / remapping etc.
     * Always use this to get animated property values,
     * even if currently it doesn't do anything
     */
    FrameTime relative_time(FrameTime time) const;

    QRectF local_bounding_rect(FrameTime t) const override;
    QTransform local_transform_matrix(model::FrameTime t) const override;

    void add_shapes(model::FrameTime t, math::bezier::MultiBezier & bez) const override;

private:
    std::vector<ReferenceTarget*> valid_precomps() const;
    bool is_valid_precomp(ReferenceTarget* node) const;

signals:
    void opacity_changed(float op);

protected:
    void on_paint(QPainter*, FrameTime, PaintMode) const override;

private slots:
    void on_transform_matrix_changed();
};

} // namespace model