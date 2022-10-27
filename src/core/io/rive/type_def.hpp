#pragma once

#include <vector>
#include <unordered_map>
#include <QString>
#include <QVariant>

#include "type_ids.hpp"

namespace glaxnimate::io::rive {

using Identifier = quint64;

enum class PropertyType
{
    VarUint = 0,
    Bool    = 1, // Byte
    String  = 2,
    Bytes   = 3, // Raw String
    Float   = 4,
    Color   = 5, // Uint
};

struct Property
{
    QString name;
    PropertyType type = PropertyType::VarUint;
};

struct ObjectDefinition
{
    QString name;
    TypeId type_id = TypeId::NoType;
    TypeId extends = TypeId::NoType;
    std::unordered_map<Identifier, Property> properties;
};

struct Object;

struct PropertyAnimation
{
    Identifier property_id = 0;
    std::vector<Object*> keyframes = {};
};

struct Object
{
    TypeId type_id = TypeId::NoType;
    std::vector<const ObjectDefinition*> definitions;
    QVariantMap properties;
    std::unordered_map<Identifier, Property> property_definitions;
    std::vector<Object*> children;
    std::vector<PropertyAnimation> animations;

    bool has_type(TypeId id) const
    {
        if ( type_id == id )
            return true;

        for ( std::size_t i = 1; i < definitions.size(); i++ )
            if ( definitions[i]->type_id == id )
                return true;

        return false;
    }

    bool has_value(const QString& prop) const
    {
        return properties.find(prop) != properties.end();
    }

    template<class T>
    T get(const QString& name, T default_value = {}) const
    {
        auto iter = properties.find(name);
        if ( iter == properties.end() )
            return default_value;
        return iter->value<T>();
    }

    QString property_name(Identifier id) const
    {
        auto it = property_definitions.find(id);
        if ( it == property_definitions.end() )
            return {};
        return it->second.name;
    }
};

extern std::unordered_map<TypeId, ObjectDefinition> defined_objects;


} // namespace glaxnimate::io::rive
