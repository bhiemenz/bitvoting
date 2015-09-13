#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "gui/dialognewelection.h"
#include "election.h"
#include "gui/dialogmanagekeys.h"
#include "database/electiondb.h"
#include "gui/wizardvote.h"
#include "gui/dialogobjectselect.h"
#include "store.h"

#include <string>

#include <QStandardItem>
#include <QFileDialog>
#include <QMessageBox>
#include <QSortFilterProxyModel>

// ================================================================
// Helper functions

std::string keyToStr(SignKeyPair skp)
{
    return skp.second.GetID().GetHex();
}

// ----------------------------------------------------------------

std::string tallyToStr(uint256 hash)
{
    Block* block = NULL;
    BlockChainDB::getBlockByTransaction(hash, &block);

    // get time when block was mined
    QDateTime time = QDateTime::fromMSecsSinceEpoch(block->header.time);
    delete block;

    return time.toString("dd.MM.yyyy HH:mm").toStdString();
}

// ----------------------------------------------------------------

std::string blockToStr(Block* block)
{
    // get time when block was mined
    QDateTime time = QDateTime::fromMSecsSinceEpoch(block->header.time);
    return "Block from: " + time.toString("dd.MM.yyyy HH:mm").toStdString();
}

// ================================================================

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    this->ui->setupUi(this);

    // handle update UI requests
    connect(this, &MainWindow::updateElectionListSignal,
            this, &MainWindow::onUpdateElectionList);

    // initialize list
    this->onUpdateElectionList();
}

// ----------------------------------------------------------------

MainWindow::~MainWindow()
{
    delete this->model;
    delete this->ui;
}

// ====================================================================

void MainWindow::updateElectionList()
{
    // get back to GUI thread
    emit updateElectionListSignal();
}

// ----------------------------------------------------------------

void MainWindow::onUpdateElectionList()
{
    // delete old model
    delete this->ui->tbElections->model();
    delete this->model;
    this->model = NULL;

    // read all elections im involved with
    this->model = new ElectionTableModel(ElectionDB::GetAll(), this);
    QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(this->model);
    this->ui->tbElections->setModel(proxyModel);

    // handle selection changed events
    connect(this->ui->tbElections->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::ontbElectionsSelectionChanged);

    // reset buttons
    this->ui->btnVote->setEnabled(false);
    this->ui->btnTally->setEnabled(false);
    this->ui->btnResults->setEnabled(false);
}

// ----------------------------------------------------------------

void MainWindow::ontbElectionsSelectionChanged(const QModelIndex& current, const QModelIndex&)
{
    // get original index
    QSortFilterProxyModel* model = (QSortFilterProxyModel*) this->ui->tbElections->model();
    this->selectedRow = model->mapToSource(current).row();

    ElectionManager* manager = this->model->getDataAt(this->selectedRow);

    if (!manager || !manager->transaction)
        return;

    Log::i("(GUI/M) Selected Election %s", manager->transaction->getHash().ToString().c_str());

    // set buttons accordingly
    this->ui->btnVote->setEnabled(manager->amIVoter());
    this->ui->btnTally->setEnabled(manager->amICreator() && !manager->ended);
    this->ui->btnResults->setEnabled(manager->resultsAvailable());
}

// ====================================================================
// Button interactions

void MainWindow::on_btnElection_clicked()
{
    // check election keys
    std::vector<SignKeyPair> keys;
    SignKeyStore::getAllKeysOfType(Role::KEY_ELECTION, keys);

    if (keys.size() == 0)
    {
        QMessageBox::warning(this, "Attention", "No Election keys found! Please generate one!");
        return;
    }

    SignKeyPair key = keys.at(0);
    if (keys.size() > 1)
    {
        // let user select key from dialog
        if (!ObjectSelectDialog::getObject(this, "Select Key", "Select a key to create the election with:", keys, &key, &keyToStr))
            return;
    }

    // show file dialog to export partial private keys of paillier
    QMessageBox::information(this, "Attention", "Please select a destination where the private keys should be exported to!");

    QString directory = QFileDialog::getExistingDirectory(this, "Export Private Keys", QDir::currentPath());

    if (directory.isNull() || directory.isEmpty())
        return;

    // show create election dialog
    NewElectionDialog dialog(this);

    if (!dialog.exec())
        return;

    // create election from user input
    Election* election = NULL;
    paillier_partialkey_t** privateKeys = NULL;
    dialog.createElection(&election, &privateKeys);

    // send to controller
    std::string dir = directory.toStdString();
    if (!this->controller->onElectionCreated(election, key, dir, privateKeys))
    {
        Log::e("(GUI/M) Controller::onElection failed!");
        return;
    }

    QMessageBox::information(this, "Success", "Election was successfully created and transmitted!\nIt may take a while before it is accepted...");
}

