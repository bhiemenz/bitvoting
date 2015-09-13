#include "wizardvote.h"
#include "ui_wizardvote.h"
#include "gui/wizardquestionpage.h"

#include <boost/foreach.hpp>

// ================================================================

WizardVote::WizardVote(Election* election, QWidget *parent) :
    QWizard(parent),
    ui(new Ui::WizardVote)
{
    this->ui->setupUi(this);

    // create a new page for every question
    BOOST_FOREACH(Question question, election->questions)
    {
        this->addPage(new WizardQuestionPage(question, this));
    }
}

// ----------------------------------------------------------------

WizardVote::~WizardVote()
{
    delete this->ui;
}

// ----------------------------------------------------------------

std::set<Ballot>
WizardVote::getVotes() const
{
    std::set<Ballot> result;

    // gather ballots
    for (int i = 0; i < this->pageIds().size(); i++)
    {
        // get corresponding page
        QWizardPage* page = this->page(this->pageIds().at(i));
        WizardQuestionPage* questionPage = (WizardQuestionPage*) page;

        result.insert(questionPage->getBallot());
    }

    return result;
}
