// File: cpp/tools/foundation_external_golden_runner.cpp
//
// Build as a standalone C++20 executable. The runner invokes a supplied
// foundation binary as a child process and writes immutable binary artifacts.
//
// Example:
//   foundation_external_golden_runner ./prometheus_praxis_foundation ./goldens
//
// The implementation uses POSIX process and pipe APIs. A non-POSIX platform
// reports an explicit unsupported-platform error without producing artifacts.

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace prometheus_praxis::foundation::golden_runner {

struct ExternalGoldenArtifact {
    std::string command;
    std::vector<std::string> argv;
    int exit_code{};
    std::string stdout_bytes;
    std::string stderr_bytes;
    bool captured_stdout{};
    bool captured_stderr{};
    bool newline_present_stdout{};
    bool newline_present_stderr{};
    std::string fixture_identity;
    std::string binary_identity;
    std::string capture_timestamp;
};

struct ExternalGoldenRunResult {
    bool started{};
    bool exited{};
    bool captured_completely{};
    std::string error;
    ExternalGoldenArtifact artifact;
};

namespace {

bool EndsWithNewline(const std::string& bytes) {
    return !bytes.empty() && bytes.back() == '\n';
}

std::string UtcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

std::string SanitizeFixtureTag(std::string_view fixture_tag) {
    std::string filename;
    filename.reserve(fixture_tag.size());

    for (const unsigned char character : fixture_tag) {
        if ((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '-' || character == '_') {
            filename.push_back(static_cast<char>(character));
        } else {
            filename.push_back('_');
        }
    }

    return filename.empty() ? "unnamed_fixture" : filename;
}

bool IsSafeOutputDirectory(std::string_view output_directory) {
    if (output_directory.empty()) {
        return false;
    }

    const std::filesystem::path path{output_directory};
    return path.is_absolute() || path.has_parent_path();
}

void AppendUint32(std::string& output, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32U; shift += 8U) {
        output.push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

void AppendInt32(std::string& output, std::int32_t value) {
    AppendUint32(output, static_cast<std::uint32_t>(value));
}

bool ReadUint32(std::string_view bytes, std::size_t& offset, std::uint32_t& value) {
    if (bytes.size() - offset < 4U) {
        return false;
    }

    value = 0U;
    for (unsigned int shift = 0; shift < 32U; shift += 8U) {
        value |= static_cast<std::uint32_t>(
                     static_cast<unsigned char>(bytes[offset++]))
                 << shift;
    }
    return true;
}

bool ReadInt32(std::string_view bytes, std::size_t& offset, std::int32_t& value) {
    std::uint32_t raw{};
    if (!ReadUint32(bytes, offset, raw)) {
        return false;
    }
    value = static_cast<std::int32_t>(raw);
    return true;
}

bool AppendString(std::string& output, std::string_view value) {
    if (value.size() > UINT32_MAX) {
        return false;
    }

    AppendUint32(output, static_cast<std::uint32_t>(value.size()));
    output.append(value.data(), value.size());
    return true;
}

bool ReadString(std::string_view bytes, std::size_t& offset, std::string& value) {
    std::uint32_t length{};
    if (!ReadUint32(bytes, offset, length) ||
        static_cast<std::size_t>(length) > bytes.size() - offset) {
        return false;
    }

    value.assign(bytes.data() + offset, length);
    offset += length;
    return true;
}

std::string SerializeArtifact(const ExternalGoldenArtifact& artifact) {
    std::string bytes{"PPGOLDEN1", 9U};

    if (artifact.argv.size() > UINT32_MAX) {
        return {};
    }

    AppendUint32(bytes, static_cast<std::uint32_t>(artifact.argv.size()));
    AppendInt32(bytes, artifact.exit_code);
    bytes.push_back(artifact.captured_stdout ? '\1' : '\0');
    bytes.push_back(artifact.captured_stderr ? '\1' : '\0');
    bytes.push_back(artifact.newline_present_stdout ? '\1' : '\0');
    bytes.push_back(artifact.newline_present_stderr ? '\1' : '\0');

    if (!AppendString(bytes, artifact.command) ||
        !AppendString(bytes, artifact.fixture_identity) ||
        !AppendString(bytes, artifact.binary_identity) ||
        !AppendString(bytes, artifact.capture_timestamp)) {
        return {};
    }

    for (const std::string& argument : artifact.argv) {
        if (!AppendString(bytes, argument)) {
            return {};
        }
    }

    if (!AppendString(bytes, artifact.stdout_bytes) ||
        !AppendString(bytes, artifact.stderr_bytes)) {
        return {};
    }

    return bytes;
}

std::optional<ExternalGoldenArtifact> DeserializeArtifact(std::string_view bytes) {
    if (bytes.size() < 9U || bytes.substr(0U, 9U) != "PPGOLDEN1") {
        return std::nullopt;
    }

    std::size_t offset = 9U;
    std::uint32_t argv_size{};
    std::int32_t exit_code{};
    if (!ReadUint32(bytes, offset, argv_size) || !ReadInt32(bytes, offset, exit_code) ||
        bytes.size() - offset < 4U) {
        return std::nullopt;
    }

    ExternalGoldenArtifact artifact;
    artifact.exit_code = exit_code;
    artifact.captured_stdout = bytes[offset++] != '\0';
    artifact.captured_stderr = bytes[offset++] != '\0';
    artifact.newline_present_stdout = bytes[offset++] != '\0';
    artifact.newline_present_stderr = bytes[offset++] != '\0';

    if (!ReadString(bytes, offset, artifact.command) ||
        !ReadString(bytes, offset, artifact.fixture_identity) ||
        !ReadString(bytes, offset, artifact.binary_identity) ||
        !ReadString(bytes, offset, artifact.capture_timestamp)) {
        return std::nullopt;
    }

    artifact.argv.reserve(argv_size);
    for (std::uint32_t index = 0U; index < argv_size; ++index) {
        std::string argument;
        if (!ReadString(bytes, offset, argument)) {
            return std::nullopt;
        }
        artifact.argv.push_back(std::move(argument));
    }

    if (!ReadString(bytes, offset, artifact.stdout_bytes) ||
        !ReadString(bytes, offset, artifact.stderr_bytes) ||
        offset != bytes.size()) {
        return std::nullopt;
    }

    return artifact;
}

#if defined(__unix__) || defined(__APPLE__)

bool ReadPipeFully(int descriptor, std::string& output, std::string& error) {
    std::array<char, 4096U> buffer{};
    for (;;) {
        const ssize_t count = read(descriptor, buffer.data(), buffer.size());
        if (count > 0) {
            output.append(buffer.data(), static_cast<std::size_t>(count));
            continue;
        }
        if (count == 0) {
            return true;
        }
        if (errno == EINTR) {
            continue;
        }
        error = std::string("pipe read failed: ") + std::strerror(errno);
        return false;
    }
}

#endif

}  // namespace

ExternalGoldenRunResult RunLegacyGoldenFixture(
    std::string_view executable_path,
    std::string_view command,
    std::string_view fixture_tag) {
    ExternalGoldenRunResult result;
    result.artifact.command = std::string(command);
    result.artifact.argv = {std::string(executable_path), std::string(command)};
    result.artifact.fixture_identity = std::string(fixture_tag);
    result.artifact.binary_identity = std::string(executable_path);
    result.artifact.capture_timestamp = UtcTimestamp();

#if defined(__unix__) || defined(__APPLE__)
    if (executable_path.empty() || !std::filesystem::exists(executable_path)) {
        result.error = "executable path does not exist";
        return result;
    }

    int stdout_pipe[2]{-1, -1};
    int stderr_pipe[2]{-1, -1};
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        if (stdout_pipe[0] >= 0) {
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
        }
        if (stderr_pipe[0] >= 0) {
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
        }
        result.error = std::string("pipe creation failed: ") + std::strerror(errno);
        return result;
    }

    const pid_t child = fork();
    if (child < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        result.error = std::string("process creation failed: ") + std::strerror(errno);
        return result;
    }

    if (child == 0) {
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

        char* arguments[] = {
            const_cast<char*>(result.artifact.argv[0].c_str()),
            const_cast<char*>(result.artifact.argv[1].c_str()),
            nullptr};
        execv(result.artifact.argv[0].c_str(), arguments);
        _exit(127);
    }

    result.started = true;
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    std::string stdout_error;
    std::string stderr_error;
    const bool stdout_complete =
        ReadPipeFully(stdout_pipe[0], result.artifact.stdout_bytes, stdout_error);
    const bool stderr_complete =
        ReadPipeFully(stderr_pipe[0], result.artifact.stderr_bytes, stderr_error);
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    int status{};
    if (waitpid(child, &status, 0) < 0) {
        result.error = std::string("waitpid failed: ") + std::strerror(errno);
        return result;
    }

    result.exited = WIFEXITED(status);
    result.artifact.exit_code = result.exited ? WEXITSTATUS(status) : 128;
    result.artifact.captured_stdout = stdout_complete;
    result.artifact.captured_stderr = stderr_complete;
    result.artifact.newline_present_stdout = EndsWithNewline(result.artifact.stdout_bytes);
    result.artifact.newline_present_stderr = EndsWithNewline(result.artifact.stderr_bytes);
    result.captured_completely =
        result.started && result.exited && stdout_complete && stderr_complete;

    if (!stdout_complete) {
        result.error = stdout_error;
    } else if (!stderr_complete) {
        result.error = stderr_error;
    } else if (!result.exited) {
        result.error = "child process did not exit normally";
    }

    return result;
#else
    static_cast<void>(executable_path);
    static_cast<void>(command);
    static_cast<void>(fixture_tag);
    result.error = "external golden capture requires POSIX process APIs";
    return result;
#endif
}

std::vector<ExternalGoldenArtifact> CaptureBaselineGoldenSet(
    std::string_view executable_path) {
    const std::array<std::pair<std::string_view, std::string_view>, 4U> fixtures{{
        {"--foundation-self-check", "foundation_self_check"},
        {"--foundation-extension-self-test", "foundation_extension_self_test"},
        {"--foundation-unknown-command", "unknown_command"},
        {"", "wrong_arity"},
    }};

    std::vector<ExternalGoldenArtifact> artifacts;
    artifacts.reserve(fixtures.size());

    for (const auto& [command, fixture_tag] : fixtures) {
        const ExternalGoldenRunResult result =
            RunLegacyGoldenFixture(executable_path, command, fixture_tag);
        if (result.captured_completely) {
            artifacts.push_back(result.artifact);
        }
    }

    return artifacts;
}

bool PersistGoldenArtifact(
    const ExternalGoldenArtifact& artifact,
    std::string_view output_directory) {
    if (!IsSafeOutputDirectory(output_directory) || artifact.fixture_identity.empty()) {
        return false;
    }

    const std::string bytes = SerializeArtifact(artifact);
    if (bytes.empty()) {
        return false;
    }

    std::error_code error;
    const std::filesystem::path directory{output_directory};
    std::filesystem::create_directories(directory, error);
    if (error) {
        return false;
    }

    const std::filesystem::path destination =
        directory / (SanitizeFixtureTag(artifact.fixture_identity) + ".ppgolden");
    if (std::filesystem::exists(destination, error) || error) {
        return false;
    }

    const std::filesystem::path temporary =
        destination.string() + ".new";
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }

    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) {
        std::filesystem::remove(temporary, error);
        return false;
    }

    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }

    return true;
}