// ----------------------------------------------------------------

void MainWindow::on_btnKeys_clicked()
{
    // show dialog for managing keys
    ManageKeysDialog dialog(this->controller, this);
    dialog.exec();
}

// ----------------------------------------------------------------

void MainWindow::on_btnVote_clicked()
{
    ElectionManager* manager = this->model->getDataAt(this->selectedRow);

    if (!manager || !manager->transaction)
        return;

    if (manager->ended)
    {
        QMessageBox::warning(this, "Election ended!", "You cannot vote for this election anymore as it has already been closed!");
        return;
    }

    if (!manager->amIVoter())
        return;

    // select voting signature key
    std::vector<SignKeyPair> keys;
    SignKeyStore::getAllKeysOfType(Role::KEY_VOTE, keys);

    // remove those that are not eligible for this election
    std::vector<SignKeyPair>::iterator iter;
    for (iter = keys.begin(); iter != keys.end();)
    {
        if (!manager->isVoterEligible(iter->second))
            iter = keys.erase(iter);
        else
            iter++;
    }

    // should not be called (since vote button should only enable if eligible)
    if (keys.size() == 0)
        return;

    SignKeyPair key = keys.at(0);
    if (keys.size() > 1)
    {
        // let user select key from dialog
        if (!ObjectSelectDialog::getObject(this, "Select Key", "Select a key to vote with:", keys, &key, &keyToStr))
            return;
    }

    // check if user has already voted w/ that key
    if (manager->myVotes.size() > 0 && manager->myVotes.count(key.second.GetID()))
    {
        QMessageBox::StandardButton result;
        result = QMessageBox::warning(this, "Invalidate vote!",
                                      "You have already cast a vote in the past!\nVoting again, will invalidate your first vote!",
                                       QMessageBox::Ok | QMessageBox::Cancel);

        if (result == QMessageBox::Cancel)
            return;
    }

    // call voting wizard
    WizardVote dialog(manager->transaction->election, this);

    if (!dialog.exec())
        return;

    // collect votes (also abstained ones) and send back to controller
    std::set<Ballot> votes = dialog.getVotes();
    if (!controller->onVote(manager, votes, key))
    {
        Log::e("(GUI/M) Controller::onVote failed!");
        return;
    }

    QMessageBox::information(this, "Success", "Successfully transmitted your votes!\nIt may take a while before they are accepted...");
}

// ----------------------------------------------------------------

