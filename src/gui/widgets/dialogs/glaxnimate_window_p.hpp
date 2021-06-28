#ifndef GLAXNIMATEWINDOW_P_H
#define GLAXNIMATEWINDOW_P_H

#include <QToolButton>
#include <QMessageBox>
#include <QPointer>

#include "QtColorWidgets/color_delegate.hpp"
#include "QtColorWidgets/color_palette_model.hpp"

#include "ui_glaxnimate_window.h"
#include "glaxnimate_window.hpp"

#include "model/document.hpp"
#include "command/structure_commands.hpp"

#include "graphics/document_scene.hpp"
#include "item_models/document_node_model.hpp"
#include "item_models/property_model_single.hpp"
#include "item_models/comp_filter_model.hpp"
#include "item_models/asset_proxy_model.hpp"

#include "style/dock_widget_style.hpp"
#include "style/property_delegate.hpp"

#include "app/settings/settings.hpp"
#include "app/scripting/script_engine.hpp"
#include "app/log/log_model.hpp"

#include "plugin/plugin.hpp"
#include "utils/pseudo_mutex.hpp"

class IoStatusDialog;
class AboutDialog;
class ViewTransformWidget;
class FlowLayout;
class ShapeStylePreviewWidget;

namespace tools {
class Tool;
} // namespace tools

namespace model {
class PreCompLayer;
} // namespace model

class GlaxnimateWindow::Private
{
public:
    struct CompState
    {
        CompState(model::VisualNode* single)
        : selection(1, single), current(single)
        {}

        CompState() : current(nullptr) {}

        std::vector<model::VisualNode*> selection;
        model::VisualNode* current;
    };

    enum class AlignPosition
    {
        Begin   = 0x01,
        Center  = 0x02,
        End     = 0x04,
    };

    enum class AlignDirection
    {
        Horizontal = 0x10,
        Vertical   = 0x20,
    };

    Ui::GlaxnimateWindow ui;

    std::unique_ptr<model::Document> current_document;

    item_models::PropertyModelSingle property_model;
    item_models::DocumentNodeModel document_node_model;
    item_models::CompFilterModel comp_model;
    item_models::AssetProxyModel asset_model;
    graphics::DocumentScene scene;
    model::Composition* comp = nullptr;


    GlaxnimateWindow* parent = nullptr;

    QStringList recent_files;
    io::Options export_options;
    bool current_document_has_file = false;

    QPointer<model::BrushStyle> main_brush;
    QPointer<model::BrushStyle> secondary_brush;

    tools::Tool* active_tool = nullptr;
    std::map<QString, std::vector<QWidget*>> tool_widgets;
    std::map<QString, std::vector<QAction*>> tool_actions;

    std::vector<CompState> comp_selections;

    QLabel* label_mouse_pos = nullptr;
    QLabel* label_recording = nullptr;
    QWidget* widget_recording = nullptr;
    ShapeStylePreviewWidget* widget_current_style = nullptr;

    utils::PseudoMutex update_current;
    utils::PseudoMutex update_selection;

    // "set and forget" kinda variables
    int autosave_timer = 0;
    int autosave_timer_mins = 0;
    bool autosave_load = false;
    QString undo_text;
    QString redo_text;
    color_widgets::ColorDelegate color_delegate;
    style::PropertyDelegate property_delegate;
    style::DockWidgetStyle dock_style;
    ViewTransformWidget* view_trans_widget;
    bool started = false;
    IoStatusDialog* dialog_import_status;
    IoStatusDialog* dialog_export_status;
    AboutDialog* about_dialog;
    FlowLayout* dock_tools_layout;
    app::log::LogModel log_model;
    color_widgets::ColorPaletteModel palette_model;

    // document
    void setup_document(const QString& filename);
    void do_setup_document();
    void setup_document_new(const QString& filename);
    bool setup_document_open(const io::Options& options);
    void setup_document_ptr(std::unique_ptr<model::Document> doc);
    void refresh_title();
    bool close_document();
    bool save_document(bool force_dialog, bool export_opts);
    void document_open();
    void document_open_from_filename(const QString& filename);
    void document_reload();
    void preview_lottie(const QString& renderer);
    void preview_svg();
    void preview(io::ImportExport& exporter, const QVariantMap& options);
    void save_frame_bmp();
    void save_frame_svg();
    void validate_tgs();
    void autosave_timer_load_settings();
    void autosave_timer_start(int mins = -1);
    void autosave_timer_tick();
    QString backup_name();
    QString backup_name(const QUuid& uuid);
    void load_backup(const QUuid& uuid);
    QString drop_event_data(QDropEvent* ev);
    void import_image();
    void import_file();

