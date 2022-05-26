// to compile:  run "node-gyp build" in folder one level up from this file

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

bool clearConsole(HANDLE);
bool setConsoleTextColor(HANDLE, uint8_t, uint8_t, uint8_t);
void returnConsoleToOriginal(HANDLE, DWORD);
bool deleteConsoleLines(HANDLE, int);
bool setConsoleWindowTitle(HANDLE, std::string);

Napi::Value PipeMessage( const Napi::CallbackInfo& info){
    Napi::Env env = info.Env();

    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if(!GetConsoleMode(hStdOut, &mode)) return Napi::Number::New(env,-20 + GetLastError());
    const DWORD originalMode = mode;
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    

    if(!SetConsoleMode(hStdOut, mode)) return Napi::Number::New(env,-30 + GetLastError());
    if(!setConsoleWindowTitle(hStdOut, "Grid MIDI - Electron GUI")) return Napi::Number::New(env,-60 + GetLastError());
    if(!setConsoleTextColor(hStdOut,255,200,200)) return Napi::Number::New(env,-40 + GetLastError());
    if(!deleteConsoleLines(hStdOut,6)) return Napi::Number::New(env,-50 + GetLastError());
    // std::cout << "********** Entering cpp module code. fn(PipeMessage) *************" << std::endl;
    
    // Get the first paramter passed to the function from the originating JS. It should be a string.
    // Convert it to s std::string.
    std::string aString = info[0].ToString().Utf8Value().c_str();
    
    // Create a LPCTSTR
    LPCTSTR lpvMessage = std::wstring(aString.begin(),aString.end()).c_str();

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
            // std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl << std::endl;
            returnConsoleToOriginal(hStdOut, originalMode);
            return Napi::Number::New(env,-10 + GetLastError());
        }

        // All pipe instances are busy, so wait for 5 seconds. 
        if (!WaitNamedPipe(lpszPipename, 5000))
        {
            printf("Could not open pipe: 5 second wait timed out.");
            // std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl << std::endl;
            returnConsoleToOriginal(hStdOut, originalMode);
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
        // std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl << std::endl;
        returnConsoleToOriginal(hStdOut, originalMode);
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
        // std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl << std::endl;
        returnConsoleToOriginal(hStdOut, originalMode);
        return Napi::Number::New(env,-4);
    }

    printf("Message sent. Reply: ");

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
        // std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl;
        returnConsoleToOriginal(hStdOut, originalMode);
        return Napi::Number::New(env,-5);
    }

    CloseHandle(hPipe);
    std::wstring temp = std::wstring(chBuf);
    // std::cout << "********** Exiting cpp module code. fn(PipeMessage) *************" << std::endl;
    returnConsoleToOriginal(hStdOut, originalMode);
    return Napi::String::New(env,std::string(temp.begin(),temp.end()));
}

Napi::Value IsPipeReady( const Napi::CallbackInfo& info){
    // std::cout << "** Entering cpp module code. fn(IsPipeReady) **";
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
            // std::cout << "** Exiting cpp module code. fn(IsPipeReady) **" << std::endl;
            return Napi::Boolean::New(env,false);
        }
    }
    CloseHandle(hPipe);
    // std::cout << "** Exiting cpp module code. fn(IsPipeReady) **" << std::endl;
    return Napi::Boolean::New(env,true);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "PipeMessage"), Napi::Function::New(env,PipeMessage));
    exports.Set(Napi::String::New(env, "IsPipeReady"), Napi::Function::New(env,IsPipeReady));
    return exports;
}

NODE_API_MODULE(addon, Init)

bool clearConsole(HANDLE hndl){
    DWORD written = 0;
    PCWSTR sequence = L"\x1b[2J";
    return WriteConsoleW(hndl, sequence, (DWORD)wcslen(sequence), &written, NULL);
}

bool setConsoleTextColor(HANDLE hndl, uint8_t r, uint8_t g, uint8_t b){
    DWORD written = 0;
    std::string temp = "\x1b[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    // LPCTSTR lpvMessage = std::wstring(aString.begin(),aString.end()).c_str();
    PCWSTR sequence = std::wstring(temp.begin(),temp.end()).c_str();
    return WriteConsoleW(hndl, sequence, (DWORD)wcslen(sequence), &written, NULL);
}

void returnConsoleToOriginal(HANDLE hndl, DWORD mode){
    setConsoleTextColor(hndl,255,255,255);
    SetConsoleMode(hndl, mode);
}

bool deleteConsoleLines(HANDLE hndl, int n){
    DWORD written = 0;
    std::string temp = "\x1b[" + std::to_string(n) + "M";
    PCWSTR seq = std::wstring(temp.begin(),temp.end()).c_str();
    return WriteConsoleW(hndl, seq, (DWORD)wcslen(seq), &written, NULL);
}

bool setConsoleWindowTitle(HANDLE hndl, std::string str){
    DWORD written = 0;
    std::string temp = "\x1b]0;" + str + "\x1b\x5c";
    PCWSTR seq = std::wstring(temp.begin(),temp.end()).c_str();
    return WriteConsoleW(hndl, seq, (DWORD)wcslen(seq), &written, NULL);
}