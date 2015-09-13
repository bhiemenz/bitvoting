/*=============================================================================

Dialog for creating new elections.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef NEWELECTIONDIALOG_H
#define NEWELECTIONDIALOG_H

#include "election.h"
#include "electionmanager.h"
#include "settings.h"

#include "paillier/paillier.h"

#include <QtWidgets/QDialog>
#include <QProgressDialog>
#include <QThread>
#include <QListWidgetItem>

// ==========================================================================

namespace Ui {
class NewElectionDialog;
}

// ==========================================================================

class BackgroundWorker : public QThread
{
    Q_OBJECT

    // Generate paillier key pairs
    void run() Q_DECL_OVERRIDE
    {
        Log::i("(GUI/NE) Generating keys...");

        // create homomorphic keys
        paillier_keygen(Settings::PAILLIER_BITS, this->nTrustees, this->nTrustees,
                        this->publicKey, this->privateKeys, paillier_get_rand_devurandom);

        emit ready();
    }

public:
    BackgroundWorker(int nTrustees, paillier_pubkey_t** pubKeyOut, paillier_partialkey_t*** privKeysOut) :
        QThread(),
        nTrustees(nTrustees),
        publicKey(pubKeyOut),
        privateKeys(privKeysOut) {}

signals:
    void ready();

private:
    // How many trustee keys should be generated
    int nTrustees = 0;

    // Output variables
    paillier_pubkey_t** publicKey = NULL;
    paillier_partialkey_t*** privateKeys = NULL;
};

// ==========================================================================

class NewElectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewElectionDialog(QWidget *parent = 0);
    ~NewElectionDialog();

    // ----------------------------------------------------------------

    // Create a new election from the user input
    void createElection(Election**, paillier_partialkey_t***);

    // Will be called when the dialog is about to close
    void done(int);

private slots:
    void on_votersAddBtn_clicked();
    void on_trusteesAddBtn_clicked();
    void on_votersRemoveBtn_clicked();
    void on_trusteesRemoveBtn_clicked();
    void on_btnQuestionAdd_clicked();
    void on_btnQuestionRemove_clicked();
    void on_votersImportBtn_clicked();
    void on_trusteesImportBtn_clicked();
    void onKeyCreationFinished();
    void on_listQuestions_itemChanged(QListWidgetItem *item);
    void on_votersList_itemChanged(QListWidgetItem *item);
    void on_trusteesList_itemChanged(QListWidgetItem *item);

private:
    Ui::NewElectionDialog *ui;

    // Progress Dialog which is shown during key generation
    QProgressDialog* progressDialog = NULL;

    // References to the paillier keys
    paillier_pubkey_t* publicKey = NULL;
    paillier_partialkey_t** privateKeys = NULL;

    // Set of all trustees for election
    std::set<CKeyID> trustees;


    // ----------------------------------------------------------------

    // Verifies the input of the user
    bool verifyInputs();

    // Verifies a specific fingerprint
    bool checkFingerprint(const QString &fingerprint, bool showInfo);

    // Imports fingerprints to the given list
    void importFingerprintsToList(QListWidget *list);
};

#endif // NEWELECTIONDIALOG_H
