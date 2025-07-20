#pragma once

#include "ip/net/common.hpp"
#include "buffer.hpp"

namespace internetprotocol {
    // HTTP
    std::string request_method_to_string(request_method_e method) {
        switch (method) {
            case CONNECT: return "CONNECT";
            case DEL: return "DELETE";
            case GET: return "GET";
            case HEAD: return "HEAD";
            case OPTIONS: return "OPTIONS";
            case PATCH: return "PATCH";
            case POST: return "POST";
            case PUT: return "PUT";
            case TRACE: return "TRACE";
            default:
                return "";
        }
    }

    request_method_e string_to_request_method(const std::string &method_str) {
        static const std::unordered_map<std::string, request_method_e> method_map = {
            {"CONNECT", CONNECT},
            {"DELETE", DEL},
            {"GET", GET},
            {"HEAD", HEAD},
            {"OPTIONS", OPTIONS},
            {"PATCH", PATCH},
            {"POST", POST},
            {"PUT", PUT},
            {"TRACE", TRACE}
        };

        auto it = method_map.find(method_str);
        if (it != method_map.end()) {
            return it->second;
        }

        return UNKNOWN;
    }


    inline std::string header_enum_to_string(http_headers_e header) {
        switch (header) {
            case Accept: return "Accept";
            case Accept_CH: return "Accept-CH";
            case Accept_Encoding: return "Accept-Encoding";
            case Accept_Language: return "Accept-Language";
            case Accept_Patch: return "Accept-Patch";
            case Accept_Post: return "Accept-Post";
            case Accept_Ranges: return "Accept-Ranges";
            case Access_Control_Allow_Credentials: return "Access-Control-Allow-Credentials";
            case Access_Control_Allow_Headers: return "Access-Control-Allow-Headers";
            case Access_Control_Allow_Methods: return "Access-Control-Allow-Methods";
            case Access_Control_Allow_Origin: return "Access-Control-Allow-Origin";
            case Access_Control_Expose_Headers: return "Access-Control-Expose-Headers";
            case Access_Control_Max_Age: return "Access-Control-Max-Age";
            case Access_Control_Request_Headers: return "Access-Control-Request-Headers";
            case Access_Control_Request_Method: return "Access-Control-Request-Method";
            case Age: return "Age";
            case Allow: return "Allow";
            case Alt_Svc: return "Alt-Svc";
            case Alt_Used: return "Alt-Used";
            case Attribution_Reporting_EligibleExperimental: return "Attribution-Reporting-Eligible";
            case Attribution_Reporting_Register_SourceExperimental: return "Attribution-Reporting-Register-Source";
            case Attribution_Reporting_Register_TriggerExperimental: return "Attribution-Reporting-Register-Trigger";
            case Authorization: return "Authorization";
            case Available_DictionaryExperimental: return "Available-Dictionary";
            case Cache_Control: return "Cache-Control";
            case Clear_Site_Data: return "Clear-Site-Data";
            case Connection: return "Connection";
            case Digest: return "Digest";
            case Content_Disposition: return "Content-Disposition";
            case Content_DPR: return "Content-DPR";
            case Content_Encoding: return "Content-Encoding";
            case Content_Language: return "Content-Language";
            case Content_Length: return "Content-Length";
            case Content_Location: return "Content-Location";
            case Content_Range: return "Content-Range";
            case Content_Security_Policy: return "Content-Security-Policy";
            case Content_Security_Policy_Report_Only: return "Content-Security-Policy-Report-Only";
            case Content_Type: return "Content-Type";
            case Cookie: return "Cookie";
            case Critical_CHExperimental: return "Critical-CH";
            case Cross_Origin_Embedder_Policy: return "Cross-Origin-Embedder-Policy";
            case Cross_Origin_Opener_Policy: return "Cross-Origin-Opener-Policy";
            case Cross_Origin_Resource_Policy: return "Cross-Origin-Resource-Policy";
            case Date: return "Date";
            case Device_Memory: return "Device-Memory";
            case Dictionary_IDExperimental: return "Dictionary-ID";
            case DNT: return "DNT";
            case DownlinkExperimental: return "Downlink";
            case DPR: return "DPR";
            case Early_DataExperimental: return "Early-Data";
            case ECTExperimental: return "ECT";
            case ETag: return "ETag";
            case Expect: return "Expect";
            case Expect_CT: return "Expect-CT";
            case Expires: return "Expires";
            case Forwarded: return "Forwarded";
            case From: return "From";
            case Host: return "Host";
            case If_Match: return "If-Match";
            case If_Modified_Since: return "If-Modified-Since";
            case If_None_Match: return "If-None-Match";
            case If_Range: return "If-Range";
            case If_Unmodified_Since: return "If-Unmodified-Since";
            case Integrity_Policy: return "Integrity-Policy";
            case Integrity_Policy_Report_Only: return "Integrity-Policy-Report-Only";
            case Keep_Alive: return "Keep-Alive";
            case Last_Modified: return "Last-Modified";
            case Link: return "Link";
            case Location: return "Location";
            case Max_Forwards: return "Max-Forwards";
            case NELExperimental: return "NEL";
            case No_Vary_SearchExperimental: return "No-Vary-Search";
            case Observe_Browsing_TopicsExperimental: return "Observe-Browsing-Topics";
            case Origin: return "Origin";
            case Origin_Agent_Cluster: return "Origin-Agent-Cluster";
            case Feature_PolicyExperimental: return "Feature-Policy";
            case Pragma: return "Pragma";
            case Prefer: return "Prefer";
            case Preference_Applied: return "Preference-Applied";
            case Priority: return "Priority";
            case Proxy_Authenticate: return "Proxy-Authenticate";
            case Proxy_Authorization: return "Proxy-Authorization";
            case Range: return "Range";
            case Referer: return "Referer";
            case Referrer_Policy: return "Referrer-Policy";
            case Refresh: return "Refresh";
            case Report_To: return "Report-To";
            case Reporting_Endpoints: return "Reporting-Endpoints";
            case Repr_Digest: return "Repr-Digest";
            case Retry_After: return "Retry-After";
            case RTTExperimental: return "RTT";
            case Save_DataExperimental: return "Save-Data";
            case Sec_Browsing_TopicsExperimental: return "Sec-Browsing-Topics";
            case Sec_CH_Prefers_Color_SchemeExperimental: return "Sec-CH-Prefers-Color-Scheme";
            case Sec_CH_Prefers_Reduced_MotionExperimental: return "Sec-CH-Prefers-Reduced-Motion";
            case Sec_CH_Prefers_Reduced_TransparencyExperimental: return "Sec-CH-Prefers-Reduced-Transparency";
            case Sec_CH_UAExperimental: return "Sec-CH-UA";
            case Sec_CH_UA_ArchExperimental: return "Sec-CH-UA-Arch";
            case Sec_CH_UA_BitnessExperimental: return "Sec-CH-UA-Bitness";
            case Sec_CH_UA_Form_FactorsExperimental: return "Sec-CH-UA-Form-Factors";
            case Sec_CH_UA_Full_Version: return "Sec-CH-UA-Full-Version";
            case Sec_CH_UA_Full_Version_ListExperimental: return "Sec-CH-UA-Full-Version-List";
            case Sec_CH_UA_MobileExperimental: return "Sec-CH-UA-Mobile";
            case Sec_CH_UA_ModelExperimental: return "Sec-CH-UA-Model";
            case Sec_CH_UA_PlatformExperimental: return "Sec-CH-UA-Platform";
            case Sec_CH_UA_Platform_VersionExperimental: return "Sec-CH-UA-Platform-Version";
            case Sec_CH_UA_WoW64Experimental: return "Sec-CH-UA-WoW64";
            case Sec_Fetch_Dest: return "Sec-Fetch-Dest";
            case Sec_Fetch_Mode: return "Sec-Fetch-Mode";
            case Sec_Fetch_Site: return "Sec-Fetch-Site";
            case Sec_Fetch_User: return "Sec-Fetch-User";
            case Sec_GPCExperimental: return "Sec-GPC";
            case Sec_Purpose: return "Sec-Purpose";
            case Sec_Speculation_TagsExperimental: return "Sec-Speculation-Tags";
            case Sec_WebSocket_Accept: return "Sec-WebSocket-Accept";
            case Sec_WebSocket_Extensions: return "Sec-WebSocket-Extensions";
            case Sec_WebSocket_Key: return "Sec-WebSocket-Key";
            case Sec_WebSocket_Protocol: return "Sec-WebSocket-Protocol";
            case Sec_WebSocket_Version: return "Sec-WebSocket-Version";
            case Server: return "Server";
            case Server_Timing: return "Server-Timing";
            case Service_Worker: return "Service-Worker";
            case Service_Worker_Allowed: return "Service-Worker-Allowed";
            case Service_Worker_Navigation_Preload: return "Service-Worker-Navigation-Preload";
            case Set_Cookie: return "Set-Cookie";
            case Set_Login: return "Set-Login";
            case SourceMap: return "SourceMap";
            case Speculation_RulesExperimental: return "Speculation-Rules";
            case Strict_Transport_Security: return "Strict-Transport-Security";
            case Supports_Loading_ModeExperimental: return "Supports-Loading-Mode";
            case TE: return "TE";
            case Timing_Allow_Origin: return "Timing-Allow-Origin";
            case Tk: return "Tk";
            case Trailer: return "Trailer";
            case Transfer_Encoding: return "Transfer-Encoding";
            case Upgrade: return "Upgrade";
            case Upgrade_Insecure_Requests: return "Upgrade-Insecure-Requests";
            case Use_As_DictionaryExperimental: return "Use-As-Dictionary";
            case User_Agent: return "User-Agent";
            case Vary: return "Vary";
            case Via: return "Via";
            case Viewport_Width: return "Viewport-Width";
            case Want_Digest: return "Want-Digest";
            case Want_Repr_Digest: return "Want-Repr-Digest";
            case Warning: return "Warning";
            case Width: return "Width";
            case WWW_Authenticate: return "WWW-Authenticate";
            case X_Content_Type_Options: return "X-Content-Type-Options";
            case X_DNS_Prefetch_Control: return "X-DNS-Prefetch-Control";
            case X_Forwarded_For: return "X-Forwarded-For";
            case X_Forwarded_Host: return "X-Forwarded-Host";
            case X_Forwarded_Proto: return "X-Forwarded-Proto";
            case X_Frame_Options: return "X-Frame-Options";
            case X_Permitted_Cross_Domain_Policies: return "X-Permitted-Cross-Domain-Policies";
            case X_Powered_By: return "X-Powered-By";
            case X_Robots_Tag: return "X-Robots-Tag";
            case X_XSS_Protection: return "X-XSS-Protection";
            default: return "";
        }
    }

