#include "document_node.hpp"
#include "document.hpp"

#include <QPainter>
#include <QGraphicsItem>

model::DocumentNode::DocumentNode(Document* document)
    : Object(document)
{
    uuid.set_value(QUuid::createUuid());
}


QString model::DocumentNode::docnode_name() const
{
    return name.get();
}

QColor model::DocumentNode::docnode_group_color() const
{
    if ( !docnode_valid_color() )
    {
        if ( auto parent = docnode_group_parent() )
            return parent->docnode_group_color();
        else
            return Qt::white;
    }
    return group_color.get();
}

void model::DocumentNode::on_property_changed(const BaseProperty* prop, const QVariant&)
{
    if ( prop == &name )
    {
        emit docnode_name_changed(this->name.get());
    }
    else if ( prop == &group_color )
    {
        if ( !group_icon.isNull() )
        {
            if ( docnode_valid_color() )
                group_icon.fill(group_color.get());
            else
                group_icon.fill(Qt::white);
        }
        docnode_on_update_group(true);
    }
}

QString model::DocumentNode::object_name() const
{
    if ( name.get().isEmpty() )
        return type_name_human();
    return name.get();
}


bool model::DocumentNode::docnode_is_instance(const QString& type_name) const
{
    if ( type_name.isEmpty() )
        return true;

    for ( const QMetaObject* meta = metaObject(); meta; meta = meta->superClass() )
    {
        if ( detail::naked_type_name(meta->className()) == type_name )
            return true;
    }

    return false;
}

void model::DocumentNode::docnode_on_update_group(bool force)
{
    if ( force || docnode_valid_color() )
    {
        emit docnode_group_color_changed(this->group_color.get());
        for ( const auto& gc : docnode_group_children() )
            gc->docnode_on_update_group();
    }
}

bool model::DocumentNode::docnode_valid_color() const
{
    QColor col = group_color.get();
    return col.isValid() && col.alpha() > 0;
}

const QPixmap & model::DocumentNode::docnode_group_icon() const
{
    if ( !docnode_valid_color() )
    {
        if ( auto parent = docnode_group_parent() )
            return parent->docnode_group_icon();
    }

    if ( group_icon.isNull() )
    {
        group_icon = QPixmap{32, 32};
        group_icon.fill(docnode_group_color());
    }

    return group_icon;
}

bool model::DocumentNode::docnode_locked_by_ancestor() const
{
    for ( const DocumentNode* n = this; n; n = n->docnode_parent() )
    {
        if ( n->locked_ )
            return true;
    }

    return false;
}

void model::DocumentNode::paint(QPainter* painter, FrameTime time, PaintMode mode) const
{
    painter->save();
    on_paint(painter, time, mode);
    if ( mode == Recursive )
        for ( const auto& c : docnode_children() )
            c->paint(painter, time, mode);
    painter->restore();
}

bool model::DocumentNode::docnode_selectable() const
{
    if ( !visible_ || locked_ )
        return false;
    auto p = docnode_parent();
    if ( p )
        return p->docnode_selectable();
    return true;
}

bool model::DocumentNode::docnode_visible_recursive() const
{
    if ( !visible_ )
        return false;
    auto p = docnode_parent();
    if ( p )
        return p->docnode_selectable();
    return true;
}