#pragma once

#include <QList>
#include <QIcon>
#include <QUuid>

#include "model/animation/animatable.hpp"

namespace model {

class Document;
class ReferencePropertyBase;

/**
 * \brief Base class for elements of the document tree, that need to show in the tree view etc.
 */
class DocumentNode : public Object
{
    Q_OBJECT

public:
    /**
     * @brief Name of the node, used to display it in the UI
     */
    GLAXNIMATE_PROPERTY(QString, name, "")
    /**
     * @brief Color of the node the tree UI to highlight grouped items
     *
     * Generally parent/child relationshitps define groups but layers can
     * be grouped with each other even if they are children of a composition
     */
    GLAXNIMATE_PROPERTY(QColor, group_color, QColor(0, 0, 0, 0))
    /**
     * @brief Unique identifier for the node
     */
    GLAXNIMATE_PROPERTY_RO(QUuid, uuid, {})

private:
    class ChildRange;

    using get_func_t = DocumentNode* (DocumentNode::*) (int) const;
    using count_func_t = int (DocumentNode::*) () const;

    class ChildIterator
    {
    public:

        ChildIterator& operator++() noexcept { ++index; return *this; }
        DocumentNode* operator->() const { return (parent->*get_func)(index); }
        DocumentNode* operator*() const { return (parent->*get_func)(index); }
        bool operator==(const ChildIterator& oth) const noexcept
        {
            return parent == oth.parent && index == oth.index;
        }
        bool operator!=(const ChildIterator& oth) const noexcept
        {
            return !(*this == oth );
        }

    private:
        ChildIterator(const DocumentNode* parent, int index, get_func_t get_func) noexcept
        : parent(parent), index(index), get_func(get_func) {}

        const DocumentNode* parent;
        int index;
        get_func_t get_func;
        friend ChildRange;
    };


    class ChildRange
    {
    public:
        ChildIterator begin() const noexcept { return ChildIterator{parent, 0, get_func}; }
        ChildIterator end() const noexcept { return ChildIterator{parent, size(), get_func}; }
        int size() const { return (parent->*count_func)(); }

    private:
        ChildRange(const DocumentNode* parent, get_func_t get_func, count_func_t count_func) noexcept
        : parent(parent), get_func(get_func), count_func(count_func) {}
        const DocumentNode* parent;
        get_func_t get_func;
        count_func_t count_func;
        friend DocumentNode;
    };


public:
    enum PaintMode
    {
        Transformed,    ///< Paint only this, apply transform
        NoTransform,    ///< Paint only this, don't apply transform
        Recursive       ///< Paint this and children, apply transform
    };

    explicit DocumentNode(Document* document);

    virtual QIcon docnode_icon() const = 0;

    virtual DocumentNode* docnode_parent() const = 0;
    virtual int docnode_child_count() const = 0;
    virtual DocumentNode* docnode_child(int index) const = 0;
    virtual int docnode_child_index(DocumentNode* dn) const = 0;

    virtual DocumentNode* docnode_group_parent() const { return docnode_parent(); }
    virtual int docnode_group_child_count() const { return docnode_child_count(); }
    virtual DocumentNode* docnode_group_child(int index) const { return docnode_child(index); }

    /**
     * \brief If \b true, the node is mainly used to contain other nodes so it can be ignored on selections
     *
     * This does not affect docnode_selectable() for this or its ancestors
     */
    virtual bool docnode_selection_container() const { return true; }

    /**
     * \brief Bounding rect in local coordinates (current frame)
     */
    virtual QRectF local_bounding_rect(FrameTime t) const = 0;
//     /**
//      * \brief Bounding rect in local coordinates for the given time
//      */
//     virtual QRectF local_bounding_rect(FrameTime) const { return bounding_rect(t); }
//     /**
//      * \brief Polygon created by applying the object transform to local_bounding_rect()
//      */
//     virtual QPolygonF unaligned_bounding_rect(FrameTime t) const
//     {
//         QRectF br = bounding_rect(t);
//         return QPolygonF({br.topLeft(), br.topRight(), br.bottomRight(), br.bottomLeft()});
//     }
//     /**
//      * \brief Bounding rect in global coordinates (ie: bounding rect of unaligned_bounding_rect)
//      */
//     virtual QRectF bounding_rect(FrameTime t) const = 0;

    bool docnode_visible() const { return visible_; }
    bool docnode_locked() const { return locked_; }
    bool docnode_selectable() const;
    bool docnode_visible_recursive() const;

    bool docnode_locked_by_ancestor() const;

    QString object_name() const override;

    QString docnode_name() const;

    QColor docnode_group_color() const;
    const QPixmap& docnode_group_icon() const;

    ChildRange docnode_children() const noexcept
    {
        return ChildRange{this, &DocumentNode::docnode_child, &DocumentNode::docnode_child_count};
    }
    ChildRange docnode_group_children() const noexcept
    {
        return ChildRange{this, &DocumentNode::docnode_group_child, &DocumentNode::docnode_group_child_count};
    }

    template<class T=DocumentNode>
    T* docnode_find_by_uuid(const QUuid& uuid) const
    {
        for ( DocumentNode* child : docnode_children() )
        {
            if ( child->uuid.get() == uuid )
                return qobject_cast<T*>(child);
            if ( T* matched = child->docnode_find_by_uuid<T>(uuid) )
                return matched;
        }

        return nullptr;
    }

