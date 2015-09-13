/*=============================================================================

Dialog for managing signature and paillier keys.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef DIALOGMANAGEKEYS_H
#define DIALOGMANAGEKEYS_H

#include "gui/tablemodel.h"
#include "bitcoin/key.h"
#include "database/paillierdb.h"
#include "database/electiondb.h"
#include "controller.h"

#include <QDialog>

// ==========================================================================

namespace Ui {
class ManageKeysDialog;
}

std::string roleToStr(Role r);

// ==========================================================================

class SignKeyTableModel : public TableModel<SignKeyPair>
{
public:
    SignKeyTableModel(std::set<SignKeyPair> data, QObject* parent = 0)
        : TableModel(data, parent)
    {
        this->initialize();
    }

    SignKeyTableModel(std::vector<SignKeyPair> data, QObject* parent = 0)
        : TableModel(data, parent)
    {
        this->initialize();
    }

protected:
    // s. TableModel
    char const* getHeader() const
    {
        return "Fingerprint;Role";
    }

    // s. TableModel
    std::string getProperty(SignKeyPair key, int i) const
    {
        switch(i)
        {
        case 0:
            return key.second.GetID().ToString();
        case 1:
            return roleToStr(key.first.getRole());
        default:
            break;
        }

        return "";
    }
};

// ==========================================================================

class PaillierTableModel : public TableModel<ElectionPrivateKey>
{
public:
    PaillierTableModel(std::set<ElectionPrivateKey> data, QObject* parent = 0)
        : TableModel(data, parent)
    {
        this->initialize();
    }

    PaillierTableModel(std::vector<ElectionPrivateKey> data, QObject* parent = 0)
        : TableModel(data, parent)
    {
        this->initialize();
    }

protected:
    // s. TableModel
    char const* getHeader() const
    {
        return "Election;Fingerprint";
    }

    // s. TableModel
    std::string getProperty(ElectionPrivateKey key, int i) const
    {
        switch(i)
        {
        case 0:
        {
            // find corresponding election
            ElectionManager* manager = NULL;
            bool result = ElectionDB::Get(key.election, &manager);

            // try to resolve (may be not there yet)
            if (!result || !manager || !manager->transaction)
                return "Unknown (" + key.election.ToString() + ")";

            return manager->transaction->election->name;
        }
        case 1:
            return key.signatureKey.GetHex();
        default:
            break;
        }

        return "";
    }
};

// ==========================================================================

class ManageKeysDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ManageKeysDialog(Controller*, QWidget *parent = 0);
    ~ManageKeysDialog();

private slots:
    void on_cbKeyRole_currentIndexChanged(int index);
    void on_btnExport_clicked();
    void on_btnClose_clicked();
    void on_btnImport_clicked();
    void on_btnNew_clicked();

    // ----------------------------------------------------------------

    void ontbKeysSelectionChanged(const QModelIndex&, const QModelIndex&);

private:
    Ui::ManageKeysDialog *ui;

    // Reference to the controller
    Controller* controller;

    // Stores the currently selected row
    int selectedRow = -1;

    // Current models for both tabs
    SignKeyTableModel* model0 = NULL;
    PaillierTableModel* model1 = NULL;
};

#endif // DIALOGMANAGEKEYS_H
