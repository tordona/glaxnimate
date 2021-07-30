#include "svg_format.hpp"

#include <QFileInfo>

#include "utils/gzip.hpp"
#include "svg_parser.hpp"
#include "svg_renderer.hpp"

glaxnimate::io::Autoreg<glaxnimate::io::svg::SvgFormat> glaxnimate::io::svg::SvgFormat::autoreg;

bool glaxnimate::io::svg::SvgFormat::on_open(QIODevice& file, const QString& filename, model::Document* document, const QVariantMap& options)
{
    /// \todo layer mode setting
    SvgParser::GroupMode mode = SvgParser::Inkscape;

    auto on_error = [this](const QString& s){warning(s);};
    try
    {
        QSize forced_size = options["forced_size"].toSize();

        if ( utils::gzip::is_compressed(file) )
        {
            utils::gzip::GzipStream decompressed(&file, on_error);
            decompressed.open(QIODevice::ReadOnly);
            SvgParser(&decompressed, mode, document, on_error, this, forced_size).parse_to_document();
            return true;
        }

        SvgParser(&file, mode, document, on_error, this, forced_size).parse_to_document();
        return true;

    }
    catch ( const SvgParseError& err )
    {
        error(err.formatted(QFileInfo(filename).baseName()));
        return false;
    }
}

glaxnimate::io::SettingList glaxnimate::io::svg::SvgFormat::save_settings() const
{
    return {};
}

bool glaxnimate::io::svg::SvgFormat::on_save(QIODevice& file, const QString& filename, model::Document* document, const QVariantMap& options)
{
    auto on_error = [this](const QString& s){warning(s);};
    SvgRenderer rend(SMIL);
    rend.write_document(document);
    if ( filename.endsWith(".svgz") || options.value("compressed", false).toBool() )
    {
        utils::gzip::GzipStream compressed(&file, on_error);
        compressed.open(QIODevice::WriteOnly);
        rend.write(&compressed, false);
    }
    else
    {
        rend.write(&file, true);
    }

    return true;
}

