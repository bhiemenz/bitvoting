#include "dialogmanagekeys.h"
#include "ui_dialogmanagekeys.h"

#include "helper.h"
#include "store.h"
#include "bitcoin/key.h"
#include "database/paillierdb.h"
#include "gui/dialogobjectselect.h"
#include "gui/fileextension.h"

#include <QInputDialog>
#include <QFileDialog>
#include <QClipboard>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QDialogButtonBox>

// ----------------------------------------------------------------
// Helper functions

std::string roleToStr(Role r)
{
    switch(r)
    {
    case Role::KEY_ELECTION:
        return "Election";
    case Role::KEY_MINING:
        return "Mining";
    case Role::KEY_TRUSTEE:
        return "Trustee";
    case Role::KEY_VOTE:
        return "Voting";
    default:
        break;
    }

    return "Unknown";
}

// ----------------------------------------------------------------

ManageKeysDialog::ManageKeysDialog(Controller* controller, QWidget *parent) :
    controller(controller),
    QDialog(parent),
    ui(new Ui::ManageKeysDialog)
{
    this->ui->setupUi(this);
    this->on_cbKeyRole_currentIndexChanged(0);
}

ManageKeysDialog::~ManageKeysDialog()
{
    delete this->ui;
}

// ----------------------------------------------------------------

void ManageKeysDialog::on_cbKeyRole_currentIndexChanged(int index)
{
    this->selectedRow = -1;
    this->ui->btnExport->setEnabled(false);
    delete this->ui->tbKeys->model();

    if (index == 0) // Signing Keys
    {
        this->ui->btnNew->setEnabled(true);

        std::vector<SignKeyPair> keys;
        SignKeyStore::getAllKeys(keys);

        // do not show mining keys
        std::vector<SignKeyPair>::iterator iter;
        for (iter = keys.begin(); iter != keys.end();)
        {
            if (iter->first.getRole() == Role::KEY_MINING)
                iter = keys.erase(iter);
            else
                iter++;
        }

        Log::i("(GUI/MK) Loaded %d signature keys from database", keys.size());

        delete this->model0;
        this->model0 = NULL;
        this->model0 = new SignKeyTableModel(keys, this);
        QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel(this);
        proxyModel->setSourceModel(this->model0);
        this->ui->tbKeys->setModel(proxyModel);
    }
    else // Paillier Keys
    {
        this->ui->btnNew->setEnabled(false);

        // load paillier keys
        std::vector<ElectionPrivateKey> keys = PaillierDB::GetAll();

        Log::i("(GUI/MK) Loaded %d paillier keys from database", keys.size());

        delete this->model1;
        this->model1 = NULL;
        this->model1 = new PaillierTableModel(keys, this);
        QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel(this);
        proxyModel->setSourceModel(this->model1);
        this->ui->tbKeys->setModel(proxyModel);
    }

    // handle selection changed events
    connect(this->ui->tbKeys->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &ManageKeysDialog::ontbKeysSelectionChanged);
}

// ----------------------------------------------------------------

void ManageKeysDialog::ontbKeysSelectionChanged(const QModelIndex& current, const QModelIndex&)
{
    QSortFilterProxyModel* model = (QSortFilterProxyModel*) this->ui->tbKeys->model();
    this->selectedRow = model->mapToSource(current).row();

    if (this->ui->cbKeyRole->currentIndex() != 0)
    {
        ElectionPrivateKey epk = this->model1->getDataAt(this->selectedRow);
        Log::i("(GUI/MK) Clicked on Election %s", epk.election.ToString().c_str());
        return;
    }

    this->ui->btnExport->setEnabled(true);

    // copy fingerprint to clipboard
    QClipboard *clipboard = QApplication::clipboard();
    CKeyID fingerprint = this->model0->getDataAt(this->selectedRow).second.GetID();
    clipboard->setText(fingerprint.ToString().c_str());
}

// ----------------------------------------------------------------
// Button interactions

