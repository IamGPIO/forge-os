#include "unity.h"
#include "sv2_protocol.h"
#include "mining.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

TEST_CASE("SV2 parse open extended channel extranonce bounds", "[sv2]")
{
    // request_id(4) + channel_id(4) + target(32) + extranonce_size(2) + B0_32(1+N) + group_channel_id(4)
    uint8_t payload[128];
    uint32_t req_id, chan_id, group_id;
    uint8_t target[32];
    uint16_t extranonce_size;
    uint8_t prefix[32];
    uint8_t prefix_len;

    // Valid: prefix_len = 8, extranonce_size = 8 (total 16 <= 32)
    memset(payload, 0, sizeof(payload));
    int pos = 0;
    payload[pos++] = 1; pos += 3;
    payload[pos++] = 2; pos += 3;
    pos += 32;
    payload[pos++] = 8; payload[pos++] = 0;
    payload[pos++] = 8;
    pos += 8;
    payload[pos++] = 3; pos += 3;

    TEST_ASSERT_EQUAL(0, sv2_parse_open_extended_channel_success(
        payload, pos, &req_id, &chan_id, target, &extranonce_size, prefix, &prefix_len, &group_id));
    TEST_ASSERT_EQUAL_UINT16(8, extranonce_size);
    TEST_ASSERT_EQUAL_UINT8(8, prefix_len);
    TEST_ASSERT_EQUAL_UINT32(3, group_id);

    // extranonce_size = 33 (> 32)
    payload[40] = 33; payload[41] = 0;
    TEST_ASSERT_EQUAL(-1, sv2_parse_open_extended_channel_success(
        payload, pos, &req_id, &chan_id, target, &extranonce_size, prefix, &prefix_len, &group_id));

    // prefix_len = 20, extranonce_size = 16 (sum 36 > 32)
    payload[40] = 16; payload[41] = 0;
    payload[42] = 20;
    uint32_t len_36 = 40 + 2 + 1 + 20 + 4;
    TEST_ASSERT_EQUAL(-1, sv2_parse_open_extended_channel_success(
        payload, len_36, &req_id, &chan_id, target, &extranonce_size, prefix, &prefix_len, &group_id));

    // Exact boundary: prefix_len = 16, extranonce_size = 16 (sum 32)
    payload[40] = 16; payload[41] = 0;
    payload[42] = 16;
    uint32_t len_32 = 40 + 2 + 1 + 16 + 4;
    TEST_ASSERT_EQUAL(0, sv2_parse_open_extended_channel_success(
        payload, len_32, &req_id, &chan_id, target, &extranonce_size, prefix, &prefix_len, &group_id));
    TEST_ASSERT_EQUAL_UINT16(16, extranonce_size);
    TEST_ASSERT_EQUAL_UINT8(16, prefix_len);

    // extranonce_size below the minimum we ask for
    payload[40] = 1; payload[41] = 0;
    TEST_ASSERT_EQUAL(-1, sv2_parse_open_extended_channel_success(
        payload, len_32, &req_id, &chan_id, target, &extranonce_size, prefix, &prefix_len, &group_id));

    // Trailing or missing byte
    payload[40] = 16; payload[41] = 0;
    TEST_ASSERT_EQUAL(-1, sv2_parse_open_extended_channel_success(
        payload, len_32 + 1, &req_id, &chan_id, target, &extranonce_size, prefix, &prefix_len, &group_id));
    TEST_ASSERT_EQUAL(-1, sv2_parse_open_extended_channel_success(
        payload, len_32 - 1, &req_id, &chan_id, target, &extranonce_size, prefix, &prefix_len, &group_id));
}

