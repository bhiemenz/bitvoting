#include "dialognewelection.h"
#include "ui_dialognewelection.h"

#include "helper.h"
#include "database/paillierdb.h"
#include "gui/fileextension.h"

#include <cstdlib>
#include <istream>

#include <boost/algorithm/string.hpp>

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QRegExpValidator>

NewElectionDialog::NewElectionDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewElectionDialog)
{
    ui->setupUi(this);

    // set ending to next week
    QDateTime defaultDate = QDateTime::currentDateTime().addDays(7);
    defaultDate.setTime(QTime::fromString("23:59"));
    this->ui->dtEnding->setDateTime(defaultDate);

    // set validator for name (letters and numbers)
    QRegExpValidator *nameVali = new QRegExpValidator(QRegExp("[\\d\\w ]+"));
    this->ui->nameLineEdit->setValidator(nameVali);
}

// ----------------------------------------------------------------

NewElectionDialog::~NewElectionDialog()
{
    delete ui;
}

// ----------------------------------------------------------------

void NewElectionDialog::createElection(Election** electionOut, paillier_partialkey_t*** privateKeysOut)
{
    // --- fetch user inputs ---

    std::string name = ui->nameLineEdit->text().toStdString();
    boost::algorithm::trim(name);

    std::string desc = ui->descriptionPlaintextEdit->toPlainText().toStdString();
    boost::algorithm::trim(desc);

    std::vector<Question> questions;
    for (int i = 0; i < this->ui->listQuestions->count(); i++)
    {
        std::string current = this->ui->listQuestions->item(i)->text().toStdString();
        boost::algorithm::trim(current);
        questions.push_back(Question(current));
    }

    qint64 endingTime = ui->dtEnding->dateTime().toMSecsSinceEpoch();

    std::set<CKeyID> voters;
    for (int i = 0; i < this->ui->votersList->count(); i++)
    {
        std::string current = this->ui->votersList->item(i)->text().toStdString();
        voters.insert(CKeyID(uint160(current)));
    }

    // trustee set was already read in (before paillier key creation)

    // --- create election ---

    Election* result = new Election(questions, voters, this->trustees);
    result->name = name;
    result->description = desc;
    result->encPubKey = this->publicKey;
    result->probableEndingTime = endingTime;

    *electionOut = result;
    *privateKeysOut = this->privateKeys;
}

// ----------------------------------------------------------------

void NewElectionDialog::on_votersAddBtn_clicked()
{
    // add new fingerprint to voter list
    QListWidgetItem* item = new QListWidgetItem("New Fingerprint");
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    this->ui->votersList->addItem(item);
    this->ui->votersList->editItem(item);
}

// ----------------------------------------------------------------

void NewElectionDialog::on_trusteesAddBtn_clicked()
{
    // add new fingerprint to trustee list
    QListWidgetItem* item = new QListWidgetItem("New Fingerprint");
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    this->ui->trusteesList->addItem(item);
    this->ui->trusteesList->editItem(item);
}

// ----------------------------------------------------------------

void NewElectionDialog::on_votersRemoveBtn_clicked()
{
    // remove fingerprint from voter list
    QListWidgetItem* lwi = ui->votersList->currentItem();
    ui->votersList->removeItemWidget(lwi);
    delete lwi;
}

// ----------------------------------------------------------------

void NewElectionDialog::on_trusteesRemoveBtn_clicked()
{
    // remove fingerprint from trustee list
    QListWidgetItem* lwi = ui->trusteesList->currentItem();
    ui->trusteesList->removeItemWidget(lwi);
    delete lwi;
}

// ----------------------------------------------------------------

void NewElectionDialog::on_votersImportBtn_clicked()
{
    // import pk or text file containing fingerprints
    importFingerprintsToList(this->ui->votersList);
}

// ----------------------------------------------------------------

void NewElectionDialog::on_trusteesImportBtn_clicked()
{
    // import pk or text file containing fingerprints
    importFingerprintsToList(this->ui->trusteesList);
}

// ----------------------------------------------------------------

void NewElectionDialog::on_btnQuestionAdd_clicked()
{
    // add question to list
    QListWidgetItem* item = new QListWidgetItem("New Question");
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    this->ui->listQuestions->addItem(item);
    this->ui->listQuestions->editItem(item);
}

