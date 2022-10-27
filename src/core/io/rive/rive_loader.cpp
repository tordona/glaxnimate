#include "rive_loader.hpp"

#include "io/animated_properties.hpp"
#include "model/assets/assets.hpp"
#include "model/shapes/precomp_layer.hpp"
#include "model/shapes/rect.hpp"
#include "model/shapes/ellipse.hpp"
#include "model/shapes/polystar.hpp"
#include "model/shapes/path.hpp"
#include "model/shapes/fill.hpp"
#include "model/shapes/stroke.hpp"

using namespace glaxnimate;
using namespace glaxnimate::io;
using namespace glaxnimate::io::rive;

namespace {

struct Artboard
{
    Artboard() = default;

    Artboard(Object* first, Object* last)
        : object(first),
            children(object),
            child_count(last - first - 1)
    {}

    Object* operator->() const { return object; }

    Object* object = nullptr;
    Object* children = nullptr;
    Identifier child_count = 0;
    VarUint timeline_duration = 0;
    VarUint keyframe_timeline_duration = 0;
};

template<class T> T load_property_get_keyframe(const detail::JoinedPropertyKeyframe& kf, std::size_t index);
template<> Float load_property_get_keyframe<Float>(const detail::JoinedPropertyKeyframe& kf, std::size_t index)
{
    return kf.values[index].vector()[0];
}
template<> VarUint load_property_get_keyframe<VarUint>(const detail::JoinedPropertyKeyframe& kf, std::size_t index)
{
    return kf.values[index].vector()[0];
}
template<> QColor load_property_get_keyframe<QColor>(const detail::JoinedPropertyKeyframe& kf, std::size_t index)
{
    return kf.values[index].color();
}

template<class... T, class PropT, class Func, std::size_t... Ind, std::size_t N>
void load_property_impl(Object* rive, PropT& property, const detail::AnimatedProperties& animations,
                    const std::array<const char*, N>& names, T... defvals, const Func& value_func, std::index_sequence<Ind...>)
{
    property.set(value_func(rive->get<T>(names[Ind], defvals)...));

    for ( const auto& kf : animations.joined(std::vector<QString>(names.begin(), names.end())) )
        property.set_keyframe(kf.time, value_func(load_property_get_keyframe<T>(kf, Ind)...))->set_transition(kf.transition);

}

template<class... T, class PropT, class Func>
void load_property(Object* rive, PropT& property, const detail::AnimatedProperties& animations,
                    const std::array<const char*, sizeof...(T)>& names, T... defvals, const Func& value_func)
{
    load_property_impl<T...>(rive, property, animations, names, defvals..., value_func, std::index_sequence_for<T...>{});
}

template<class T, class PropT>
void load_property(Object* rive, PropT& property, const detail::AnimatedProperties& animations, const char* name, T defval = {})
{
    property.set(rive->get<T>(name, defval));

    for ( const auto& kf : animations.joined({name}) )
        property.set_keyframe(kf.time, load_property_get_keyframe<T>(kf, 0))->set_transition(kf.transition);
}

QPointF make_point(Float x, Float y)
{
    return QPointF(x, y);
}


struct LoadCotext
{
    void new_artboard(Object* iterator)
    {
        artboards[iterator] = Artboard(iterator, &objects.back());
        artboard = &artboards[iterator];
    }


    Object* artboard_child(Identifier id) const
    {
        if ( artboard && id < artboard->child_count )
            return artboard->children + id;
        return nullptr;
    }

    LoadCotext(RiveFormat* format, model::Document* document)
        : document(document), format(format)
    {
    }


