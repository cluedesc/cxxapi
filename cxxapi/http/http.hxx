/**
 * @file http.hxx
 * @brief Core HTTP types, status codes, methods, and utility functions for the cxxapi.
 */

#ifndef CXXAPI_HTTP_HXX
#define CXXAPI_HTTP_HXX

#include "utils/utils.hxx"

namespace cxxapi::http {
    /** @brief Type alias for HTTP URI. */
    using uri_t = std::string;

    /** @brief Type alias for HTTP message body. */
    using body_t = std::string;

    /** @brief Type alias for HTTP path. */
    using path_t = std::string;

    /** @brief Type alias for JSON payloads. */
    using json_t = shared::json_traits_t;

    /** @brief Type alias for HTTP headers (case-insensitive keys). */
    using headers_t = std::map<std::string, std::string, internal::ci_less_t>;

    /** @brief Type alias for HTTP query parameters (case-insensitive keys). */
    using params_t = std::map<std::string, std::string, internal::ci_less_t>;

    /** @brief Type alias for HTTP cookies. */
    using cookies_t = std::vector<std::string>;

    /**
     * @brief HTTP status codes.
     */
    enum struct e_status : std::int16_t {
        // 1xx Informational
        continue_status = 100,     ///< The server has received the request and is continuing the process.
        switching_protocols = 101, ///< The server is switching protocols as per the client's request.
        processing = 102,          ///< The server has received the request and is processing it (WebDAV).
        early_hints = 103,         ///< The server is sending preliminary information.

        // 2xx Success
        ok = 200,                            ///< The request was successful.
        created = 201,                       ///< The resource was successfully created.
        accepted = 202,                      ///< The request has been accepted for processing.
        non_authoritative_information = 203, ///< The request was successful, but the information may be from a third-party.
        no_content = 204,                    ///< The request was successful but there is no content to return.
        reset_content = 205,                 ///< The request was successful and the client should reset the view.
        partial_content = 206,               ///< The server is returning partial content.
        multi_status = 207,                  ///< Multiple status codes are provided (WebDAV).
        already_reported = 208,              ///< The members of a DAV binding have already been reported.
        im_used = 226,                       ///< The server has fulfilled the request using the IMS (Instance Manipulation) protocol.

        // 3xx Redirection
        multiple_choices = 300,   ///< The requested resource has multiple options.
        moved_permanently = 301,  ///< The resource has been permanently moved to a new URL.
        found = 302,              ///< The resource has been temporarily moved to a new URL.
        see_other = 303,          ///< The client should make a GET request to another URL.
        not_modified = 304,       ///< The resource has not been modified since the last request.
        use_proxy = 305,          ///< The client should use a proxy to access the resource.
        temporary_redirect = 307, ///< The resource has been temporarily redirected.
        permanent_redirect = 308, ///< The resource has been permanently redirected.

        // 4xx Client Error
        bad_request = 400,                     ///< The request was malformed or invalid.
        unauthorized = 401,                    ///< The client must authenticate to access the resource.
        payment_required = 402,                ///< The request cannot be processed without payment.
        forbidden = 403,                       ///< The client does not have permission to access the resource.
        not_found = 404,                       ///< The resource could not be found.
        method_not_allowed = 405,              ///< The request method is not allowed.
        not_acceptable = 406,                  ///< The server cannot generate a response that matches the client's constraints.
        proxy_authentication_required = 407,   ///< Proxy authentication is required.
        request_timeout = 408,                 ///< The client did not send a request in the time allowed.
        conflict = 409,                        ///< The request could not be processed due to a conflict.
        gone = 410,                            ///< The resource is no longer available.
        length_required = 411,                 ///< The request must specify a valid content length.
        precondition_failed = 412,             ///< One or more preconditions in the request failed.
        payload_too_large = 413,               ///< The request payload is too large to process.
        uri_too_long = 414,                    ///< The URI is too long to process.
        unsupported_media_type = 415,          ///< The request's media type is not supported.
        range_not_satisfiable = 416,           ///< The requested range cannot be satisfied.
        expectation_failed = 417,              ///< The expectation specified in the request header could not be met.
        im_a_teapot = 418,                     ///< The server refuses to brew coffee due to being a teapot (RFC 2324).
        misdirected_request = 421,             ///< The request was directed to the wrong server.
        unprocessable_entity = 422,            ///< The server understands the request but cannot process it (WebDAV).
        locked = 423,                          ///< The resource is locked (WebDAV).
        failed_dependency = 424,               ///< The request failed due to a failed dependency (WebDAV).
        too_early = 425,                       ///< The server is unwilling to process the request due to it being too early.
        upgrade_required = 426,                ///< The client should upgrade to a newer protocol.
        precondition_required = 428,           ///< A precondition required by the server is not met.
        too_many_requests = 429,               ///< The client has sent too many requests in a given amount of time.
        request_header_fields_too_large = 431, ///< The request's header fields are too large.
        unavailable_for_legal_reasons = 451,   ///< The resource is unavailable due to legal reasons.

