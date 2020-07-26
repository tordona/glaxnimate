#include "plugin.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "app/settings/widget_builder.hpp"
#include "scripting/script_engine.hpp"
#include "app/application.hpp"

QAction * scripting::PluginActionRegistry::make_qaction ( scripting::ActionService* action )
{
    QAction* act = new QAction;
    act->setIcon(action->plugin()->make_icon(action->icon));
    if ( action->label.isEmpty() )
        act->setText(action->plugin()->data().name);
    else
        act->setText(action->label);
    act->setToolTip(action->tooltip);
    connect(act, &QAction::triggered, action, &ActionService::trigger);
    connect(action, &ActionService::disabled, act, &QAction::deleteLater);

    return act;
}

void scripting::PluginActionRegistry::add_action ( scripting::ActionService* action )
{
    if ( enabled_actions.contains(action) )
        return;

    enabled_actions.insert(action);
    emit action_added(action);
}

void scripting::PluginActionRegistry::remove_action ( scripting::ActionService* action )
{
    if ( !enabled_actions.contains(action) )
        return;

    enabled_actions.remove(action);
    emit action_removed(action);

}

QIcon scripting::ActionService::service_icon() const
{
    return plugin()->make_icon(icon);
}


void scripting::ActionService::trigger() const
{
    QVariantMap settings_value;
    if ( !script.settings.empty() )
    {
        if ( !app::settings::WidgetBuilder().show_dialog(
            script.settings, settings_value, plugin()->data().name
        ) )
            return;
    }

    plugin()->run_script(script, settings_value);
}


void scripting::Plugin::run_script ( const scripting::PluginScript& script, const QVariantMap& settings ) const
{
     if ( !data_.engine )
         return;

     /// @todo run from the engine
     Q_UNUSED(script) Q_UNUSED(settings);
}

void scripting::PluginRegistry::load()
{
    QString writable_path = app::Application::instance()->writable_data_path("plugins");

    for ( const QString& path : app::Application::instance()->data_paths("plugins") )
    {
        bool writable = path == writable_path;
        QDir pathdir(path);
        for ( const auto& entry : pathdir.entryList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::Readable) )
        {
            QDir entrydir(pathdir.absoluteFilePath(entry));
            if ( entrydir.exists("plugin.json") )
            {
                load_plugin(entrydir.absoluteFilePath("plugin.json"), writable);
            }
        }
    }
}

bool scripting::PluginRegistry::load_plugin ( const QString& path, bool user_installed )
{
    QFileInfo file_info(path);
    if ( !file_info.exists() || !file_info.isFile() || !file_info.isReadable() )
    {
        qWarning() << "Cannot read plugin file " << path;
        return false;
    }

    QFile file(file_info.absoluteFilePath());
    if ( !file.open(QFile::ReadOnly) )
    {
        qWarning() << "Cannot read plugin file " << path;
        return false;
    }

    QJsonDocument jdoc;

    try {
        jdoc = QJsonDocument::fromJson(file.readAll());
    } catch ( const QJsonParseError& err ) {
        qWarning() << "Invalid plugin file " << path << err.errorString();
        return false;
    }

    if ( !jdoc.isObject() )
    {
        qWarning() << "Invalid plugin file " << path << "not an object";
        return false;
    }

    const QJsonObject jobj = jdoc.object();

    PluginData data;
    data.dir = file_info.dir();
    data.id = QFileInfo(data.dir.path()).fileName();
    data.version = jobj["version"].toInt(0);

    auto it = names.find(data.id);
    int overwrite = -1;
    if ( it != names.end() )
    {
        Plugin* plug = plugins_[*it].get();
        if ( plug->data().version >= data.version )
        {
            qWarning() << "Skipping " << path << " (newer version exists)";
            return false;
        }

        if ( plug->enabled() )
        {
            qWarning() << "Skipping " << path << " (older version is currently enabled)";
            return false;
        }

        overwrite = *it;
    }

    data.engine_name = jobj["engine"].toString();
    data.engine = ScriptEngineFactory::instance().engine(data.engine_name);
    if ( !data.engine)
    {
        qWarning() << "Plugin" << path << "refers to an unknown engine" << data.engine_name;
    }


    data.name = jobj["name"].toString(data.id);
    data.author = jobj["author"].toString();
    data.icon = jobj["icon"].toString();

    QJsonArray arr = jobj["services"].toArray();
    if ( arr.empty() )
    {
        qWarning() << "Plugin" << path << "does not provide any services";
        return false;
    }

    for ( const QJsonValue& val : arr )
    {
        if ( !val.isObject() )
            qWarning() << "Skipping invalid service in " << path;
        else
            load_service(val.toObject(), data);
    }

    if ( data.services.empty() )
    {
        qWarning() << "Plugin" << path << "does not provide any valid services";
        return false;
    }

    std::unique_ptr<Plugin> plugin = std::make_unique<Plugin>(std::move(data), user_installed);

    if ( overwrite != -1 )
    {
        plugins_[overwrite] = std::move(plugin);
    }
    else
    {
        names[plugin->data().id] = plugins_.size();
        plugins_.push_back(std::move(plugin));
    }
    return true;
}

