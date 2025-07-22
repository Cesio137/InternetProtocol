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
            case Accept: return "accept";
            case Accept_CH: return "accept-ch";
            case Accept_Encoding: return "accept-encoding";
            case Accept_Language: return "accept-language";
            case Accept_Patch: return "accept-patch";
            case Accept_Post: return "accept-post";
            case Accept_Ranges: return "accept-ranges";
            case Access_Control_Allow_Credentials: return "access-control-allow-credentials";
            case Access_Control_Allow_Headers: return "access-control-allow-headers";
            case Access_Control_Allow_Methods: return "access-control-allow-methods";
            case Access_Control_Allow_Origin: return "access-control-allow-origin";
            case Access_Control_Expose_Headers: return "access-control-expose-headers";
            case Access_Control_Max_Age: return "access-control-max-age";
            case Access_Control_Request_Headers: return "access-control-request-headers";
            case Access_Control_Request_Method: return "access-control-request-method";
            case Age: return "age";
            case Allow: return "allow";
            case Alt_Svc: return "alt-svc";
            case Alt_Used: return "alt-used";
            case Attribution_Reporting_EligibleExperimental: return "attribution-reporting-eligible";
            case Attribution_Reporting_Register_SourceExperimental: return "attribution-reporting-register-source";
            case Attribution_Reporting_Register_TriggerExperimental: return "attribution-reporting-register-trigger";
            case Authorization: return "authorization";
            case Available_DictionaryExperimental: return "available-dictionary";
            case Cache_Control: return "cache-control";
            case Clear_Site_Data: return "clear-site-data";
            case Connection: return "connection";
            case Digest: return "digest";
            case Content_Disposition: return "content-disposition";
            case Content_DPR: return "content-dpr";
            case Content_Encoding: return "content-encoding";
            case Content_Language: return "content-language";
            case Content_Length: return "content-length";
            case Content_Location: return "content-location";
            case Content_Range: return "content-range";
            case Content_Security_Policy: return "content-security-policy";
            case Content_Security_Policy_Report_Only: return "content-security-policy-report-only";
            case Content_Type: return "content-type";
            case Cookie: return "cookie";
            case Critical_CHExperimental: return "critical-ch";
            case Cross_Origin_Embedder_Policy: return "cross-origin-embedder-policy";
            case Cross_Origin_Opener_Policy: return "cross-origin-opener-policy";
            case Cross_Origin_Resource_Policy: return "cross-origin-resource-policy";
            case Date: return "date";
            case Device_Memory: return "device-memory";
            case Dictionary_IDExperimental: return "dictionary-id";
            case DNT: return "dnt";
            case DownlinkExperimental: return "downlink";
            case DPR: return "dpr";
            case Early_DataExperimental: return "early-data";
            case ECTExperimental: return "ect";
            case ETag: return "etag";
            case Expect: return "expect";
            case Expect_CT: return "expect-ct";
            case Expires: return "expires";
            case Forwarded: return "forwarded";
            case From: return "from";
            case Host: return "host";
            case If_Match: return "if-match";
            case If_Modified_Since: return "if-modified-since";
            case If_None_Match: return "if-none-match";
            case If_Range: return "if-range";
            case If_Unmodified_Since: return "if-unmodified-since";
            case Integrity_Policy: return "integrity-policy";
            case Integrity_Policy_Report_Only: return "integrity-policy-report-only";
            case Keep_Alive: return "keep-alive";
            case Last_Modified: return "last-modified";
            case Link: return "link";
            case Location: return "location";
            case Max_Forwards: return "max-forwards";
            case NELExperimental: return "nel";
            case No_Vary_SearchExperimental: return "no-vary-search";
            case Observe_Browsing_TopicsExperimental: return "observe-browsing-topics";
            case Origin: return "origin";
            case Origin_Agent_Cluster: return "origin-agent-cluster";
            case Feature_PolicyExperimental: return "feature-policy";
            case Pragma: return "pragma";
            case Prefer: return "prefer";
            case Preference_Applied: return "preference-applied";
            case Priority: return "priority";
            case Proxy_Authenticate: return "proxy-authenticate";
            case Proxy_Authorization: return "proxy-authorization";
            case Range: return "range";
            case Referer: return "referer";
            case Referrer_Policy: return "referrer-policy";
            case Refresh: return "refresh";
            case Report_To: return "report-to";
            case Reporting_Endpoints: return "reporting-endpoints";
            case Repr_Digest: return "repr-digest";
            case Retry_After: return "retry-after";
            case RTTExperimental: return "rtt";
            case Save_DataExperimental: return "save-data";
            case Sec_Browsing_TopicsExperimental: return "sec-browsing-topics";
            case Sec_CH_Prefers_Color_SchemeExperimental: return "sec-ch-prefers-color-scheme";
            case Sec_CH_Prefers_Reduced_MotionExperimental: return "sec-ch-prefers-reduced-motion";
            case Sec_CH_Prefers_Reduced_TransparencyExperimental: return "sec-ch-prefers-reduced-transparency";
            case Sec_CH_UAExperimental: return "sec-ch-ua";
            case Sec_CH_UA_ArchExperimental: return "sec-ch-ua-arch";
            case Sec_CH_UA_BitnessExperimental: return "sec-ch-ua-bitness";
            case Sec_CH_UA_Form_FactorsExperimental: return "sec-ch-ua-form-factors";
            case Sec_CH_UA_Full_Version: return "sec-ch-ua-full-version";
            case Sec_CH_UA_Full_Version_ListExperimental: return "sec-ch-ua-full-version-list";
            case Sec_CH_UA_MobileExperimental: return "sec-ch-ua-mobile";
            case Sec_CH_UA_ModelExperimental: return "sec-ch-ua-model";
            case Sec_CH_UA_PlatformExperimental: return "sec-ch-ua-platform";
            case Sec_CH_UA_Platform_VersionExperimental: return "sec-ch-ua-platform-version";
            case Sec_CH_UA_WoW64Experimental: return "sec-ch-ua-wow64";
            case Sec_Fetch_Dest: return "sec-fetch-dest";
            case Sec_Fetch_Mode: return "sec-fetch-mode";
            case Sec_Fetch_Site: return "sec-fetch-site";
            case Sec_Fetch_User: return "sec-fetch-user";
            case Sec_GPCExperimental: return "sec-gpc";
            case Sec_Purpose: return "sec-purpose";
            case Sec_Speculation_TagsExperimental: return "sec-speculation-tags";
            case Sec_WebSocket_Accept: return "sec-websocket-accept";
            case Sec_WebSocket_Extensions: return "sec-websocket-extensions";
            case Sec_WebSocket_Key: return "sec-websocket-key";
            case Sec_WebSocket_Protocol: return "sec-websocket-protocol";
            case Sec_WebSocket_Version: return "sec-websocket-version";
            case Server: return "server";
            case Server_Timing: return "server-timing";
            case Service_Worker: return "service-worker";
            case Service_Worker_Allowed: return "service-worker-allowed";
            case Service_Worker_Navigation_Preload: return "service-worker-navigation-preload";
            case Set_Cookie: return "set-cookie";
            case Set_Login: return "set-login";
            case SourceMap: return "sourcemap";
            case Speculation_RulesExperimental: return "speculation-rules";
            case Strict_Transport_Security: return "strict-transport-security";
            case Supports_Loading_ModeExperimental: return "supports-loading-mode";
            case TE: return "te";
            case Timing_Allow_Origin: return "timing-allow-origin";
            case Tk: return "tk";
            case Trailer: return "trailer";
            case Transfer_Encoding: return "transfer-encoding";
            case Upgrade: return "upgrade";
            case Upgrade_Insecure_Requests: return "upgrade-insecure-requests";
            case Use_As_DictionaryExperimental: return "use-as-dictionary";
            case User_Agent: return "user-agent";
            case Vary: return "vary";
            case Via: return "via";
            case Viewport_Width: return "viewport-width";
            case Want_Digest: return "want-digest";
            case Want_Repr_Digest: return "want-repr-digest";
            case Warning: return "warning";
            case Width: return "width";
            case WWW_Authenticate: return "www-authenticate";
            case X_Content_Type_Options: return "x-content-type-options";
            case X_DNS_Prefetch_Control: return "x-dns-prefetch-control";
            case X_Forwarded_For: return "x-forwarded-for";
            case X_Forwarded_Host: return "x-forwarded-host";
            case X_Forwarded_Proto: return "x-forwarded-proto";
            case X_Frame_Options: return "x-frame-options";
            case X_Permitted_Cross_Domain_Policies: return "x-permitted-cross-domain-policies";
            case X_Powered_By: return "x-powered-by";
            case X_Robots_Tag: return "x-robots-tag";
            case X_XSS_Protection: return "x-xss-protection";
            default: return "";
        }
    }

    inline http_headers_e string_to_header_enum(const std::string &header_str) {
        static std::unordered_map<std::string, http_headers_e> header_map = {
            {"accept", Accept},
            {"accept-ch", Accept_CH},
            {"accept-encoding", Accept_Encoding},
            {"accept-language", Accept_Language},
            {"accept-patch", Accept_Patch},
            {"accept-post", Accept_Post},
            {"accept-ranges", Accept_Ranges},
            {"access-control-allow-credentials", Access_Control_Allow_Credentials},
            {"access-control-allow-headers", Access_Control_Allow_Headers},
            {"access-control-allow-methods", Access_Control_Allow_Methods},
            {"access-control-allow-origin", Access_Control_Allow_Origin},
            {"access-control-expose-headers", Access_Control_Expose_Headers},
            {"access-control-max-age", Access_Control_Max_Age},
            {"access-control-request-headers", Access_Control_Request_Headers},
            {"access-control-request-method", Access_Control_Request_Method},
            {"age", Age},
            {"allow", Allow},
            {"alt-svc", Alt_Svc},
            {"alt-used", Alt_Used},
            {"attribution-reporting-eligible", Attribution_Reporting_EligibleExperimental},
            {"attribution-reporting-register-source", Attribution_Reporting_Register_SourceExperimental},
            {"attribution-reporting-register-trigger", Attribution_Reporting_Register_TriggerExperimental},
            {"authorization", Authorization},
            {"available-dictionary", Available_DictionaryExperimental},
            {"cache-control", Cache_Control},
            {"clear-site-data", Clear_Site_Data},
            {"connection", Connection},
            {"digest", Digest},
            {"content-disposition", Content_Disposition},
            {"content-dpr", Content_DPR},
            {"content-encoding", Content_Encoding},
            {"content-language", Content_Language},
            {"content-length", Content_Length},
            {"content-location", Content_Location},
            {"content-range", Content_Range},
            {"content-security-policy", Content_Security_Policy},
            {"content-security-policy-report-only", Content_Security_Policy_Report_Only},
            {"content-type", Content_Type},
            {"cookie", Cookie},
            {"critical-ch", Critical_CHExperimental},
            {"cross-origin-embedder-policy", Cross_Origin_Embedder_Policy},
            {"cross-origin-opener-policy", Cross_Origin_Opener_Policy},
            {"cross-origin-resource-policy", Cross_Origin_Resource_Policy},
            {"date", Date},
            {"device-memory", Device_Memory},
            {"dictionary-id", Dictionary_IDExperimental},
            {"dnt", DNT},
            {"downlink", DownlinkExperimental},
            {"dpr", DPR},
            {"early-data", Early_DataExperimental},
            {"ect", ECTExperimental},
            {"etag", ETag},
            {"expect", Expect},
            {"expect-ct", Expect_CT},
            {"expires", Expires},
            {"forwarded", Forwarded},
            {"from", From},
            {"host", Host},
            {"if-match", If_Match},
            {"if-modified-since", If_Modified_Since},
            {"if-none-match", If_None_Match},
            {"if-range", If_Range},
            {"if-unmodified-since", If_Unmodified_Since},
            {"integrity-policy", Integrity_Policy},
            {"integrity-policy-report-only", Integrity_Policy_Report_Only},
            {"keep-alive", Keep_Alive},
            {"last-modified", Last_Modified},
            {"link", Link},
            {"location", Location},
            {"max-forwards", Max_Forwards},
            {"nel", NELExperimental},
            {"no-vary-search", No_Vary_SearchExperimental},
            {"observe-browsing-topics", Observe_Browsing_TopicsExperimental},
            {"origin", Origin},
            {"origin-agent-cluster", Origin_Agent_Cluster},
            {"feature-policy", Feature_PolicyExperimental},
            {"pragma", Pragma},
            {"prefer", Prefer},
            {"preference-applied", Preference_Applied},
            {"priority", Priority},
            {"proxy-authenticate", Proxy_Authenticate},
            {"proxy-authorization", Proxy_Authorization},
            {"range", Range},
            {"referer", Referer},
            {"referrer-policy", Referrer_Policy},
            {"refresh", Refresh},
            {"report-to", Report_To},
            {"reporting-endpoints", Reporting_Endpoints},
            {"repr-digest", Repr_Digest},
            {"retry-after", Retry_After},
            {"rtt", RTTExperimental},
            {"save-data", Save_DataExperimental},
            {"sec-browsing-topics", Sec_Browsing_TopicsExperimental},
            {"sec-ch-prefers-color-scheme", Sec_CH_Prefers_Color_SchemeExperimental},
            {"sec-ch-prefers-reduced-motion", Sec_CH_Prefers_Reduced_MotionExperimental},
            {"sec-ch-prefers-reduced-transparency", Sec_CH_Prefers_Reduced_TransparencyExperimental},
            {"sec-ch-ua", Sec_CH_UAExperimental},
            {"sec-ch-ua-arch", Sec_CH_UA_ArchExperimental},
            {"sec-ch-ua-bitness", Sec_CH_UA_BitnessExperimental},
            {"sec-ch-ua-form-factors", Sec_CH_UA_Form_FactorsExperimental},
            {"sec-ch-ua-full-version", Sec_CH_UA_Full_Version},
            {"sec-ch-ua-full-version-list", Sec_CH_UA_Full_Version_ListExperimental},
            {"sec-ch-ua-mobile", Sec_CH_UA_MobileExperimental},
            {"sec-ch-ua-model", Sec_CH_UA_ModelExperimental},
            {"sec-ch-ua-platform", Sec_CH_UA_PlatformExperimental},
            {"sec-ch-ua-platform-version", Sec_CH_UA_Platform_VersionExperimental},
            {"sec-ch-ua-wow64", Sec_CH_UA_WoW64Experimental},
            {"sec-fetch-dest", Sec_Fetch_Dest},
            {"sec-fetch-mode", Sec_Fetch_Mode},
            {"sec-fetch-site", Sec_Fetch_Site},
            {"sec-fetch-user", Sec_Fetch_User},
            {"sec-gpc", Sec_GPCExperimental},
            {"sec-purpose", Sec_Purpose},
            {"sec-speculation-tags", Sec_Speculation_TagsExperimental},
            {"sec-websocket-accept", Sec_WebSocket_Accept},
            {"sec-websocket-extensions", Sec_WebSocket_Extensions},
            {"sec-websocket-key", Sec_WebSocket_Key},
            {"sec-websocket-protocol", Sec_WebSocket_Protocol},
            {"sec-websocket-version", Sec_WebSocket_Version},
            {"server", Server},
            {"server-timing", Server_Timing},
            {"service-worker", Service_Worker},
            {"service-worker-allowed", Service_Worker_Allowed},
            {"service-worker-navigation-preload", Service_Worker_Navigation_Preload},
            {"set-cookie", Set_Cookie},
            {"set-login", Set_Login},
            {"sourcemap", SourceMap},
            {"speculation-rules", Speculation_RulesExperimental},
            {"strict-transport-security", Strict_Transport_Security},
            {"supports-loading-mode", Supports_Loading_ModeExperimental},
            {"te", TE},
            {"timing-allow-origin", Timing_Allow_Origin},
            {"tk", Tk},
            {"trailer", Trailer},
            {"transfer-encoding", Transfer_Encoding},
            {"upgrade", Upgrade},
            {"upgrade-insecure-requests", Upgrade_Insecure_Requests},
            {"use-as-dictionary", Use_As_DictionaryExperimental},
            {"user-agent", User_Agent},
            {"vary", Vary},
            {"via", Via},
            {"viewport-width", Viewport_Width},
            {"want-digest", Want_Digest},
            {"want-repr-digest", Want_Repr_Digest},
            {"warning", Warning},
            {"width", Width},
            {"www-authenticate", WWW_Authenticate},
            {"x-content-type-options", X_Content_Type_Options},
            {"x-dns-prefetch-control", X_DNS_Prefetch_Control},
            {"x-forwarded-for", X_Forwarded_For},
            {"x-forwarded-host", X_Forwarded_Host},
            {"x-forwarded-proto", X_Forwarded_Proto},
            {"x-frame-options", X_Frame_Options},
            {"x-permitted-cross-domain-policies", X_Permitted_Cross_Domain_Policies},
            {"x-powered-by", X_Powered_By},
            {"x-robots-tag", X_Robots_Tag},
            {"x-xss-protection", X_XSS_Protection}
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
                        "content-length: " + std::to_string(req.body.length()) + "\r\n";
            }
        }
        payload += "\r\n";

        if (!req.body.empty()) payload += req.body;

        std::cout << payload << std::endl;

        return payload;
    }

    inline std::string prepare_response(const http_response_t &res) {
        std::string payload;

        payload = "HTTP/" + std::string(res.version) + " " + std::to_string(res.status_code) + " " + res.status_message
                  + "\r\n";
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
            req.headers.insert_or_assign(string_to_header_enum(string_to_lower(key)), value);
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
            res.headers.insert_or_assign(string_to_header_enum(string_to_lower(key)), value);
        }
    }
}