TEST_CASE("SV2 parse submit shares error", "[sv2]")
{
    uint8_t payload[64];
    memset(payload, 0, sizeof(payload));
    payload[0] = 1;  // channel_id = 1
    payload[4] = 42; // seq_num = 42
    payload[8] = 9;  // strlen = 9
    memcpy(payload + 9, "stale-job", 9);

    uint32_t chan_id, seq;
    char error_code[32];
    TEST_ASSERT_EQUAL(0, sv2_parse_submit_shares_error(payload, 9 + 9, &chan_id, &seq, error_code, sizeof(error_code)));
    TEST_ASSERT_EQUAL_UINT32(1, chan_id);
    TEST_ASSERT_EQUAL_UINT32(42, seq);
    TEST_ASSERT_EQUAL_STRING("stale-job", error_code);

    // String length runs past the payload
    TEST_ASSERT_EQUAL(-1, sv2_parse_submit_shares_error(payload, 9 + 8, &chan_id, &seq, error_code, sizeof(error_code)));
}

static int build_extended_job(uint8_t *payload, bool has_min_ntime, bool rolling_allowed)
{
    int pos = 0;
    payload[pos++] = 5; pos += 3;  // channel_id = 5
    payload[pos++] = 12; pos += 3; // job_id = 12
    payload[pos++] = has_min_ntime ? 1 : 0;
    if (has_min_ntime) {
        payload[pos++] = 0xb5; payload[pos++] = 0x25; payload[pos++] = 0x70; payload[pos++] = 0x64;
    }
    payload[pos++] = 0x00; payload[pos++] = 0x00; payload[pos++] = 0x00; payload[pos++] = 0x20; // version
    payload[pos++] = rolling_allowed ? 1 : 0;
    payload[pos++] = 1; // merkle_count
    memset(payload + pos, 0xab, 32);
    pos += 32;
    payload[pos++] = 4; payload[pos++] = 0; // prefix
    payload[pos++] = 1; payload[pos++] = 2; payload[pos++] = 3; payload[pos++] = 4;
    payload[pos++] = 3; payload[pos++] = 0; // suffix
    payload[pos++] = 5; payload[pos++] = 6; payload[pos++] = 7;
    return pos;
}

TEST_CASE("SV2 parse new extended mining job", "[sv2]")
{
    uint8_t payload[256] = {0};
    int len = build_extended_job(payload, true, true);

    sv2_extended_job_t job;
    TEST_ASSERT_EQUAL(0, sv2_parse_new_extended_mining_job(payload, len, &job));
    TEST_ASSERT_EQUAL_UINT32(5, job.channel_id);
    TEST_ASSERT_EQUAL_UINT32(12, job.job_id);
    TEST_ASSERT_TRUE(job.has_min_ntime);
    TEST_ASSERT_EQUAL_HEX32(0x647025b5, job.min_ntime);
    TEST_ASSERT_EQUAL_HEX32(0x20000000, job.version);
    TEST_ASSERT_TRUE(job.version_rolling_allowed);
    TEST_ASSERT_EQUAL(1, job.merkle_path_count);
    TEST_ASSERT_EQUAL_HEX8(0xab, job.merkle_path[0]);
    TEST_ASSERT_EQUAL(4, job.coinbase_prefix_len);
    TEST_ASSERT_EQUAL_HEX8(1, job.coinbase_prefix[0]);
    TEST_ASSERT_EQUAL_HEX8(4, job.coinbase_prefix[3]);
    TEST_ASSERT_EQUAL(3, job.coinbase_suffix_len);
    TEST_ASSERT_EQUAL_HEX8(5, job.coinbase_suffix[0]);
    TEST_ASSERT_EQUAL_HEX8(7, job.coinbase_suffix[2]);

    // Truncated anywhere: never reads past the payload
    for (int cut = 0; cut < len; cut++) {
        TEST_ASSERT_EQUAL(-1, sv2_parse_new_extended_mining_job(payload, cut, &job));
    }
}

TEST_CASE("SV2 extended future job without version rolling", "[sv2]")
{
    uint8_t payload[256] = {0};
    int len = build_extended_job(payload, false, false);

    sv2_extended_job_t job;
    TEST_ASSERT_EQUAL(0, sv2_parse_new_extended_mining_job(payload, len, &job));
    TEST_ASSERT_FALSE(job.has_min_ntime);
    TEST_ASSERT_FALSE(job.version_rolling_allowed);
    TEST_ASSERT_EQUAL_HEX32(0x20000000, job.version);
}

