#ifndef LOM_LAUNCHER_PROBLEM_TEXT_H
#define LOM_LAUNCHER_PROBLEM_TEXT_H

#include "lom/launcher/disc/disc_importer.h"
#include "lom/launcher/disc/folder_verifier.h"

#include <QString>

namespace lom::launcher {
class ProblemText {
  public:
    struct Message {
        QString title;
        QString body;
        QString details;
    };

    /**
     * @brief Describe an image that cannot be used.
     * @param identification Identification result other than Supported.
     * @param catalog Releases, for the expected identities.
     */
    static Message for_image(const DiscImporter::Identification& identification, const ReleaseCatalog& catalog);

    /**
     * @brief Describe a failed installation.
     * @param result Install result other than Installed.
     * @param games_root Destination root.
     */
    static Message for_install(const DiscImporter::InstallResult& result, const QString& games_root);

    /**
     * @brief Describe a folder that cannot be used.
     * @param result Verification result other than Verified.
     * @param folder Selected folder.
     * @param previously_verified Whether this folder was the active installation.
     */
    static Message for_folder(const FolderVerifier::Result& result, const QString& folder, bool previously_verified);
};

}

#endif
