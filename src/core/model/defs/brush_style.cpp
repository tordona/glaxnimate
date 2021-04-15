#include "brush_style.hpp"

QIcon model::BrushStyle::instance_icon() const
{
    if ( icon.isNull() )
    {
        icon = QPixmap(32, 32);
        fill_icon(icon);
    }
    return icon;
}
