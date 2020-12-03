#pragma once

#include <string>
#include <Windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#include "API.hpp"

//#define HTTP_VERBOSE
#define INTERNET_FLAGS INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE
namespace HTTP
{
    /* V2 HTTP */
    template <class PostData, class ResponseFormat>
    bool Post(std::string URL, PostData Data, ResponseFormat* Output)
    {
        size_t ResponseSize = sizeof(ResponseFormat);
        size_t MaximumAllowedResponseSize = 64 * 1024; // 64 kb
        assert(ResponseSize <= MaximumAllowedResponseSize);

#ifdef HTTP_VERBOSE
        std::cout << "Called HTTP::Post with url = " << URL << std::endl;
#endif
        // split into host and directory
        // [ garbage ][     host part    ][     directory part     ]
        //   https://    www.example.com     /subdirectory/api.php
        if (URL.rfind("http://") == 0)
            URL = URL.substr(7);
        else if (URL.rfind("https://") == 0)
            URL = URL.substr(8);

        int FirstSlash = URL.find("/");
        std::string Host = URL;
        std::string Directory = "";
        if (FirstSlash >= 0)
        {
            Host = URL.substr(0, FirstSlash);
            Directory = URL.substr(FirstSlash + 1);
        }

#ifdef HTTP_VERBOSE
        std::cout << "HTTP has determined that the Host is \"" + Host + "\" and Directory is \"" + Directory + "\"" << std::endl;
        std::cout << "Now opening the request..." << std::endl;
#endif // HTTP_VERBOSE

        // make a null terminated array of content types
        LPCSTR ContentType = "application/octet-stream";
        LPCSTR ContentTypes[] = { ContentType, NULL };

        static HINTERNET hInternet = InternetOpenA(
            // user agent
            NULL, // TODO maybe rename this to make it harder to find in wireshark?
            // something like a DNS service
            INTERNET_OPEN_TYPE_PRECONFIG,
            // only used if above is set to INTERNET_OPEN_TYPE_PROXY
            NULL, NULL,
            // flags
            0
        );
        HINTERNET hConnection = InternetConnectA(
            // our internet configuration
            hInternet,
            // host, e.g. www.example.com, note: no https://
            Host.c_str(),
            // port to connect on 443 = https
            INTERNET_DEFAULT_HTTPS_PORT,
            // uername and password, null because we authenticate in the request itself
            NULL, NULL,
            // service type (allowed: ftp, http, gopher)
            INTERNET_SERVICE_HTTP,
            // flags
            0,
            // context, not used in this application
            0
        );
        HINTERNET hRequest = HttpOpenRequestA(hConnection,
            // method
            "POST",
            // file to search for
            Directory.c_str(),
            // http version
            "HTTP/1.1",
            // referrer
            0,
            // Long pointer to a null-terminated array of string pointers indicating content types accepted by the client
            ContentTypes,
            // flags
            INTERNET_FLAGS,
            // request context, not useful for this application
            0
        );
        bool HTTPSuccess = HttpSendRequestA(hRequest,
            // headers and length
            NULL, 0,
            // post data and post length
            (void*)&Data, sizeof(Data)
        );

#ifdef HTTP_VERBOSE
        if (!HTTPSuccess)
            std::cout << "HTTP::Post failed with error code " << GetLastError() << std::endl;
        else
            std::cout << "HTTP::Post succeeded" << std::endl;
#endif // HTTP_VERBOSE

        if (!HTTPSuccess) return false;

        // read binary data from response
        size_t BytesRead = 0;
        BYTE* Buffer = (BYTE*)malloc(MaximumAllowedResponseSize);
        bool ReadFileSuccess = InternetReadFile(hRequest, Buffer, MaximumAllowedResponseSize, (LPDWORD)(&BytesRead));

#ifdef HTTP_VERBOSE
        if (!ReadFileSuccess)
            std::cout << "Failed to read response w/ error " << GetLastError() << std::endl;
        else
            std::cout << "Successfully validated response" << std::endl;
#endif // HTTP_VERBOSE

        if (!ReadFileSuccess) return false;

        bool responseValid = BytesRead == ResponseSize;

#ifdef HTTP_VERBOSE
        if (!responseValid)
            std::cout << "The response was invalid, expected " << ResponseSize << " bytes but got " << BytesRead << " bytes" << std::endl;
        else
            std::cout << "Response is valid: got " << BytesRead << " bytes - now writing to output @ 0x" << std::hex << (DWORD)Output << std::dec << std::endl;
#endif // HTTP_VERBOSE

        if (!responseValid) return false;

        memcpy((void*)Output, (void*)Buffer, ResponseSize);
        free(Buffer);
        return true;
    }
}