    void preprocess_object(Object* iterator)
    {
        if ( iterator->type_id == TypeId::Artboard )
        {
            new_artboard(iterator);
        }
        else if ( iterator->type_id == TypeId::KeyedObject )
        {
            if ( !artboard )
            {
                format->warning(QObject::tr("Unexpected Keyed Object"));
                return;
            }
            auto id = iterator->get<Identifier>("objectId", artboard->child_count);
            keyed_object = artboard_child(id);
            keyed_property = nullptr;
            if ( !keyed_object )
            {
                format->warning(QObject::tr("Invalid Keyed Object id %1").arg(id));
                return;
            }
        }
        else if ( iterator->type_id == TypeId::KeyedProperty )
        {
            if ( !keyed_object )
            {
                format->warning(QObject::tr("Unexpected Keyed Property"));
                return;
            }

            auto id = iterator->get<Identifier>("propertyKey");

            if ( !keyed_object->property_definitions.count(id) )
            {
                format->warning(QObject::tr("Unknown Keyed Property id %1").arg(id));
                return;
            }

            keyed_object->animations.push_back({id, {}});
            keyed_property = &keyed_object->animations.back();
        }
        else if ( iterator->type_id == TypeId::LinearAnimation )
        {
            if ( !artboard )
            {
                format->warning(QObject::tr("Unexpected Animation"));
                return;
            }

            auto duration = iterator->get<VarUint>("duration");
            if ( duration > artboard->timeline_duration )
                artboard->timeline_duration = duration;
        }
        else if ( iterator->has_type(TypeId::KeyFrame) )
        {
            if ( !keyed_property )
            {
                format->warning(QObject::tr("Unexpected Keyframe"));
                return;
            }

            auto frame = iterator->get<VarUint>("duration");
            if ( frame > artboard->keyframe_timeline_duration )
                artboard->keyframe_timeline_duration = frame;

            keyed_property->keyframes.push_back(iterator);
        }
        else if ( iterator->has_value("parentId") )
        {
            auto parent_id = iterator->get<Identifier>("parentId");
            auto parent = artboard_child(parent_id);
            if ( !parent )
                format->warning(QObject::tr("Could not find parent with id %s").arg(parent_id));
            else
                parent->children.push_back(iterator);
        }
    }

    void process_object(Object* iterator)
    {
        if ( iterator->type_id == TypeId::Artboard )
        {
            process_artboard(iterator);
        }
    }

    void process_artboard(Object* iterator)
    {
        auto comp = document->assets()->precompositions->values.insert(std::make_unique<model::Precomposition>(document));

        comp->name.set(iterator->get<QString>("name"));
        add_shapes(iterator, comp->shapes);

        auto precomp_layer = std::make_unique<model::PreCompLayer>(document);
        precomp_layer->name.set(comp->name.get());
        precomp_layer->size.set(QSize(
            iterator->get<Float>("width"),
            iterator->get<Float>("height")
        ));
        detail::AnimatedProperties animations = load_animations(iterator);
        load_transform(iterator, precomp_layer->transform.get(), animations);
        precomp_layer->opacity.set(iterator->get<Float>("opacity", 1));
        precomp_layer->composition.set(comp);

        const auto& artboard = artboards.at(iterator);
        float last_frame = artboard.timeline_duration == 0 ? artboard.keyframe_timeline_duration : artboard.timeline_duration;
        document->main()->animation->last_frame.set(qMax(document->main()->animation->last_frame.get(), last_frame));

        if ( document->assets()->precompositions->values.size() == 1 )
        {
            document->main()->width.set(precomp_layer->size.get().width());
            document->main()->height.set(precomp_layer->size.get().height());
        }

        document->main()->shapes.insert(std::move(precomp_layer));
    }

    void add_shapes(Object* parent, model::ObjectListProperty<model::ShapeElement>& prop)
    {

        std::vector<std::unique_ptr<model::ShapeElement>> shapes;

        for ( Object* child : parent->children )
        {
            if ( child == parent )
            {
                format->error(QObject::tr("Parent circular reference detected"));
                continue;
            }

            auto shape = load_shape(child);
            if ( shape )
            {
                if ( child->has_type(TypeId::Path) )
                    shapes.emplace_back(std::move(shape));
                else
                    prop.insert(std::move(shape));
            }
        }

        for ( auto& shape : shapes )
            prop.insert(std::move(shape));
    }

    std::unique_ptr<model::Layer> load_shape_layer(Object* shape, const detail::AnimatedProperties& animations)
    {
        auto layer = std::make_unique<model::Layer>(document);
        load_shape_group(shape, layer.get(), animations);
        return layer;
    }

    void load_transform(Object* rive, model::Transform* transform, const detail::AnimatedProperties& animations)
    {
        load_property<Float, Float>(rive, transform->position, animations, {"x", "y"}, 0, 0, &make_point);
        load_property<Float>(rive, transform->rotation, animations, "rotation");
        load_property<Float, Float>(rive, transform->scale, animations, {"scaleX", "scaleX"}, 1, 1, [](Float x, Float y){
            return QVector2D(x, y);
        });
    }

    void load_shape_group(Object* shape, model::Group* group, const detail::AnimatedProperties& animations)
    {
        load_property<Float>(shape, group->opacity, animations, "opacity", 1);
        load_transform(shape, group->transform.get(), animations);
        group->name.set(shape->get<String>("name"));
        add_shapes(shape, group->shapes);
    }

