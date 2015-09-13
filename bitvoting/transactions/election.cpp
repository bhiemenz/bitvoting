#include "transactions/election.h"

#include "election.h"

VerifyResult
TxElection::verify() /*const*/
{
    if (!this->verifySignature())
            return VR_SIGN_ERROR;

    Election* e = this->election;

    // check that election has appropriate values
    bool checkAttributes = (e->encPubKey != NULL &&
            e->questions.size() > 0 &&
            e->trustees.size() > 0 &&
            e->voters.size() > 0);

    return checkAttributes ? VR_OK : VR_ELEC_ERROR;
}
