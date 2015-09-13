/*=============================================================================

Represents a single question in a wizard for voting

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef WIZARDQUESTIONPAGE_H
#define WIZARDQUESTIONPAGE_H

#include "election.h"

#include <QWizardPage>

// ==========================================================================

namespace Ui {
class WizardQuestionPage;
}

// ==========================================================================

class WizardQuestionPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit WizardQuestionPage(Question&, QWidget *parent = 0);
    ~WizardQuestionPage();
    // ----------------------------------------------------------------

    // Called before the next page will be shown
    bool validatePage();

    // Obtain the ballot for the shown question
    Ballot getBallot() const;

private:
    Ui::WizardQuestionPage *ui;

    // Stores the ID of the shown question
    uint160 question;
};

#endif // WIZARDQUESTIONPAGE_H
