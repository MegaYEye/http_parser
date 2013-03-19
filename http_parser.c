#include "http_parser.h"

#define _BSD_SOURCE

#include <string.h>
#include <stdlib.h>

#define curchar buffer[p]
#define nextchar buffer[p+1]

#define next() \
    buffer[p] = 0; \
    buffer += (p+1); \
    p = 0;

#define if_eof(r) if (curchar == 0) return r;

#define check_method(m) \
    if (!p && !strcmp(m, message->method)) p = sizeof(m);

#define check_protocol(p) \
    if (!(strcmp("HTTP/1.0", p) || strcmp("HTTP/1.1", p))) {\
        return 400;\
    }

#define next_at_newline(r) \
    buffer = strchr(buffer, '\n'); \
    if (!buffer) return r; \
    next(); \
    if (curchar == '\r') {\
        next(); \
    }

int http_parser(char *buffer, struct http_message *message, int type) {
    if (type != http_request && type != http_response) return -1;

    int p = 0;
    memset(message, 0, sizeof(struct http_message));
    message->type = type;

    message->method = type == http_request ? buffer : NULL;
    message->protocol = type == http_response ? buffer : NULL;
    buffer = strchr(buffer, ' ');
    if (!buffer) return 400;
    next();

    if (type == http_request) {
        check_method("GET");
        check_method("HEAD");
        check_method("POST");
        check_method("PUT");
        check_method("DELETE");
        check_method("TRACE");
        check_method("OPTIONS");
        check_method("CONNECT");
        check_method("PATCH");
        if (!p) return 400;
        p = 0;
    } else {
        check_protocol(message->protocol);
    }

    message->url = type == http_request ? buffer : NULL;
    message->status = type == http_response ? buffer : NULL;
    buffer = strchr(buffer, ' ');
    if (!buffer) return 414;
    next();

    if (type == http_request && strstr(message->url, "/") != message->url)
        return 400;
    else if (type == http_response && atoi(message->status) == 0)
        return 400;

    message->protocol = type == http_request ? buffer : message-protocol;
    message->line = type == http_response ? buffer : NULL;
    next_at_newline(400);
    if (type == http_request) {
        check_protocol(message->protocol);
    }

    int containsHostHeader = 0;
    int h_index = 0;


    while (1) {
        if (curchar != 0 && (curchar == '\n' || (curchar == '\r' && nextchar == '\n'))) {
            next();
            if (curchar == '\n') {
                next();
            }
            break;
        }
        if (h_index < http_header_max)
        message->headers[h_index].name = buffer;

        buffer = strchr(buffer, ':');
        if (!buffer) return 413;

        next();

        while (curchar != 0 && (curchar == ' ' || curchar == '\r' || curchar == '\n' || curchar == '\t')) {
            next();
        }
        if_eof(413);

        if (h_index < http_header_max)
            message->headers[h_index].value = buffer;

        next_at_newline(413);

        struct http_header *header = &message->headers[h_index];

        if (type == http_request) {
            // is connection keep-alive? phase 1
            if (!strcasecmp("Connection", header->name)) {
                if (message->protocol[7] == '0' && !strcasecmp("Keep-Alive", header->value)) {
                    message->connection_keepalive = 1;
                } else if (message->protocol[7] == '1' && !strcasecmp("Close", header->value)) {
                    message->connection_keepalive = 0;
                }
                message->connection_close = ! message->connection_keepalive;
            }

            // is gzip supported?
            if (!strcasecmp("Accept-Encoding", header->name)) {
              if (strstr(header->value, "gzip")) {
                message->gzip_supported = 1;
              }
            }

            // check Host header presence, required in HTTP/1.1
            if (!strcasecmp("Host", header->name)) {
              containsHostHeader = 1;
            }
        }

        h_index++;
    }


    if (type == http_request) {
        // is connection keep-alive? phase 2
        if (! message->connection_keepalive && ! message->connection_close) {
            if (message->protocol[7] == '0') {
                message->connection_keepalive = 0;
            } else {
                message->connection_keepalive = 1;
            }
            message->connection_close = ! message->connection_keepalive;
        }

        // required Host header
        if (message->protocol[7] == '1' && !containsHostHeader) {
            return 400;
        }
    }

    message->body = buffer;

    return 0;
}
