/*=============================================================================

Registered file extensions

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef FILEEXTENSION_H
#define FILEEXTENSION_H

#include <string>
#include <QString>

// ==========================================================================

namespace FileExtension
{
    // text files
    const std::string TXT_EXTENSION = std::string(".txt");
    const QString TXT_FILTER = QString("Text files(*.txt)");

    // public key files
    const std::string BPK_EXTENSION = std::string(".bpk");
    const QString BPK_FILTER = QString("Public key files(*.bpk)");

    // key pair files
    const std::string BSK_EXTENSION = std::string(".bsk");
    const QString BSK_FILTER = QString("Key pair files(*.bsk)");
}

#endif