    std::unique_ptr<model::ShapeElement> load_shape(Object* shape)
    {
        detail::AnimatedProperties animations = load_animations(shape);

        switch ( shape->type_id )
        {
            case TypeId::Shape:
                return load_shape_layer(shape, animations);
            case TypeId::Rectangle:
                return load_rectangle(shape, animations);
            case TypeId::Ellipse:
                return load_ellipse(shape, animations);
            case TypeId::Fill:
                return load_fill(shape, animations);
            case TypeId::Stroke:
                return load_stroke(shape, animations);
            case TypeId::Polygon:
                return load_polygon(shape, animations, model::PolyStar::Polygon);
            case TypeId::Star:
                return load_polygon(shape, animations, model::PolyStar::Star);
            case TypeId::Triangle:
                return load_triangle(shape, animations);
            case TypeId::PointsPath:
                return load_path(shape, animations);
            case TypeId::TrimPath:
            case TypeId::Bone:
            case TypeId::RootBone:
            case TypeId::RadialGradient:
            case TypeId::ClippingShape:
            case TypeId::NestedArtboard:
            case TypeId::Image:
            case TypeId::Text:
                /// \todo
            default:
                return {};
        }
    }

    std::unique_ptr<model::Group> load_rectangle(Object* object, const detail::AnimatedProperties& animations)
    {
        auto group = std::make_unique<model::Group>(document);
        auto shape = std::make_unique<model::Rect>(document);
        shape->name.set(object->get<String>("name"));

        load_property<Float, Float, Float, Float>(object, shape->rounded, animations,
            {"cornerRadiusTL", "cornerRadiusBL", "cornerRadiusBR", "cornerRadiusTR"},
            0, 0, 0, 0,
            [](Float tl, Float bl, Float br, Float tr){
                return (tl + bl + br + tr) / 4;
            }
        );

        load_property<Float, Float>(object, shape->size, animations, {"width", "height"}, 0, 0, [](Float x, Float y){
            return QSizeF(x, y);
        });

        group->shapes.insert(std::move(shape));
        load_shape_group(object, group.get(), animations);
        return group;
    }

    std::unique_ptr<model::Group> load_ellipse(Object* object, const detail::AnimatedProperties& animations)
    {
        auto group = std::make_unique<model::Group>(document);
        auto shape = std::make_unique<model::Ellipse>(document);
        shape->name.set(object->get<String>("name"));


        load_property<Float, Float>(object, shape->size, animations, {"width", "height"}, 0, 0, [](Float x, Float y){
            return QSizeF(x, y);
        });

        group->shapes.insert(std::move(shape));
        load_shape_group(object, group.get(), animations);
        return group;
    }

    std::unique_ptr<model::Fill> load_fill(Object* object, const detail::AnimatedProperties& animations)
    {
        auto shape = std::make_unique<model::Fill>(document);
        load_styler(object, shape.get(), animations);
        /// \todo fillRule
        return shape;
    }

    std::unique_ptr<model::Stroke> load_stroke(Object* object, const detail::AnimatedProperties& animations)
    {
        auto shape = std::make_unique<model::Stroke>(document);
        load_styler(object, shape.get(), animations);
        load_property<Float>(object, shape->width, animations, "thickness");
        /// \todo cap + join
        return shape;
    }

    void load_styler(Object* object, model::Styler* shape, const detail::AnimatedProperties& animations)
    {
        shape->name.set(object->get<String>("name"));
        shape->visible.set(object->get<bool>("isVisible", true));
        load_property<Float>(object, shape->opacity, animations, "opacity", 1);

        for ( const auto& child : object->children )
        {
            if ( child->type_id == TypeId::SolidColor )
                load_property<QColor>(child, shape->color, load_animations(child), "colorValue", QColor("#747474"));
            else if ( child->type_id == TypeId::LinearGradient )
                shape->use.set(load_gradient(child, model::Gradient::Linear));
            else if ( child->type_id == TypeId::RadialGradient )
                shape->use.set(load_gradient(child, model::Gradient::Radial));
        }
    }

    model::Gradient* load_gradient(Object* object, model::Gradient::GradientType type)
    {
        auto colors = document->assets()->add_gradient_colors();
        colors->name.set(object->get<String>("name"));

        auto gradient = document->assets()->add_gradient();
        gradient->name.set(object->get<String>("name"));
        gradient->colors.set(colors);
        gradient->type.set(type);

        auto animations = load_animations(object);
        load_property<Float, Float>(object, gradient->start_point, animations, {"startX", "startY"}, 0, 0, &make_point);
        load_property<Float, Float>(object, gradient->end_point, animations, {"endX", "endY"}, 0, 0, &make_point);

        /// \todo color animations
        QGradientStops stops;
        for ( const auto& child : object->children )
        {
            if ( child->type_id == TypeId::GradientStop )
            {
                stops.push_back({
                    child->get<Float>("position"),
                    child->get<QColor>("colorValue"),
                });
            }
        }
        colors->colors.set(stops);

        return gradient;
    }

