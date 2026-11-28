#include "files_provider_fake.h"

#include <chrono>
#include <sstream>

namespace lom::launcher::test {

namespace {

constexpr char root_key[] = "/";

/**
 * @brief The error for a path that isn't there.
 * @return The error.
 */
std::error_code missing() {
    return std::make_error_code(std::errc::no_such_file_or_directory);
}

class CommittingBuffer : public std::stringbuf {
  public:
    /**
     * @brief A buffer that hands what's written to the fake disk when it's flushed or closed.
     * @param disk The fake.
     * @param key The file's key.
     * @param start What the file holds already, when it's added to.
     */
    CommittingBuffer(FilesProviderFake& disk, std::string key, const std::string& start)
        : std::stringbuf(start, std::ios::in | std::ios::out | std::ios::ate), disk_(disk), key_(std::move(key)) {
    }

    /**
     * @brief Hand over what's written, as closing a file does.
     */
    ~CommittingBuffer() override {
        sync();
    }

  protected:
    /**
     * @brief Hand over what's written, as flushing a file does.
     * @return 0.
     */
    int sync() override {
        disk_.commit(key_, str());
        return 0;
    }

  private:
    FilesProviderFake& disk_;
    std::string key_;
};

class CommittingStream : public std::ostream {
  public:
    /**
     * @brief A stream onto a file on the fake disk.
     * @param disk The fake.
     * @param key The file's key.
     * @param start What the file holds already, when it's added to.
     */
    CommittingStream(FilesProviderFake& disk, std::string key, const std::string& start) : std::ostream(nullptr), buffer_(disk, std::move(key), start) {
        rdbuf(&buffer_);
    }

  private:
    CommittingBuffer buffer_;
};

}

FilesProviderFake::FilesProviderFake() : clock_(std::filesystem::file_time_type::clock::now()) {
    nodes_[root_key].directory = true;
    use_files_provider(this);
}

FilesProviderFake::~FilesProviderFake() {
    use_files_provider(nullptr);
}

std::string FilesProviderFake::key(const std::filesystem::path& path) {
    std::string text = std::filesystem::absolute(path).lexically_normal().generic_u8string();

    while (text.size() > 1 && text.back() == '/') {
        text.pop_back();
    }

    return text;
}

std::string FilesProviderFake::parent_key(const std::string& key) {
    const auto slash = key.find_last_of('/');
    return slash == 0 || slash == std::string::npos ? std::string(root_key) : key.substr(0, slash);
}

bool FilesProviderFake::folder_holds_anything(const std::string& folder) const {
    const std::string prefix = folder == root_key ? folder : folder + "/";
    const auto next = nodes_.upper_bound(prefix);
    return next != nodes_.end() && next->first.compare(0, prefix.size(), prefix) == 0;
}

void FilesProviderFake::make_folders(const std::string& folder) {
    if (folder != root_key) {
        make_folders(parent_key(folder));
    }

    nodes_[folder].directory = true;
}

std::filesystem::file_time_type FilesProviderFake::tick() {
    clock_ += std::chrono::seconds(1);
    return clock_;
}

void FilesProviderFake::add_file(const std::filesystem::path& path, const std::string& bytes) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string file = key(path);
    make_folders(parent_key(file));
    Node& node = nodes_[file];
    node.directory = false;
    node.bytes = bytes;
    node.written = tick();
}

void FilesProviderFake::add_link(const std::filesystem::path& path, const std::filesystem::path& target) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string link = key(path);
    const auto found = nodes_.find(key(target));
    make_folders(parent_key(link));
    Node node = found == nodes_.end() ? Node() : found->second;
    node.link = key(target);
    node.written = tick();
    nodes_[link] = node;
}

void FilesProviderFake::add_folder(const std::filesystem::path& path) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    make_folders(key(path));
}

void FilesProviderFake::set_last_write_time(const std::filesystem::path& path, std::filesystem::file_time_type time) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto found = nodes_.find(key(path));

    if (found != nodes_.end()) {
        found->second.written = time;
    }
}

std::string FilesProviderFake::contents(const std::filesystem::path& path) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto found = nodes_.find(key(path));
    return found == nodes_.end() ? std::string() : found->second.bytes;
}

bool FilesProviderFake::has(const std::filesystem::path& path) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    return nodes_.count(key(path)) != 0;
}

void FilesProviderFake::commit(const std::string& file, const std::string& bytes) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    Node& node = nodes_[file];
    node.directory = false;
    node.bytes = bytes;
    node.written = tick();
}

bool FilesProviderFake::exists(const std::filesystem::path& path, std::error_code& code) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    code.clear();
    return nodes_.count(key(path)) != 0;
}

