#include "document_scene.hpp"

#include <unordered_set>

#include "model/graphics/document_node_graphics_item.hpp"

class model::graphics::DocumentScene::Private
{
public:
    static constexpr int editor_z = 1000;
    static constexpr int data_key_ptr = 0;

    using EditorMap = std::unordered_map<DocumentNode*, std::vector<std::unique_ptr<QGraphicsItem>>>;

    EditorMap::iterator remove_selection(const EditorMap::iterator& it)
    {
        return node_to_editors.erase(it);
    }
    
    
    model::DocumentNode* item_to_node(const QGraphicsItem* item) const
    {
        return item->data(data_key_ptr).value<model::DocumentNode*>();
    }

    
    std::vector<model::graphics::DocumentNodeGraphicsItem*> items_to_nodes(const QList<QGraphicsItem*>& items) const
    {
        std::vector<DocumentNodeGraphicsItem*> nodes;
        for ( auto item : items )
        {
            if ( item_to_node(item) )
                nodes.push_back(static_cast<DocumentNodeGraphicsItem*>(item));
        }
        return nodes;
    }


    Document* document = nullptr;
    std::unordered_map<DocumentNode*, DocumentNodeGraphicsItem*> node_to_item;
    EditorMap node_to_editors;

};

model::graphics::DocumentScene::DocumentScene()
    : d(std::make_unique<Private>())
{
}

model::graphics::DocumentScene::~DocumentScene()
{
    clear_selection();
}

void model::graphics::DocumentScene::set_document ( model::Document* document )
{
    if ( d->document )
    {
        disconnect_node(d->document->animation());
    }

    clear_selection();
    clear();

    d->document = document;

    if ( d->document )
    {
        connect_node(d->document->animation());
    }
}

void model::graphics::DocumentScene::connect_node ( model::DocumentNode* node )
{
    DocumentNodeGraphicsItem* child = node->docnode_make_graphics_item();
    if ( !child )
        return;

    d->node_to_item[node] = child;
    child->setData(Private::data_key_ptr, QVariant::fromValue(node));
    connect(node, &model::DocumentNode::docnode_child_add_end, this, &DocumentScene::connect_node);
    connect(node, &model::DocumentNode::docnode_child_remove_end, this, &DocumentScene::disconnect_node);
    connect(node, &model::DocumentNode::docnode_visible_changed, child, &DocumentNodeGraphicsItem::set_visible);

    DocumentNodeGraphicsItem* parent = nullptr;
    if ( auto parent_node = node->docnode_parent() )
    {
        auto it = d->node_to_item.find(parent_node);
        if ( it != d->node_to_item.end() )
            parent = it->second;
        child->setZValue(parent_node->docnode_child_index(node));
    }

    if ( parent )
        child->setParentItem(parent);
    else
        addItem(child);

    for ( DocumentNode* child : node->docnode_children() )
        connect_node(child);
}

void model::graphics::DocumentScene::disconnect_node ( model::DocumentNode* node )
{
    disconnect(node, &model::DocumentNode::docnode_child_add_end, this, &DocumentScene::connect_node);
    disconnect(node, &model::DocumentNode::docnode_child_remove_end, this, &DocumentScene::disconnect_node);

    for ( DocumentNode* child : node->docnode_children() )
        disconnect_node(child);

    auto item = d->node_to_item.find(node);
    if ( item != d->node_to_item.end() )
    {
        removeItem(item->second);
        delete item->second;
        d->node_to_item.erase(item);
    }
}

void model::graphics::DocumentScene::add_selection(model::DocumentNode* node)
{
    auto it = d->node_to_item.find(node);
    if ( it != d->node_to_item.end() )
        it->second->setSelected(true);

    if ( d->node_to_editors.find(node) != d->node_to_editors.end() )
        return;

    auto items = node->docnode_make_graphics_editor();
    for ( const auto& item : items )
    {
        item->setZValue(Private::editor_z);
        addItem(item.get());
    }

    d->node_to_editors.emplace(node, std::move(items));
}


void model::graphics::DocumentScene::remove_selection(model::DocumentNode* node)
{
    auto it = d->node_to_editors.find(node);
    if ( it != d->node_to_editors.end() )
    {
        d->remove_selection(it);
    }
}

void model::graphics::DocumentScene::toggle_selection(model::DocumentNode* node)
{
    if ( d->node_to_editors.count(node) )
        remove_selection(node);
    else
        add_selection(node);
}


void model::graphics::DocumentScene::clear_selection()
{
    d->node_to_editors.clear();
}

model::DocumentNode* model::graphics::DocumentScene::item_to_node(const QGraphicsItem* item) const
{
    return d->item_to_node(item);
}

void model::graphics::DocumentScene::user_select(const std::vector<model::DocumentNode *>& nodes, SelectFlags flags)
{
    std::vector<model::DocumentNode *> deselected;
    std::unordered_set<model::DocumentNode*> sel(nodes.begin(), nodes.end());
    if ( flags == Replace )
    {
        for ( auto it = d->node_to_editors.begin(); it != d->node_to_editors.end(); )
        {
            if ( sel.count(it->first) )
            {
                ++it;
            }
            else
            {
                deselected.push_back(it->first);
                it = d->remove_selection(it);
            }
        }
    }

    if ( flags == Replace || flags == Append )
    {
        for ( model::DocumentNode* n : nodes )
            add_selection(n);
        emit node_user_selected(nodes, deselected);
    }
    else if ( flags )
    {
        std::vector<model::DocumentNode *> selected;
        
        for ( model::DocumentNode* node : nodes )
        {
            auto it = d->node_to_editors.find(node);
            if ( it == d->node_to_editors.end() )
            {
                selected.push_back(node);
                add_selection(node);
            }
            else
            {
                deselected.push_back(node);
                d->remove_selection(it);
            }
        }
        
        emit node_user_selected(selected, deselected);
    }

}

std::vector<model::graphics::DocumentNodeGraphicsItem*> model::graphics::DocumentScene::nodes(const QPointF& point, const QTransform& device_transform) const
{
    return d->items_to_nodes(items(point, Qt::IntersectsItemShape, Qt::DescendingOrder, device_transform));
}

std::vector<model::graphics::DocumentNodeGraphicsItem*> model::graphics::DocumentScene::nodes(const QPainterPath& path, const QTransform& device_transform) const
{
    return d->items_to_nodes(items(path, Qt::IntersectsItemShape, Qt::DescendingOrder, device_transform));
}
