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
#include <cstdint>

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

    enum EStatusCode {
        Invalid_0 = 0,

        // Information responses
        Continue_100 = 100,
        SwitchingProtocol_101 = 101,
        Processing_102 = 102,
        EarlyHints_103 = 103,

        // Successful responses
        OK_200 = 200,
        Created_201 = 201,
        Accepted_202 = 202,
        NonAuthoritativeInformation_203 = 203,
        NoContent_204 = 204,
        ResetContent_205 = 205,
        PartialContent_206 = 206,
        MultiStatus_207 = 207,
        AlreadyReported_208 = 208,
        IMUsed_226 = 226,

        // Redirection messages
        MultipleChoices_300 = 300,
        MovedPermanently_301 = 301,
        Found_302 = 302,
        SeeOther_303 = 303,
        NotModified_304 = 304,
        UseProxy_305 = 305,
        unused_306 = 306,
        TemporaryRedirect_307 = 307,
        PermanentRedirect_308 = 308,

        // Client error responses
        BadRequest_400 = 400,
        Unauthorized_401 = 401,
        PaymentRequired_402 = 402,
        Forbidden_403 = 403,
        NotFound_404 = 404,
        MethodNotAllowed_405 = 405,
        NotAcceptable_406 = 406,
        ProxyAuthenticationRequired_407 = 407,
        RequestTimeout_408 = 408,
        Conflict_409 = 409,
        Gone_410 = 410,
        LengthRequired_411 = 411,
        PreconditionFailed_412 = 412,
        PayloadTooLarge_413 = 413,
        UriTooLong_414 = 414,
        UnsupportedMediaType_415 = 415,
        RangeNotSatisfiable_416 = 416,
        ExpectationFailed_417 = 417,
        ImATeapot_418 = 418,
        MisdirectedRequest_421 = 421,
        UnprocessableContent_422 = 422,
        Locked_423 = 423,
        FailedDependency_424 = 424,
        TooEarly_425 = 425,
        UpgradeRequired_426 = 426,
        PreconditionRequired_428 = 428,
        TooManyRequests_429 = 429,
        RequestHeaderFieldsTooLarge_431 = 431,
        UnavailableForLegalReasons_451 = 451,

        // Server error responses
        InternalServerError_500 = 500,
        NotImplemented_501 = 501,
        BadGateway_502 = 502,
        ServiceUnavailable_503 = 503,
        GatewayTimeout_504 = 504,
        HttpVersionNotSupported_505 = 505,
        VariantAlsoNegotiates_506 = 506,
        InsufficientStorage_507 = 507,
        LoopDetected_508 = 508,
        NotExtended_510 = 510,
        NetworkAuthenticationRequired_511 = 511,
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