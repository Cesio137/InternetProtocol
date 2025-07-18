#pragma once

#define ASIO_NOEXCEPT

#include <asio.hpp>
#include <set>
#include <map>
#ifdef ENABLE_SSL
#include <asio/ssl.hpp>
#include <asio/ssl/stream.hpp>
#endif
#include "ip/tcp/tcpremote.hpp"

using namespace asio::ip;

namespace ip {
    inline extern asio::thread_pool thread_pool(std::thread::hardware_concurrency());

    /**
     * This function stops the threads as soon as possible. As a result of calling stop_threads(), pending function objects may be never be invoked.
     */
    inline void stop_threads() {
        thread_pool.stop();
    }

    /**
     * This function blocks until the threads in the pool have completed. If join_threads() is not called prior to join_threads(), the join_threads() call will wait until the pool has no more outstanding work.
     */
    inline void join_threads() {
        thread_pool.join();
    }

    typedef enum : uint8_t {
        v4 = 0,
        v6 = 1,
    } protocol_type_e;

    typedef enum : uint8_t {
        none = 0x00,
        verify_peer = 0x01,
        verify_fail_if_no_peer_cert = 0x02,
        verify_client_once = 0x04,
    } verify_mode_e;

    typedef asio::ssl::context_base::file_format file_format_e;

    /**
     * @brief Security context configuration options
     *
     * This structure stores the necessary settings to establish a security context
     * for secure communications, including certificates and private keys.
     * All certificate and key values are loaded directly from memory rather than files.
     */
    struct security_context_opts {
        /// Private key content in memory. Can be left empty
        std::string private_key;

        /// Certificate content in memory. Can be left empty
        std::string cert;

        /// Certificate chain content in memory. Can be left empty
        std::string cert_chain;

        /// RSA private key content in memory. Can be left empty
        std::string rsa_private_key;

        /// Format of the certificate and key data (default: PEM)
        file_format_e file_format = file_format_e::pem;

        /// Verification mode (default: verify_peer)
        verify_mode_e verify_mode = verify_peer;

        /// Hostname for verification. Can be left empty
        std::string host_name_verification;
    };

    // HTTP
    typedef enum : uint8_t {
        UNKNOWN = 0,
        CONNECT = 1,
        DEL = 2,
        GET = 3,
        HEAD = 4,
        OPTIONS = 5,
        PATCH = 6,
        POST = 7,
        PUT = 8,
        TRACE = 9
    } request_method_e;