TEST_CASE("SV2 setup requires version rolling", "[sv2]")
{
    uint32_t standard_flags = sv2_setup_flags_for_channel(SV2_CHANNEL_STANDARD);
    uint32_t extended_flags = sv2_setup_flags_for_channel(SV2_CHANNEL_EXTENDED);

    TEST_ASSERT_BITS_HIGH(SV2_SETUP_FLAGS_REQUIRES_STANDARD_JOBS, standard_flags);
    TEST_ASSERT_BITS_HIGH(SV2_SETUP_FLAGS_REQUIRES_VERSION_ROLLING, standard_flags);
    TEST_ASSERT_BITS_LOW(SV2_SETUP_FLAGS_REQUIRES_STANDARD_JOBS, extended_flags);
    TEST_ASSERT_BITS_HIGH(SV2_SETUP_FLAGS_REQUIRES_VERSION_ROLLING, extended_flags);
    TEST_ASSERT_TRUE(sv2_setup_success_allows_version_rolling(0));
    TEST_ASSERT_TRUE(sv2_setup_success_allows_version_rolling(SV2_SETUP_SUCCESS_FLAGS_REQUIRES_EXT_CHANNELS));
    TEST_ASSERT_FALSE(sv2_setup_success_allows_version_rolling(SV2_SETUP_SUCCESS_FLAGS_REQUIRES_FIXED_VERSION));
}

TEST_CASE("SV2 accepts direct and group channel broadcasts", "[sv2]")
{
    TEST_ASSERT_TRUE(sv2_channel_or_group_matches(7, 7, 11));
    TEST_ASSERT_TRUE(sv2_channel_or_group_matches(11, 7, 11));
    TEST_ASSERT_FALSE(sv2_channel_or_group_matches(12, 7, 11));
    TEST_ASSERT_TRUE(sv2_channel_or_group_matches(7, 7, 0));
    TEST_ASSERT_FALSE(sv2_channel_or_group_matches(0, 7, 0));
}

TEST_CASE("SV2 submit shares frame layout", "[sv2]")
{
    uint8_t frame[SV2_SUBMIT_SHARES_MAX_FRAME_SIZE];
    uint8_t extranonce[4] = {0xde, 0xad, 0xbe, 0xef};

    int len = sv2_build_submit_shares(frame, sizeof(frame), 7, 3, 12, 0x11223344, 0x55667788, 0x20000000, extranonce, 4);
    TEST_ASSERT_EQUAL(SV2_FRAME_HEADER_SIZE + SV2_SUBMIT_SHARES_PAYLOAD_SIZE + 1 + 4, len);

    sv2_frame_header_t hdr;
    sv2_parse_frame_header(frame, &hdr);
    TEST_ASSERT_EQUAL_HEX16(SV2_CHANNEL_MSG_FLAG, hdr.extension_type);
    TEST_ASSERT_EQUAL_HEX8(SV2_MSG_SUBMIT_SHARES_EXTENDED, hdr.msg_type);
    TEST_ASSERT_EQUAL_UINT32(len - SV2_FRAME_HEADER_SIZE, hdr.msg_length);
    TEST_ASSERT_EQUAL_HEX8(0x44, frame[SV2_FRAME_HEADER_SIZE + 12]); // nonce, little endian
    TEST_ASSERT_EQUAL_HEX8(4, frame[SV2_FRAME_HEADER_SIZE + 24]);
    TEST_ASSERT_EQUAL_HEX8(0xef, frame[SV2_FRAME_HEADER_SIZE + 28]);

    len = sv2_build_submit_shares(frame, sizeof(frame), 7, 3, 12, 0x11223344, 0x55667788, 0x20000000, NULL, 0);
    sv2_parse_frame_header(frame, &hdr);
    TEST_ASSERT_EQUAL(SV2_FRAME_HEADER_SIZE + SV2_SUBMIT_SHARES_PAYLOAD_SIZE, len);
    TEST_ASSERT_EQUAL_HEX8(SV2_MSG_SUBMIT_SHARES_STANDARD, hdr.msg_type);
}