void scripting::PluginRegistry::load_service ( const QJsonObject& jobj, scripting::PluginData& data ) const
{
    QString type = jobj["type"].toString();

    if ( type == "action" )
    {
        std::unique_ptr<ActionService> act = std::make_unique<ActionService>();
        act->script = load_script(jobj["script"].toObject());
        if ( !act->script.valid() )
        {
            qWarning() << "Skipping action with invalid script";
            return;
        }
        act->label = jobj["label"].toString();
        act->tooltip = jobj["tooltip"].toString();
        act->icon = jobj["icon"].toString();
        data.services.emplace_back(std::move(act));
    }
    else
    {
        qWarning() << "Skipping invalid service type" << type;
    }


}

scripting::PluginScript scripting::PluginRegistry::load_script ( const QJsonObject& jobj ) const
{
    PluginScript s;
    s.module = jobj["module"].toString();
    s.function = jobj["function"].toString();
    for ( const QJsonValue& set : jobj["settings"].toArray() )
    {
        load_setting(set.toObject(), s);
    }

    return s;
}

void scripting::PluginRegistry::load_setting ( const QJsonObject& jobj, scripting::PluginScript& script ) const
{
    QString type = jobj["type"].toString();
    QString slug = jobj["slug"].toString();
    if ( slug.isEmpty() )
    {
        qWarning() << "Skipping setting with no slug";
        return;
    }
    QString label = jobj["label"].toString(slug);
    QString description = jobj["slug"].toString();
    QVariant default_value = jobj["default"].toVariant();

    if ( type == "info" )
        script.settings.emplace_back(slug, label, description);
    else if ( type == "bool" )
        script.settings.emplace_back(slug, label, description, default_value.toBool());
    else if ( type == "int" )
        script.settings.emplace_back(slug, label, description, default_value.toInt(), jobj["min"].toInt(), jobj["max"].toInt());
    else if ( type == "float" )
        script.settings.emplace_back(slug, label, description, default_value.toFloat(), jobj["min"].toDouble(), jobj["max"].toDouble());
    else if ( type == "string" )
        script.settings.emplace_back(slug, label, description, default_value.toString());
    else if ( type == "choice" )
        script.settings.emplace_back(slug, label, description, app::settings::Setting::String, load_choices(jobj["choices"]));
    else
        qWarning() << "Unknown type" << type << "for plugin setting" << slug;
}

QVariantMap scripting::PluginRegistry::load_choices ( const QJsonValue& val ) const
{
    QVariantMap ret;

    if ( val.isObject() )
    {
        QJsonObject obj = val.toObject();
        for ( auto it = obj.begin(); it != obj.end(); ++it )
            ret[it.key()] = it->toVariant();
    }
    else if ( val.isArray() )
    {
        for ( const auto& i : val.toArray() )
        {
            QVariant v = i.toVariant();
            ret[v.toString()] = v;
        }
    }

    return ret;
}


scripting::Plugin * scripting::PluginRegistry::plugin ( const QString& id ) const
{
    auto it = names.find(id);
    if ( it == names.end() )
        return {};
    return plugins_[*it].get();
}

