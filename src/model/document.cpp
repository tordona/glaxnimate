#include "document.hpp"

class model::Document::Private
{
public:
    Animation animation;
    QUndoStack undo_stack;
    QVariantMap metadata;
    io::SavedIoOptions exporter;
};


model::Document::Document(const QString& filename)
    : d ( std::make_unique<model::Document::Private>() )
{
    d->exporter.filename = filename;
}

model::Document::~Document() = default;

QString model::Document::filename() const
{
    return d->exporter.filename;
}

model::Animation & model::Document::animation()
{
    return d->animation;
}

QVariantMap & model::Document::metadata() const
{
    return d->metadata;
}

QUndoStack & model::Document::undo_stack()
{
    return d->undo_stack;
}


const io::SavedIoOptions & model::Document::export_options() const
{
    return d->exporter;
}

void model::Document::set_export_options(const io::SavedIoOptions& opt)
{
    bool em = opt.filename != d->exporter.filename;
    d->exporter = opt;
    if ( em )
        emit filename_changed(d->exporter.filename);
}

