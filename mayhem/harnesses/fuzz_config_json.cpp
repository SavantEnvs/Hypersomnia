// Fuzz the config JSON reader (config.json, conf.d/*.json, --apply-config and server-synced
// variables): config_json_table::load_patch_from_string over a default table.
#include <cstddef>
#include <cstdint>
#include <string>

#include "application/config_json_table.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	const std::string json(reinterpret_cast<const char*>(data), size);

	try {
		config_json_table table;
		table.load_patch_from_string(json, "fuzz_config.json");
	}
	catch (const config_read_error&) {
	}

	return 0;
}