// ----------------------------------------------------------------

void NewElectionDialog::on_btnQuestionRemove_clicked()
{
    // remove question
    QListWidgetItem* item = this->ui->listQuestions->currentItem();
    this->ui->listQuestions->removeItemWidget(item);
    delete item;
}

// ----------------------------------------------------------------

bool NewElectionDialog::verifyInputs()
{
    // ----- verify inputs -----

    // name is mandatory and max. 25 characters
    std::string name = ui->nameLineEdit->text().toStdString();
    boost::algorithm::trim(name);
    if(name.size() == 0 || name.size() > 25)
    {
        QMessageBox::warning(this, "Attention", "Please change your name (1-25 characters)");
        return false;
    }

    // description should not be longer than 255 characters
    std::string desc = ui->descriptionPlaintextEdit->toPlainText().toStdString();
    boost::algorithm::trim(desc);
    if(desc.size() > 255)
    {
        QMessageBox::warning(this, "Attention", "Your description is too long, only 255 characters are allowed");
        return false;
    }

    // ending time should not be in the past
    qint64 endingTime = ui->dtEnding->dateTime().toMSecsSinceEpoch();
    QDateTime currentTime = QDateTime::currentDateTime();
    if(currentTime.toMSecsSinceEpoch() >= endingTime)
    {
        QMessageBox::warning(this, "Attention", "Your ending time is in the past");
        return false;
    }

    // at least one question
    if(this->ui->listQuestions->count() == 0)
    {
        QMessageBox::warning(this, "Attention", "Please write at least one question");
        return false;
    }

    // at least one voter
    if(this->ui->votersList->count() == 0)
    {
        QMessageBox::warning(this, "Attention", "Please add at least one voter");
        return false;
    }

    // check each fingerprint
    for (int i = 0; i < this->ui->votersList->count(); i++)
    {
        QString current = this->ui->votersList->item(i)->text();
        if(!checkFingerprint(current, false))
        {
            QMessageBox::warning(this, "Attention", std::string("Please rectify trustee`s fingerprint on row: " + std::to_string(i+1)).c_str());
            return false;
        }
    }

    // at least one trustee
    if(this->ui->trusteesList->count() == 0)
    {
        QMessageBox::warning(this, "Attention", "Please add at least one trustee");
        return false;
    }

    // check each fingerprint
    for (int i = 0; i < this->ui->trusteesList->count(); i++)
    {
        QString current = this->ui->trusteesList->item(i)->text();
        if(!checkFingerprint(current, false))
        {
            QMessageBox::warning(this, "Attention", std::string("Please rectify trustee`s fingerprint on row: " + std::to_string(i+1)).c_str());
            return false;
        }
    }

    return true;
}

// ----------------------------------------------------------------

void NewElectionDialog::done(int value)
{
    if (QDialog::Accepted != value)
    {
        QDialog::done(value);
        return;
    }

    // verify user inputs
    if (!this->verifyInputs())
        return;

    // fetch trustee list, because their number is required for paillier key creation
    for (int i = 0; i < this->ui->trusteesList->count(); i++)
    {
        std::string current = this->ui->trusteesList->item(i)->text().toStdString();
        this->trustees.insert(CKeyID(uint160(current)));
    }

    // generate and export paillier private keys
    this->progressDialog= new QProgressDialog("Please wait while the keys are generated...", QString(), 0, 0, this);
    this->progressDialog->setWindowModality(Qt::WindowModal);
    this->progressDialog->setWindowFlags(this->progressDialog->windowFlags() & ~Qt::WindowCloseButtonHint);
    this->progressDialog->show();

    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    this->publicKey = NULL;
    this->privateKeys = NULL;
    BackgroundWorker* progressThread = new BackgroundWorker(trustees.size(),
                                                            &this->publicKey,
                                                            &this->privateKeys);

    connect(progressThread, &BackgroundWorker::ready,
            this, &NewElectionDialog::onKeyCreationFinished);
    connect(progressThread, &BackgroundWorker::finished,
            progressThread, &QObject::deleteLater);

    progressThread->start();

}

// ----------------------------------------------------------------

