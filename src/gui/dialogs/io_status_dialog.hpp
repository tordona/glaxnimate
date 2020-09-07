#pragma once

#include <memory>

#include <QDialog>

#include "io/base.hpp"


class IoStatusDialog : public QDialog
{
    Q_OBJECT

public:
    IoStatusDialog(const QIcon& icon, const QString& title, bool delete_on_close, QWidget* parent = nullptr);
    ~IoStatusDialog();

    void reset(io::ImportExport* ie, const QString& label);

protected:
    void closeEvent(QCloseEvent* ev) override;
    void changeEvent(QEvent *e) override;

private:
    void _on_error(const QString& message);
    void _on_progress_max_changed(int max);
    void _on_progress(int value);
    void _on_completed(bool success);

    class Private;
    std::unique_ptr<Private> d;
};