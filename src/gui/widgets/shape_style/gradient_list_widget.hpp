#ifndef GRADIENTLISTWIDGET_H
#define GRADIENTLISTWIDGET_H

#include <memory>
#include <QWidget>

namespace model {
    class Document;
    class Fill;
    class Stroke;
} // namespace model
class GlaxnimateWindow;

class GradientListWidget : public QWidget
{
    Q_OBJECT

public:
    GradientListWidget(QWidget* parent = nullptr);
    ~GradientListWidget();

    void set_document(model::Document* doc);
    void set_targets(model::Fill* fill, model::Stroke* stroke);
    void set_window(GlaxnimateWindow* window);

protected:
    void changeEvent ( QEvent* e ) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // GRADIENTLISTWIDGET_H