#pragma once

#include "model/object.hpp"
#include "model/property/object_list_property.hpp"
#include "model/property/sub_object_property.hpp"
#include "named_color.hpp"
#include "bitmap.hpp"
#include "gradient.hpp"
#include "precomposition.hpp"


namespace model {


namespace detail {
    DocumentNode* defs(model::Document* doc);
} // detail

template<class T, class Derived>
class AssetListBase : public DocumentNode
{
protected:
    using Ctor = AssetListBase;

public:
    ObjectListProperty<T> values{this, "values",
        &AssetListBase::on_added,
        &AssetListBase::on_removed,
        &AssetListBase::docnode_child_add_begin,
        &AssetListBase::docnode_child_remove_begin,
        &AssetListBase::docnode_child_move_begin,
        &AssetListBase::docnode_child_move_end
    };

public:
    using DocumentNode::DocumentNode;

    DocumentNode* docnode_parent() const override
    {
        return detail::defs(document());
    }

    int docnode_child_count() const override
    {
        return values.size();
    }

    DocumentNode* docnode_child(int index) const override
    {
        return values[index];
    }

    int docnode_child_index(DocumentNode* dn) const override
    {
        return values.index_of(static_cast<T*>(dn));
    }

    QIcon instance_icon() const override
    {
        return tree_icon();
    }

protected:
    virtual void on_added(T* obj, int row)
    {
        Q_UNUSED(row);
        obj->attach();
        emit docnode_child_add_end(obj);
    }

    virtual void on_removed(T* obj, int row)
    {
        Q_UNUSED(row);
        obj->detach();
        emit docnode_child_remove_end(obj);
    }
};

#define ASSET_LIST_CLASS(type)                  \
    GLAXNIMATE_PROPERTY_LIST_IMPL(type, values) \
public:                                         \
    using Ctor::Ctor;                           \
// END

class NamedColorList : public AssetListBase<NamedColor, NamedColorList>
{
    GLAXNIMATE_OBJECT(NamedColorList)
    ASSET_LIST_CLASS(NamedColor)

public:
    QIcon tree_icon() const override;
    QString type_name_human() const override { return tr("Swatch"); }

signals:
    void color_changed(int position, model::NamedColor* color);
    void color_added(int position, model::NamedColor* color);
    void color_removed(int position, model::NamedColor* color);

protected:
    void on_added(model::NamedColor* color, int position) override;
    void on_removed(model::NamedColor* color, int position) override;
};

class BitmapList : public AssetListBase<Bitmap, BitmapList>
{
    GLAXNIMATE_OBJECT(BitmapList)
    ASSET_LIST_CLASS(Bitmap)

public:
    QIcon tree_icon() const override;
    QString type_name_human() const override { return tr("Images"); }
};

class GradientColorsList : public AssetListBase<GradientColors, GradientColorsList>
{
    GLAXNIMATE_OBJECT(GradientColorsList)
    ASSET_LIST_CLASS(GradientColors)

public:
    QIcon tree_icon() const override;
    QString type_name_human() const override { return tr("Gradient Colors"); }
};

class GradientList : public AssetListBase<Gradient, GradientList>
{
    GLAXNIMATE_OBJECT(GradientList)
    ASSET_LIST_CLASS(Gradient)

public:
    QIcon tree_icon() const override;
    QString type_name_human() const override { return tr("Gradients"); }
};

class PrecompositionList : public AssetListBase<Precomposition, PrecompositionList>
{
    GLAXNIMATE_OBJECT(PrecompositionList)
    ASSET_LIST_CLASS(Precomposition)

public:
    QIcon tree_icon() const override;

protected:
    void on_added(model::Precomposition* obj, int position) override;
    void on_removed(model::Precomposition* obj, int position) override;
    QString type_name_human() const override { return tr("Precompositions"); }

signals:
    void precomp_added(model::Precomposition* obj, int position);
};


class Defs : public DocumentNode
{
    GLAXNIMATE_OBJECT(Defs)

    GLAXNIMATE_SUBOBJECT(NamedColorList, colors)
    GLAXNIMATE_SUBOBJECT(BitmapList, images)
    GLAXNIMATE_SUBOBJECT(GradientColorsList, gradient_colors)
    GLAXNIMATE_SUBOBJECT(GradientList, gradients)
    GLAXNIMATE_SUBOBJECT(PrecompositionList, precompositions)

public:
    using DocumentNode::DocumentNode;

    Q_INVOKABLE model::NamedColor* add_color(const QColor& color, const QString& name = {});
    Q_INVOKABLE model::Bitmap* add_image_file(const QString& filename, bool embed);
    Q_INVOKABLE model::Bitmap* add_image(const QImage& image, const QString& store_as = "png");
    Q_INVOKABLE model::GradientColors* add_gradient_colors(int index = -1);
    Q_INVOKABLE model::Gradient* add_gradient(int index = -1);

    DocumentNode* docnode_parent() const override;
    int docnode_child_count() const override;
    DocumentNode* docnode_child(int index) const override;
    int docnode_child_index(DocumentNode* dn) const override;
    QIcon tree_icon() const override;
    QIcon instance_icon() const override;
    QString type_name_human() const override { return tr("Assets"); }
};

} // namespace model
