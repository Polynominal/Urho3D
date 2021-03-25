//
// Copyright (c) 2008-2019 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include "../Container/ArrayPtr.h"
#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../Core/Thread.h"
#include "../Engine/EngineEvents.h"
#include "../IO/File.h"
#include "../IO/FileSystem.h"
#include "../IO/IOEvents.h"
#include "../IO/Log.h"
#include <iostream>

#ifndef MINI_URHO
#include <SDL/SDL_filesystem.h>
#endif

#include <sys/stat.h>
#include <cstdio>

#ifdef _WIN32
#ifndef _MSC_VER
#define _WIN32_IE 0x501
#endif
#include <windows.h>
#include <shellapi.h>
#include <direct.h>
#include <shlobj.h>
#include <sys/types.h>
#include <sys/utime.h>
#else
#include <dirent.h>
#include <cerrno>
#include <unistd.h>
#include <utime.h>
#include <sys/wait.h>
#define MAX_PATH 256
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "../DebugNew.h"
#include "Container/Sort.h"


namespace Urho3D
{

void LogErrorPHYSFS(const String& prefix, const String& fileName )
{
    URHO3D_LOGERROR(prefix + ": '" + fileName + "' reason: " + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()) );
}
void LogErrorPHYSFSAny(const String& prefix)
{
    URHO3D_LOGERROR(prefix + " reason: " + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()) );
}
int DoSystemCommand(const String& commandLine, bool redirectToLog, Context* context)
{
#if defined(TVOS) || defined(IOS)
    return -1;
#else
#if !defined(__EMSCRIPTEN__) && !defined(MINI_URHO)
    if (!redirectToLog)
#endif
        return system(commandLine.CString());

#if !defined(__EMSCRIPTEN__) && !defined(MINI_URHO)
    // Get a platform-agnostic temporary file name for stderr redirection
    String stderrFilename;
    String adjustedCommandLine(commandLine);
    char* prefPath = SDL_GetPrefPath("urho3d", "temp");
    if (prefPath)
    {
        stderrFilename = String(prefPath) + "command-stderr";
        adjustedCommandLine += " 2>" + stderrFilename;
        SDL_free(prefPath);
    }

#ifdef _MSC_VER
    #define popen _popen
    #define pclose _pclose
#endif

    // Use popen/pclose to capture the stdout and stderr of the command
    FILE* file = popen(adjustedCommandLine.CString(), "r");
    if (!file)
        return -1;

    // Capture the standard output stream
    char buffer[128];
    while (!feof(file))
    {
        if (fgets(buffer, sizeof(buffer), file))
            URHO3D_LOGRAW(String(buffer));
    }
    int exitCode = pclose(file);

    // Capture the standard error stream
    if (!stderrFilename.Empty())
    {
        SharedPtr<File> errFile(new File(context, stderrFilename, FILE_READ));
        while (!errFile->IsEof())
        {
            unsigned numRead = errFile->Read(buffer, sizeof(buffer));
            if (numRead)
                Log::WriteRaw(String(buffer, numRead), true);
        }
    }

    return exitCode;
#endif
#endif
}

int DoSystemRun(const String& fileName, const Vector<String>& arguments)
{
#ifdef TVOS
    return -1;
#else
    String fixedFileName = GetNativePath(fileName);

#ifdef _WIN32
    // Add .exe extension if no extension defined
    if (GetExtension(fixedFileName).Empty())
        fixedFileName += ".exe";

    String commandLine = "\"" + fixedFileName + "\"";
    for (unsigned i = 0; i < arguments.Size(); ++i)
        commandLine += " " + arguments[i];

    STARTUPINFOW startupInfo;
    PROCESS_INFORMATION processInfo;
    memset(&startupInfo, 0, sizeof startupInfo);
    memset(&processInfo, 0, sizeof processInfo);

    WString commandLineW(commandLine);
    if (!CreateProcessW(nullptr, (wchar_t*)commandLineW.CString(), nullptr, nullptr, 0, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo))
        return -1;

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);

    return exitCode;
