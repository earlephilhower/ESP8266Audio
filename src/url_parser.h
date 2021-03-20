/*
basic parts from https://github.com/jaysonsantos/url-parser-c, 
converted to h class and fixed bugs by jayzakk@gmail.com
For ESP8266Audio project

Don't use https, it won't work without local CA/certificates.
Someone might add BearSSL and use setInsecure()
*/

#ifndef url_parser_h
#define url_parser_h

#include <assert.h>
#include <string>
#include <cstring>

class UrlParser
{

private:
    char *protocol = NULL;
    char *host = NULL;
    int port;
    char *path = NULL;
    char *query_string = NULL;

    void free_parsed_url()
    {
        if (protocol != NULL)
        {
            free(protocol);
            protocol = NULL;
        }
        if (host != NULL)
        {
            free(host);
            host = NULL;
        }
        if (path != NULL)
        {
            free(path);
            path = NULL;
        }
        if (query_string != NULL)
        {
            free(query_string);
            query_string = NULL;
        }
    }

    void parse_url(const char *url)
    {
        char *local_url = (char *)malloc(sizeof(char) * (strlen(url) + 1));
        char *token;
        char *token_host;
        char *host_port;

        char *token_ptr;
        char *host_token_ptr;

        strcpy(local_url, url);
        free_parsed_url();

        token = strtok_r(local_url, ":", &token_ptr);
        protocol = (char *)malloc(sizeof(char) * strlen(token) + 1);

        strcpy(protocol, token);
        for (int i = 0; i < strlen(protocol); i++)
        {
            protocol[i] = tolower(protocol[i]);
        }

        // Host:Port
        token = strtok_r(NULL, "/", &token_ptr);
        if (token)
        {
            host_port = (char *)malloc(sizeof(char) * (strlen(token) + 1));
            strcpy(host_port, token);
        }
        else
        {
            host_port = (char *)malloc(sizeof(char) * 1);
            strcpy(host_port, "");
        }

        token_host = strtok_r(host_port, ":", &host_token_ptr);
        if (token_host)
        {
            host = (char *)malloc(sizeof(char) * strlen(token_host) + 1);
            strcpy(host, token_host);
        }
        else
        {
            host = NULL;
        }

        // Port
        token_host = strtok_r(NULL, ":", &host_token_ptr);
        if (token_host)
        {
            port = atoi(token_host);
        }
        else
        {
            port = 0;
            if (isHTTP())
            {
                port = 80;
            }
            if (isHTTPS())
            {
                port = 443;
            }
            if (isFTP())
            {
                port = 21;
            }
        }

        token_host = strtok_r(NULL, ":", &host_token_ptr);
        assert(token_host == NULL);

        token = strtok_r(NULL, "?", &token_ptr);
        if (token)
        {
            path = (char *)malloc(sizeof(char) * (strlen(token) + 2));
            strcpy(path, "/");
            strcat(path, token);
        }
        else
        {
            if (token_ptr == NULL)
            {
                path = (char *)malloc(sizeof(char) * 2);
                strcpy(path, "/");
            }
            else
            {
                path = (char *)malloc(sizeof(char) * (strlen(token_ptr) + 2));
                strcpy(path, "/");
                strcat(path, token_ptr);
            }
        }

        token = strtok_r(NULL, "?", &token_ptr);
        if (token)
        {
            query_string = (char *)malloc(sizeof(char) * (strlen(token) + 1));
            strcpy(query_string, token);
        }
        else
        {
            query_string = NULL;
        }

        token = strtok_r(NULL, "?", &token_ptr);
        assert(token == NULL);

        free(local_url);
        free(host_port);
    }

public:
    UrlParser()
    {
    }

    ~UrlParser()
    {
        free_parsed_url();
    }

    void parse(const char *url)
    {
        parse_url(url);
    }

    char *getProtocol()
    {
        return protocol;
    }

    char *getHost()
    {
        return host;
    }

    char *getPath()
    {
        return path;
    }

    char *getQuery()
    {
        return query_string;
    }

    int getPort()
    {
        return port;
    }

    boolean isHTTP()
    {
        String s = getProtocol();
        return s == "http";
    }

    boolean isHTTPS()
    {
        String s = getProtocol();
        return s == "https";
    }

    boolean isFTP()
    {
        String s = getProtocol();
        return s == "ftp";
    }
};

#endif