    inline http_headers_e string_to_header_enum(const std::string &header_str) {
        static std::unordered_map<std::string, http_headers_e> header_map = {
            {"Accept", Accept},
            {"Accept-CH", Accept_CH},
            {"Accept-Encoding", Accept_Encoding},
            {"Accept-Language", Accept_Language},
            {"Accept-Patch", Accept_Patch},
            {"Accept-Post", Accept_Post},
            {"Accept-Ranges", Accept_Ranges},
            {"Access-Control-Allow-Credentials", Access_Control_Allow_Credentials},
            {"Access-Control-Allow-Headers", Access_Control_Allow_Headers},
            {"Access-Control-Allow-Methods", Access_Control_Allow_Methods},
            {"Access-Control-Allow-Origin", Access_Control_Allow_Origin},
            {"Access-Control-Expose-Headers", Access_Control_Expose_Headers},
            {"Access-Control-Max-Age", Access_Control_Max_Age},
            {"Access-Control-Request-Headers", Access_Control_Request_Headers},
            {"Access-Control-Request-Method", Access_Control_Request_Method},
            {"Age", Age},
            {"Allow", Allow},
            {"Alt-Svc", Alt_Svc},
            {"Alt-Used", Alt_Used},
            {"Attribution-Reporting-Eligible", Attribution_Reporting_EligibleExperimental},
            {"Attribution-Reporting-Register-Source", Attribution_Reporting_Register_SourceExperimental},
            {"Attribution-Reporting-Register-Trigger", Attribution_Reporting_Register_TriggerExperimental},
            {"Authorization", Authorization},
            {"Available-Dictionary", Available_DictionaryExperimental},
            {"Cache-Control", Cache_Control},
            {"Clear-Site-Data", Clear_Site_Data},
            {"Connection", Connection},
            {"Digest", Digest},
            {"Content-Disposition", Content_Disposition},
            {"Content-DPR", Content_DPR},
            {"Content-Encoding", Content_Encoding},
            {"Content-Language", Content_Language},
            {"Content-Length", Content_Length},
            {"Content-Location", Content_Location},
            {"Content-Range", Content_Range},
            {"Content-Security-Policy", Content_Security_Policy},
            {"Content-Security-Policy-Report-Only", Content_Security_Policy_Report_Only},
            {"Content-Type", Content_Type},
            {"Cookie", Cookie},
            {"Critical-CH", Critical_CHExperimental},
            {"Cross-Origin-Embedder-Policy", Cross_Origin_Embedder_Policy},
            {"Cross-Origin-Opener-Policy", Cross_Origin_Opener_Policy},
            {"Cross-Origin-Resource-Policy", Cross_Origin_Resource_Policy},
            {"Date", Date},
            {"Device-Memory", Device_Memory},
            {"Dictionary-ID", Dictionary_IDExperimental},
            {"DNT", DNT},
            {"Downlink", DownlinkExperimental},
            {"DPR", DPR},
            {"Early-Data", Early_DataExperimental},
            {"ECT", ECTExperimental},
            {"ETag", ETag},
            {"Expect", Expect},
            {"Expect-CT", Expect_CT},
            {"Expires", Expires},
            {"Forwarded", Forwarded},
            {"From", From},
            {"Host", Host},
            {"If-Match", If_Match},
            {"If-Modified-Since", If_Modified_Since},
            {"If-None-Match", If_None_Match},
            {"If-Range", If_Range},
            {"If-Unmodified-Since", If_Unmodified_Since},
            {"Integrity-Policy", Integrity_Policy},
            {"Integrity-Policy-Report-Only", Integrity_Policy_Report_Only},
            {"Keep-Alive", Keep_Alive},
            {"Last-Modified", Last_Modified},
            {"Link", Link},
            {"Location", Location},
            {"Max-Forwards", Max_Forwards},
            {"NEL", NELExperimental},
            {"No-Vary-Search", No_Vary_SearchExperimental},
            {"Observe-Browsing-Topics", Observe_Browsing_TopicsExperimental},
            {"Origin", Origin},
            {"Origin-Agent-Cluster", Origin_Agent_Cluster},
            {"Feature-Policy", Feature_PolicyExperimental},
            {"Pragma", Pragma},
            {"Prefer", Prefer},
            {"Preference-Applied", Preference_Applied},
            {"Priority", Priority},
            {"Proxy-Authenticate", Proxy_Authenticate},
            {"Proxy-Authorization", Proxy_Authorization},
            {"Range", Range},
            {"Referer", Referer},
            {"Referrer-Policy", Referrer_Policy},
            {"Refresh", Refresh},
            {"Report-To", Report_To},
            {"Reporting-Endpoints", Reporting_Endpoints},
            {"Repr-Digest", Repr_Digest},
            {"Retry-After", Retry_After},
            {"RTT", RTTExperimental},
            {"Save-Data", Save_DataExperimental},
            {"Sec-Browsing-Topics", Sec_Browsing_TopicsExperimental},
            {"Sec-CH-Prefers-Color-Scheme", Sec_CH_Prefers_Color_SchemeExperimental},
            {"Sec-CH-Prefers-Reduced-Motion", Sec_CH_Prefers_Reduced_MotionExperimental},
            {"Sec-CH-Prefers-Reduced-Transparency", Sec_CH_Prefers_Reduced_TransparencyExperimental},
            {"Sec-CH-UA", Sec_CH_UAExperimental},
            {"Sec-CH-UA-Arch", Sec_CH_UA_ArchExperimental},
            {"Sec-CH-UA-Bitness", Sec_CH_UA_BitnessExperimental},
            {"Sec-CH-UA-Form-Factors", Sec_CH_UA_Form_FactorsExperimental},
            {"Sec-CH-UA-Full-Version", Sec_CH_UA_Full_Version},
            {"Sec-CH-UA-Full-Version-List", Sec_CH_UA_Full_Version_ListExperimental},
            {"Sec-CH-UA-Mobile", Sec_CH_UA_MobileExperimental},
            {"Sec-CH-UA-Model", Sec_CH_UA_ModelExperimental},
            {"Sec-CH-UA-Platform", Sec_CH_UA_PlatformExperimental},
            {"Sec-CH-UA-Platform-Version", Sec_CH_UA_Platform_VersionExperimental},
            {"Sec-CH-UA-WoW64", Sec_CH_UA_WoW64Experimental},
            {"Sec-Fetch-Dest", Sec_Fetch_Dest},
            {"Sec-Fetch-Mode", Sec_Fetch_Mode},
            {"Sec-Fetch-Site", Sec_Fetch_Site},
            {"Sec-Fetch-User", Sec_Fetch_User},
            {"Sec-GPC", Sec_GPCExperimental},
            {"Sec-Purpose", Sec_Purpose},
            {"Sec-Speculation-Tags", Sec_Speculation_TagsExperimental},
            {"Sec-WebSocket-Accept", Sec_WebSocket_Accept},
            {"Sec-WebSocket-Extensions", Sec_WebSocket_Extensions},
            {"Sec-WebSocket-Key", Sec_WebSocket_Key},
            {"Sec-WebSocket-Protocol", Sec_WebSocket_Protocol},
            {"Sec-WebSocket-Version", Sec_WebSocket_Version},
            {"Server", Server},
            {"Server-Timing", Server_Timing},
            {"Service-Worker", Service_Worker},
            {"Service-Worker-Allowed", Service_Worker_Allowed},
            {"Service-Worker-Navigation-Preload", Service_Worker_Navigation_Preload},
            {"Set-Cookie", Set_Cookie},
            {"Set-Login", Set_Login},
            {"SourceMap", SourceMap},
            {"Speculation-Rules", Speculation_RulesExperimental},
            {"Strict-Transport-Security", Strict_Transport_Security},
            {"Supports-Loading-Mode", Supports_Loading_ModeExperimental},
            {"TE", TE},
            {"Timing-Allow-Origin", Timing_Allow_Origin},
            {"Tk", Tk},
            {"Trailer", Trailer},
            {"Transfer-Encoding", Transfer_Encoding},
            {"Upgrade", Upgrade},
            {"Upgrade-Insecure-Requests", Upgrade_Insecure_Requests},
            {"Use-As-Dictionary", Use_As_DictionaryExperimental},
            {"User-Agent", User_Agent},
            {"Vary", Vary},
            {"Via", Via},
            {"Viewport-Width", Viewport_Width},
            {"Want-Digest", Want_Digest},
            {"Want-Repr-Digest", Want_Repr_Digest},
            {"Warning", Warning},
            {"Width", Width},
            {"WWW-Authenticate", WWW_Authenticate},
            {"X-Content-Type-Options", X_Content_Type_Options},
            {"X-DNS-Prefetch-Control", X_DNS_Prefetch_Control},
            {"X-Forwarded-For", X_Forwarded_For},
            {"X-Forwarded-Host", X_Forwarded_Host},
            {"X-Forwarded-Proto", X_Forwarded_Proto},
            {"X-Frame-Options", X_Frame_Options},
            {"X-Permitted-Cross-Domain-Policies", X_Permitted_Cross_Domain_Policies},
            {"X-Powered-By", X_Powered_By},
            {"X-Robots-Tag", X_Robots_Tag},
            {"X-XSS-Protection", X_XSS_Protection}
        };

        auto it = header_map.find(header_str);
        if (it != header_map.end()) {
            return it->second;
        }

        return Unknown;
    }

