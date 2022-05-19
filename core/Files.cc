#include "core/Files.h"
#include "core/Context.h"
#include "core/FileHash.h"
#include "core/GlobalState.h"
#include <vector>

#include "absl/strings/match.h"

template class std::vector<std::shared_ptr<sorbet::core::File>>;
template class std::shared_ptr<sorbet::core::File>;
using namespace std;

namespace sorbet::core {

namespace {

constexpr auto EXTERNAL_PREFIX = "external/com_stripe_ruby_typer/"sv;

struct SigilInfo {
    LocOffsets loc;
    StrictLevel strictLevel;
};

SigilInfo strictSigilInfo(string_view source) {
    /*
     * StrictLevel::None: <none>
     * StrictLevel::False: # typed: false
     * StrictLevel::True: # typed: true
     * StrictLevel::Strict: # typed: strict
     * StrictLevel::String: # typed: strong
     * StrictLevel::Autogenerated: # typed: autogenerated
     */
    size_t start = 0;
    while (true) {
        start = source.find("typed:", start);
        if (start == string_view::npos) {
            return {LocOffsets::none(), StrictLevel::None};
        }

        auto comment_start = start;
        while (comment_start > 0) {
            --comment_start;
            auto c = source[comment_start];
            if (c == ' ') {
                continue;
            } else {
                break;
            }
        }
        if (source[comment_start] != '#') {
            ++start;
            continue;
        }

        start += 6;
        while (start < source.size() && source[start] == ' ') {
            ++start;
        }

        if (start >= source.size()) {
            return {LocOffsets::none(), StrictLevel::None};
        }
        auto end = start + 1;
        while (end < source.size() && source[end] != ' ' && source[end] != '\n') {
            ++end;
        }
        if (source[end - 1] == '\r') {
            end -= 1;
        }

        string_view suffix = source.substr(start, end - start);
        LocOffsets loc{static_cast<uint32_t>(start), static_cast<uint32_t>(end)};
        if (suffix == "ignore") {
            return {loc, StrictLevel::Ignore};
        } else if (suffix == "false") {
            return {loc, StrictLevel::False};
        } else if (suffix == "true") {
            return {loc, StrictLevel::True};
        } else if (suffix == "strict") {
            return {loc, StrictLevel::Strict};
        } else if (suffix == "strong") {
            return {loc, StrictLevel::Strong};
        } else if (suffix == "autogenerated") {
            return {loc, StrictLevel::Autogenerated};
        } else if (suffix == "__STDLIB_INTERNAL") {
            return {loc, StrictLevel::Stdlib};
        } else {
            // TODO(nelhage): We should report an error here to help catch
            // typos. This would require refactoring so this function has
            // access to GlobalState or can return errors to someone who
            // does.
        }

        start = end;
    }
}

} // namespace

StrictLevel File::fileStrictSigil(string_view source) {
    return strictSigilInfo(source).strictLevel;
}

LocOffsets File::locStrictSigil(string_view source) {
    return strictSigilInfo(source).loc;
}

CompiledLevel File::fileCompiledSigil(string_view source) {
    size_t start = 0;
    while (true) {
        start = source.find("compiled:", start);

        if (start == string_view::npos) {
            return CompiledLevel::None;
        }

        auto comment_start = start;
        while (comment_start > 0) {
            --comment_start;
            auto c = source[comment_start];
            if (c == ' ') {
                continue;
            } else {
                break;
            }
        }

        if (source[comment_start] != '#') {
            ++start;
            continue;
        }

        // skip over 'compiled:'
        start += 9;
        while (start < source.size() && source[start] == ' ') {
            ++start;
        }

        if (start >= source.size()) {
            return CompiledLevel::None;
        }

        auto end = start + 1;
        while (end < source.size() && source[end] != ' ' && source[end] != '\n') {
            ++end;
        }
        if (source[end - 1] == '\r') {
            end -= 1;
        }

        string_view suffix = source.substr(start, end - start);
        if (suffix == "false") {
            return CompiledLevel::False;
        } else if (suffix == "true") {
            return CompiledLevel::True;
        } else {
            // TODO(nelhage): We should report an error here to help catch
            // typos. This would require refactoring so this function has
            // access to GlobalState or can return errors to someone who
            // does.
        }

        start = end;
    }
}

PackagedLevel File::filePackagedSigil(string_view source) {
    size_t start = 0;
    while (true) {
        start = source.find("packaged:", start);
        if (start == string_view::npos) {
            return PackagedLevel::None;
        }

        auto comment_start = start;
        while (comment_start > 0) {
            --comment_start;
            auto c = source[comment_start];
            if (c == ' ') {
                continue;
            } else {
                break;
            }
        }

        if (source[comment_start] != '#') {
            ++start;
            continue;
        }

        start += 9;
        while (start < source.size() && source[start] == ' ') {
            ++start;
        }

        if (start >= source.size()) {
            return PackagedLevel::None;
        }

        auto end = start + 1;
        while (end < source.size() && source[end] != ' ' && source[end] != '\n') {
            ++end;
        }
        if (source[end - 1] == '\r') {
            end -= 1;
        }

        string_view suffix = source.substr(start, end - start);
        if (suffix == "false") {
            return PackagedLevel::False;
        } else if (suffix == "true") {
            return PackagedLevel::True;
        } else {
            // TODO(nelhage): We should report an error here to help catch
            // typos. This would require refactoring so this function has
            // access to GlobalState or can return errors to someone who
            // does.
        }

        start = end;
    }

    return PackagedLevel::None;
}

bool isTestPath(string_view path) {
    return absl::EndsWith(path, ".test.rb") || absl::StrContains(path, "/test/");
}

bool isPackageRBIPath(string_view path) {
    return absl::EndsWith(path, ".package.rbi");
}

bool isPackagePath(string_view path) {
    auto pos = path.rfind("/");
    if (pos != string_view::npos) {
        path = path.substr(pos + 1);
    }

    return path == "__package.rb";
}

File::Flags::Flags(string_view path)
    : cached(false), hasParseErrors(false), isPackagedTest(isTestPath(path)), isPackageRBI(isPackageRBIPath(path)),
      isPackage(isPackagePath(path)), isOpenInClient(false) {}

File::File(string &&path_, string &&source_, Type sourceType, uint32_t epoch)
    : epoch(epoch), sourceType(sourceType), flags(path_), packagedLevel{File::filePackagedSigil(source_)},
      path_(move(path_)), source_(move(source_)), originalSigil(fileStrictSigil(this->source_)),
      strictLevel(originalSigil), compiledLevel(fileCompiledSigil(this->source_)) {}

unique_ptr<File> File::deepCopy(GlobalState &gs) const {
    string sourceCopy = source_;
    string pathCopy = path_;
    auto ret = make_unique<File>(move(pathCopy), move(sourceCopy), sourceType, epoch);
    ret->lineBreaks_ = lineBreaks_;
    ret->minErrorLevel_ = minErrorLevel_;
    ret->strictLevel = strictLevel;
    return ret;
}

void File::setFileHash(unique_ptr<const FileHash> hash) {
    // If hash_ != nullptr, then the contents of hash_ and hash should be identical.
    // Avoid needlessly invalidating references to *hash_.
    if (hash_ == nullptr) {
        flags.cached = false;
        hash_ = move(hash);
    }
}

const shared_ptr<const FileHash> &File::getFileHash() const {
    return hash_;
}

FileRef::FileRef(unsigned int id) : _id(id) {}

const File &FileRef::data(const GlobalState &gs) const {
    ENFORCE(gs.files[_id]);
    ENFORCE(gs.files[_id]->sourceType != File::Type::TombStone);
    ENFORCE(gs.files[_id]->sourceType != File::Type::NotYetRead);
    return dataAllowingUnsafe(gs);
}

File &FileRef::data(GlobalState &gs) const {
    ENFORCE(gs.files[_id]);
    ENFORCE(gs.files[_id]->sourceType != File::Type::TombStone);
    ENFORCE(gs.files[_id]->sourceType != File::Type::NotYetRead);
    return dataAllowingUnsafe(gs);
}

const File &FileRef::dataAllowingUnsafe(const GlobalState &gs) const {
    ENFORCE(_id < gs.filesUsed());
    return *(gs.files[_id]);
}

File &FileRef::dataAllowingUnsafe(GlobalState &gs) const {
    ENFORCE(_id < gs.filesUsed());
    return *(gs.files[_id]);
}

string_view File::path() const {
    return this->path_;
}

string_view File::source() const {
    ENFORCE(this->sourceType != File::Type::TombStone);
    ENFORCE(this->sourceType != File::Type::NotYetRead);
    return this->source_;
}

StrictLevel File::minErrorLevel() const {
    return minErrorLevel_;
}

bool File::isPayload() const {
    return sourceType == File::Type::PayloadGeneration || sourceType == File::Type::Payload;
}

bool File::isRBI() const {
    return absl::EndsWith(path(), ".rbi");
}

bool File::isStdlib() const {
    return this->originalSigil == StrictLevel::Stdlib;
}

bool File::isPackage() const {
    return flags.isPackage;
}

void File::setIsPackage(bool isPackage) {
    this->flags.isPackage = isPackage;
}

bool File::isOpenInClient() const {
    return this->flags.isOpenInClient;
}

void File::setIsOpenInClient(bool isOpenInClient) {
    this->flags.isOpenInClient = isOpenInClient;
}

vector<int> &File::lineBreaks() const {
    ENFORCE(this->sourceType != File::Type::TombStone);
    ENFORCE(this->sourceType != File::Type::NotYetRead);
    auto ptr = atomic_load(&lineBreaks_);
    if (ptr != nullptr) {
        return *ptr;
    } else {
        auto my = make_shared<vector<int>>(findLineBreaks(this->source_));
        atomic_compare_exchange_weak(&lineBreaks_, &ptr, my);
        return lineBreaks();
    }
}

int File::lineCount() const {
    return lineBreaks().size() - 1;
}

string_view File::getLine(int i) const {
    auto &lineBreaks = this->lineBreaks();
    ENFORCE(i < lineBreaks.size());
    ENFORCE(i > 0);
    auto start = lineBreaks[i - 1] + 1;
    auto end = lineBreaks[i];
    return source().substr(start, end - start);
}

string File::censorFilePathForSnapshotTests(string_view orig) {
    string_view result = orig;
    if (absl::StartsWith(result, EXTERNAL_PREFIX)) {
        // When running tests from outside of the sorbet repo, the files have a different path in the sandbox.
        result.remove_prefix(EXTERNAL_PREFIX.size());
    }

    if (absl::StartsWith(result, URL_PREFIX)) {
        // This is so that changing RBIs doesn't mean invalidating every symbol-table exp test.
        result.remove_prefix(URL_PREFIX.size());
        if (absl::StartsWith(result, EXTERNAL_PREFIX)) {
            result.remove_prefix(EXTERNAL_PREFIX.size());
        }
    }

    if (absl::StartsWith(orig, URL_PREFIX)) {
        return fmt::format("{}{}", URL_PREFIX, result);
    } else {
        return string(result);
    }
}

bool File::isPackagedTest() const {
    return flags.isPackagedTest;
}

bool File::isPackageRBI() const {
    return flags.isPackageRBI;
}

bool File::hasParseErrors() const {
    return flags.hasParseErrors;
}

void File::setHasParseErrors(bool value) {
    flags.hasParseErrors = value;
}

bool File::cached() const {
    return flags.cached;
}

void File::setCached(bool value) {
    flags.cached = value;
}

bool File::isPackaged() const {
    switch (this->packagedLevel) {
        case PackagedLevel::False:
            return false;

        case PackagedLevel::True:
        case PackagedLevel::None:
            return true;
    }
}

} // namespace sorbet::core