        // 5xx Server Error
        internal_server_error = 500,          ///< The server encountered an internal error.
        not_implemented = 501,                ///< The server does not support the requested functionality.
        bad_gateway = 502,                    ///< The server received an invalid response from an upstream server.
        service_unavailable = 503,            ///< The server is temporarily unavailable.
        gateway_timeout = 504,                ///< The server did not receive a timely response from an upstream server.
        http_version_not_supported = 505,     ///< The server does not support the HTTP version used in the request.
        variant_also_negotiates = 506,        ///< The server encountered an error while negotiating a variant response.
        insufficient_storage = 507,           ///< The server cannot store the representation.
        loop_detected = 508,                  ///< An infinite loop was detected during processing.
        not_extended = 510,                   ///< Further extensions are required to fulfill the request.
        network_authentication_required = 511 ///< Network authentication is required.
    };

    /**
     * @brief HTTP request methods.
     */
    enum struct e_method : std::int16_t {
        get,     ///< GET method for retrieving resources
        head,    ///< HEAD method for retrieving headers only
        post,    ///< POST method for creating resources
        put,     ///< PUT method for updating resources
        delete_, ///< DELETE method for removing resources
        connect, ///< CONNECT method for establishing tunnels
        options, ///< OPTIONS method for describing communication options
        trace,   ///< TRACE method for diagnostic purposes
        patch,   ///< PATCH method for partial updates
        unknown  ///< Unknown or unsupported method
    };

    /**
     * @brief Convert HTTP method enum to string.
     * @param method HTTP method enum value.
     * @return String representation of the HTTP method.
     */
    CXXAPI_INLINE constexpr std::string method_to_str(const e_method& method) {
        switch (method) {
            case e_method::get:
                return "GET";
            case e_method::head:
                return "HEAD";
            case e_method::post:
                return "POST";
            case e_method::put:
                return "PUT";
            case e_method::delete_:
                return "DELETE";
            case e_method::connect:
                return "CONNECT";
            case e_method::options:
                return "OPTIONS";
            case e_method::trace:
                return "TRACE";
            case e_method::patch:
                return "PATCH";
            default:
                return "UNKNOWN";
        }
    }

    /**
     * @brief Convert HTTP method string to enum value.
     * @param method_str String representation of the HTTP method.
     * @return Corresponding HTTP method enum value.
     */
    CXXAPI_INLINE constexpr e_method str_to_method(const std::string_view& method_str) {
        switch (utils::fnv1a_hash(method_str)) {
            case utils::fnv1a_hash("GET"):
                return e_method::get;

            case utils::fnv1a_hash("HEAD"):
                return e_method::head;

            case utils::fnv1a_hash("POST"):
                return e_method::post;

            case utils::fnv1a_hash("PUT"):
                return e_method::put;

            case utils::fnv1a_hash("DELETE"):
                return e_method::delete_;

            case utils::fnv1a_hash("CONNECT"):
                return e_method::connect;

            case utils::fnv1a_hash("OPTIONS"):
                return e_method::options;

            case utils::fnv1a_hash("TRACE"):
                return e_method::trace;

            case utils::fnv1a_hash("PATCH"):
                return e_method::patch;

            default:
                return e_method::unknown;
        }
    }

    /**
     * @brief Asynchronously send a chunked HTTP response.
     * @param socket TCP socket to write to.
     * @param str Data chunk to send.
     * @return Awaitable for the asynchronous write operation.
     */
    CXXAPI_NOINLINE static boost::asio::awaitable<void> send_chunk_async(
        boost::asio::ip::tcp::socket& socket,

        const std::string_view& str
    ) {
        co_await boost::asio::async_write(
            socket,

            boost::asio::buffer(fmt::format("{:X}\r\n{}\r\n", str.size(), str)),

            boost::asio::use_awaitable
        );
    }
}

#include "request/request.hxx"

#include "response/response.hxx"

#include "http_ctx/http_ctx.hxx"

#endif // CXXAPI_HTTP_HXX
