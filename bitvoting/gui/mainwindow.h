/*=============================================================================

Main Application Window: Showing the user a list of elections, letting him
vote, tally, see results, manage his keys and create elections.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "controller.h"
#include "electionmanager.h"
#include "gui/tablemodel.h"
#include "utils/comparison.h"

#include <cstdio>

#include <QMainWindow>
#include <QDateTime>

// ==========================================================================

namespace Ui {
class MainWindow;
}

// ==========================================================================

class ElectionTableModel : public TableModel<ElectionManager*, pt_cmp>
{
public:
    ElectionTableModel(std::set<ElectionManager*, pt_cmp> data, QObject* parent = 0)
        : TableModel(data, parent)
    {
        this->initialize();
    }

    ElectionTableModel(std::vector<ElectionManager*> data, QObject* parent = 0)
        : TableModel(data, parent)
    {
        this->initialize();
    }

protected:
    // s. TableModel
    char const* getHeader() const
    {
        return "Name;Ending;Creator;Voter;Trustee;Voted;Participation;Results";
    }

    // s. TableModel
    std::string getTooltip(ElectionManager* manager, int) const
    {
        if (!manager->transaction)
            return "";

        // show description as tooltip
        return manager->transaction->election->description;
    }

    // s. TableModel
    std::string getProperty(ElectionManager* manager, int i) const
    {
        if (!manager->transaction)
            return "";

        Election* e = manager->transaction->election;

        switch (i)
        {
        case 0:
            return e->name;
        case 1:
        {
            // parse ending time
            QDateTime ending = QDateTime::fromMSecsSinceEpoch(e->probableEndingTime);
            return ending.toString("dd.MM.yyyy HH:mm").toStdString();
        }
        case 2:
            return manager->amICreator() ? "Yes" : "No";
        case 3:
            return manager->amIVoter() ? "Yes" : "No";
        case 4:
            return manager->amITrustee() ? "Yes" : "No";
        case 5:
            return manager->alreadyVoted() ? "Yes" : "No";
        case 6:
        {
            // show participation
            int voted = manager->votesRegistered.size();
            int eligible = manager->transaction->election->voters.size();
            float percentage = (float) voted / eligible * 100.0;

            char buffer [128];
            sprintf(buffer, "%d / %d (%.0f%%)", voted, eligible, percentage);

            return buffer;
        }
        case 7:
        {
            if (manager->resultsAvailable())
                return "Yes";

            if (manager->tallies.size() == 0)
                return "No";

            // show how many trustees have already counted
            int needed = manager->transaction->election->encPubKey->threshold;
            int trusteesTallied = manager->tallies.begin()->second.size();
            float percentage = (float) trusteesTallied / needed * 100.0;

            char buffer [128];
            sprintf(buffer, "%d / %d (%.0f%%)", trusteesTallied, needed, percentage);

            return buffer;
        }
        default:
            break;
        }

        return "";
    }
};

// ==========================================================================

class Controller;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    // ----------------------------------------------------------------

    // Register controller
    inline void setController(Controller* c)
    {
        this->controller = c;
    }

    // Force the UI to update the list of elections shown
    void updateElectionList();

signals:
    void updateElectionListSignal();

private slots:
    void on_btnElection_clicked();
    void on_btnKeys_clicked();
    void on_btnVote_clicked();
    void on_btnTally_clicked();
    void on_btnResults_clicked();

    // ----------------------------------------------------------------

    void onUpdateElectionList();
    void ontbElectionsSelectionChanged(const QModelIndex&, const QModelIndex&);

private:
    Ui::MainWindow *ui;

    // Stores the controller
    Controller* controller = NULL;

    // Stores the current table model
    ElectionTableModel* model = NULL;

    // Holds the currently selected row
    int selectedRow = -1;
};

#endif // MAINWINDOW_H