    detail::AnimatedProperties load_animations(Object* object)
    {
        using namespace glaxnimate::io::detail;

        AnimatedProperties props;
        for ( const auto& anim : object->animations )
        {
            AnimatedProperty& prop = props.properties[object->property_name(anim.property_id)];
            for ( auto kf : anim.keyframes )
            {
                model::KeyframeTransition transition; /// \todo
                prop.keyframes.push_back({
                    kf->get<Float>("frame", 0),
                    ValueVariant(kf->properties.value("value")),
                    transition
                });
            }
        }
        return props;
    }

    std::unique_ptr<model::Group> load_polygon(Object* object, const detail::AnimatedProperties& animations, model::PolyStar::StarType type)
    {
        auto group = std::make_unique<model::Group>(document);
        load_shape_group(object, group.get(), animations);

        auto shape = std::make_unique<model::PolyStar>(document);
        shape->name.set(object->get<String>("name"));
        shape->type.set(type);
        /// \todo cornerRadius / width / height
        load_property<VarUint>(object, shape->points, animations, "points", 5);
        shape->outer_radius.set(100);

        load_property<Float>(object, shape->inner_radius, animations, {"innerRadius"}, 0.5, [](Float pc){
            return pc * 100;
        });

        load_property<Float>(object, shape->points, animations, "points", 5);


        load_property<Float, Float, Float, Float>(object, group->transform->scale, animations,
            {"scaleX", "scaleY", "width", "height"},
            1, 1, 0, 0, [](Float sx, Float sy, Float w, Float h){
            return QVector2D(w / 200 * sx, h / 200 * sy);
        });

        group->shapes.insert(std::move(shape));
        return group;
    }


    std::unique_ptr<model::Group> load_triangle(Object* object, const detail::AnimatedProperties& animations)
    {
        auto group = std::make_unique<model::Group>(document);
        auto shape = std::make_unique<model::Path>(document);
        shape->name.set(object->get<String>("name"));


        load_property<Float, Float>(object, shape->shape, animations, {"width", "height"}, 0, 0, [](Float w, Float h){
            math::bezier::Bezier path;
            path.add_point({-w/2, h/2});
            path.add_point({0, -h/2});
            path.add_point({w/2, h/2});
            path.close();
            return path;
        });

        group->shapes.insert(std::move(shape));
        load_shape_group(object, group.get(), animations);
        return group;
    }

    std::unique_ptr<model::Path> load_path(Object* object, const detail::AnimatedProperties& animations)
    {
        auto shape = std::make_unique<model::Path>(document);
        shape->name.set(object->get<String>("name"));
        bool closed = object->get<bool>("isClosed");
        shape->closed.set(closed);

        math::bezier::Bezier bez;
        for ( const auto& child : object->children )
        {
            math::bezier::Point p;
            p.pos = QPointF(child->get<Float>("x", 0), child->get<Float>("y", 0));
            if ( child->type_id == TypeId::CubicMirroredVertex )
            {
                p.type = math::bezier::Symmetrical;
                auto tangent = math::from_polar<QPointF>(
                    child->get<Float>("distance"),
                    child->get<Float>("rotation")
                );
                p.tan_in = p.pos - tangent;
                p.tan_out = p.pos + tangent;
            }
            else if ( child->type_id == TypeId::CubicAsymmetricVertex )
            {
                p.type = math::bezier::Smooth;
                p.tan_in = p.pos - math::from_polar<QPointF>(
                    child->get<Float>("inDistance"),
                    child->get<Float>("rotation")
                );
                p.tan_out = p.pos + math::from_polar<QPointF>(
                    child->get<Float>("outDistance"),
                    child->get<Float>("rotation")
                );
            }
            else if ( child->type_id == TypeId::CubicDetachedVertex )
            {
                p.type = math::bezier::Corner;
                p.tan_in = p.pos + math::from_polar<QPointF>(
                    child->get<Float>("inDistance"),
                    child->get<Float>("inRotation")
                );
                p.tan_out = p.pos + math::from_polar<QPointF>(
                    child->get<Float>("outDistance"),
                    child->get<Float>("outRotation")
                );
            }
            else if ( child->type_id == TypeId::StraightVertex )
            {
                p.type = math::bezier::Corner;
                p.tan_in = p.tan_out = p.pos;
            }
            else
            {
                continue;
            }

            bez.push_back(p);
        }

        bez.set_closed(closed);

        /// \todo animation
        Q_UNUSED(animations);
        shape->shape.set(bez);

        return shape;
    }

