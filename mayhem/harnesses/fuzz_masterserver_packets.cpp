// Fuzz the out-of-band UDP packet readers: what the masterserver reads from game servers and
// clients (masterserver_request, heartbeat-on-ping), what game servers and clients read from
// the masterserver (masterserver_response, gameserver_command, ping responses) and the
// server_heartbeat the signalling server decodes. The first byte selects the reader.
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
#include "application/masterserver/masterserver_commands.h"
#include "application/masterserver/masterserver_requests.h"
#include "application/masterserver/heartbeat_on_ping.h"
#include "application/masterserver/gameserver_command_readwrite.h"
#include "application/masterserver/server_heartbeat.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	if (size == 0) {
		return -1;
	}

	const uint8_t selector = data[0];
	const uint8_t* const packet = data + 1;
	const std::size_t n = size - 1;
	const auto bytes = reinterpret_cast<const std::byte*>(packet);

	try {
		switch (selector % 6) {
			case 0: {
				const auto request = augs::from_bytes<masterserver_request>(bytes, n);
				(void)request;
				break;
			}
			case 1: {
				const auto response = augs::from_bytes<masterserver_response>(bytes, n);
				(void)response;
				break;
			}
			case 2: {
				if (const auto header = ::read_heartbeat_on_ping_header(packet, n)) {
					(void)header;
				}

				if (const auto hb = ::read_heartbeat_on_ping(packet, n)) {
					(void)hb;
				}
				break;
			}
			case 3: {
				const auto command = read_gameserver_command(bytes, n);
				(void)command;
				break;
			}
			case 4: {
				(void)read_ping_response(packet, n);
				break;
			}
			case 5: {
				const auto hb = augs::from_bytes<server_heartbeat>(bytes, n);
				(void)hb;
				break;
			}
		}
	}
	catch (const augs::stream_read_error&) {
	}

	return 0;
}