#else
    pid_t pid = fork();
    if (!pid)
    {
        PODVector<const char*> argPtrs;
        argPtrs.Push(fixedFileName.CString());
        for (unsigned i = 0; i < arguments.Size(); ++i)
            argPtrs.Push(arguments[i].CString());
        argPtrs.Push(nullptr);

        execvp(argPtrs[0], (char**)&argPtrs[0]);
        return -1; // Return -1 if we could not spawn the process
    }
    else if (pid > 0)
    {
        int exitCode;
        wait(&exitCode);
        return exitCode;
    }
    else
        return -1;
#endif
#endif
}

/// Base class for async execution requests.
class AsyncExecRequest : public Thread
{
public:
    /// Construct.
    explicit AsyncExecRequest(unsigned& requestID) :
        requestID_(requestID)
    {
        // Increment ID for next request
        ++requestID;
        if (requestID == M_MAX_UNSIGNED)
            requestID = 1;
    }

    /// Return request ID.
    unsigned GetRequestID() const { return requestID_; }

    /// Return exit code. Valid when IsCompleted() is true.
    int GetExitCode() const { return exitCode_; }

    /// Return completion status.
    bool IsCompleted() const { return completed_; }

protected:
    /// Request ID.
    unsigned requestID_{};
    /// Exit code.
    int exitCode_{};
    /// Completed flag.
    volatile bool completed_{};
};

/// Async system command operation.
class AsyncSystemCommand : public AsyncExecRequest
{
public:
    /// Construct and run.
    AsyncSystemCommand(unsigned requestID, const String& commandLine) :
        AsyncExecRequest(requestID),
        commandLine_(commandLine)
    {
        Run();
    }

    /// The function to run in the thread.
    void ThreadFunction() override
    {
        exitCode_ = DoSystemCommand(commandLine_, false, nullptr);
        completed_ = true;
    }

private:
    /// Command line.
    String commandLine_;
};

/// Async system run operation.
class AsyncSystemRun : public AsyncExecRequest
{
public:
    /// Construct and run.
    AsyncSystemRun(unsigned requestID, const String& fileName, const Vector<String>& arguments) :
        AsyncExecRequest(requestID),
        fileName_(fileName),
        arguments_(arguments)
    {
        Run();
    }

    /// The function to run in the thread.
    void ThreadFunction() override
    {
        exitCode_ = DoSystemRun(fileName_, arguments_);
        completed_ = true;
    }

private:
    /// File to run.
    String fileName_;
    /// Command line split in arguments.
    const Vector<String>& arguments_;
};

FileSystem::FileSystem(Context* context) :
    Object(context)
{
    SubscribeToEvent(E_BEGINFRAME, URHO3D_HANDLER(FileSystem, HandleBeginFrame));
    // Subscribe to console commands
    SetExecuteConsoleCommands(true);
}

FileSystem::~FileSystem()
{
    // If any async exec items pending, delete them
    if (asyncExecQueue_.Size())
    {
        for (List<AsyncExecRequest*>::Iterator i = asyncExecQueue_.Begin(); i != asyncExecQueue_.End(); ++i)
            delete(*i);

        asyncExecQueue_.Clear();
    }
}
void FileSystem::PermitSymLinks()
{
    PHYSFS_permitSymbolicLinks(1);
}
String FileSystem::GetSearchPaths()
{
    String output = String("");
    char **i;
    for (i = PHYSFS_getSearchPath(); *i != NULL; i++)
    {
        output = output.Append(String(*i) + "\n");
    }
    PHYSFS_freeList(*i);
    return output;
}
String FileSystem::GetWriteDirectory()
{
    return String(PHYSFS_getWriteDir());

}
bool FileSystem::LoadIdentity(const String& organization,const String& appName)
{
    const char *prefdir = PHYSFS_getPrefDir(organization.CString(), appName.CString());
    if (not prefdir)
    {
        URHO3D_LOGERRORF("Failed to find preference directory for '%s','%s'",organization,appName);
        return false;
    }
    if (!PHYSFS_mount(prefdir, nullptr, 0))
    {
        LogErrorPHYSFSAny("Failed to mount preference directory");
        return false;
    }
    if (!PHYSFS_setWriteDir(prefdir))
    {
        LogErrorPHYSFSAny("Failed to set write directory to preference directory");
        return false;
    }
    URHO3D_LOGINFOF("Set prefered dir: %s",prefdir);

    return true;
}

