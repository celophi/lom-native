#include "services/update_checker.h"

#include "lom/launcher/common/json.h"
#include "lom/launcher/common/semantic_version.h"
#include "services/update_feed_provider.h"

namespace lom::launcher {

namespace {

constexpr char key_tag[] = "tag_name";
constexpr char key_body[] = "body";
constexpr char key_html_url[] = "html_url";
constexpr char key_draft[] = "draft";
constexpr char key_prerelease[] = "prerelease";
constexpr char key_assets[] = "assets";
constexpr char key_asset_name[] = "name";
constexpr char key_asset_url[] = "browser_download_url";

}

UpdateChecker::UpdateChecker(UpdateFeedProvider* feed, QString current_version, QString repository, QObject* parent)
    : QObject(parent), feed_(feed), current_version_(std::move(current_version)), repository_(std::move(repository)) {
}

void UpdateChecker::check() {
    if (feed_ == nullptr) {
        Result result;
        result.status = Result::Status::NotConfigured;
        emit checked(result);
        return;
    }

    if (checking_) {
        return;
    }

    checking_ = true;
    feed_->fetch_latest_release([this](const UpdateFeedProvider::Response& response) {
        checking_ = false;
        Result result;

        if (response.ok) {
            result = interpret(response.body, current_version_);
        } else {
            result.status = Result::Status::CouldNotCheck;
            result.error = feed_error(response, repository_);
        }

        emit checked(result);
    });
}

QString UpdateChecker::feed_error(const UpdateFeedProvider::Response& response, const QString& repository) {
    return response.not_found ? QStringLiteral("GitHub couldn't find the releases of %1").arg(repository) : response.error;
}

UpdateChecker::Result UpdateChecker::interpret(const QByteArray& body, const QString& current_version) {
    Result result;
    std::string error;
    const auto document = Json::parse(body.toStdString(), &error);
    const auto current = SemanticVersion::parse(current_version.toStdString());

    if (!document || !current) {
        result.error = QString::fromStdString(document ? "this build has no release version" : "the release information was malformed");
        return result;
    }

    const Json* tag = document->find(key_tag);
    const Json* draft = document->find(key_draft);
    const Json* prerelease = document->find(key_prerelease);
    const auto latest = tag == nullptr ? std::nullopt : SemanticVersion::parse(tag->as_string());

    if (!latest || (draft != nullptr && draft->as_bool(false)) || (prerelease != nullptr && prerelease->as_bool(false)) || latest->is_prerelease()) {
        result.error = "the latest release is not a usable stable version";
        return result;
    }

    result.latest_version = QString::fromStdString(latest->to_string());

    if (const Json* notes = document->find(key_body)) {
        result.release_notes = QString::fromStdString(notes->as_string());
    }

    if (const Json* page = document->find(key_html_url)) {
        const QUrl url(QString::fromStdString(page->as_string()));

        if (url.scheme() == QLatin1String("https")) {
            result.release_page = url;
        }
    }

    if (const Json* assets = document->find(key_assets); assets != nullptr && assets->type() == Json::Type::Array) {
        for (const Json& asset : assets->as_array()) {
            const Json* name = asset.find(key_asset_name);
            const Json* link = asset.find(key_asset_url);
            const QUrl url = link == nullptr ? QUrl() : QUrl(QString::fromStdString(link->as_string()));

            if (name != nullptr && !name->as_string().empty() && url.scheme() == QLatin1String("https")) {
                result.assets.insert(QString::fromStdString(name->as_string()), url);
            }
        }
    }

    result.status = latest->compare(*current) > 0 ? Result::Status::UpdateAvailable : Result::Status::UpToDate;
    return result;
}

}
