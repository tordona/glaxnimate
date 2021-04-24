#ifndef FONT_FONTSTYLEDIALOG_H
#define FONT_FONTSTYLEDIALOG_H

#include <memory>
#include <qdialog.h>

namespace font {

class FontStyleDialog : public QDialog
{
    Q_OBJECT

public:
    FontStyleDialog(QWidget* parent = nullptr);
    ~FontStyleDialog();

    void set_font(const QFont& font);
    const QFont& font() const;
    QStringList favourites() const;
    void set_favourites(const QStringList& fav);

    void set_preview_text(const QString& text);

protected:
    void changeEvent ( QEvent* e ) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

}

#endif // FONT_FONTSTYLEDIALOG_H