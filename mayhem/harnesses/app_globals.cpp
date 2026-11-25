// The process-wide globals the application defines in src/main.cpp and src/work.cpp (its entry
// point and main loop, which the harnesses do not link). Library code only references them through
// path helpers; the harnesses never start the application, so they stay empty / unset here.
#include <functional>
#include <optional>
#include <string>

#include "augs/filesystem/path.h"
#include "augs/persistent_filesystem.h"
/* work.cpp's own (no-op off the web) persistent filesystem hooks. */
#include "augs/persistent_filesystem.hpp"

augs::path_type CALLING_CWD;
augs::path_type APPDATA_DIR;
augs::path_type USER_DIR;
augs::path_type KEYS_DIR;

std::function<void()> ensure_handler;

augs::path_type get_user_dir(const std::optional<std::string>& steam_id) {
	return ::APPDATA_DIR / (steam_id.has_value() ? *steam_id : std::string("user"));
}

augs::path_type get_keys_dir(const augs::path_type& user_dir, const bool) {
	return user_dir / "keys";
}
