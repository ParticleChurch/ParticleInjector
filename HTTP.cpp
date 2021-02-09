#include "HTTP.hpp"
#include "Debug.hpp"

namespace HTTP
{
    std::string userAgent = "WinHTTP/1.0";
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT; // 443
    DWORD requestFlags = INTERNET_FLAG_SECURE | defaultNoCacheFlags | defaultBaseFlags;
    std::string contentType = "application/x-www-form-urlencoded";

    byte* Post(std::string URL, std::string input, DWORD* bytesRead)
    {
        // split string into host and directory
        // [ garbage ][     host part    ][     directory part     ]
        //   https://    www.example.com     /subdirectory/api.php

        // remove protocol from url
        if (URL.rfind("http://") == 0)
        {
            URL = URL.substr(7);
        }
        else if (URL.rfind("https://") == 0)
        {
            URL = URL.substr(8);
        }

        // split host and directory
        int directoryIndex = URL.find("/");
        std::string host = URL;
        std::string directory = "";
        if (directoryIndex >= 0)
        {
            host = URL.substr(0, directoryIndex);
            directory = URL.substr(directoryIndex + 1);
        }

        HINTERNET hInternet = InternetOpen(
            userAgent.c_str(),
            INTERNET_OPEN_TYPE_PRECONFIG,
            NULL, NULL,
            0
        );
        if (!hInternet)
        {
            Debug::Log("err; !hInternet " + std::to_string(GetLastError()));
            return nullptr;
        }

        HINTERNET hConnection = InternetConnect(
            hInternet,
            host.c_str(),
            INTERNET_DEFAULT_HTTPS_PORT,
            NULL, NULL,
            INTERNET_SERVICE_HTTP,
            0, 0
        );
        if (!hConnection)
        {
            Debug::Log("err; !hConnection " + std::to_string(GetLastError()));
            return nullptr;
        }

        // "Long pointer to a null-terminated array of string pointers indicating content types accepted by the client"
        LPCSTR acceptContentTypes[]{ "*/*", 0 };

        HINTERNET hRequest = HttpOpenRequest(hConnection,
            "POST",
            directory.c_str(),
            "HTTP/1.1",
            0,
            acceptContentTypes,
            requestFlags,
            0
        );
        if (!hRequest)
        {
            Debug::Log("err; !hRequest " + std::to_string(GetLastError()));
            return nullptr;
        }

        bool headersAdded = HttpAddRequestHeaders(
            hRequest,
            ("Content-Type: " + contentType + "\r\n").c_str(),
            -1L,
            HTTP_ADDREQ_FLAG_REPLACE
        );

        bool requestSuccess = HttpSendRequest(hRequest,
            // headers and length, not supported yet
            NULL, 0,
            // post data and post length
            (void*)input.c_str(), input.size()
        );
        
        if (!requestSuccess)
        {
            Debug::Log("err; !requestSuccess " + std::to_string(GetLastError()));
            return nullptr;
        }

        // dynamic buffer to hold file
        byte* fileBuffer = NULL;
        DWORD fileSize = 0;
        
        // buffer for each kb of the file
        byte* chunkBuffer = (byte*)malloc(1024);
        DWORD chunkSize = 1024;
        DWORD chunkBytesRead = 0;
        if (!chunkBuffer)
        {
            Debug::Log("err; !chunkBuffer");
            return nullptr;
        }

        do
        {
            chunkBytesRead = 0;
            InternetReadFile(hRequest, chunkBuffer, chunkSize, &chunkBytesRead);

            byte* re = (byte*)realloc(fileBuffer, fileSize + chunkBytesRead);
            if (!re)
            {
                Debug::Log("err; !realloc");
                free(fileBuffer);
                free(chunkBuffer);
                return nullptr;
            }
            else
            {
                fileBuffer = re;
            }
            memcpy(fileBuffer + fileSize, chunkBuffer, chunkBytesRead);
            fileSize += chunkBytesRead;

        } while (chunkBytesRead > 0);
        free(chunkBuffer);

        if (fileSize == 0)
        {
            Debug::Log("err; size = 0 " + std::to_string(GetLastError()));
            free(fileBuffer);
            return nullptr;
        }
        if (bytesRead)
            *bytesRead = fileSize;

        return fileBuffer;
    }
}