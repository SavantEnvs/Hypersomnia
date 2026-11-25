// Fuzz the identity certificate parsers a game server runs on what a connecting client sends:
// identity_cert::read_blob over the fixed-size connect blob (cert + password), and
// identity_cert::from_bytes + verify over a raw cert.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "application/network/identity_cert.h"

static void try_verify(const identity_cert::cert& c) {
	/* No broker keys: only self-signed "key_" accounts can verify; the parse + checks still run. */
	const std::vector<augs::ed25519_public_key_type> broker_keys;
	const std::array<uint8_t, 256> binding{};
	const std::array<uint8_t, 64> signature{};
	identity_cert::verify_failure why = identity_cert::verify_failure::NONE;

	(void)identity_cert::verify(c, broker_keys, 1700000000, binding.data(), signature.data(), &why);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	if (const auto c = identity_cert::from_bytes(reinterpret_cast<const std::byte*>(data), size)) {
		try_verify(*c);
	}

	/* read_blob reads exactly blob_bytes: pad (or cut) the input to that size, as the netcode user data is. */
	identity_cert::blob_type blob{};
	std::memcpy(blob.data(), data, std::min(size, blob.size()));

	if (const auto b = identity_cert::read_blob(blob.data())) {
		if (b->identity) {
			try_verify(*b->identity);
		}
	}

	return 0;
}
