#pragma once

#include <unordered_set>

#include "model/document_node.hpp"
#include "model/property/reference_property.hpp"

namespace model {

class Asset : public DocumentNode, public AssetBase
{
    Q_OBJECT

public:
    using DocumentNode::DocumentNode;

signals:
    void users_changed();

protected:
    void on_users_changed() override
    {
        emit users_changed();
    }

    model::DocumentNode* to_reftarget() override { return this; }


    int docnode_child_count() const override { return 0; }
    DocumentNode* docnode_child(int) const override { return nullptr; }
    int docnode_child_index(DocumentNode*) const override { return -1; }
    QIcon tree_icon() const override { return instance_icon(); }
};



} // namespace model
