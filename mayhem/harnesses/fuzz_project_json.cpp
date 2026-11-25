// Fuzz the editor project (map) JSON reader: community maps are downloaded from game servers
// and the map catalogue, then parsed with editor_project_readwrite::read_project_json.
// The official content (resources + map) is built once and only read afterwards.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <filesystem>
#include <system_error>
#include <memory>
#include <string>

#include "application/setups/editor/packaged_official_content.h"
#include "application/setups/editor/project/editor_project.h"
#include "application/setups/editor/project/editor_project_readwrite.h"
#include "augs/readwrite/json_readwrite_errors.h"
#include "augs/readwrite/stream_read_error.h"

static const packaged_official_content& officials() {
	static const auto content = std::make_unique<packaged_official_content>();
	return *content;
}

/*
	The folder the downloaded project lives in. It must exist: path sanitization canonicalizes it,
	and read_project_json reads files the project names from inside it (e.g. GIF frame metadata).
	A dedicated empty folder under $TMPDIR, created once and never written to, so what an input
	reads depends only on the input (not on corpus/artifact files in the fuzzer's working directory).
*/
static augs::path_type project_dir;

static augs::path_type make_empty_project_dir() {
	std::error_code ec;
	const char* tmp = std::getenv("TMPDIR");
	const auto base = augs::path_type(tmp && *tmp ? tmp : "/tmp");
	const auto dir = base / "hypersomnia_fuzz_project_json";

	std::filesystem::create_directories(dir, ec);

	if (!ec && std::filesystem::is_directory(dir, ec)) {
		return dir;
	}

	const auto cwd = std::filesystem::current_path(ec);
	return ec ? augs::path_type(".") : augs::path_type(cwd);
}

/*
	The hypersomnia/ folder the official content (content/) is read from, like the game, which runs
	from it. In the image it is /mayhem/hypersomnia; next to the binary is tried first so a copy of
	the image elsewhere works too. Mayhem may start the binary through a loader or from a copy, where
	/proc/self/exe does not point into /mayhem, so the absolute path is always tried as well.
*/
static augs::path_type find_hypersomnia_dir() {
	std::error_code ec;
	std::vector<augs::path_type> candidates;

	const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);

	if (!ec) {
		candidates.push_back(exe.parent_path() / "hypersomnia");
	}

	candidates.push_back("/mayhem/hypersomnia");

	for (const auto& c : candidates) {
		if (std::filesystem::is_directory(c / "content", ec)) {
			return c;
		}
	}

	return {};
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	std::error_code ec;
	const auto start_dir = std::filesystem::current_path(ec);

	project_dir = make_empty_project_dir();

	const auto hs_dir = find_hypersomnia_dir();

	if (!hs_dir.empty()) {
		std::filesystem::current_path(hs_dir, ec);
	}

	(void)officials();

	/*
		Go back: libFuzzer resolves the corpus directories and writes artifacts relative to the
		working directory it was started in. The content is only read, never written.
	*/
	if (!start_dir.empty()) {
		std::filesystem::current_path(start_dir, ec);
	}

	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	if (size == 0) {
		return -1;
	}

	const auto& off = officials();
	/* First byte selects the reading mode, the rest is the project JSON. */
	const uint8_t mode = data[0];
	const std::string json(reinterpret_cast<const char*>(data + 1), size - 1);

	auto settings = editor_project_readwrite::reading_settings();
	settings.strict = (mode & 1) != 0;
	settings.read_inactive_nodes = (mode & 2) != 0;
	settings.read_scratchpad = (mode & 4) != 0;

	try {
		augs::secure_hash_type hash;
		auto project = editor_project_readwrite::read_project_json(
			project_dir,
			json,
			off.resources,
			off.resource_map,
			settings,
			&hash
		);
		(void)project;
	}
	catch (const augs::json_deserialization_error&) {
	}
	catch (const augs::stream_read_error&) {
	}

	return 0;
}
