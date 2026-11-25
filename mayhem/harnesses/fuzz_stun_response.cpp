// Fuzz the STUN binding-response reader used for NAT detection and traversal
// (read_stun_response over the bytes a STUN server - or anyone spoofing it - sends back).
#include <cstddef>
#include <cstdint>

/* Same include order as nat_detection_session.cpp, which reads these packets. */
#include "application/nat/nat_detection_session.h"
#include "augs/string/typesafe_sprintf.h"
#include "augs/log.h"
#include "application/network/address_utils.h"
#include "augs/network/netcode_sockets.h"
#include "augs/network/netcode_utils.h"
#include "application/masterserver/masterserver.h"
#include "augs/readwrite/to_bytes.h"
#include "augs/readwrite/pointer_to_buffer.h"
#include "augs/readwrite/byte_readwrite.h"
#include "application/nat/stun_request.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	/* The request we "sent": a fixed transaction id so a response can match it. */
	STUNMessageHeader request;
	request.type = htons(0x0001);
	request.length = htons(0x0000);
	request.cookie = htonl(0x2112A442);
	request.identifier = { 0x01020304u, 0x05060708u, 0x090a0b0cu };

	const auto result = read_stun_response(request, reinterpret_cast<const std::byte*>(data), size);
	(void)result;

	return 0;
}
