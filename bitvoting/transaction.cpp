#include "transaction.h"

#include <stdexcept>
#include <sstream>

#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/serialization.hpp>

#include "export.h"

BITVOTING_CLASS_EXPORT_IMPLEMENT(Signable)
BITVOTING_CLASS_EXPORT_IMPLEMENT(Transaction)
#include "transactions/vote.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxVote)
#include "transactions/election.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxElection)
#include "transactions/tally.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxTally)
#include "transactions/trustee_tally.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(TxTrusteeTally)
#include "block.h"
BITVOTING_CLASS_EXPORT_IMPLEMENT(Block)

std::string printVerifyResult(const VerifyResult result)
{
    switch(result)
    {
    case VerifyResult::VR_OK:
        return "Tx verification was successfull";
    case VerifyResult::VR_SIGN_ERROR:
        return "Signature could not be verified";
    case VerifyResult::VR_TX_MISSING:
        return "Required transaction is missing";
    case VerifyResult::VR_USER_REJECTED:
        return "Permission denied for transaction";
    case VerifyResult::VR_PK_MISMATCH:
        return "Tally was not signed from election creator";
    case VerifyResult::VR_LAST_VOTES:
        return "Votes for election are missing";
    case VerifyResult::VR_BALLOT_ERROR:
        return "Error during ballot operations";
    case VerifyResult::VR_ELEC_ERROR:
        return "Election has incomplete attributes";
    default: break;
    }

    return "VerifyResult - Unknown";
}

// ====================================================================

bool
Signable::sign(SignKeyPair keys)
{
    this->verificationKey = keys.second;
    uint256 hash = this->getHash();
    return keys.first.Sign(hash, this->signature);
}

bool
Signable::verifySignature() /*const*/
{
    uint256 hash = this->getHash();
    const std::vector<unsigned char> signature = this->signature;
    return this->verificationKey.Verify(hash, signature);
}

const uint256
Signable::getHash() /*const*/
{
    // make sure signature is not hashed!
    this->hashing = true;

    if (!this->verificationKey.IsValid())
        Log::e("(Signable) Found invalid signature!");

    // self reference
    Signable* obj = this;

    // serialize object
    std::stringstream stream;
    boost::archive::text_oarchive oa(stream);
    oa << obj;
    std::string strObj = stream.str();

    // generate hash
    SHA256_CTX ctx;
    int r1 = SHA256_Init(&ctx);
    int r2 = SHA256_Update(&ctx, strObj.c_str(), strObj.size());
    uint256 hash1;
    int r3 = SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);

    if(r1 != 1 || r2 != 1 || r3 != 1)
        throw std::runtime_error("Critical error during hash creation");

    this->hashing = false;

    return hash2;
}
