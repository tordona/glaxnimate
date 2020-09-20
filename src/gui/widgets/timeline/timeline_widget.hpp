#pragma once

#include <memory>

#include <QGraphicsView>

#include "model/document.hpp"


class TimelineWidget : public QGraphicsView
{
    Q_OBJECT
    
public:
    TimelineWidget(QWidget* parent = nullptr);
    ~TimelineWidget();
    
    void set_document(model::Document* document);
    void clear();
    void set_active(model::DocumentNode* node);
    void add_container(model::AnimationContainer* cont);
    void add_animatable(model::AnimatableBase* anim);
    int row_height() const;
    void set_row_height(int w);
    int header_height() const;
    
    void select(model::AnimatableBase* anim);
    
    model::AnimatableBase* animatable_at(const QPoint& viewport_pos);
    std::pair<model::KeyframeBase*, model::KeyframeBase*> keyframe_at(const QPoint& viewport_pos);
    
public slots:
    void update_timeline_start(model::FrameTime start);
    void update_timeline_end(model::FrameTime end);
    void reset_view();

protected:    
    void mousePressEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
    void paintEvent(QPaintEvent * event) override;
    void resizeEvent(QResizeEvent * event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void leaveEvent(QEvent * event) override;
    void keyPressEvent(QKeyEvent * event) override;

    
signals:
    void frame_clicked(int frame);
    void animatable_clicked(model::AnimatableBase* anim);
    
private:
    class Private;
    
    std::unique_ptr<Private> d;
};
