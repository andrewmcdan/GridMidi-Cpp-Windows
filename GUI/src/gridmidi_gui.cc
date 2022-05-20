#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#include <napi.h>
#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <iostream>
#define BUFSIZE 8192

Napi::Value PipeMessage( const Napi::CallbackInfo& info){
    std::cout << "********** Entering cpp module code. fn(PipeMessage) *************" << std::endl << std::endl;
    Napi::Env env = info.Env();

    // Get the first paramter passed to the function from the originating JS. It should be a string.
    // Convert it to s std::string.
    std::string aString = info[0].ToString().Utf8Value().c_str();

    std::cout << "astring: " << aString << std::endl;
    
    // Create a LPCTSTR
    LPCTSTR lpvMessage = std::wstring(aString.begin(),aString.end()).c_str();

    _tprintf(TEXT("lpvMessage: %s\n"), lpvMessage);

    HANDLE hPipe;
    TCHAR  chBuf[BUFSIZE];
    BOOL   fSuccess = FALSE;
    DWORD  cbRead, cbToWrite, cbWritten, dwMode;
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\gridMidi");

    // Try to open a named pipe; wait for it, if necessary. 

    while (1)
    {
        hPipe = CreateFileW(
            lpszPipename,   // pipe name 
            GENERIC_READ |  // read and write access 
            GENERIC_WRITE,
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            0,              // default attributes 
            NULL);          // no template file 


        // Break if the pipe handle is valid. 
        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs. 
        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            _tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
            std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl << std::endl;
            return Napi::Number::New(env,-10 + GetLastError());
        }

        // All pipe instances are busy, so wait for 20 seconds. 
        if (!WaitNamedPipe(lpszPipename, 20000))
        {
            printf("Could not open pipe: 20 second wait timed out.");
            std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl << std::endl;
            return Napi::Number::New(env,-2);
        }
    }

    // Here we set lpvMessage to the same value as above. This has to be done because of a weird glitch in CreateFileW()
    // that overwrites lpvMessage with the either garbage or the contents of lpszPipename. Never could figure out why
    // this was happening, but setting lpvMessage again fixed the issue.
    lpvMessage = std::wstring(aString.begin(),aString.end()).c_str();

    // The pipe connected; change to message-read mode. 
    dwMode = PIPE_READMODE_MESSAGE;
    fSuccess = SetNamedPipeHandleState(
        hPipe,    // pipe handle 
        &dwMode,  // new pipe mode 
        NULL,     // don't set maximum bytes 
        NULL);    // don't set maximum time 
    if (!fSuccess)
    {
        _tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
        std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl << std::endl;
        return Napi::Number::New(env,-3);
    }

    // Send a message to the pipe server. 
    cbToWrite = (lstrlen(lpvMessage) + 1) * sizeof(TCHAR);
    _tprintf(TEXT("Sending %d byte message: \"%s\"\n"), cbToWrite, lpvMessage);

    fSuccess = WriteFile(
        hPipe,                  // pipe handle 
        lpvMessage,             // message 
        cbToWrite,              // message length 
        &cbWritten,             // bytes written 
        NULL);                  // not overlapped 

    if (!fSuccess)
    {
        _tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
        std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl << std::endl;
        return Napi::Number::New(env,-4);
    }

    printf("Message sent to server, receiving reply as follows:\n");

    do
    {
        // Read from the pipe. 
        fSuccess = ReadFile(
            hPipe,    // pipe handle 
            chBuf,    // buffer to receive reply 
            BUFSIZE * sizeof(TCHAR),  // size of buffer 
            &cbRead,  // number of bytes read 
            NULL);    // not overlapped 

        if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
            break;

        _tprintf(TEXT("\"%s\"\n"), chBuf);
        // _tprintf(TEXT("\"%i\"\n"), cbRead);
    } while (!fSuccess);  // repeat loop if ERROR_MORE_DATA 

    if (!fSuccess)
    {
        _tprintf(TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError());
        std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl;
        return Napi::Number::New(env,-5);
    }

    printf("<End of message>\n");
    // _getch();

    CloseHandle(hPipe);
    std::wstring temp = std::wstring(chBuf);
    std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl;
    return Napi::String::New(env,std::string(temp.begin(),temp.end()));
}

Napi::Value IsPipeReady( const Napi::CallbackInfo& info){
    std::cout << "** Entering cpp module code. fn(IsPipeReady) **";
    Napi::Env env = info.Env();
    HANDLE hPipe;
    LPCTSTR lpszPipename = TEXT("\\\\.\\pipe\\gridMidi");

    // Try to open a named pipe.
    while (1)
    {
        hPipe = CreateFile(
            lpszPipename,   // pipe name 
            GENERIC_READ |  // read and write access 
            GENERIC_WRITE,
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            0,              // default attributes 
            NULL);          // no template file 

        // Break if the pipe handle is valid. 
        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs. 
        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            //_tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
            std::cout << "** Exiting cpp module code. fn(IsPipeReady) **" << std::endl;
            return Napi::Boolean::New(env,false);
        }
    }
    CloseHandle(hPipe);
    std::cout << "** Exiting cpp module code. fn(IsPipeReady) **" << std::endl;
    return Napi::Boolean::New(env,true);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "PipeMessage"), Napi::Function::New(env,PipeMessage));
    exports.Set(Napi::String::New(env, "IsPipeReady"), Napi::Function::New(env,IsPipeReady));
    return exports;
}

NODE_API_MODULE(addon, Init)
