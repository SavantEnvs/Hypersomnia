// Fuzz augs::image::from_bytes: map textures and player avatars arrive as bytes and are
// decoded by the custom .bin format, lodepng or stb_image depending on the extension.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "augs/image/image.h"
#include "augs/readwrite/stream_read_error.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	if (size == 0) {
		return -1;
	}

	/* First byte picks the extension the bytes are reported under (it selects the decoder). */
	static const char* const names[] = { "fuzz.png", "fuzz.bin", "fuzz.jpg" };
	const char* const name = names[data[0] % 3];

	std::vector<std::byte> bytes(
		reinterpret_cast<const std::byte*>(data + 1),
		reinterpret_cast<const std::byte*>(data + size)
	);

	try {
		augs::image img;
		img.from_bytes(bytes, name);
		(void)img.get_size();
	}
	catch (const augs::image_loading_error&) {
	}
	catch (const augs::stream_read_error&) {
	}

	return 0;
}
