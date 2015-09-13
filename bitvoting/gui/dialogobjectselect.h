/*=============================================================================

Dialog for selecting a generic object in a drop down selection.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef DIALOGKEYSELECT_H
#define DIALOGKEYSELECT_H

#include <string>
#include <vector>

#include <QWidget>
#include <QInputDialog>

#include <boost/foreach.hpp>

class ObjectSelectDialog
{
public:
    // Show a dropdown dialog with several objects as a choice
    template<typename T>
    static bool getObject(QWidget*, std::string, std::string, std::vector<T>&, T*, std::string(*)(T));
};

// ==========================================================================

template<typename T>
bool ObjectSelectDialog::getObject(QWidget* parent, std::string caption, std::string label, std::vector<T>& list, T* out, std::string(*to_str)(T))
{
    // convert items to string list using the given function pointer
    QStringList items;
    BOOST_FOREACH(T item, list)
    {
        items.append((*to_str)(item).c_str());
    }

    // show dropdown dialog
    bool ok = false;
    QString result = QInputDialog::getItem(parent, caption.c_str(), label.c_str(), items, 0, false, &ok);

    if (!ok)
        return false;

    // find selection in string list
    int i = items.indexOf(result);
    if (i < 0)
        return false;

    // return corresponding item
    *out = list.at(i);

    return true;
}

#endif // DIALOGKEYSELECT_H