bool FilesProviderFake::is_directory(const std::filesystem::path& path, std::error_code& code) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    code.clear();
    const auto found = nodes_.find(key(path));
    return found != nodes_.end() && found->second.directory;
}

bool FilesProviderFake::is_regular_file(const std::filesystem::path& path, std::error_code& code) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    code.clear();
    const auto found = nodes_.find(key(path));
    return found != nodes_.end() && !found->second.directory;
}

std::uintmax_t FilesProviderFake::file_size(const std::filesystem::path& path, std::error_code& code) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto found = nodes_.find(key(path));

    if (found == nodes_.end() || found->second.directory) {
        code = found == nodes_.end() ? missing() : std::make_error_code(std::errc::is_a_directory);
        return static_cast<std::uintmax_t>(-1);
    }

    code.clear();
    return found->second.bytes.size();
}

std::filesystem::file_time_type FilesProviderFake::last_write_time(const std::filesystem::path& path, std::error_code& code) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto found = nodes_.find(key(path));

    if (found == nodes_.end()) {
        code = missing();
        return std::filesystem::file_time_type::min();
    }

    code.clear();
    return found->second.written;
}

std::vector<FileEntry> FilesProviderFake::list(const std::filesystem::path& folder, bool recursive, std::error_code& code) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string start = key(folder);
    const auto found = nodes_.find(start);
    std::vector<FileEntry> entries;

    if (found == nodes_.end() || !found->second.directory) {
        code = found == nodes_.end() ? missing() : std::make_error_code(std::errc::not_a_directory);
        return entries;
    }

    code.clear();
    const std::string prefix = start == root_key ? start : start + "/";

    for (auto it = nodes_.upper_bound(prefix); it != nodes_.end() && it->first.compare(0, prefix.size(), prefix) == 0; ++it) {
        const std::string rest = it->first.substr(prefix.size());

        if (!recursive && rest.find('/') != std::string::npos) {
            continue;
        }

        FileEntry entry;
        entry.path = std::filesystem::u8path(it->first);
        entry.directory = it->second.directory;
        entry.regular = !it->second.directory;
        entry.symlink = !it->second.link.empty();
        entry.size = entry.regular ? it->second.bytes.size() : 0;
        entries.push_back(entry);
    }

    return entries;
}

bool FilesProviderFake::create_directories(const std::filesystem::path& path, std::error_code& code) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string folder = key(path);
    code.clear();

    for (std::string at = folder;; at = parent_key(at)) {
        const auto found = nodes_.find(at);

        if (found != nodes_.end() && !found->second.directory) {
            code = std::make_error_code(std::errc::not_a_directory);
            return false;
        }

        if (at == root_key) {
            break;
        }
    }

    const bool made = nodes_.count(folder) == 0;
    make_folders(folder);
    return made;
}

bool FilesProviderFake::remove(const std::filesystem::path& path, std::error_code& code) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string target = key(path);
    const auto found = nodes_.find(target);
    code.clear();

    if (found == nodes_.end()) {
        return false;
    }

    if (found->second.directory && folder_holds_anything(target)) {
        code = std::make_error_code(std::errc::directory_not_empty);
        return false;
    }

    nodes_.erase(found);
    return true;
}

std::uintmax_t FilesProviderFake::remove_all(const std::filesystem::path& path, std::error_code& code) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string target = key(path);
    code.clear();

    if (nodes_.count(target) == 0) {
        return 0;
    }

    std::uintmax_t removed = 1;
    const std::string prefix = target + "/";

    for (auto it = nodes_.upper_bound(prefix); it != nodes_.end() && it->first.compare(0, prefix.size(), prefix) == 0;) {
        it = nodes_.erase(it);
        ++removed;
    }

    nodes_.erase(target);
    return removed;
}

void FilesProviderFake::rename(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& code) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string source = key(from), target = key(to);
    const auto found = nodes_.find(source);
    code.clear();

    if (found == nodes_.end()) {
        code = missing();
        return;
    }

    if (source == target) {
        return;
    }

    if (nodes_.count(parent_key(target)) == 0) {
        code = missing();
        return;
    }

    const auto there = nodes_.find(target);

    if (there != nodes_.end()) {
        if (there->second.directory != found->second.directory) {
            code = std::make_error_code(there->second.directory ? std::errc::is_a_directory : std::errc::not_a_directory);
            return;
        }

        if (there->second.directory && folder_holds_anything(target)) {
            code = std::make_error_code(std::errc::directory_not_empty);
            return;
        }

        nodes_.erase(there);
    }

    std::map<std::string, Node> moved;
    moved[target] = found->second;
    const std::string prefix = source + "/";

    for (auto it = nodes_.upper_bound(prefix); it != nodes_.end() && it->first.compare(0, prefix.size(), prefix) == 0;) {
        moved[target + it->first.substr(source.size())] = it->second;
        it = nodes_.erase(it);
    }

    nodes_.erase(source);
    nodes_.insert(moved.begin(), moved.end());
}