bool IsAbsolutePath(const String& pathName)
{
    if (pathName.Empty())
        return false;

    String path = GetInternalPath(pathName);

    if (path[0] == '/')
        return true;

#ifdef _WIN32
    if (path.Length() > 1 && IsAlpha(path[0]) && path[1] == ':')
        return true;
#endif

    return false;
}

bool FileSystem::MountArchive(const String& fileName, const String& mountPoint, bool priority)
{
    //CHECK IF has a file extension, if it does its OK!



    // if (GetExtension(fileName).Empty()){ URHO3D_LOGERROR("Cannot bind an archive without a file extension"); return false; }
    // if (!FileExists(fileName)){ URHO3D_LOGERRORF("Cannot find archive %s",fileName); return false; };

    String pathName = fileName;
    if (!IsAbsolutePath(pathName))
    {
        pathName = GetRealFileDir(pathName) + String("/") + pathName;
    }
    pathName = GetNativePath(RemoveTrailingSlash(pathName));

    URHO3D_LOGINFOF("Mounting PHYSFS archive: '%s' at '%s'",pathName.CString(), mountPoint.CString());

    int append = 0;
    if (priority)
        append = 1;

    if (!PHYSFS_mount(pathName.CString(), mountPoint.CString(), append))
    {
        LogErrorPHYSFS("Failed to mount archive",pathName);
        return false;
    }
    return true;
}

bool FileSystem::UnmountArchive(const String& fileName)
{

    String pathName = fileName;
    if (!IsAbsolutePath(pathName))
    {
        pathName = GetRealFileDir(pathName) + String("/") + pathName;
    }
    pathName = GetNativePath(RemoveTrailingSlash(pathName));

    URHO3D_LOGINFOF("Unmounting PHYSFS archive: '%s'",pathName.CString());


    //CHECK IF has a file extension, if it does its OK!
    if (!PHYSFS_unmount(pathName.CString()))
    {
        LogErrorPHYSFS("Failed to unmount archive",pathName);
        return false;
    }
    return true;

}
String FileSystem::GetMountPoint(const String& dirName)
{


    const char* str = PHYSFS_getMountPoint(dirName.CString());
    if (str == nullptr)
    {
        LogErrorPHYSFS("Failed to get mount point",dirName);
        return String();
    }else
    {
        return String(str);
    }
}


bool FileSystem::CreateDir(const String& pathName)
{

    if (!CheckAccess(pathName))
    {
        URHO3D_LOGERROR("Access denied to " + pathName);
        return false;
    }

    // Create each of the parents if necessary
    String parentPath = GetParentPath(pathName);
    if (parentPath.Length() > 1 && !DirExists(parentPath))
    {
        if (!CreateDir(parentPath))
            return false;
    }

    String path = GetNativePath(RemoveTrailingSlash(pathName));
    bool success = PHYSFS_mkdir(path.CString());
    if (success)
    {
        URHO3D_LOGDEBUG("Created directory " + path);
    }
    else
    {
        LogErrorPHYSFS("Failed to create directory",path);
    }

    return success;
}

void FileSystem::SetExecuteConsoleCommands(bool enable)
{
    if (enable == executeConsoleCommands_)
        return;

    executeConsoleCommands_ = enable;
    if (enable)
        SubscribeToEvent(E_CONSOLECOMMAND, URHO3D_HANDLER(FileSystem, HandleConsoleCommand));
    else
        UnsubscribeFromEvent(E_CONSOLECOMMAND);
}

int FileSystem::SystemCommand(const String& commandLine, bool redirectStdOutToLog)
{
    if (allowedPaths_.Empty())
        return DoSystemCommand(commandLine, redirectStdOutToLog, context_);
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return -1;
    }
}

int FileSystem::SystemRun(const String& fileName, const Vector<String>& arguments)
{
    if (allowedPaths_.Empty())
        return DoSystemRun(fileName, arguments);
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return -1;
    }
}

