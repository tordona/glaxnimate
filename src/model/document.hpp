#pragma once


#include <QDir>
#include <QUndoStack>

#include "animation.hpp"
#include "io/options.hpp"

namespace model {

class Document : public QObject
{
    Q_OBJECT

public:
    explicit Document(const QString& filename);
    ~Document();

    QString filename() const;

    QVariantMap& metadata() const;

    Animation& animation();

    QUndoStack& undo_stack();

    const io::Options& export_options() const;

    void set_export_options(const io::Options& opt);

    QString generate_id();

    const QUuid& uuid() const;
    void set_uuid(const QUuid& uuid);


signals:
    void filename_changed(const QString& n);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace model
