#pragma once

#include <vector>

#include <QIcon>

#include "app/settings/setting.hpp"

namespace app::settings {

class SettingGroup
{
public:
    using iterator = SettingList::const_iterator;

    QString slug;
    QString label;
    QString icon;
    SettingList settings;

    QVariant get_variant(const QString& setting_slug, const QVariantMap& values) const
    {
        for ( const Setting& setting : settings )
            if ( setting.slug == setting_slug )
                return setting.get_variant(values);
        return {};
    }

    bool set_variant(const QString& setting_slug, QVariantMap& values, const QVariant& value) const
    {
        for ( const Setting& setting : settings )
        {
            if ( setting.slug == setting_slug )
            {
                if ( !setting.valid_variant(value) )
                    return false;
                values[setting.slug] = value;
                if ( setting.side_effects )
                    setting.side_effects(value);
                return true;
            }
        }
        return false;
    }


    iterator begin() const { return settings.begin(); }
    iterator end() const  { return settings.end(); }
};

} // namespace app::settings