    model::Document* document = nullptr;
    std::map<Object*, Artboard> artboards;
    std::vector<Object> objects;
    Artboard* artboard = nullptr;
    Object* keyed_object = nullptr;
    PropertyAnimation* keyed_property = nullptr;
    RiveFormat* format = nullptr;
};


bool gather_definitions(TypeId type_id, Object& object, RiveFormat* format)
{
    auto it = defined_objects.find(type_id);
    if ( it == defined_objects.end() )
    {
        format->error(QObject::tr("Unknown object of type %1").arg(int(type_id)));
        return false;
    }

    const auto& def = it->second;

    object.definitions.push_back(&def);

    if ( def.extends != TypeId::NoType )
    {
        if ( !gather_definitions(def.extends, object, format) )
            return false;
    }

    object.property_definitions.insert(def.properties.begin(), def.properties.end());
    return true;
}


} // namespace

RiveLoader::RiveLoader(RiveStream& stream, RiveFormat* format)
    : document(nullptr),
    stream(stream),
    format(format)
{
}

std::vector<Object> RiveLoader::load_object_list()
{
    extra_props = stream.read_property_table();
    if ( stream.has_error() )
        return {};

    std::vector<Object> objects;
    while ( !stream.has_error() && !stream.eof() )
        objects.emplace_back(read_object());
    return objects;
}

bool RiveLoader::load_document(model::Document* document)
{
    extra_props = stream.read_property_table();

    if ( stream.has_error() )
    {
        format->error(QObject::tr("Could not read property table"));
        return false;
    }

    LoadCotext context(format, document);

    while ( !stream.has_error() && !stream.eof() )
        context.objects.emplace_back(read_object());

    for ( auto& object : context.objects )
        context.preprocess_object(&object);


    for ( auto& object : context.objects )
        context.process_object(&object);

    return true;
}

Object RiveLoader::read_object()
{
    auto type_id = TypeId(stream.read_varuint());
    if ( stream.has_error() )
    {
        format->error(QObject::tr("Could not load object type ID"));
        return {};
    }

    Object obj;
    obj.type_id = type_id;

    if ( !gather_definitions(type_id, obj, format) )
        return {};

    while ( true )
    {
        Identifier prop_id = stream.read_varuint();
        if ( stream.has_error() )
        {
            format->error(QObject::tr("Could not load property ID in %1 (%2)")
                .arg(int(type_id)).arg(obj.definitions[0]->name));
            return {};
        }

        if ( prop_id == 0 )
            break;

        auto prop_def = obj.property_definitions.find(prop_id);
        if ( prop_def == obj.property_definitions.end() )
        {
            auto unknown_it = extra_props.find(prop_id);
            if ( unknown_it == extra_props.end() )
            {
                format->error(QObject::tr("Unknown property %1 of %2 (%3)")
                    .arg(prop_id).arg(int(type_id)).arg(obj.definitions[0]->name));
                return {};
            }
            else
            {
                format->warning(QObject::tr("Skipping unknown property %1 of %2 (%3)")
                        .arg(prop_id).arg(int(type_id)).arg(obj.definitions[0]->name));
            }
        }
        else
        {
            obj.properties[prop_def->second.name] = read_property_value(prop_def->second.type);
            if ( stream.has_error() )
            {
                format->error(QObject::tr("Error loading property %1 (%2) of %3 (%4)")
                    .arg(prop_id).arg(prop_def->second.name).arg(int(type_id)).arg(obj.definitions[0]->name));
                return {};
            }
        }
    }

    return obj;
}


QVariant RiveLoader::read_property_value(PropertyType type)
{
    switch ( type )
    {
        case PropertyType::Bool:
            return bool(stream.next());
        case PropertyType::Bytes:
            return stream.read_raw_string();
        case PropertyType::String:
            return stream.read_string();
        case PropertyType::VarUint:
            return QVariant::fromValue(stream.read_varuint());
        case PropertyType::Float:
            return stream.read_float();
        case PropertyType::Color:
            return QColor::fromRgba(stream.read_uint());
    }

    return {};
}