    // ui
    void setupUi(bool restore_state, bool debug, GlaxnimateWindow* parent);
    void retranslateUi(QMainWindow* parent);
    void view_fit();
    void reload_recent_menu();
    void most_recent_file(const QString& s);
    void show_warning(const QString& title, const QString& message, app::log::Severity icon = app::log::Warning);
    void help_about();
    void shutdown();
    void switch_tool(tools::Tool* tool);
    void switch_tool_action(QAction* action);
    void status_message(const QString& msg, int duration=5000);
    void set_color_def(model::BrushStyle* sty, bool secondary);
    QString get_open_image_file(const QString& title, const QString& dir, QString* out_dir = nullptr);
    void set_brush_reference(model::BrushStyle* sty, bool secondary);
    void trace_dialog(model::DocumentNode* object);
    void mouse_moved(const QPointF& pos);
    template<class T> void add_modifier_menu_action(QMenu* menu);
    void show_startup_dialog();

    void init_actions();
    void init_plugins();
    tools::Tool* init_tools_ui();
    void init_item_views();
    void init_status_bar();
    void init_docks();
    void init_menus();
    void init_debug();
    void init_tools(tools::Tool* to_activate);
    void init_restore_state();
    void init_template_menu();

    // Model
    model::Composition* current_composition();
    model::VisualNode* current_document_node();
    void set_current_document_node(model::VisualNode* node);
    model::ShapeElement* current_shape();
    model::ShapeListProperty* current_shape_container();
    std::vector<model::VisualNode*> cleaned_selection();
    std::vector<model::VisualNode*> copy();
    void paste(bool as_comp);
    void paste_document(model::Document* document, const QString& macro_name, bool as_comp);
    void cut();
    void duplicate_selection();
    void delete_selected();
    void move_current(command::ReorderCommand::SpecialPosition pos);
    void group_shapes();
    void ungroup_shapes();
    void move_to();
    void cleanup_document();
    void to_path();
    void convert_to_path(const std::vector<model::ShapeElement*>& shapes, std::vector<model::ShapeElement*>* out);
    void align(AlignDirection direction, AlignPosition position, bool outside);
    QPointF align_point(const QRectF& rect, AlignDirection direction, AlignPosition position);
    void dropped(const QMimeData* data);

    void switch_composition(model::Composition* comp, int i);
    void setup_composition(model::Composition* comp, int index = -1);
    void add_composition();
    void objects_to_new_composition(
        model::Composition* comp,
        const std::vector<model::VisualNode*>& objects,
        model::ObjectListProperty<model::ShapeElement>* layer_parent,
        int layer_index
    );
    void on_remove_precomp(int index);
    void shape_to_precomposition(model::ShapeElement* node);

    void layer_new_layer();
    void layer_new_fill();
    void layer_new_stroke();
    void layer_new_group();
    void layer_new_impl(std::unique_ptr<model::ShapeElement> layer);
    void layer_delete();
    void layer_duplicate();
    void layer_new_comp(QAction* act);
    model::PreCompLayer* layer_new_comp(model::Precomposition* comp);

    void text_put_on_path();
    void text_remove_from_path();

    // Selection
    void document_treeview_selection_changed(const QItemSelection &selected, const QItemSelection &deselected);
    void document_treeview_current_changed(const QModelIndex& index);
    void scene_selection_changed(const std::vector<model::VisualNode*>& selected, const std::vector<model::VisualNode*>& deselected);
    void timeline_current_node_changed(model::VisualNode* node);
    /**
     * \brief makes \p node the current object in all views
     */
    void set_current_object(model::DocumentNode* node);
    /**
     * \brief Forwards the selection to all models
     */
    void selection_changed(const std::vector<model::VisualNode*>& selected, const std::vector<model::VisualNode*>& deselected);
};

#endif // GLAXNIMATEWINDOW_P_H
