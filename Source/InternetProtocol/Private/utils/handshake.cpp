/** 
 * Copyright © 2025 Nathan Miguel
 */

#include "utils/handshake.h"


std::array<uint8, 20> sha1(const FString& input) {
	uint64 bit_length = input.Len() * 8;
	std::string padded_input = TCHAR_TO_UTF8(*input);

	padded_input += static_cast<char>(0x80);

	while ((padded_input.size() % 64) != 56) {
		padded_input += static_cast<char>(0x00);
	}

	for (int i = 7; i >= 0; --i) {
		padded_input += static_cast<char>((bit_length >> (i * 8)) & 0xFF);
	}

	uint32_t h[5] = {h0, h1, h2, h3, h4};

	for (size_t i = 0; i < padded_input.size(); i += 64) {
		uint32 w[80] = {0};

		for (int j = 0; j < 16; ++j) {
			w[j] = static_cast<uint8>(padded_input[i + j * 4]) << 24 |
				    static_cast<uint8>(padded_input[i + j * 4 + 1]) << 16 |
					static_cast<uint8>(padded_input[i + j * 4 + 2]) << 8 |
					static_cast<uint8>(padded_input[i + j * 4 + 3]);
		}

        for (int j = 16; j < 80; ++j) {
            w[j] = (w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16]);
            w[j] = (w[j] << 1) | (w[j] >> 31);
        }

        uint32 a = h[0];
        uint32 b = h[1];
        uint32 c = h[2];
        uint32 d = h[3];
        uint32 e = h[4];

		for (int j = 0; j < 80; ++j) {
			uint32 f, k;

            if (j < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (j < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (j < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32 temp = ((a << 5) | (a >> 27)) + f + e + k + w[j];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
		}

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
	}

	std::array<uint8_t, 20> hash{};
    for (int i = 0; i < 5; ++i) {
        hash[i * 4] = (h[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = h[i] & 0xFF;
    }

    return hash;
}

FString base64_encode(const uint8* input, size_t length) {
	FString result;
	int i = 0;
	uint32 octet_a, octet_b, octet_c, triple;

	while (length >= 3) {
		octet_a = input[i++];
		octet_b = input[i++];
		octet_c = input[i++];

		triple = (octet_a << 16) + (octet_b << 8) + octet_c;

		result.AppendChar(base64_chars[(triple >> 18) & 0x3F]);
		result.AppendChar(base64_chars[(triple >> 12) & 0x3F]);
		result.AppendChar(base64_chars[(triple >> 6) & 0x3F]);
		result.AppendChar(base64_chars[triple & 0x3F]);

		length -= 3;
	}

	if (length > 0) {
		octet_a = input[i++];
		octet_b = length > 1 ? input[i++] : 0;

		triple = (octet_a << 16) + (octet_b << 8);

		result.AppendChar(base64_chars[(triple >> 18) & 0x3F]);
		result.AppendChar(base64_chars[(triple >> 12) & 0x3F]);
		result.AppendChar(length == 2 ? base64_chars[(triple >> 6) & 0x3F] : '=');
		result.AppendChar('=');
	}

	return result;
}

FString generate_accept_key(const FString& sec_websocket_key) {
	FString concatenated = sec_websocket_key + magic_string;
	auto hash = sha1(concatenated);
	return base64_encode(hash.data(), hash.size());
}

bool validate_handshake_request(const FHttpRequest& req_handshake, FHttpResponse& res_handshake) {
	if (req_handshake.Headers.Contains("connection")) {
		res_handshake.Body = "\"Connection\" header not found";
		return false;
	}
	if (req_handshake.Headers["connection"] != "Upgrade") {
		res_handshake.Body = "\"Connection\" header value is not \"Upgrade\"";
		return false;
	}

	if (req_handshake.Headers.Contains("upgrade")) {
		res_handshake.Body = "\"Upgrade\" header not found";
		return false;
	}
	if (req_handshake.Headers["upgrade"] != "websocket") {
		res_handshake.Body = "\"Upgrade\" header value is not \"websocket\"";
		return false;
	}

	if (req_handshake.Headers.Contains("sec-websocket-key")) {
		res_handshake.Body = "\"Sec-WebSocket-Key\" header not found";
		return false;
	}

	if (req_handshake.Headers.Contains("sec-websocket-version")) {
		res_handshake.Body = "\"Sec-WebSocket-Version\" header not found";
		return false;
	}
	if (req_handshake.Headers["sec-websocket-version"] != "13") {
		res_handshake.Body = "Invalid \"Sec-WebSocket-Version\" header value";
		return false;
	}
	return true;
}

bool validate_handshake_response(const FHttpRequest& req_handshake, FHttpResponse& res_handshake) {
	if (!res_handshake.Headers.Contains("connection")) {
		res_handshake.Body = "Connection header not found";
		return false;
	}
	if (res_handshake.Headers["connection"] != "Upgrade") {
		res_handshake.Body = "\"Connection\" header value is not \"Upgrade\"";
		return false;
	}

	if (!res_handshake.Headers.Contains("upgrade")) {
		res_handshake.Body = "Upgrade header not found";
		return false;
	}
	if (res_handshake.Headers["upgrade"] != "websocket") {
		res_handshake.Body = "\"Upgrade\" header value is not \"websocket\"";
		return false;
	}

	if (!res_handshake.Headers.Contains("sec-websocket-accept")) {
		res_handshake.Body = "Sec-WebSocket-Accept header not found";
		return false;
	}
	FString req_key = req_handshake.Headers["Sec-WebSocket-Key"];
	FString res_key = res_handshake.Headers["sec-websocket-accept"];
	FString encoded_key = generate_accept_key(req_key);
	if (encoded_key != res_key) {
		res_handshake.Body = "Invalid handshake: \"Sec-WebSocket-Accept\" is invalid.";
		return false;
	}

	return true;
}