    typedef enum {
        Unknown,

        Accept,
        Accept_CH,
        Accept_Encoding,
        Accept_Language,
        Accept_Patch,
        Accept_Post,
        Accept_Ranges,
        Access_Control_Allow_Credentials,
        Access_Control_Allow_Headers,
        Access_Control_Allow_Methods,
        Access_Control_Allow_Origin,
        Access_Control_Expose_Headers,
        Access_Control_Max_Age,
        Access_Control_Request_Headers,
        Access_Control_Request_Method,
        Age,
        Allow,
        Alt_Svc,
        Alt_Used,
        Attribution_Reporting_EligibleExperimental,
        Attribution_Reporting_Register_SourceExperimental,
        Attribution_Reporting_Register_TriggerExperimental,
        Authorization,
        Available_DictionaryExperimental,
        Cache_Control,
        Clear_Site_Data,
        Connection,
        Digest,
        Content_Disposition,
        Content_DPR,
        Content_Encoding,
        Content_Language,
        Content_Length,
        Content_Location,
        Content_Range,
        Content_Security_Policy,
        Content_Security_Policy_Report_Only,
        Content_Type,
        Cookie,
        Critical_CHExperimental,
        Cross_Origin_Embedder_Policy,
        Cross_Origin_Opener_Policy,
        Cross_Origin_Resource_Policy,
        Date,
        Device_Memory,
        Dictionary_IDExperimental,
        DNT,
        DownlinkExperimental,
        DPR,
        Early_DataExperimental,
        ECTExperimental,
        ETag,
        Expect,
        Expect_CT,
        Expires,
        Forwarded,
        From,
        Host,
        If_Match,
        If_Modified_Since,
        If_None_Match,
        If_Range,
        If_Unmodified_Since,
        Integrity_Policy,
        Integrity_Policy_Report_Only,
        Keep_Alive,
        Last_Modified,
        Link,
        Location,
        Max_Forwards,
        NELExperimental,
        No_Vary_SearchExperimental,
        Observe_Browsing_TopicsExperimental,
        Origin,
        Origin_Agent_Cluster,
        Feature_PolicyExperimental,
        Pragma,
        Prefer,
        Preference_Applied,
        Priority,
        Proxy_Authenticate,
        Proxy_Authorization,
        Range,
        Referer,
        Referrer_Policy,
        Refresh,
        Report_To,
        Reporting_Endpoints,
        Repr_Digest,
        Retry_After,
        RTTExperimental,
        Save_DataExperimental,
        Sec_Browsing_TopicsExperimental,
        Sec_CH_Prefers_Color_SchemeExperimental,
        Sec_CH_Prefers_Reduced_MotionExperimental,
        Sec_CH_Prefers_Reduced_TransparencyExperimental,
        Sec_CH_UAExperimental,
        Sec_CH_UA_ArchExperimental,
        Sec_CH_UA_BitnessExperimental,
        Sec_CH_UA_Form_FactorsExperimental,
        Sec_CH_UA_Full_Version,
        Sec_CH_UA_Full_Version_ListExperimental,
        Sec_CH_UA_MobileExperimental,
        Sec_CH_UA_ModelExperimental,
        Sec_CH_UA_PlatformExperimental,
        Sec_CH_UA_Platform_VersionExperimental,
        Sec_CH_UA_WoW64Experimental,
        Sec_Fetch_Dest,
        Sec_Fetch_Mode,
        Sec_Fetch_Site,
        Sec_Fetch_User,
        Sec_GPCExperimental,
        Sec_Purpose,
        Sec_Speculation_TagsExperimental,
        Sec_WebSocket_Accept,
        Sec_WebSocket_Extensions,
        Sec_WebSocket_Key,
        Sec_WebSocket_Protocol,
        Sec_WebSocket_Version,
        Server,
        Server_Timing,
        Service_Worker,
        Service_Worker_Allowed,
        Service_Worker_Navigation_Preload,
        Set_Cookie,
        Set_Login,
        SourceMap,
        Speculation_RulesExperimental,
        Strict_Transport_Security,
        Supports_Loading_ModeExperimental,
        TE,
        Timing_Allow_Origin,
        Tk,
        Trailer,
        Transfer_Encoding,
        Upgrade,
        Upgrade_Insecure_Requests,
        Use_As_DictionaryExperimental,
        User_Agent,
        Vary,
        Via,
        Viewport_Width,
        Want_Digest,
        Want_Repr_Digest,
        Warning,
        Width,
        WWW_Authenticate,
        X_Content_Type_Options,
        X_DNS_Prefetch_Control,
        X_Forwarded_For,
        X_Forwarded_Host,
        X_Forwarded_Proto,
        X_Frame_Options,
        X_Permitted_Cross_Domain_Policies,
        X_Powered_By,
        X_Robots_Tag,
        X_XSS_Protection
    } headers_e;

    struct http_request_t {
        request_method_e method = GET;
        std::string path = "/";
        std::string version = "1.1";
        std::map<std::string, std::string> params;
        std::map<headers_e, std::string> headers;
        std::string body;
    };

    struct http_response_t {
        int status_code = 200;
        std::string status_message;
        std::string version = "1.1";
        std::map<headers_e, std::string> headers;
        std::string body;
    };

