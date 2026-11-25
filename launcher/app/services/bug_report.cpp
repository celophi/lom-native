#include "services/bug_report.h"

#include "lom/launcher/common/zip_writer.h"
#include "window/app_paths.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QTimeZone>
#include <QUrlQuery>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace lom::launcher {

namespace {

constexpr char run_prefix[] = "game-";
constexpr const char* run_suffixes[] = {"-system.txt", "-crash.txt", ".log", ".dmp"};
constexpr char system_suffix[] = "-system.txt";
constexpr char zip_prefix[] = "lom-native-bug-";
constexpr char zip_suffix[] = ".zip";
constexpr char zip_time_format[] = "yyyyMMdd-HHmmss";
constexpr std::uintmax_t launcher_log_tail = 512 * 1024;
constexpr char session_key[] = "session: ";
constexpr char os_key[] = "os: ";
constexpr char gpu_key[] = "device: ";
constexpr const char* text_extensions[] = {".log", ".txt", ".json", ".ini"};
constexpr char session_end_file[] = "end";
constexpr int compression_level = 9;
constexpr int qcompress_prefix_bytes = 6;
constexpr int qcompress_trailer_bytes = 4;
constexpr char issue_template[] = "bug_report.yml";

/**
 * @brief Check how a string ends.
 * @param text Text.
 * @param suffix Ending.
 * @return True when text ends with suffix.
 */
bool ends_with(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/**
 * @brief Read a whole file, or its last bytes.
 * @param path File.
 * @param tail Read at most this many bytes from the end; 0 reads everything.
 * @return Contents, or nothing if it can't be read.
 */
std::optional<std::string> read_file(const std::filesystem::path& path, std::uintmax_t tail = 0) {
    std::ifstream in(path, std::ios::binary);

    if (!in) {
        return std::nullopt;
    }

    if (tail > 0) {
        in.seekg(0, std::ios::end);
        const std::streamoff size = in.tellg();

        if (size > std::streamoff(tail)) {
            in.seekg(size - std::streamoff(tail));
        } else {
            in.seekg(0);
        }
    }

    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

/**
 * @brief Whether a file is text the home folder can be taken out of.
 * @param name File name.
 * @return True for logs, reports, settings and the session's end reason.
 */
bool is_text(const std::string& name) {
    if (name == session_end_file) {
        return true;
    }

    return std::any_of(std::begin(text_extensions), std::end(text_extensions), [&](const char* extension) {
        return ends_with(name, extension);
    });
}

/**
 * @brief Replace the home folder with "~", in both slash styles.
 * @param text File contents.
 * @param home Home folder.
 * @return Contents without it.
 */
std::string redact(const std::string& text, const QString& home) {
    if (home.isEmpty()) {
        return text;
    }

    QString result = QString::fromUtf8(text.data(), qsizetype(text.size()));
    result.replace(QDir::fromNativeSeparators(home), QStringLiteral("~"), Qt::CaseSensitive);
    result.replace(QDir::toNativeSeparators(home), QStringLiteral("~"), Qt::CaseSensitive);
    const QByteArray bytes = result.toUtf8();
    return std::string(bytes.constData(), std::size_t(bytes.size()));
}

/**
 * @brief Raw deflate through Qt's zlib.
 * @param data Bytes.
 * @return Deflated bytes, or nothing if zlib failed.
 */
std::optional<std::string> deflate(const std::string& data) {
    const QByteArray packed = qCompress(QByteArray::fromRawData(data.data(), qsizetype(data.size())), compression_level);

    if (packed.size() < qcompress_prefix_bytes + qcompress_trailer_bytes) {
        return std::nullopt;
    }

    return std::string(packed.constData() + qcompress_prefix_bytes, std::size_t(packed.size() - qcompress_prefix_bytes - qcompress_trailer_bytes));
}

/**
 * @brief The first line starting with key, after the key.
 * @param text Report text.
 * @param key Line start.
 * @return The rest of the line, or empty.
 */
QString keyed_line(const std::string& text, const char* key) {
    std::size_t at = 0;
    const std::string wanted(key);

    while (at < text.size()) {
        std::size_t end = text.find('\n', at);

        if (end == std::string::npos) {
            end = text.size();
        }

        if (text.compare(at, wanted.size(), wanted) == 0) {
            return QString::fromUtf8(text.data() + at + wanted.size(), qsizetype(end - at - wanted.size())).trimmed();
        }

        at = end + 1;
    }

    return QString();
}

class Bundle {
  public:
    /**
     * @brief Start a bundle.
     * @param inputs Report inputs, for the home folder and the time.
     */
    explicit Bundle(const BugReport::Inputs& inputs) : zip_(deflate), home_(inputs.home), now_(inputs.now) {
    }

    /**
     * @brief Add text or bytes under a name.
     * @param name Path inside the zip.
     * @param data Contents.
     */
    void add(const std::string& name, const std::string& data) {
        zip_.add(name, is_text(name) ? redact(data, home_) : data, now_);
    }

    /**
     * @brief Add a file if it's there.
     * @param name Path inside the zip.
     * @param path File.
     * @param tail Only its last bytes; 0 for all of it.
     */
    void add_file(const std::string& name, const std::filesystem::path& path, std::uintmax_t tail = 0) {
        if (const std::optional<std::string> data = read_file(path, tail)) {
            add(name, *data);
        }
    }

    /**
     * @brief Add every file under a folder.
     * @param prefix Folder inside the zip, ending in "/".
     * @param folder Folder.
     */
    void add_folder(const std::string& prefix, const std::filesystem::path& folder) {
        std::error_code code;
        std::vector<std::filesystem::path> files;

        for (auto it = std::filesystem::recursive_directory_iterator(folder, code); !code && it != std::filesystem::recursive_directory_iterator();
             it.increment(code)) {
            if (it->is_regular_file(code)) {
                files.push_back(it->path());
            }
        }

        std::sort(files.begin(), files.end());

        for (const std::filesystem::path& file : files) {
            add_file(prefix + file.lexically_relative(folder).generic_u8string(), file);
        }
    }

    /**
     * @brief Finish the zip.
     * @return Archive bytes.
     */
    std::string finish() const {
        return zip_.finish();
    }

  private:
    ZipWriter zip_;
    QString home_;
    std::time_t now_;
};

}

std::string BugReport::run_of(const std::string& file_name) {
    if (file_name.rfind(run_prefix, 0) != 0) {
        return std::string();
    }

    for (const char* suffix : run_suffixes) {
        if (ends_with(file_name, suffix)) {
            return file_name.substr(0, file_name.size() - std::string(suffix).size());
        }
    }

    return std::string();
}

std::optional<std::filesystem::path> BugReport::latest_run(const std::filesystem::path& logs_directory) {
    std::string newest;
    std::error_code code;

    for (const auto& entry : std::filesystem::directory_iterator(logs_directory, code)) {
        const std::string run = run_of(entry.path().filename().u8string());

        if (!run.empty() && run > newest) {
            newest = run;
        }
    }

    if (newest.empty()) {
        return std::nullopt;
    }

    return logs_directory / std::filesystem::u8path(newest);
}

std::optional<BugReport::Result> BugReport::build(const Inputs& inputs, QString* error) {
    Bundle bundle(inputs);
    const std::filesystem::path logs = inputs.run_prefix.parent_path();
    const std::string run = inputs.run_prefix.filename().u8string();
    std::error_code code;

    bundle.add("report.txt", inputs.summary.toStdString());

    std::vector<std::filesystem::path> run_files;

    for (const auto& entry : std::filesystem::directory_iterator(logs, code)) {
        if (run_of(entry.path().filename().u8string()) == run) {
            run_files.push_back(entry.path());
        }
    }

    std::sort(run_files.begin(), run_files.end());

    for (const std::filesystem::path& file : run_files) {
        bundle.add_file("game/" + file.filename().u8string(), file);
    }

    bundle.add_file("launcher/launcher.log", inputs.launcher_log, launcher_log_tail);
    bundle.add_file("launcher/launcher.json", inputs.launcher_settings);

    if (!inputs.save_root.empty()) {
        bundle.add_folder("cards/", inputs.save_root);
    }

    Result result;
    const std::optional<std::string> system = read_file(std::filesystem::u8path(inputs.run_prefix.u8string() + system_suffix));

    if (system) {
        result.os = keyed_line(*system, os_key);
        result.gpu = keyed_line(*system, gpu_key);
        const QString session = keyed_line(*system, session_key);
        const std::filesystem::path session_folder = AppPaths::to_path(session);

        if (!session.isEmpty() && std::filesystem::is_directory(session_folder, code)) {
            bundle.add_folder("session/", session_folder);
        }
    }

    std::filesystem::create_directories(inputs.output_directory, code);
    const QString stamp = QDateTime::fromSecsSinceEpoch(inputs.now, QTimeZone::UTC).toString(QString::fromLatin1(zip_time_format));
    result.zip = inputs.output_directory / std::filesystem::u8path(zip_prefix + stamp.toStdString() + zip_suffix);
    const std::string archive = bundle.finish();
    std::ofstream out(result.zip, std::ios::binary | std::ios::trunc);

    if (!out || !out.write(archive.data(), std::streamsize(archive.size())) || !(out.close(), out)) {
        *error = QString::fromStdString(result.zip.u8string());
        return std::nullopt;
    }

    result.size = archive.size();
    return result;
}

QUrl BugReport::issue_url(const QString& repository, const QString& version, const Result& result) {
    QUrl url(QStringLiteral("https://github.com/%1/issues/new").arg(repository));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("template"), QString::fromLatin1(issue_template));
    query.addQueryItem(QStringLiteral("version"), version);

    if (!result.os.isEmpty()) {
        query.addQueryItem(QStringLiteral("os"), result.os);
    }

    if (!result.gpu.isEmpty()) {
        query.addQueryItem(QStringLiteral("gpu"), result.gpu);
    }

    url.setQuery(query);
    return url;
}

}