bool FilesProviderFake::copy_file(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options,
                                  std::error_code& code) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string source = key(from), target = key(to);
    const auto found = nodes_.find(source);
    code.clear();

    if (found == nodes_.end() || found->second.directory) {
        code = found == nodes_.end() ? missing() : std::make_error_code(std::errc::is_a_directory);
        return false;
    }

    if (nodes_.count(parent_key(target)) == 0) {
        code = missing();
        return false;
    }

    if (nodes_.count(target) != 0) {
        if ((options & std::filesystem::copy_options::skip_existing) != std::filesystem::copy_options::none) {
            return false;
        }

        if ((options & std::filesystem::copy_options::overwrite_existing) == std::filesystem::copy_options::none) {
            code = std::make_error_code(std::errc::file_exists);
            return false;
        }
    }

    Node copy = found->second;
    copy.written = tick();
    nodes_[target] = copy;
    return true;
}

void FilesProviderFake::copy(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options, std::error_code& code) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string source = key(from), target = key(to);
    const auto found = nodes_.find(source);
    code.clear();

    if (found == nodes_.end()) {
        code = missing();
        return;
    }

    if (!found->second.directory) {
        copy_file(from, to, options, code);
        return;
    }

    make_folders(target);
    const bool recursive = (options & std::filesystem::copy_options::recursive) != std::filesystem::copy_options::none;
    std::error_code ignored;

    for (const FileEntry& entry : list(from, false, ignored)) {
        const std::filesystem::path inside = to / entry.path.filename();

        if (entry.directory) {
            if (recursive) {
                copy(entry.path, inside, options, code);
            }
        } else {
            copy_file(entry.path, inside, options, code);
        }

        if (code) {
            return;
        }
    }
}

std::filesystem::space_info FilesProviderFake::space(const std::filesystem::path& path, std::error_code& code) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    code.clear();
    (void)path;
    return std::filesystem::space_info{free_space * 2, free_space, free_space};
}

std::filesystem::path FilesProviderFake::weakly_canonical(const std::filesystem::path& path, std::error_code& code) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    code.clear();
    const auto found = nodes_.find(key(path));
    return std::filesystem::u8path(found != nodes_.end() && !found->second.link.empty() ? found->second.link : key(path));
}

bool FilesProviderFake::equivalent(const std::filesystem::path& first, const std::filesystem::path& second, std::error_code& code) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string a = key(first), b = key(second);

    if (nodes_.count(a) == 0 || nodes_.count(b) == 0) {
        code = missing();
        return false;
    }

    code.clear();
    return a == b;
}

std::filesystem::perms FilesProviderFake::permissions_of(const std::filesystem::path& path) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto found = nodes_.find(key(path));
    return found == nodes_.end() ? std::filesystem::perms::none : found->second.permissions;
}

void FilesProviderFake::permissions(const std::filesystem::path& path, std::filesystem::perms permissions, std::error_code& code) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto found = nodes_.find(key(path));

    if (found == nodes_.end()) {
        code = missing();
        return;
    }

    code.clear();
    found->second.permissions = permissions;
}

std::unique_ptr<std::istream> FilesProviderFake::open_read(const std::filesystem::path& path, std::ios::openmode mode) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto found = nodes_.find(key(path));
    auto stream = std::make_unique<std::istringstream>(found == nodes_.end() ? std::string() : found->second.bytes, mode | std::ios::in);

    if (found == nodes_.end() || found->second.directory) {
        stream->setstate(std::ios::failbit);
    }

    return stream;
}

std::unique_ptr<std::ostream> FilesProviderFake::open_write(const std::filesystem::path& path, std::ios::openmode mode) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string file = key(path);
    const auto parent = nodes_.find(parent_key(file));
    const auto found = nodes_.find(file);

    if (parent == nodes_.end() || !parent->second.directory || (found != nodes_.end() && found->second.directory)) {
        auto failed = std::make_unique<std::ostringstream>();
        failed->setstate(std::ios::failbit);
        return failed;
    }

    const bool append = (mode & std::ios::app) != 0;
    const std::string start = append && found != nodes_.end() ? found->second.bytes : std::string();
    commit(file, start);
    return std::make_unique<CommittingStream>(*this, file, start);
}

}