    template<class T=DocumentNode>
    T* docnode_find_by_name(const QString& name) const
    {
        for ( DocumentNode* child : docnode_children() )
        {
            if ( child->name.get() == name )
                return qobject_cast<T*>(child);
            if ( T* matched = child->docnode_find_by_name<T>(name) )
                return matched;
        }

        return nullptr;
    }

    template<class T=DocumentNode>
    std::vector<T*> docnode_find(const QString& type_name, bool include_self = true)
    {
        std::vector<T*> matches;
        const char* t_name = T::staticMetaObject.className();
        if ( include_self )
            docnode_find_impl_add(type_name, matches, t_name);
        docnode_find_impl(type_name, matches, t_name);
        return matches;
    }


    bool docnode_is_instance(const QString& type_name) const;

    void paint(QPainter* painter, FrameTime time, PaintMode mode) const;

private:
    template<class T=DocumentNode>
    std::vector<T*> docnode_find_impl(const QString& type_name, std::vector<T*>& matches, const char* t_name)
    {
        for ( DocumentNode* child : docnode_children() )
        {
            child->docnode_find_impl_add(type_name, matches, t_name);
            child->docnode_find_impl(type_name, matches, t_name);
        }
    }
    template<class T=DocumentNode>
    void docnode_find_impl_add(const QString& type_name, std::vector<T*>& matches, const char* t_name)
    {
        if ( inherits(t_name) && docnode_is_instance(type_name) )
            matches.push_back(static_cast<T*>(this));
    }

protected:
    void docnode_on_update_group(bool force = false);
    void on_property_changed(const BaseProperty* prop, const QVariant&) override;
    bool docnode_valid_color() const;
    virtual void on_paint(QPainter*, FrameTime, PaintMode) const {}


public slots:
    void docnode_set_visible(bool visible)
    {
        emit docnode_visible_changed(visible_ = visible);
    }

    void docnode_set_locked(bool locked)
    {
        emit docnode_locked_changed(locked_ = locked);
    }

signals:
    void docnode_child_add_begin(int row);
    void docnode_child_add_end(DocumentNode* node);

    void docnode_child_remove_begin(int row);
    void docnode_child_remove_end(DocumentNode* node);

    void docnode_visible_changed(bool);
    void docnode_locked_changed(bool);
    void docnode_name_changed(const QString&);
    void docnode_group_color_changed(const QColor&);

    void bounding_rect_changed();

private:
    bool visible_ = true;
    bool locked_ = false;
    mutable QPixmap group_icon;
};


/**
 * \brief Base class for document nodes that enclose an animation
 */
class AnimationContainer: public DocumentNode
{
    Q_OBJECT
    GLAXNIMATE_PROPERTY(int,    first_frame,  0, &AnimationContainer::first_frame_changed, &AnimationContainer::validate_first_frame, PropertyTraits::Visual)
    GLAXNIMATE_PROPERTY(int,    last_frame, 180, &AnimationContainer::last_frame_changed,  &AnimationContainer::validate_last_frame,  PropertyTraits::Visual)

public:
    using DocumentNode::DocumentNode;

signals:
    void first_frame_changed(int);
    void last_frame_changed(int);

private:
    bool validate_first_frame(int f) const
    {
        return f >= 0 && f < last_frame.get();
    }

    bool validate_last_frame(int f) const
    {
        return f > first_frame.get();
    }
};


class ReferencePropertyBase : public BaseProperty
{
    Q_GADGET
public:
    ReferencePropertyBase(
        DocumentNode* obj,
        const QString& name,
        PropertyCallback<std::vector<DocumentNode*>, void> valid_options,
        PropertyCallback<bool, DocumentNode*> is_valid_option,
        PropertyTraits::Flags flags = PropertyTraits::Visual)
        : BaseProperty(obj, name, PropertyTraits{PropertyTraits::ObjectReference, flags}),
        valid_options_(std::move(valid_options)),
        is_valid_option_(std::move(is_valid_option))
    {
    }

    std::vector<DocumentNode*> valid_options() const
    {
        return valid_options_(object());
    }

    bool is_valid_option(DocumentNode* ptr) const
    {
        return is_valid_option_(object(), ptr);
    }

    void set_time(FrameTime) override {}

private:
    PropertyCallback<std::vector<DocumentNode*>, void> valid_options_;
    PropertyCallback<bool, DocumentNode*> is_valid_option_;
};


template<class Type>
class ReferenceProperty : public ReferencePropertyBase
{
public:
    using value_type = Type*;

    using ReferencePropertyBase::ReferencePropertyBase;

    bool set(Type* value)
    {
        if ( !is_valid_option(value) )
            return false;
        value_ = value;
        value_changed();
        return true;
    }

    Type* get() const
    {
        return value_;
    }

    QVariant value() const override
    {
        if ( !value_ )
            return {};
        return QVariant::fromValue(value_);
    }

    bool set_value(const QVariant& val) override
    {
        if ( auto v = detail::variant_cast<Type*>(val) )
            return set(*v);
        return true;
    }

private:
    Type* value_ = nullptr;
};



} // namespace model