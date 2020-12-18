#include "node_menu.hpp"

#include <QDesktopServices>

#include "model/shapes/group.hpp"
#include "model/shapes/image.hpp"
#include "model/shapes/precomp_layer.hpp"
#include "model/defs/defs.hpp"

#include "command/structure_commands.hpp"
#include "command/animation_commands.hpp"
#include "command/property_commands.hpp"
#include "command/shape_commands.hpp"
#include "command/undo_macro_guard.hpp"

#include "widgets/dialogs/shape_parent_dialog.hpp"

namespace {

class ResetTransform
{
public:
    void operator()() const
    {
        doc->push_command(new command::SetMultipleAnimated(
            NodeMenu::tr("ResetTransform"),
            true,
            {
                &trans->position,
                &trans->scale,
                &trans->rotation
            },
            trans->anchor_point.get(),
            QVector2D(1, 1),
            0
        ));
    }

    model::Document* doc;
    model::Transform* trans;
};

QAction* action_for_node(model::ReferenceTarget* node, model::ReferenceTarget* selected, QMenu* parent, QActionGroup* group)
{
    QAction* act = new QAction(parent);
    if ( node )
    {
        act->setIcon(node->reftarget_icon());
        act->setText(node->object_name());
    }
    else
    {
        act->setIcon(QIcon::fromTheme("edit-none"));
        act->setText(NodeMenu::tr("None"));
    }

    act->setActionGroup(group);
    act->setCheckable(true);
    act->setChecked(node == selected);
    act->setData(QVariant::fromValue(node));
    parent->addAction(act);
    return act;
}

void move_action(
    NodeMenu* menu,
    model::ShapeElement* node,
    command::ReorderCommand::SpecialPosition pos
)
{
    QIcon icon;
    QString label;

    switch ( pos )
    {
        case command::ReorderCommand::MoveTop:
            icon = QIcon::fromTheme("layer-top");
            label = NodeMenu::tr("Move to Top");
            break;
        case command::ReorderCommand::MoveUp:
            icon = QIcon::fromTheme("layer-raise");
            label = NodeMenu::tr("Raise");
            break;
        case command::ReorderCommand::MoveDown:
            icon = QIcon::fromTheme("layer-lower");
            label = NodeMenu::tr("Lower");
            break;
        case command::ReorderCommand::MoveBottom:
            icon = QIcon::fromTheme("layer-bottom");
            label = NodeMenu::tr("Move to Bottom");
            break;
    }

    QAction* act = menu->addAction(icon, label, menu, [node, pos]{
        node->push_command(new command::ReorderCommand(node, pos));
    });

    int position = pos;
    if ( !command::ReorderCommand::resolve_position(node, position) )
        act->setEnabled(false);
}

QMenu* menu_ref_property(const QIcon& icon, const QString& text, QWidget* parent, model::ReferencePropertyBase* prop)
{
    QMenu* menu = new QMenu(text, parent);
    menu->setIcon(icon);
    QActionGroup* group = new QActionGroup(parent);
    auto value = prop->value().value<model::ReferenceTarget*>();
    for ( auto other_lay : prop->valid_options() )
        action_for_node(other_lay, value, menu, group);

    QObject::connect(menu, &QMenu::triggered, parent, [prop](QAction* act){
        prop->object()->push_command(
            new command::SetPropertyValue(
                prop,
                prop->value(),
                act->data(),
                true
            )
        );
    });

    return menu;
}

template<class ToType>
class ConvertGroupType
{
public:
    ConvertGroupType(model::Group* from)
        : from(from), owner(static_cast<model::ShapeListProperty*>(from->docnode_parent()->get_property("shapes")))
    {}