std::optional<ExternalGoldenArtifact> LoadGoldenArtifact(
    std::string_view output_directory,
    std::string_view command) {
    if (!IsSafeOutputDirectory(output_directory)) {
        return std::nullopt;
    }

    std::error_code error;
    const std::filesystem::path directory{output_directory};
    if (!std::filesystem::is_directory(directory, error) || error) {
        return std::nullopt;
    }

    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(directory, error)) {
        if (error || !entry.is_regular_file()) {
            continue;
        }

        std::ifstream stream(entry.path(), std::ios::binary);
        std::string bytes{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        const std::optional<ExternalGoldenArtifact> artifact =
            DeserializeArtifact(bytes);
        if (artifact.has_value() && artifact->command == command) {
            return artifact;
        }
    }

    return std::nullopt;
}

bool RawBytesEqual(std::string_view left, std::string_view right) {
    return left == right;
}

std::string DescribeExternalGoldenCapture(
    const ExternalGoldenRunResult& result) {
    std::ostringstream description;
    description << "fixture=" << result.artifact.fixture_identity
                << "; command=" << result.artifact.command
                << "; started=" << (result.started ? "true" : "false")
                << "; exited=" << (result.exited ? "true" : "false")
                << "; complete=" << (result.captured_completely ? "true" : "false")
                << "; exit_code=" << result.artifact.exit_code
                << "; stdout_bytes=" << result.artifact.stdout_bytes.size()
                << "; stderr_bytes=" << result.artifact.stderr_bytes.size()
                << "; stdout_newline="
                << (result.artifact.newline_present_stdout ? "true" : "false")
                << "; stderr_newline="
                << (result.artifact.newline_present_stderr ? "true" : "false");

    if (!result.error.empty()) {
        description << "; error=" << result.error;
    }

    return description.str();
}

}  // namespace prometheus_praxis::foundation::golden_runner

int main(int argc, char* argv[]) {
    using prometheus_praxis::foundation::golden_runner::CaptureBaselineGoldenSet;
    using prometheus_praxis::foundation::golden_runner::PersistGoldenArtifact;

    if (argc != 3) {
        std::cerr << "usage: foundation_external_golden_runner "
                     "<foundation-executable> <safe-output-directory>\n";
        return 64;
    }

    const std::vector<
        prometheus_praxis::foundation::golden_runner::ExternalGoldenArtifact>
        artifacts = CaptureBaselineGoldenSet(argv[1]);

    bool persisted_all = artifacts.size() == 4U;
    for (const auto& artifact : artifacts) {
        persisted_all = PersistGoldenArtifact(artifact, argv[2]) && persisted_all;
    }

    return persisted_all ? 0 : 2;
}