    inline std::string prepare_request(const http_request_t &req, const std::string &address, const uint16_t port) {
        std::string payload;

        payload = request_method_to_string(req.method) + " " + req.path;
        if (!req.params.empty()) {
            payload += "?";
            bool first = true;
            for (const std::pair<std::string, std::string> param: req.params) {
                if (!first) payload += "&";
                payload += param.first + "=" + param.second;
                first = false;
            }
        }
        payload += " HTTP/" + req.version + "\r\n";

        payload += "Host: " + address;
        if (port != 80 && port != 443) payload += ":" + std::to_string(port);
        payload += "\r\n";

        if (!req.headers.empty()) {
            for (const std::pair<http_headers_e, std::string> header: req.headers) {
                payload += header_enum_to_string(header.first) + ": " + header.second + "\r\n";
            }
            if (req.headers.find(Content_Length) == req.headers.end()) {
                payload +=
                    "Content-Length: " + std::to_string(req.body.length()) + "\r\n";
            }
        }
        payload += "\r\n";

        if (!req.body.empty()) payload += req.body;

        std::cout << payload << std::endl;

        return payload;
    }

    inline std::string prepare_response(const http_response_t &res) {
        std::string payload;

        payload = "HTTP/" + std::string(res.version) + " " + std::to_string(res.status_code) + " " + res.status_message + "\r\n";
        if (!res.headers.empty()) {
            for (const std::pair<http_headers_e, std::string> header: res.headers) {
                payload += header_enum_to_string(header.first) + ": " + header.second + "\r\n";
            }
            if (res.headers.find(Content_Length) == res.headers.end()) {
                payload +=
                    "Content-Length: " + std::to_string(res.body.length()) + "\r\n";
            }
        }
        payload += "\r\n";
        if (!res.body.empty()) payload += res.body;

        return payload;
    }

    inline void req_append_header(http_request_t &req, const std::string &headerline) {
        size_t pos = headerline.find(':');
        if (pos != std::string::npos) {
            std::string key = trim_whitespace(headerline.substr(0, pos));
            std::string value = trim_whitespace(headerline.substr(pos + 1));
            /*
            std::vector<std::string> values = split_string(value, ';');
            std::transform(
                values.begin(), values.end(), values.begin(),
                [&](const std::string &str) { return trim_whitespace(str); });
            */
            req.headers.insert_or_assign(string_to_header_enum(key), value);
        }
    }

    inline void res_append_header(http_response_t &res, const std::string &headerline) {
        size_t pos = headerline.find(':');
        if (pos != std::string::npos) {
            std::string key = trim_whitespace(headerline.substr(0, pos));
            std::string value = trim_whitespace(headerline.substr(pos + 1));
            /*
            std::vector<std::string> values = split_string(value, ';');
            std::transform(
                values.begin(), values.end(), values.begin(),
                [&](const std::string &str) { return trim_whitespace(str); });
            */
            res.headers.insert_or_assign(string_to_header_enum(key), value);
        }
    }
}