void MainWindow::on_btnTally_clicked()
{
    ElectionManager* manager = this->model->getDataAt(this->selectedRow);

    if (!manager || !manager->transaction)
        return;

    if (manager->ended)
        return;

    if (!manager->amICreator())
        return;

    if (!manager->votesRegistered.size())
    {
        QMessageBox::warning(this, "No Votes!", "There are yet no votes cast in this election!");
        return;
    }

    // get last 5 blocks containing votes regarding this election
    Block* currentBlock = NULL;
    if (BlockChainDB::getLatestBlock(&currentBlock) != BlockChainStatus::BC_OK)
    {
        Log::e("(GUI/M) Could not retrieve latest block!");
        return;
    }

    uint256 hash = manager->transaction->getHash();

    std::vector<Block*> relevantBlocks;
    uint256 previousLastBlock = 0;
    BlockChainStatus status;
    do
    {
        bool includeBlock = false;

        // iterate over all transactions in current block
        BOOST_FOREACH(Transaction* t, currentBlock->transactions)
        {
            // ... see if i find a tx_tally for this election (from me) pointing to a block (do not go futher than that!)
            if (t->getType() == TxType::TX_VOTE)
            {
                TxVote* vote = (TxVote*) t;

                if (vote->election != hash)
                    continue;

                includeBlock = true;
            }

            // check if last tallied block was already found
            if (previousLastBlock != 0)
                continue;

            if (t->getType() != TxType::TX_TALLY)
                continue;

            TxTally* tally = (TxTally*) t;

            if (tally->election != hash)
                continue;

            previousLastBlock = tally->lastBlock;
        }

        // found relevant vote, search for previous block
        if (includeBlock)
            relevantBlocks.push_back(currentBlock);

        // get previous block (only if it was not used for another tally before)
        status = BlockChainStatus::BC_INVALID_BLOCK;
        if (previousLastBlock != currentBlock->header.hashPrevBlock)
            status = BlockChainDB::getBlock(currentBlock->header.hashPrevBlock, &currentBlock);
    }
    while(status == BlockChainStatus::BC_OK && relevantBlocks.size() < 5);

    // this should never be called
    if (relevantBlocks.size() == 0)
    {
        QMessageBox::warning(this, "No Votes!", "There are no votes that have not been tallied before!");
        return;
    }

    currentBlock = relevantBlocks.at(0);
    if (relevantBlocks.size() > 0)
    {
        // let user decide
        if (!ObjectSelectDialog::getObject(this, "Select Block", "Select the last block that should be included in this tally:", relevantBlocks, &currentBlock, blockToStr))
            return;
    }

    // ask user if election should end
    QMessageBox::StandardButton result;
    result = QMessageBox::question(this, "End Election?",
                                   "Do you want to close this election entirely?\nThen no more votes will be accepted!",
                                   QMessageBox::Yes|QMessageBox::No);

    bool ending = (result == QMessageBox::Yes);
    uint256 lastBlock = currentBlock->getHash();

    // send tally message
    if (!this->controller->onTally(manager, ending, lastBlock))
    {
        Log::e("(GUI/M) Controller::onTally failed!");
        return;
    }

    QMessageBox::information(this, "Success",
                             "Successfully transmitted the request!\nIt may take a while before enough trustees performed the tallying and thus the results are available...");
}

// ----------------------------------------------------------------

void MainWindow::on_btnResults_clicked()
{
    ElectionManager* manager = this->model->getDataAt(this->selectedRow);

    if (!manager || !manager->transaction)
        return;

    if (!manager->resultsAvailable())
        return;

    // get all available tallies
    std::vector<uint256> tallies;
    std::map<uint256, std::set<Ballot>>::iterator iter;
    for (iter = manager->results.begin(); iter != manager->results.end(); iter++)
    {
        // check if results are available for the given tally (should never fail)
        if (iter->second.size() > 0)
            tallies.push_back(iter->first);
    }

    // should never be called
    if (tallies.size() == 0)
        return;

    uint256 tally = tallies.at(0);
    if (tallies.size() > 1)
    {
        // let user decide
        if (!ObjectSelectDialog::getObject(this, "Select Tally Point", "Select a tally point:", tallies, &tally, &tallyToStr))
            return;
    }

    Log::i("Showing results for '%s' (%s)",
           manager->transaction->election->name.c_str(),
           manager->transaction->getHash().ToString().c_str());

    std::set<Ballot> ballots = manager->results[tally];

    // prepare results dialog
    std::string results;
    BOOST_FOREACH(Ballot ballot, ballots)
    {
        // obtain original question
        Question question;
        if (!manager->getQuestion(ballot.questionID, question))
            continue;

        results += question.question;
        results += ":\t" + std::to_string(ballot.answer);
        results += "\n";
    }

    // show results
    std::string caption = "Results: " + manager->transaction->election->name;
    QMessageBox::information(this, caption.c_str(), results.c_str());
}
