/*=============================================================================

Base class table model for representing generic set/vector data

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef TABLEMODEL_H
#define TABLEMODEL_H

#include <set>
#include <vector>
#include <string>

#include <QAbstractTableModel>
#include <QString>

// ==========================================================================

template<typename T, typename C = std::less<T>>
class TableModel : public QAbstractTableModel
{
public:
    TableModel(std::set<T, C> data, QObject* parent = 0)
        : QAbstractTableModel(parent),
          data_(data.begin(), data.end()) {}

    TableModel(std::vector<T> data, QObject* parent = 0)
        : QAbstractTableModel(parent),
          data_(data) {}

    // ----------------------------------------------------------------

    // Returns how many rows the dataset has
    int rowCount(const QModelIndex&) const
    {
        return this->data_.size();
    }

    // Returns how many columns the dataset has
    int columnCount(const QModelIndex&) const
    {
        return this->headerList.count();
    }

    // Returns the respective data for the given row and column
    QVariant data(const QModelIndex &index, int role) const
    {
        // check for display or tooltip
        if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
            return QVariant::Invalid;

        // get data entry
        T entry = this->data_.at(index.row());

        // return corresponding data
        if (role == Qt::ToolTipRole)
            return this->getTooltip(entry, index.column()).c_str();
        else
            return this->getProperty(entry, index.column()).c_str();
    }

    // Returns the respective header entry
    QVariant headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
            return QVariant::Invalid;

        return this->headerList.at(section);
    }

    // Returns a specific data entry
    T getDataAt(int row) const
    {
        return this->data_.at(row);
    }

protected:
    // Will be called to obtain the property in column i for item T
    virtual std::string getProperty(T, int) const = 0;

    // Will be called to obtain the header labels
    virtual char const* getHeader() const = 0;

    // Will be called to obtain the tooltip for item T in column i
    virtual std::string getTooltip(T, int) const
    {
        return "";
    }

    // Parses the header labels
    void initialize()
    {
        this->headerList = QString(this->getHeader()).split(";");
    }

private:
    // Stores header fields
    QStringList headerList;

    // Stores data
    std::vector<T> data_;
};

#endif // TABLEMODEL_H
