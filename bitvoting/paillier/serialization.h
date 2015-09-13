/*=============================================================================

Provides serialization method implementations for all missing paillier
structures needed for sending them over the network or hashing.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef PAILLIER_SERIALIZATION_H
#define PAILLIER_SERIALIZATION_H

#include "paillier.h"

#include <set>
#include <vector>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/split_free.hpp>

// ==========================================================================

// see GMP docs for the +2

namespace boost {
namespace serialization {
    template<class Archive>
    void save(Archive& a, const paillier_ciphertext_proof_t& t, unsigned int)
    {
        char hex1[mpz_sizeinbase(t.c, 16) + 2];
        mpz_get_str(hex1, 16, t.c);
        std::string str1(hex1);

        char hex2[mpz_sizeinbase(t.e, 16) + 2];
        mpz_get_str(hex2, 16, t.e);
        std::string str2(hex2);

        char hex3[mpz_sizeinbase(t.e1, 16) + 2];
        mpz_get_str(hex3, 16, t.e1);
        std::string str3(hex3);

        char hex4[mpz_sizeinbase(t.e2, 16) + 2];
        mpz_get_str(hex4, 16, t.e2);
        std::string str4(hex4);

        char hex5[mpz_sizeinbase(t.v1, 16) + 2];
        mpz_get_str(hex5, 16, t.v1);
        std::string str5(hex5);

        char hex6[mpz_sizeinbase(t.v2, 16) + 2];
        mpz_get_str(hex6, 16, t.v2);
        std::string str6(hex6);

        a & str1;
        a & str2;
        a & str3;
        a & str4;
        a & str5;
        a & str6;
    }

    // ----------------------------------------------------------------

    template<class Archive>
    void load(Archive& a, paillier_ciphertext_proof_t& t, unsigned int)
    {
        std::string str1;
        std::string str2;
        std::string str3;
        std::string str4;
        std::string str5;
        std::string str6;

        a & str1;
        a & str2;
        a & str3;
        a & str4;
        a & str5;
        a & str6;

        const char* hex1 = str1.c_str();
        mpz_init_set_str(t.c, hex1, 16);

        const char* hex2 = str2.c_str();
        mpz_init_set_str(t.e, hex2, 16);

        const char* hex3 = str3.c_str();
        mpz_init_set_str(t.e1, hex3, 16);

        const char* hex4 = str4.c_str();
        mpz_init_set_str(t.e2, hex4, 16);

        const char* hex5 = str5.c_str();
        mpz_init_set_str(t.v1, hex5, 16);

        const char* hex6 = str6.c_str();
        mpz_init_set_str(t.v2, hex6, 16);
    }

    // ================================================================

    template<class Archive>
    void save(Archive& a, const paillier_partialdecryption_proof_t& t, unsigned int)
    {
        char hex1[mpz_sizeinbase(t.decryption, 16) + 2];
        mpz_get_str(hex1, 16, t.decryption);
        std::string str1(hex1);

        char hex2[mpz_sizeinbase(t.c4, 16) + 2];
        mpz_get_str(hex2, 16, t.c4);
        std::string str2(hex2);

        char hex3[mpz_sizeinbase(t.ci2, 16) + 2];
        mpz_get_str(hex3, 16, t.ci2);
        std::string str3(hex3);

        char hex4[mpz_sizeinbase(t.e, 16) + 2];
        mpz_get_str(hex4, 16, t.e);
        std::string str4(hex4);

        char hex5[mpz_sizeinbase(t.z, 16) + 2];
        mpz_get_str(hex5, 16, t.z);
        std::string str5(hex5);

        a & t.id;
        a & str1;
        a & str2;
        a & str3;
        a & str4;
        a & str5;
    }

    // ----------------------------------------------------------------

    template<class Archive>
    void load(Archive& a, paillier_partialdecryption_proof_t& t, unsigned int)
    {
        std::string str1;
        std::string str2;
        std::string str3;
        std::string str4;
        std::string str5;

        a & t.id;
        a & str1;
        a & str2;
        a & str3;
        a & str4;
        a & str5;

        const char* hex1 = str1.c_str();
        mpz_init_set_str(t.decryption, hex1, 16);

        const char* hex2 = str2.c_str();
        mpz_init_set_str(t.c4, hex2, 16);

        const char* hex3 = str3.c_str();
        mpz_init_set_str(t.ci2, hex3, 16);

        const char* hex4 = str4.c_str();
        mpz_init_set_str(t.e, hex4, 16);

        const char* hex5 = str5.c_str();
        mpz_init_set_str(t.z, hex5, 16);
    }
}
}

BOOST_SERIALIZATION_SPLIT_FREE(paillier_ciphertext_proof_t)
BOOST_SERIALIZATION_SPLIT_FREE(paillier_partialdecryption_proof_t)

#endif // PAILLIER_SERIALIZATION_H