    void operator()()
    {
        auto to = new ToType(from->document());
        std::unique_ptr<model::ShapeElement> uto(to);

        for ( auto prop : to->properties() )
        {
            if ( prop != &to->uuid && prop != &to->shapes )
            {
                if ( auto src = from->get_property(prop->name()) )
                    prop->assign_from(src);
            }
        }

        command::UndoMacroGuard guard(NodeMenu::tr("Convert %1 to %2").arg(from->name.get()).arg(to->type_name_human()), from->document());
        from->push_command(new command::AddObject(owner, std::move(uto), owner->index_of(from)));
        std::vector<model::ShapeElement*> shapes;
        shapes.reserve(from->shapes.size());
        for ( const auto& shape : from->shapes )
            shapes.push_back(shape.get());
        for ( std::size_t i = 0; i < shapes.size(); i++ )
            from->push_command(new command::MoveObject(shapes[i], &from->shapes, &to->shapes, i));
        from->push_command(new command::RemoveObject<model::ShapeElement>(from, owner));
    }

private:
    model::Group* from;
    model::ShapeListProperty* owner;
};

void convert_to_mask(NodeMenu* menu, model::ShapeElement* element)
{
    if ( element->docnode_parent() != element->document()->defs()->masks.get() )
        menu->addAction(QIcon::fromTheme("path-mask-edit"), NodeMenu::tr("Convert to Mask"), menu, [element]{
            auto shapes = &element->document()->defs()->masks->shapes;
            command::UndoMacroGuard guard(NodeMenu::tr("Convert %1 to Mask").arg(element->name.get()), element->document());
            element->push_command(new command::MoveShape(element, element->owner(), shapes, shapes->size()));
        });
}

void togglable_action(QMenu* menu, model::Property<bool>* prop, const QString& icon, const QString& label)
{
    auto action = menu->addAction(QIcon::fromTheme(icon), label, menu, [prop](bool value){
        prop->set_undoable(value);
    });
    action->setCheckable(true);
    action->setChecked(prop->get());
}

} // namespace



