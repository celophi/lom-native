#include "pages/problem_text.h"

#include "lom/launcher/disc/disc_image.h"
#include "window/ui_text.h"

#include <QLocale>

namespace lom::launcher {

namespace {
constexpr std::size_t max_listed_paths = 40;

/**
 * @brief Describe an image layout for people.
 * @param layout Layout identifier.
 * @return Description.
 */
QString describe_layout(const std::string& layout) {
    if (layout == DiscImage::raw_single_track_layout) {
        return QStringLiteral("raw 2352-byte sectors (BIN)");
    }

    if (layout == DiscImage::cooked_layout) {
        return QStringLiteral("2048-byte sectors (ISO)");
    }

    return QStringLiteral("unknown");
}

/**
 * @brief Append a titled list of paths.
 * @param out Details text.
 * @param title List title.
 * @param paths Paths.
 */
void append_list(QString& out, const char* title, const std::vector<std::string>& paths) {
    if (paths.empty()) {
        return;
    }

    out += QStringLiteral("%1 (%2):\n").arg(QString::fromLatin1(title)).arg(paths.size());

    for (std::size_t i = 0; i < paths.size() && i < max_listed_paths; i++) {
        out += QStringLiteral("  ") + QString::fromStdString(paths[i]) + QLatin1Char('\n');
    }

    if (paths.size() > max_listed_paths) {
        out += QStringLiteral("  ...\n");
    }
}

/**
 * @brief Build details listing an image's identity and the accepted ones.
 * @param identification Identification.
 * @param catalog Releases.
 * @return Details text.
 */
QString image_details(const DiscImporter::Identification& identification, const ReleaseCatalog& catalog) {
    QString out = QStringLiteral("File: %1\n").arg(QString::fromStdString(identification.selected.u8string()));

    if (!identification.layout.empty()) {
        out += QStringLiteral("Detected form: %1\n").arg(describe_layout(identification.layout));
    }

    if (!identification.sha1.empty()) {
        out += QStringLiteral("Size: %1 bytes\nComputed SHA-1: %2\n").arg(identification.bytes).arg(QString::fromStdString(identification.sha1));
    }

    if (identification.recognized != nullptr) {
        out += QStringLiteral("Disc serial seen: %1 (diagnostic only; it does not change the result)\n")
                   .arg(QString::fromStdString(identification.recognized->serial));
    }

    if (identification.layout == DiscImage::cooked_layout) {
        out += QStringLiteral("\nISO images are accepted when every file matches the trusted file list of a supported edition.\n");
        append_list(out, "Files that differ from the supported edition", identification.problems);
    } else {
        out += QStringLiteral("\nAccepted raw images:\n");

        for (const ReleaseCatalog::Release& release : catalog.releases()) {
            out += QStringLiteral("  %1 (%2): %3\n")
                       .arg(QString::fromStdString(release.region), QString::fromStdString(release.serial), QString::fromStdString(release.raw_sha1));
        }
    }

    if (!identification.error.empty()) {
        out += QStringLiteral("\n%1\n").arg(QString::fromStdString(identification.error));
    }

    return out;
}

}

ProblemText::Message ProblemText::for_image(const DiscImporter::Identification& identification, const ReleaseCatalog& catalog) {
    using Status = DiscImporter::Identification::Status;
    const QString details = image_details(identification, catalog);

    switch (identification.status) {
    case Status::NoTrustedFileList:
        return {ui_text::region_title, QString(ui_text::region_body).arg(QString::fromStdString(identification.recognized->display_name)), details};

    case Status::MissingTrackFile:
        return {ui_text::missing_track_title, ui_text::missing_track_body, details};

    case Status::NotFound:
    case Status::Unreadable:
        return {ui_text::unreadable_title, ui_text::unreadable_body, details};

    case Status::Malformed:
    case Status::UnsupportedFormat:
        return {ui_text::malformed_title, ui_text::malformed_body, details};

    case Status::UnsupportedLayout:
        return {ui_text::layout_title, ui_text::layout_body, details};

    case Status::Supported:
    case Status::UnknownIdentity:
    case Status::Cancelled:
        break;
    }

    return {ui_text::unsupported_title, ui_text::unsupported_body, details};
}

ProblemText::Message ProblemText::for_install(const DiscImporter::InstallResult& result, const QString& games_root) {
    using Status = DiscImporter::InstallResult::Status;
    QString details = QString::fromStdString(result.error) + QLatin1Char('\n');
    append_list(details, "Files that did not match", result.problems);

    switch (result.status) {
    case Status::InsufficientSpace:
        return {ui_text::space_title, QString(ui_text::space_body).arg(QLocale().formattedDataSize(qint64(result.required_bytes)), games_root), details};

    case Status::DestinationUnavailable:
        return {ui_text::destination_title, QString(ui_text::destination_body).arg(games_root), details};

    case Status::SourceChanged:
        return {ui_text::source_changed_title, ui_text::source_changed_body, details};

    case Status::NoManifest:
        return {ui_text::region_title, QString(ui_text::region_body).arg(QStringLiteral("This edition")), details};

    case Status::Installed:
    case Status::Cancelled:
    case Status::ExtractionFailed:
    case Status::VerificationFailed:
        break;
    }

    return {ui_text::extraction_title, ui_text::extraction_body, details};
}

ProblemText::Message ProblemText::for_folder(const FolderVerifier::Result& result, const QString& folder, bool previously_verified) {
    using Status = FolderVerifier::Result::Status;
    QString details = QStringLiteral("Folder: %1\n").arg(folder);

    if (!result.error.empty()) {
        details += QString::fromStdString(result.error) + QLatin1Char('\n');
    }

    append_list(details, "Missing", result.missing);
    append_list(details, "Not part of the game", result.unexpected);
    append_list(details, "Different contents or size", result.mismatched);
    append_list(details, "Unreadable", result.unreadable);
    append_list(details, "Different names", result.renamed);

    switch (result.status) {
    case Status::NotFound:
        return {ui_text::folder_missing_title, QString(ui_text::folder_missing_body).arg(folder), details};

    case Status::NotAGameFolder:
        return {ui_text::not_game_folder_title, ui_text::not_game_folder_body, details};

    case Status::RegionUnavailable:
        return {ui_text::region_title, QString(ui_text::region_body).arg(result.release ? QString::fromStdString(result.release->display_name) : folder),
                details};

    case Status::LayoutUnsupported:
        return {ui_text::layout_names_title, ui_text::layout_names_body, details};

    case Status::UnsafeLink:
        return {ui_text::unsafe_title, ui_text::unsafe_body, details};

    case Status::Unreadable:
        return {ui_text::unreadable_title, ui_text::unreadable_body, details};

    case Status::ChangedDuringCheck:
        return {ui_text::changed_during_title, ui_text::changed_during_body, details};

    case Status::Verified:
    case Status::Cancelled:
    case Status::UnknownContents:
        break;
    }

    if (previously_verified) {
        return {ui_text::folder_changed_title, QString(ui_text::folder_changed_body).arg(folder), details};
    }

    return {ui_text::unsupported_title, ui_text::unsupported_body, details};
}

}
