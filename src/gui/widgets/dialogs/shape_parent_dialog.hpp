#ifndef SHAPEPARENTDIALOG_H
#define SHAPEPARENTDIALOG_H

#include <QDialog>
#include <memory>

#include "model/shapes/shape.hpp"
#include "item_models/document_node_model.hpp"

class ShapeParentDialog : public QDialog
{
    Q_OBJECT

public:
    ShapeParentDialog(item_models::DocumentNodeModel* model, QWidget* parent = nullptr);
    ~ShapeParentDialog();

    model::ShapeListProperty* shape_parent() const;

    model::ShapeListProperty* get_shape_parent();

protected:
    void changeEvent(QEvent *e) override;

private slots:
    void select(const QModelIndex& index);
    void select_and_accept(const QModelIndex& index);

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // SHAPEPARENTDIALOG_H