NodeMenu::NodeMenu(model::DocumentNode* node, GlaxnimateWindow* window, QWidget* parent)
    : QMenu(node->object_name(), parent)
{
    setIcon(node->docnode_icon());
    addSection(node->docnode_icon(), node->object_name());

    togglable_action(this, &node->visible, "view-visible", tr("Visible"));
    togglable_action(this, &node->locked, "object-locked", tr("Locked"));
    addSeparator();

    if ( auto shape = qobject_cast<model::ShapeElement*>(node) )
    {
        addAction(QIcon::fromTheme("edit-delete-remove"), tr("Delete"), this, [shape]{
            shape->push_command(new command::RemoveShape(shape, shape->owner()));
        });

        addAction(QIcon::fromTheme("edit-duplicate"), tr("Duplicate"), this, [shape, window]{
            auto cmd = command::duplicate_shape(shape);
            shape->push_command(cmd);
            window->set_current_document_node(cmd->object());
        });

        addSeparator();
        model::Layer* lay = nullptr;
        auto group = qobject_cast<model::Group*>(shape);
        if ( group )
        {
            addAction(QIcon::fromTheme("transform-move"), tr("Reset Transform"), this,
                ResetTransform{group->document(), group->transform.get()}
            );

            lay = qobject_cast<model::Layer*>(shape);
            if ( lay )
            {
                addSeparator();
                addAction(QIcon::fromTheme("timeline-use-zone-on"), tr("Span All Frames"), this, [lay]{
                    command::UndoMacroGuard guard(tr("Span All Frames"), lay->document());
                    lay->animation->first_frame.set_undoable(
                        lay->document()->main()->animation->first_frame.get()
                    );
                    lay->animation->last_frame.set_undoable(
                        lay->document()->main()->animation->last_frame.get()
                    );
                });
            }
        }

        addSeparator();

        move_action(this, shape, command::ReorderCommand::MoveTop);
        move_action(this, shape, command::ReorderCommand::MoveUp);
        move_action(this, shape, command::ReorderCommand::MoveDown);
        move_action(this, shape, command::ReorderCommand::MoveBottom);

        addAction(QIcon::fromTheme("selection-move-to-layer-above"), tr("Move to..."), this, [shape, window]{
            if ( auto parent = ShapeParentDialog(window->model(), window).get_shape_parent() )
            {
                if ( shape->owner() != parent )
                    shape->push_command(new command::MoveShape(shape, shape->owner(), parent, parent->size()));
            }
        });

        if ( lay )
        {
            addAction(menu_ref_property(QIcon::fromTheme("go-parent-folder"), tr("Parent"), this, &lay->parent)->menuAction());
            addAction(QIcon::fromTheme("object-group"), tr("Convert to Group"), this, ConvertGroupType<model::Group>(lay));
            addAction(QIcon::fromTheme("component"), tr("Precompose"), this, [window, lay]{
                window->shape_to_precomposition(lay);
            });
            convert_to_mask(this, lay);
        }
        else if ( group )
        {
            addAction(QIcon::fromTheme("folder"), tr("Convert to Layer"), this, ConvertGroupType<model::Layer>(group));
            convert_to_mask(this, group);
        }
        else if ( auto image = qobject_cast<model::Image*>(shape) )
        {
            addSeparator();

            addAction(menu_ref_property(QIcon::fromTheme("folder-pictures"), tr("Image"), this, &image->image)->menuAction());

            addAction(QIcon::fromTheme("mail-attachment-symbolic"), tr("Embed"), this, [image]{
                if ( image->image.get() )
                    image->image->embed(true);
            })->setEnabled(image->image.get() && !image->image->embedded());

            addAction(QIcon::fromTheme("editimage"), tr("Open with External Application"), this, [image, window]{
                if ( image->image.get() )
                {
                    if ( !QDesktopServices::openUrl(image->image->to_url()) )
                        window->warning(tr("Could not find suitable application, check your system settings."));
                }
            })->setEnabled(image->image.get());


            addAction(QIcon::fromTheme("document-open"), tr("From File..."), this, [image, window]{
                auto bmp = image->image.get();
                QString filename = window->get_open_image_file(tr("Update Image"), bmp ? bmp->file_info().absolutePath() : "");
                if ( filename.isEmpty() )
                    return;

                command::UndoMacroGuard macro(tr("Update Image"), image->document());
                if ( bmp )
                {
                    bmp->data.set_undoable(QByteArray());
                    bmp->filename.set_undoable(filename);
                }
                else
                {
                    auto img = image->document()->defs()->add_image_file(filename, false);
                    if ( img )
                        image->image.set_undoable(QVariant::fromValue(img));
                }
            });

            addAction(QIcon::fromTheme("bitmap-trace"), tr("Trace Bitmap..."), this, [image, window]{
                window->trace_dialog(image);
            });
        }
        else if ( auto lay = qobject_cast<model::PreCompLayer*>(shape) )
        {
            addAction(QIcon::fromTheme("edit-rename"), tr("Rename from Composition"), this, [lay]{
                if ( lay->composition.get() )
                    lay->name.set_undoable(lay->composition->object_name());
            });
            addAction(QIcon::fromTheme("archive-extract"), tr("Decompose"), this, [lay]{
                command::UndoMacroGuard guard(tr("Decompose"), lay->document());

                auto comp = lay->composition.get();

                if ( comp )
                {
                    int index = lay->owner()->index_of(lay);
                    for ( const auto& child : comp->shapes )
                    {
                        std::unique_ptr<model::ShapeElement> clone(static_cast<model::ShapeElement*>(child->clone().release()));
                        clone->refresh_uuid();
                        lay->push_command(new command::AddShape(lay->owner(), std::move(clone), ++index));
                    }
                }

                lay->push_command(new command::RemoveShape(lay, lay->owner()));

                if ( comp && comp->users().empty() )
                    lay->push_command(new command::RemoveObject(comp, &lay->document()->defs()->precompositions));
            });
        }
    }
    else if ( qobject_cast<model::Composition*>(node) )
    {
        addAction(
            window->create_layer_menu()->menuAction()
        );
    }
}
