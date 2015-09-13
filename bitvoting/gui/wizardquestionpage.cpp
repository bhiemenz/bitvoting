#include "wizardquestionpage.h"
#include "ui_wizardquestionpage.h"

#include <string>

#include <boost/foreach.hpp>

#include <QMessageBox>

// ================================================================

WizardQuestionPage::WizardQuestionPage(Question& question, QWidget *parent):
    QWizardPage(parent),
    ui(new Ui::WizardQuestionPage),
    question(question.id)
{
    this->ui->setupUi(this);

    Log::i("(GUI/V) Got Question: %s (%s)", question.question.c_str(), question.id.ToString().c_str());

    // set question label
    this->ui->lbQuestion->setText(QString(question.question.c_str()));

    // fill dropdown choices
    this->ui->cbAnswer->addItem("-- No Answer Selected --");
    BOOST_FOREACH(std::string answer, question.answers)
    {
        this->ui->cbAnswer->addItem(QString(answer.c_str()));
    }
}
// ----------------------------------------------------------------

WizardQuestionPage::~WizardQuestionPage()
{
    delete this->ui;
}

// ----------------------------------------------------------------

bool
WizardQuestionPage::validatePage()
{
    if (this->ui->cbAnswer->currentIndex() > 0)
        return true;

    // warn user about abstaining from the vote
    QMessageBox::StandardButton result;
    result = QMessageBox::question(this, "Warning!",
                                   "Are you sure you want to abstain from this question?",
                                   QMessageBox::Yes|QMessageBox::No);

    if (result == QMessageBox::No)
        return false;

    return true;
}

// ----------------------------------------------------------------

Ballot
WizardQuestionPage::getBallot() const
{
    // prepare ballot
    Ballot result;
    result.questionID = this->question;
    result.answer = this->ui->cbAnswer->currentIndex() - 1;

    return result;
}