unsigned FileSystem::SystemCommandAsync(const String& commandLine)
{
#ifdef URHO3D_THREADING
    if (allowedPaths_.Empty())
    {
        unsigned requestID = nextAsyncExecID_;
        auto* cmd = new AsyncSystemCommand(nextAsyncExecID_, commandLine);
        asyncExecQueue_.Push(cmd);
        return requestID;
    }
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return M_MAX_UNSIGNED;
    }
#else
    URHO3D_LOGERROR("Can not execute an asynchronous command as threading is disabled");
    return M_MAX_UNSIGNED;
#endif
}

unsigned FileSystem::SystemRunAsync(const String& fileName, const Vector<String>& arguments)
{
#ifdef URHO3D_THREADING
    if (allowedPaths_.Empty())
    {
        unsigned requestID = nextAsyncExecID_;
        auto* cmd = new AsyncSystemRun(nextAsyncExecID_, fileName, arguments);
        asyncExecQueue_.Push(cmd);
        return requestID;
    }
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return M_MAX_UNSIGNED;
    }
#else
    URHO3D_LOGERROR("Can not run asynchronously as threading is disabled");
    return M_MAX_UNSIGNED;
#endif
}

bool FileSystem::SystemOpen(const String& fileName, const String& mode)
{
    URHO3D_LOGERROR("SYSTEM OPEN IS NOT IMPLEMENTED - THANK YOU");
    return false;
}

bool FileSystem::Copy(const String& srcFileName, const String& destFileName)
{
    if (!CheckAccess(GetPath(srcFileName)))
    {
        URHO3D_LOGERROR("Access denied to " + srcFileName);
        return false;
    }
    if (!CheckAccess(GetPath(destFileName)))
    {
        URHO3D_LOGERROR("Access denied to " + destFileName);
        return false;
    }

    SharedPtr<File> srcFile(new File(context_, srcFileName, FILE_READ));
    if (!srcFile->IsOpen())
        return false;
    SharedPtr<File> destFile(new File(context_, destFileName, FILE_WRITE));
    if (!destFile->IsOpen())
        return false;

    unsigned fileSize = srcFile->GetSize();
    SharedArrayPtr<unsigned char> buffer(new unsigned char[fileSize]);

    unsigned bytesRead = srcFile->Read(buffer.Get(), fileSize);
    unsigned bytesWritten = destFile->Write(buffer.Get(), fileSize);
    return bytesRead == fileSize && bytesWritten == fileSize;
}

bool FileSystem::Rename(const String& srcFileName, const String& destFileName)
{
    if (!CheckAccess(GetPath(srcFileName)))
    {
        URHO3D_LOGERROR("Access denied to " + srcFileName);
        return false;
    }
    if (!CheckAccess(GetPath(destFileName)))
    {
        URHO3D_LOGERROR("Access denied to " + destFileName);
        return false;
    }
    if (Copy(srcFileName, destFileName))
    {
        return Delete(srcFileName);

    }else{
        return false;
    }

}

bool FileSystem::Delete(const String& fileName)
{
    if (!CheckAccess(GetPath(fileName)))
    {
        URHO3D_LOGERROR("Access denied to " + fileName);
        return false;
    }
    if (PHYSFS_delete(fileName.CString()) == 0)
    {
        LogErrorPHYSFS("Failed to delete file",fileName);
        return true;
    }
    return false;
}

String FileSystem::GetCurrentDir() const
{
    return String(PHYSFS_getBaseDir());
}

bool FileSystem::CheckAccess(const String& pathName) const
{
    String fixedPath = AddTrailingSlash(pathName);

    // If no allowed directories defined, succeed always
    if (allowedPaths_.Empty())
        return true;

    // If there is any attempt to go to a parent directory, disallow
    if (fixedPath.Contains(".."))
        return false;

    // Check if the path is a partial match of any of the allowed directories
    for (HashSet<String>::ConstIterator i = allowedPaths_.Begin(); i != allowedPaths_.End(); ++i)
    {
        if (fixedPath.Find(*i) == 0)
            return true;
    }

    // Not found, so disallow
    return false;
}

unsigned FileSystem::GetLastModifiedTime(const String& fileName) const
{
    if (fileName.Empty() || !CheckAccess(fileName))
        return 0;

    PHYSFS_Stat stat = {};
    if (PHYSFS_stat(fileName.CString(),&stat))
    {
        return (long)stat.modtime;
    }else
    {
        return 0;
    }

}

