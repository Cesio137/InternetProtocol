#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <optional>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <random>
#include <cstdint>
#include <bitset>
#include <locale>

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

namespace InternetProtocol
{
    enum class EVerb : uint8_t
    {
        GET =       0,
        POST =      1,
        PUT =       2,
        PATCH =     3,
        DEL =       4,
        COPY =      5,
        HEAD =      6,
        OPTIONS =   7,
        LOCK =      8,
        UNLOCK =    9,
        PROPFIND = 10
    };

    enum class EOpcode : uint8_t
    {
        FRAME_CON =             0x00,
        TEXT_FRAME =            0x01,
        BINARY_FRAME =          0x02,
        NON_CONTROL_FRAMES =    0x03,
        CONNECTION_CLOSE =      0x08,
        PING =                  0x09,
        PONG =                  0x0A,
        FURTHER_FRAMES =        0x0B,
    };

    enum class ERSV : uint8_t
    {
        RSV1 = 0x40,
        RSV2 = 0x20,
        RSV3 = 0x10
    };

    struct FAsioTcp
    {
        FAsioTcp() : resolver(context), socket(context) {}
        asio::error_code error_code;
        asio::io_context context;
        asio::ip::tcp::resolver resolver;
        asio::ip::tcp::resolver::results_type endpoints;
        asio::ip::tcp::socket socket;

        FAsioTcp(const FAsioTcp& asio) : resolver(context), socket(context)
        {
            error_code = asio.error_code;
            endpoints = asio.endpoints;
        }

        FAsioTcp& operator=(const FAsioTcp& asio)
        {
            if (this != &asio)
            {
                error_code = asio.error_code;
                endpoints = asio.endpoints;
            }
            return *this;
        }
    };

    struct FAsioUdp
    {
        FAsioUdp() : resolver(context), socket(context) {}
        asio::error_code error_code;
        asio::io_context context;
        asio::ip::udp::socket socket;
        asio::ip::udp::endpoint endpoints;
        asio::ip::udp::resolver resolver;

        FAsioUdp(const FAsioUdp& asio) : resolver(context), socket(context)
        {
            error_code = asio.error_code;
            endpoints = asio.endpoints;
        }

        FAsioUdp& operator=(const FAsioUdp& asio)
        {
            if (this != &asio)
            {
                error_code = asio.error_code;
                endpoints = asio.endpoints;
            }
            return *this;
        }
    };
    

    struct FRequest
    {
        std::map<std::string, std::string> params;
        EVerb verb = EVerb::GET;
        std::string path = "/";
        std::string version = "2.0";
        std::map<std::string, std::string> headers;
        std::string body;

        void clear()
        {
            params.clear();
            verb = EVerb::GET;
            path = "/";
            version = "2.0";
            headers.clear();
            body.clear();
        }
    };

    struct FResponse
    {
        std::map<std::string, std::vector<std::string>> headers;
        int contentLenght = 0;
        std::string content;

        void appendHeader(const std::string& headerline) {
            size_t pos = headerline.find(':');
            if (pos != std::string::npos) {
                std::string key = trimWhitespace(headerline.substr(0, pos));
                std::string value = trimWhitespace(headerline.substr(pos + 1));
                if (key == "Content-Length")
                {
                    contentLenght = std::stoi(value);
                    return;
                }
                std::vector<std::string> values = splitString(value, ';');
                std::transform(values.begin(), values.end(), values.begin(), [this](const std::string& str) { return trimWhitespace(str); });
                headers.insert_or_assign(key, values);
            }
            
        }

        void setContent(const std::string& value) {
            if (value.empty())
                return;
            content = value;
        }

        void appendContent(const std::string& value) {
            if (value.empty())
                return;
            content.append(value);
        }

        void clear() {
            headers.clear();
            contentLenght = 0;
            content.clear();
        }

    private:
        std::vector<std::string> splitString(const std::string& str, char delimiter) {
            std::vector<std::string> tokens;
            std::string token;
            std::istringstream tokenStream(str);
            while (std::getline(tokenStream, token, delimiter)) {
                tokens.push_back(token);
            }
            return tokens;
        }

        std::string trimWhitespace(const std::string& str) {
            std::string::const_iterator start = str.begin();
            while (start != str.end() && std::isspace(*start)) {
                start++;
            }

            std::string::const_iterator end = str.end();
            do {
                end--;
            } while (std::distance(start, end) > 0 && std::isspace(*end));

            return std::string(start, end + 1);
        }
    };

} // namespace Nanometro