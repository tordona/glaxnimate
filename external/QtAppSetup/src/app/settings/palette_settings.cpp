#include "palette_settings.hpp"

#include <QSet>
#include <QApplication>
#include <QMetaEnum>

#include "app/widgets/widget_palette_editor.hpp"

app::settings::PaletteSettings::PaletteSettings()
    : default_palette(QGuiApplication::palette())
{
}


QString app::settings::PaletteSettings::slug() const
{
    return "palette";
}

QIcon app::settings::PaletteSettings::icon() const
{
    return QIcon::fromTheme("preferences-desktop-theme-global");
}

QString app::settings::PaletteSettings::label() const
{
    return QObject::tr("Widget Theme");
}

void app::settings::PaletteSettings::save ( QSettings& settings )
{
    settings.setValue("theme", selected);

    settings.beginWriteArray("themes", palettes.size());

    int i = 0;
    for ( auto it = palettes.begin(); it != palettes.end(); ++it, ++i )
    {
        settings.setArrayIndex(i);
        write_palette(settings, it.key(), *it);

    }

    settings.endArray();
}

void app::settings::PaletteSettings::write_palette ( QSettings& settings, const QString& name, const QPalette& palette )
{
    settings.setValue("name", name);
    for ( const auto& p : roles() )
    {
        settings.setValue(p.first + "_active", palette.color(QPalette::Active, p.second).name());
        settings.setValue(p.first + "_inactive", palette.color(QPalette::Inactive, p.second).name());
        settings.setValue(p.first + "_disabled", palette.color(QPalette::Disabled, p.second).name());
    }
}


void app::settings::PaletteSettings::load_palette ( const QSettings& settings )
{
    QString name = settings.value("name").toString();
    if ( name.isEmpty() )
        return;
    QPalette palette;


    for ( const auto& p : roles() )
    {
        palette.setColor(QPalette::Active,   p.second, QColor(settings.value(p.first + "_active").toString()));
        palette.setColor(QPalette::Inactive, p.second, QColor(settings.value(p.first + "_inactive").toString()));
        palette.setColor(QPalette::Disabled, p.second, QColor(settings.value(p.first + "_disabled").toString()));
    }

    palettes.insert(name, palette);
}


void app::settings::PaletteSettings::load ( QSettings& settings )
{
    selected = settings.value("theme").toString();

    int n = settings.beginReadArray("themes");

    for ( int i = 0; i < n; i++ )
    {
        settings.setArrayIndex(i);
        load_palette(settings);
    }

    settings.endArray();

    apply_palette(palette());
}

const QPalette& app::settings::PaletteSettings::palette() const
{
    auto it = palettes.find(selected);
    if ( it == palettes.end() )
        return default_palette;

    return *it;
}


const std::vector<std::pair<QString, QPalette::ColorRole> > & app::settings::PaletteSettings::roles()
{
    static std::vector<std::pair<QString, QPalette::ColorRole> > roles;
    if ( roles.empty() )
    {
        QSet<QString> blacklisted = {
            "Background", "Foreground", "NColorRoles"
        };
        QMetaEnum me = QMetaEnum::fromType<QPalette::ColorRole>();
        for ( int i = 0; i < me.keyCount(); i++ )
        {
            if ( blacklisted.contains(me.key(i)) )
                continue;

            roles.emplace_back(
                me.key(i),
                QPalette::ColorRole(me.value(i))
            );
        }
    }

    return roles;
}

void app::settings::PaletteSettings::set_selected ( const QString& name )
{
    selected = name;
    apply_palette(palette());
}

QWidget * app::settings::PaletteSettings::make_widget ( QWidget* parent )
{
    return new WidgetPaletteEditor(this, parent);
}

void app::settings::PaletteSettings::apply_palette(const QPalette& palette)
{
    QGuiApplication::setPalette(palette);
    QApplication::setPalette(palette);

    for ( auto window : QApplication::topLevelWidgets() )
        window->setPalette(palette);
}
