// Fuzz the game's net message deserialization (client <-> server payloads over yojimbo):
// input = [u32 message type id][message body]. Block messages get the body as their block;
// the others are read with yojimbo::ReadStream through the message's own Serialize, the same
// way the client's demo replay (replay_serialized_net_message) does. Then the payload is read
// out with the message's own read_payload, as the client/server handlers do. Both directions
// are covered: a server reads client_to_server messages, a client server_to_client ones.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

/* Same include order as the server (server_setup.cpp), which reads these messages. */
#include <sodium.h>
#include "augs/misc/date_time.h"
#include "augs/misc/pool/pool_io.hpp"
#include "augs/misc/imgui/imgui_scope_wrappers.h"
#include "augs/misc/imgui/imgui_control_wrappers.h"
#include "augs/misc/compress.h"
#include "application/setups/server/server_setup.h"
#include "application/config_json_table.h"
#include "application/network/server_adapter.hpp"
#include "augs/filesystem/file.h"
#include "application/network/net_message_translation.h"
#include "application/network/net_serialize.h"
#include "augs/readwrite/byte_readwrite.h"
#include "augs/readwrite/memory_stream.h"
#include "application/arena/arena_handle.h"
#include "application/setups/client/demo_step.h"
#include "application/network/net_message_readwrite.h"
#include "augs/misc/scope_guard.h"

template <class M, class = void>
struct has_one_arg_read : std::false_type {};

template <class M>
struct has_one_arg_read<M, std::void_t<decltype(&M::read_payload)>> : std::bool_constant<
	std::is_invocable_v<decltype(&M::read_payload), M&, payload_of_t<M>>
> {};

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	InitializeYojimbo();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	using Id = net_messages::id_t;

	std::vector<std::byte> bytes(
		reinterpret_cast<const std::byte*>(data),
		reinterpret_cast<const std::byte*>(data + size)
	);

	auto& allocator = yojimbo::GetDefaultAllocator();

	try {
		auto ar = augs::cref_memory_stream(bytes);

		Id id;
		augs::read_bytes(ar, id);

		if (id.get_index() >= Id::max_index_v) {
			return -1;
		}

		id.dispatch([&](auto* e) {
			using M = remove_cptr<decltype(e)>;

			M msg;

			auto release_msg = augs::scope_guard([&msg]() {
				msg.Release();
			});

			if constexpr(is_block_message_v<M>) {
				const auto pos = ar.get_read_pos();
				const auto block_size = bytes.size() - pos;

				if (block_size == 0 || block_size > max_block_size_v) {
					return;
				}

				const auto block_bytes = reinterpret_cast<uint8_t*>(YOJIMBO_ALLOCATE(allocator, block_size));
				augs::detail::read_raw_bytes(ar, block_bytes, block_size);
				msg.AttachBlock(allocator, block_bytes, block_size);
			}
			else {
				const auto pos = ar.get_read_pos();

				if (bytes.size() % 4 != 0) {
					bytes.resize(bytes.size() + 4 - (bytes.size() % 4));
				}

				/*
					yojimbo's BitReader loads up to 7 bytes past the last data byte; its allocation
					contract (serialize.h, BitReader::ReadBits) is that the buffer has that slack,
					as yojimbo's own packet buffers do. The slack is not part of the stream.
				*/
				const auto stream_bytes = bytes.size() - pos;
				thread_local std::vector<std::byte> padded;
				padded.assign(bytes.begin() + pos, bytes.end());
				padded.resize(stream_bytes + 8);

				auto stream = yojimbo::ReadStream(
					reinterpret_cast<const uint8_t*>(padded.data()),
					stream_bytes
				);

				if (!msg.Serialize(stream)) {
					return;
				}
			}

			if constexpr(std::is_same_v<M, net_messages::full_arena_snapshot>) {
				thread_local augs::serialization_buffers buffers;
				const cosmos_solvable_significant clean_round_state;

				cosmos_solvable_significant signi;
				all_modes_variant mode;
				uint32_t client_id = 0;
				rcon_level_type rcon = rcon_level_type::DENIED;

				(void)msg.read_payload(buffers, clean_round_state, { signi, mode, client_id, rcon });
			}
			else if constexpr(std::is_same_v<M, net_messages::player_avatar_exchange>) {
				mode_player_id player_id;
				arena_player_avatar_payload payload;
				(void)msg.read_payload(player_id, payload);
			}
			else if constexpr(has_one_arg_read<M>::value) {
				remove_cref<payload_of_t<M>> payload;
				(void)msg.read_payload(payload);
			}
		});
	}
	catch (const augs::stream_read_error&) {
	}

	return 0;
}