bool FileSystem::FileExists(const String& fileName) const
{
    if (!CheckAccess(GetPath(fileName)))
        return false;

    int a = PHYSFS_isInit();
    PHYSFS_Stat stat = {};


    String output = String("");
    char **i;
    for (i = PHYSFS_getSearchPath(); *i != NULL; i++)
    {
        output = output.Append(String(*i) + "\n");
    }


    int out = PHYSFS_stat(fileName.CString(),&stat);
    if (out)
    {
        return stat.filetype == PHYSFS_FILETYPE_REGULAR or stat.filetype == PHYSFS_FILETYPE_SYMLINK;
    }else
    {
        return false;
    }
}

bool FileSystem::DirExists(const String& pathName) const
{
    if (!CheckAccess(pathName))
        return false;

    String fileName = GetNativePath(RemoveTrailingSlash(pathName));

    PHYSFS_Stat stat = {};
    if (PHYSFS_stat(fileName.CString(),&stat))
    {
        return stat.filetype == PHYSFS_FILETYPE_DIRECTORY or stat.filetype == PHYSFS_FILETYPE_SYMLINK;
    }else
    {
        return false;
    }
}

void FileSystem::ScanDir(Vector<String>& result, const String& pathName, const String& filter, unsigned flags, bool recursive) const
{
    result.Clear();

    if (CheckAccess(pathName))
    {
        String initialPath = AddTrailingSlash(pathName);
        ScanDirInternal(result, initialPath, initialPath, filter, flags, recursive);
    }
}

String FileSystem::GetProgramDir() const
{
    return String(PHYSFS_getBaseDir());
}

String FileSystem::GetAppPreferencesDir(const String& org, const String& app) const
{
    const char* writeDir = PHYSFS_getPrefDir(org.CString(),app.CString());
    if (writeDir == nullptr)
        URHO3D_LOGWARNING("Could not get application preferences directory");
    return String(writeDir);
}

void FileSystem::RegisterPath(const String& pathName)
{
    if (pathName.Empty())
        return;

    allowedPaths_.Insert(AddTrailingSlash(pathName));
}

String FileSystem::GetRealFileDir(const String& fileName)
{
    const char* str = PHYSFS_getRealDir(fileName.CString());
    if (str == nullptr)
    {
        return String("");
    }
    return String(str);
}
void FileSystem::ScanDirInternal(Vector<String>& result, String path, const String& startPath,
    const String& filter, unsigned flags, bool recursive) const
{
    path = AddTrailingSlash(path);
    String deltaPath;
    if (path.Length() > startPath.Length())
        deltaPath = path.Substring(startPath.Length());

    String filterExtension = filter.Substring(filter.FindLast('.'));
    if (filterExtension.Contains('*'))
        filterExtension.Clear();

    path = GetNativePath(path);
    char **array = PHYSFS_enumerateFiles(path.CString());
    for (char **i = array; *i != 0; i++)
    {
        String fileName = String(*i);
        if (fileName.StartsWith(".") && !(flags & SCAN_HIDDEN))
        {
            continue;
        }


        String pathAndName = path + "/" + fileName;
        PHYSFS_Stat stat = {};
        if (PHYSFS_stat(pathAndName.CString(),&stat))
        {
            if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY)
            {
                if (flags & SCAN_DIRS)
                    result.Push(deltaPath + fileName);
                if (recursive)
                    ScanDirInternal(result, path + fileName, startPath, filter, flags, recursive);
            }
            else if (flags & SCAN_FILES)
            {
                if (filterExtension.Empty() || fileName.EndsWith(filterExtension))
                    result.Push(deltaPath + fileName);
            }
        }
    }
    PHYSFS_freeList(array);
}

void FileSystem::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    /// Go through the execution queue and post + remove completed requests
    for (List<AsyncExecRequest*>::Iterator i = asyncExecQueue_.Begin(); i != asyncExecQueue_.End();)
    {
        AsyncExecRequest* request = *i;
        if (request->IsCompleted())
        {
            using namespace AsyncExecFinished;

            VariantMap& newEventData = GetEventDataMap();
            newEventData[P_REQUESTID] = request->GetRequestID();
            newEventData[P_EXITCODE] = request->GetExitCode();
            SendEvent(E_ASYNCEXECFINISHED, newEventData);

            delete request;
            i = asyncExecQueue_.Erase(i);
        }
        else
            ++i;
    }
}

