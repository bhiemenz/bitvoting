/*=============================================================================

Wizard for voting in an election

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef WIZARDVOTE_H
#define WIZARDVOTE_H

#include "election.h"

#include <set>

#include <QWizard>

// ==========================================================================

namespace Ui {
class WizardVote;
}

// ==========================================================================

class WizardVote : public QWizard
{
    Q_OBJECT

public:
    explicit WizardVote(Election*, QWidget *parent = 0);
    ~WizardVote();

    // ----------------------------------------------------------------

    // Obtain all ballots for all questions answered by the user
    std::set<Ballot> getVotes() const;

private:
    Ui::WizardVote *ui;
};

#endif // WIZARDVOTE_H