void ManageKeysDialog::on_btnNew_clicked()
{
    if (this->ui->cbKeyRole->currentIndex() != 0)
        return;

    // --- let user chose key role ---

    std::vector<Role> list;
    list.push_back(Role::KEY_ELECTION);
    list.push_back(Role::KEY_TRUSTEE);
    list.push_back(Role::KEY_VOTE);

    Role r;
    if (!ObjectSelectDialog::getObject(this, "Select Key Role", "Role:", list, &r, &roleToStr))
        return;

    // --- generate new key ---

    SignKeyPair skp;
    SignKeyStore::genNewSignKeyPair(r, skp);

    // update list
    this->on_cbKeyRole_currentIndexChanged(this->ui->cbKeyRole->currentIndex());
}

// ----------------------------------------------------------------

void ManageKeysDialog::on_btnImport_clicked()
{
    if (this->ui->cbKeyRole->currentIndex() == 0)
    {
        // ----- import sign key pair -----
        QString file = QFileDialog::getOpenFileName(this, "Import",
                                                    QDir::currentPath(),
                                                    FileExtension::BSK_FILTER);
        if (file.isNull() || file.isEmpty())
            return;

        SignKeyPair skp;
        try
        {
            Helper::LoadFromFile(file.toStdString(), skp, true);
        }
        catch(...)
        {
            // error during loading key from disk
            QMessageBox::critical(this, "Attention", "Could not import key");
            return;
        }

        // check if imported key is valid
        if(!skp.first.IsValid() || !skp.second.IsFullyValid())
        {
            QMessageBox::critical(this, "Attention", "Key seems not to be valid.");
            return;
        }

        // check if imported key is already in list
        if(SignKeyStore::containsSignKeyPair(skp.second.GetID()))
        {
            QMessageBox::information(this, "Attention", "Key is already in list");
            return;
        }
        else
            SignKeyStore::addSignKeyPair(skp);
    }
    else
    {
        // ----- import paillier key -----
        QString file = QFileDialog::getOpenFileName(this, "Import", QDir::currentPath());

        ElectionPrivateKey epk;
        try
        {
            Helper::LoadFromFile(file.toStdString(), epk, true);
        }
        catch(...)
        {
            // error during key loading from disk
            QMessageBox::critical(this, "Attention", "Could not import key");
            return;
        }

        // check if imported paillier key information matches with own sign keys
        if (!this->controller->onNewPaillierKey(epk))
        {
            QMessageBox::critical(this, "Error", "The imported key does not fit any of your trustee keys!");
            return;
        }
        PaillierDB::Save(epk);
    }

    // inform user and update list
    QMessageBox::information(this, "Success", "Key was successfully imported!");
    this->on_cbKeyRole_currentIndexChanged(this->ui->cbKeyRole->currentIndex());
}

// ----------------------------------------------------------------

void ManageKeysDialog::on_btnExport_clicked()
{
    if (this->ui->cbKeyRole->currentIndex() != 0 || this->selectedRow < 0)
        return;

    // user can export complete key pair or public key only
    QStringList options;
    options << tr("Export complete key pair");
    options << tr("Export public key only");

    bool ok;
    QString selection = QInputDialog::getItem(this, tr("Export"),
                                         tr("What do you want to export:"),
                                              options, 0, false, &ok);

    // something is wrong with selection
    if(!ok || options.isEmpty())
        return;

    SignKeyPair skp = this->model0->getDataAt(this->selectedRow);
    QString file = QFileDialog::getSaveFileName(this, "Export", QDir::currentPath());                                               ;
    if (file.isNull() || file.isEmpty())
        return;

    try
    {
        // i = 0 -> export complete key pair
        // i = 1 -> export public key only
        int i = options.indexOf(selection);

        if(i == 0)
            Helper::SaveToFile(skp, file.toStdString().append(FileExtension::BSK_EXTENSION), true);
        else if(i == 1)
            Helper::SaveToFile(skp.second, file.toStdString().append(FileExtension::BPK_EXTENSION), true);
    }
    catch(...)
    {
        // error during key saving to disk
        QMessageBox::critical(this, "Attention", "Could not export key");
        return;
    }

    // everyting seems fine
    QMessageBox::information(this, "Success", "Key was successfully exported!");
}

// ----------------------------------------------------------------

void ManageKeysDialog::on_btnClose_clicked()
{
    QDialog::accept();
}
