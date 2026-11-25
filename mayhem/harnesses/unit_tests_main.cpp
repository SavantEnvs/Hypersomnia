// Runs Hypersomnia's own Catch unit tests (the ones the game runs at startup with
// --unit-tests-only) without the rest of the application. Catch's report goes to the
// file given as argv[1]; argv[2] is the app data dir (the tests write their scratch files to
// its cache/ folder, as the game does). Like the game, it runs with the hypersomnia/ folder as cwd.
// Exit code 0 = all passed.
#include <cstdio>
#include <exception>

#include <filesystem>

#include "augs/unit_tests.h"
#include "augs/filesystem/path.h"

extern augs::path_type APPDATA_DIR;
extern augs::path_type USER_DIR;

int main(int argc, char** argv) {
	if (argc < 3) {
		std::fprintf(stderr, "usage: %s <report path> <app data dir>\n", argv[0]);
		return 2;
	}

	APPDATA_DIR = argv[2];
	USER_DIR = APPDATA_DIR / "user";
	/* The folders the game creates at startup that the tests use. */
	std::filesystem::create_directories(APPDATA_DIR / "cache");
	std::filesystem::create_directories(USER_DIR / "downloads" / "maps");

	unit_tests_settings settings;
	settings.run = true;
	settings.log_successful = false;
	settings.break_on_failure = false;
	settings.redirect_log_to_path = argv[1];

	try {
		augs::run_unit_tests(settings);
	}
	catch (const augs::unit_test_session_error& err) {
		std::fprintf(stderr, "%s\n", err.what());
		return 1;
	}

	return 0;
}