void NewElectionDialog::onKeyCreationFinished()
{
    Log::i("(GUI/NE) Finished");
    this->progressDialog->hide();

    delete this->progressDialog;
    this->progressDialog = NULL;

    QDialog::done(QDialog::Accepted);
}

// ----------------------------------------------------------------

void NewElectionDialog::on_listQuestions_itemChanged(QListWidgetItem *item)
{
    // delete inserted question, if it was empty
    std::string question = item->text().toStdString();
    boost::algorithm::trim(question);
    if(question.size() == 0) {
        int row = this->ui->listQuestions->row(item);
        this->ui->listQuestions->takeItem(row);
    }
}

// ----------------------------------------------------------------

void NewElectionDialog::on_votersList_itemChanged(QListWidgetItem *item)
{
    // delete inserted voter id, if it was empty
    QString itemText = item->text();
    if(itemText.size() == 0)
    {
        int row = this->ui->votersList->row(item);
        this->ui->votersList->takeItem(row);
        return;
    }

    // check fingerprint
    checkFingerprint(itemText, true);
}

// ----------------------------------------------------------------

void NewElectionDialog::on_trusteesList_itemChanged(QListWidgetItem *item)
{
    // delete inserted trustee id, if it was empty
    QString itemText = item->text();
    if(itemText.size() == 0)
    {
        int row = this->ui->trusteesList->row(item);
        this->ui->trusteesList->takeItem(row);
        return;
    }

    // check fingerprint
    checkFingerprint(itemText, true);
}

// ----------------------------------------------------------------

bool NewElectionDialog::checkFingerprint(const QString &fingerprint, bool showInfo)
{
    if(fingerprint.size() != 40)
    {
        // check if fingerprint has exactly 40 characters (length of valid fingerpring)

        if(showInfo)
            QMessageBox::warning(this, "Attention", "A fingerprint has exact 40 characters");
        return false;
    }
    else
    {
        // check if fingerprints only contains of letters and numbers

        if(QRegExp("[0-9a-zA-Z]+").exactMatch(fingerprint) != 1)
        {
            if(showInfo)
                QMessageBox::warning(this, "Attention", "A fingerprint contains only letters and numbers");
            return false;
        }
    }
    return true;
}

// ----------------------------------------------------------------

void NewElectionDialog::importFingerprintsToList(QListWidget *list)
{
    QMessageBox::information(this, "Attention", "You can either import public key files or text files containing key fingerprints (one fingerprint per line).");

    // filter for bpk (public key) or txt (containing fingerprints) files
    QString f1 = FileExtension::TXT_FILTER;
    QString f2 = FileExtension::BPK_FILTER;
    QString filter(f1.append(";;").append(f2));
    QString selectedFilter;

    QStringList files = QFileDialog::getOpenFileNames(this, "Choose public keys", QDir::currentPath(), filter, &selectedFilter);
    int errorCounter = 0;

    if(selectedFilter.compare(FileExtension::BPK_FILTER) == 0)
    {
        // import public key files
        for (int i = 0; i < files.count(); i++)
        {
            std::string path = files.at(i).toStdString();
            CPubKey key;

            try
            {
                Helper::LoadFromFile<CPubKey>(path, key, true);
            }
            catch(...)
            {
                errorCounter++;
                continue;
            }

            if(key.IsFullyValid())
            {
                QString label(key.GetID().ToString().c_str());
                list->addItem(label);
            }
            else
                errorCounter++;
        }
    }
    else if(selectedFilter.compare(FileExtension::TXT_FILTER) == 0)
    {
        // import txt files including fingerprints
        for (int i = 0; i < files.count(); i++)
        {
            std::string path = files.at(i).toStdString();
            std::ifstream reader(path.c_str(), std::ios_base::in);
            std::string temp;

            while(std::getline(reader, temp))
            {
                 // check fingerprint
                if(checkFingerprint(QString::fromUtf8(temp.c_str()), false))
                {
                    QString label(temp.c_str());
                    list->addItem(label);
                }
                else
                    errorCounter++;
            }
        }
    }

    if(errorCounter > 0)
        QMessageBox::warning(this, "Attention", (std::to_string(errorCounter) + std::string(" keys could not be imported.")).c_str());
}

