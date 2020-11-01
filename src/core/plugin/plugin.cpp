#include "plugin.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "app/scripting/script_engine.hpp"
#include "app/application.hpp"

#include "plugin/action.hpp"
#include "plugin/io.hpp"

void plugin::Plugin::run_script ( const plugin::PluginScript& script, const QVariantList& args ) const
{
     if ( !data_.engine )
         return;

     emit PluginRegistry::instance().script_needs_running(*this, script, args);
}

void plugin::PluginRegistry::load()
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
    emit loaded();
}

bool plugin::PluginRegistry::load_plugin ( const QString& path, bool user_installed )
{
    logger.set_detail(path);
    QFileInfo file_info(path);
    if ( !file_info.exists() || !file_info.isFile() || !file_info.isReadable() )
    {
        logger.stream() << "Cannot read plugin file";
        return false;
    }

    QFile file(file_info.absoluteFilePath());
    if ( !file.open(QFile::ReadOnly) )
    {
        logger.stream() << "Cannot read plugin file";
        return false;
    }

    QJsonDocument jdoc;

    try {
        jdoc = QJsonDocument::fromJson(file.readAll());
    } catch ( const QJsonParseError& err ) {
        logger.stream() << "Invalid plugin file:" << err.errorString();
        return false;
    }

    if ( !jdoc.isObject() )
    {
        logger.stream() << "Invalid plugin file: not an object";
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
            logger.stream(app::log::Info) << "Skipping Plugin (newer version exists)";
            return false;
        }

        if ( plug->enabled() )
        {
            logger.stream(app::log::Info) << "Skipping Plugin (older version is currently enabled)";
            return false;
        }

        overwrite = *it;
    }

    data.engine_name = jobj["engine"].toString();
    data.engine = app::scripting::ScriptEngineFactory::instance().engine(data.engine_name);
    if ( !data.engine)
    {
        logger.stream() << "Plugin refers to an unknown engine" << data.engine_name;
    }


    data.name = jobj["name"].toString(data.id);
    data.author = jobj["author"].toString();
    data.icon = jobj["icon"].toString();

    QJsonArray arr = jobj["services"].toArray();
    if ( arr.empty() )
    {
        logger.stream() << "Plugin does not provide any services";
        return false;
    }

    for ( const QJsonValue& val : arr )
    {
        if ( !val.isObject() )
            logger.stream() << "Skipping invalid service";
        else
            load_service(val.toObject(), data);
    }

    if ( data.services.empty() )
    {
        logger.stream() << "Plugin does not provide any valid services";
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

void plugin::PluginRegistry::load_service ( const QJsonObject& jobj, plugin::PluginData& data ) const
{
    QString type = jobj["type"].toString();

    if ( type == "action" )
    {
        std::unique_ptr<ActionService> act = std::make_unique<ActionService>();
        act->script = load_script(jobj["script"].toObject());
        if ( !act->script.valid() )
        {
            logger.stream() << "Skipping action with invalid script";
            return;
        }
        act->label = jobj["label"].toString();
        act->tooltip = jobj["tooltip"].toString();
        act->icon = jobj["icon"].toString();
        if ( act->icon.isEmpty() )
            act->icon = data.icon;
        data.services.emplace_back(std::move(act));
    }
    else if ( type == "format" )
    {
        auto svc = std::make_unique<IoService>();
        svc->save = load_script(jobj["save"].toObject());
        svc->open = load_script(jobj["open"].toObject());
        if ( !svc->save.valid() && !svc->open.valid() )
        {
            logger.stream() << "Skipping format service with no open nor save";
            return;
        }

        svc->label = jobj["name"].toString();
        for ( const auto& extv : jobj["extensions"].toArray() )
        {
            QString ext = extv.toString();
            if ( ext.startsWith(".") )
            {
                logger.stream() << "Format extensions should not have the leading dot";
                ext = ext.mid(1);
            }

            if ( ext.isEmpty() )
            {
                logger.stream() << "Empty extension";
                continue;
            }

            svc->extensions.push_back(ext);
        }

        if ( svc->extensions.isEmpty() )
        {
            logger.stream() << "Skipping format service with no extensions";
            return;
        }

        data.services.emplace_back(std::move(svc));
    }
    else
    {
        logger.stream() << "Skipping invalid service type" << type;
    }


}

plugin::PluginScript plugin::PluginRegistry::load_script ( const QJsonObject& jobj ) const
{
    PluginScript s;
    s.module = jobj["module"].toString();
    s.function = jobj["function"].toString();
    QJsonArray settings = jobj["settings"].toArray();
    for ( const auto& setting : settings )
    {
        load_setting(setting.toObject(), s);
    }

    return s;
}

void plugin::PluginRegistry::load_setting (const QJsonObject& jobj, plugin::PluginScript& script ) const
{
    QString type = jobj["type"].toString();
    QString slug = jobj["name"].toString();
    if ( slug.isEmpty() )
    {
        logger.stream() << "Skipping setting with no name";
        return;
    }
    QString label = jobj["label"].toString(slug);
    QString description = jobj["description"].toString();
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
    else if ( type == "color" )
        script.settings.emplace_back(slug, label, description, app::settings::Setting::Color, default_value);
    else
        logger.stream() << "Unknown type" << type << "for plugin setting" << slug;
}

QVariantMap plugin::PluginRegistry::load_choices ( const QJsonValue& val ) const
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


plugin::Plugin * plugin::PluginRegistry::plugin ( const QString& id ) const
{
    auto it = names.find(id);
    if ( it == names.end() )
        return {};
    return plugins_[*it].get();
}