void FileSystem::HandleConsoleCommand(StringHash eventType, VariantMap& eventData)
{
    using namespace ConsoleCommand;
    if (eventData[P_ID].GetString() == GetTypeName())
        SystemCommand(eventData[P_COMMAND].GetString(), true);
}

void SplitPath(const String& fullPath, String& pathName, String& fileName, String& extension, bool lowercaseExtension)
{
    String fullPathCopy = GetInternalPath(fullPath);

    unsigned extPos = fullPathCopy.FindLast('.');
    unsigned pathPos = fullPathCopy.FindLast('/');

    if (extPos != String::NPOS && (pathPos == String::NPOS || extPos > pathPos))
    {
        extension = fullPathCopy.Substring(extPos);
        if (lowercaseExtension)
            extension = extension.ToLower();
        fullPathCopy = fullPathCopy.Substring(0, extPos);
    }
    else
        extension.Clear();

    pathPos = fullPathCopy.FindLast('/');
    if (pathPos != String::NPOS)
    {
        fileName = fullPathCopy.Substring(pathPos + 1);
        pathName = fullPathCopy.Substring(0, pathPos + 1);
    }
    else
    {
        fileName = fullPathCopy;
        pathName.Clear();
    }
}

String GetPath(const String& fullPath)
{
    String path, file, extension;
    SplitPath(fullPath, path, file, extension);
    return path;
}

String GetFileName(const String& fullPath)
{
    String path, file, extension;
    SplitPath(fullPath, path, file, extension);
    return file;
}

String GetExtension(const String& fullPath, bool lowercaseExtension)
{
    String path, file, extension;
    SplitPath(fullPath, path, file, extension, lowercaseExtension);
    return extension;
}

String GetFileNameAndExtension(const String& fileName, bool lowercaseExtension)
{
    String path, file, extension;
    SplitPath(fileName, path, file, extension, lowercaseExtension);
    return file + extension;
}

String ReplaceExtension(const String& fullPath, const String& newExtension)
{
    String path, file, extension;
    SplitPath(fullPath, path, file, extension);
    return path + file + newExtension;
}

String AddTrailingSlash(const String& pathName)
{
    String ret = pathName.Trimmed();
    ret.Replace('\\', '/');
    if (!ret.Empty() && ret.Back() != '/')
        ret += '/';
    return ret;
}

String RemoveTrailingSlash(const String& pathName)
{
    String ret = pathName.Trimmed();
    ret.Replace('\\', '/');
    if (!ret.Empty() && ret.Back() == '/')
        ret.Resize(ret.Length() - 1);
    return ret;
}

String GetParentPath(const String& path)
{
    unsigned pos = RemoveTrailingSlash(path).FindLast('/');
    if (pos != String::NPOS)
        return path.Substring(0, pos + 1);
    else
        return String();
}

String GetInternalPath(const String& pathName)
{
    return pathName.Replaced('\\', '/');
}

String GetNativePath(const String& pathName)
{
#ifdef _WIN32
    return pathName.Replaced('/', '\\');
#else
    return pathName;
#endif
}

WString GetWideNativePath(const String& pathName)
{
#ifdef _WIN32
    return WString(pathName.Replaced('/', '\\'));
#else
    return WString(pathName);
#endif
}


String FileSystem::GetTemporaryDir() const
{
#if defined(_WIN32)
#if defined(MINI_URHO)
    return getenv("TMP");
#else
    wchar_t pathName[MAX_PATH];
    pathName[0] = 0;
    GetTempPathW(SDL_arraysize(pathName), pathName);
    return AddTrailingSlash(String(pathName));
#endif
#else
    if (char* pathName = getenv("TMPDIR"))
        return AddTrailingSlash(pathName);
#ifdef P_tmpdir
    return AddTrailingSlash(P_tmpdir);
#else
    return "/tmp/";
#endif
#endif
}

}