    static std::map<int, std::string> response_status_t = {
        // 1xx Informational
        {100, "Continue"},
        {101, "Switching Protocols"},
        {102, "Processing"},
        {103, "Early Hints"},

        // 2xx Success
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {203, "Non-Authoritative Information"},
        {204, "No Content"},
        {205, "Reset Content"},
        {206, "Partial Content"},
        {207, "Multi-Status"},
        {208, "Already Reported"},
        {226, "IM Used"},

        // 3xx Redirection
        {300, "Multiple Choices"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {303, "See Other"},
        {304, "Not Modified"},
        {305, "Use Proxy"},
        {306, "Switch Proxy"},
        {307, "Temporary Redirect"},
        {308, "Permanent Redirect"},

        // 4xx Client Error
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {402, "Payment Required"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {406, "Not Acceptable"},
        {407, "Proxy Authentication Required"},
        {408, "Request Timeout"},
        {409, "Conflict"},
        {410, "Gone"},
        {411, "Length Required"},
        {412, "Precondition Failed"},
        {413, "Payload Too Large"},
        {414, "URI Too Long"},
        {415, "Unsupported Media Type"},
        {416, "Range Not Satisfiable"},
        {417, "Expectation Failed"},
        {418, "I'm a teapot"},
        {421, "Misdirected Request"},
        {422, "Unprocessable Entity"},
        {423, "Locked"},
        {424, "Failed Dependency"},
        {425, "Too Early"},
        {426, "Upgrade Required"},
        {428, "Precondition Required"},
        {429, "Too Many Requests"},
        {431, "Request Header Fields Too Large"},
        {451, "Unavailable For Legal Reasons"},

        // 5xx Server Error
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"},
        {504, "Gateway Timeout"},
        {505, "HTTP Version Not Supported"},
        {506, "Variant Also Negotiates"},
        {507, "Insufficient Storage"},
        {508, "Loop Detected"},
        {510, "Not Extended"},
        {511, "Network Authentication Required"}
    };

    // Client side

    struct client_bind_options_t {
        std::string address;
        std::string port = "8080";
        protocol_type_e protocol = v4;
    };

    struct udp_client_t {
        udp_client_t(): socket(context), resolver(context) {
        }

        asio::io_context context;
        udp::socket socket;
        udp::endpoint endpoint;
        udp::resolver resolver;
    };

    struct tcp_client_t {
        tcp_client_t(): socket(context), resolver(context) {
        }

        asio::io_context context;
        tcp::socket socket;
        tcp::endpoint endpoint;
        tcp::resolver resolver;
    };

#ifdef ENABLE_SSL
    struct tcp_client_ssl_t {
        tcp_client_ssl_t(): ssl_context(asio::ssl::context::tlsv13_client),
                            ssl_socket(context, ssl_context),
                            resolver(context) {
        }

        asio::io_context context;
        asio::ssl::context ssl_context;
        tcp::resolver resolver;
        tcp::endpoint endpoint;
        asio::ssl::stream<tcp::socket> ssl_socket;
    };
#endif

    // Server side

    struct server_bind_options_t {
        std::string address;
        uint16_t port = 8080;
        protocol_type_e protocol = v4;
        bool reuse_address = true;
    };

    struct udp_server_t {
        udp_server_t(): socket(context) {
        }

        asio::io_context context;
        udp::socket socket;
        udp::endpoint remote_endpoint;
    };

    struct tcp_server_t {
        tcp_server_t(): acceptor(context) {
        }

        asio::io_context context;
        tcp::acceptor acceptor;
        std::set<std::shared_ptr<tcp_remote_c> > clients;
    };

#ifdef ENABLE_SSL
    struct tcp_server_ssl_t {
        tcp_server_ssl_t(): acceptor(context), ssl_context(asio::ssl::context::tlsv13_server) {
        }

        asio::io_context context;
        asio::ssl::context ssl_context;
        tcp::acceptor acceptor;
        std::set<std::shared_ptr<tcp_remote_ssl_c> > ssl_clients;
    };
#endif
}
