// Fuzz the lightweight project JSON scanners that servers/clients run on a downloaded map
// before loading it: read_only_external_resources (with its path sanitization) and
// read_only_project_timestamp.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#include "augs/network/network_types.h"
#include "application/setups/editor/project/editor_project_readwrite.h"
#include "augs/readwrite/json_readwrite_errors.h"
#include "augs/readwrite/stream_read_error.h"

/*
	The folder the downloaded project lives in. It must exist: path sanitization canonicalizes it.
	A dedicated empty folder under $TMPDIR, created once and never written to, so the result
	depends only on the input (not on corpus/artifact files in the fuzzer's working directory).
*/
static augs::path_type project_dir = ".";

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	std::error_code ec;
	const char* tmp = std::getenv("TMPDIR");
	const auto base = augs::path_type(tmp && *tmp ? tmp : "/tmp");
	const auto dir = base / "hypersomnia_fuzz_project_meta";

	std::filesystem::create_directories(dir, ec);

	if (!ec && std::filesystem::is_directory(dir, ec)) {
		project_dir = dir;
		return 0;
	}

	const auto cwd = std::filesystem::current_path(ec);

	if (!ec) {
		project_dir = cwd;
	}

	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	const std::string json(reinterpret_cast<const char*>(data), size);

	try {
		auto timestamp = editor_project_readwrite::read_only_project_timestamp(json);
		(void)timestamp;
	}
	catch (const augs::json_deserialization_error&) {
	}

	try {
		auto resources = editor_project_readwrite::read_only_external_resources(
			project_dir,
			json
		);
		(void)resources;
	}
	catch (const augs::json_deserialization_error&) {
	}

	return 0;
}