TEST_CASE("SV2 target to difficulty", "[sv2]")
{
    // Difficulty 1: 0x00000000ffff0000...0000, as a little endian U256
    uint8_t target[32] = {0};
    target[26] = 0xff;
    target[27] = 0xff;
    double diff = target_to_diff(target);
    TEST_ASSERT_TRUE(diff > 0.9999 && diff < 1.0001);

    // Halving the target doubles the difficulty
    target[26] = 0xff;
    target[27] = 0x7f;
    diff = target_to_diff(target);
    TEST_ASSERT_TRUE(diff > 1.9999 && diff < 2.0001);
}

// Block 1 of the Bitcoin main chain, delivered the way an SV2 extended channel
// would: coinbase split into prefix / extranonce prefix / extranonce / suffix,
// prev hash in header byte order. Going through the same mining_notify ->
// bm_job path as the firmware must reproduce a header that meets the block's
// own target.
TEST_CASE("SV2 extended job builds a valid block 1 header", "[sv2]")
{
    static const char coinbase_hex[] =
        "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff"
        "0704ffff001d0104ffffffff0100f2052a0100000043410496b538e853519c726a2c91e61ec11600"
        "ae1390813a627c66fb8be7947be63c52da7589379515d4e0a604f8141781e62294721166bf621e73"
        "a82cbf2342c858eeac00000000";
    // Block 0 hash, as displayed (big endian)
    static const char genesis_hash_display[] =
        "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f";

    // Split the coinbase script "04ffff001d 0104": 4 bytes of extranonce
    // prefix (the pool's) and 2 bytes the miner rolls.
    const size_t prefix_hex_len = 82 + 2;       // up to and including the script length byte
    char coinbase_1[sizeof(coinbase_hex)];
    memcpy(coinbase_1, coinbase_hex, prefix_hex_len);
    coinbase_1[prefix_hex_len] = '\0';
    char extranonce1[9];
    memcpy(extranonce1, coinbase_hex + prefix_hex_len, 8);
    extranonce1[8] = '\0';
    char extranonce2[5];
    memcpy(extranonce2, coinbase_hex + prefix_hex_len + 8, 4);
    extranonce2[4] = '\0';
    const char *coinbase_2 = coinbase_hex + prefix_hex_len + 12;

    uint8_t prev_hash[32];
    hex2bin(genesis_hash_display, prev_hash, 32);
    reverse_bytes(prev_hash, 32); // header byte order, as SV2 sends it

    mining_notify notify = {0};
    char prev_hex[65];
    sv2_prev_hash_to_stratum_hex(prev_hash, prev_hex);
    notify.prev_block_hash = prev_hex;
    notify.version = 1;
    notify.target = 0x1d00ffff;
    notify.ntime = 1231469665;
    notify.difficulty = 1.0;

    char *coinbase_tx = construct_coinbase_tx(coinbase_1, coinbase_2, extranonce1, extranonce2);
    TEST_ASSERT_EQUAL_STRING(coinbase_hex, coinbase_tx);
    char *merkle_root = calculate_merkle_root_hash(coinbase_tx, NULL, 0);
    TEST_ASSERT_EQUAL_STRING("982051fd1e4ba744bbbe680e1fee14677ba1a3c3540bf7b1cdb606e857233e0e", merkle_root);

    bm_job job = construct_bm_job(&notify, merkle_root, 0);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(prev_hash, job.prev_block_hash, 32);

    double diff = test_nonce_value(&job, 2573394689u, 1);
    TEST_ASSERT_TRUE(diff >= 1.0);
    // A different nonce must not pass
    TEST_ASSERT_TRUE(test_nonce_value(&job, 2573394688u, 1) < 1.0);

    free(coinbase_tx);
    free(merkle_root);
}
