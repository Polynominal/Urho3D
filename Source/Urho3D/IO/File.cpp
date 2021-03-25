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

#include "../Core/Profiler.h"
#include "../IO/File.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../IO/MemoryBuffer.h"


#include <cstdio>
#include <LZ4/lz4.h>

#include "../DebugNew.h"
#include <iostream>

namespace Urho3D
{


static const unsigned SKIP_BUFFER_SIZE = 1024;

File::File(Context* context) :
    Object(context),
    mode_(FILE_READ),
    handle_(nullptr),

    offset_(0),
    checksum_(0),
    readSyncNeeded_(false),
    writeSyncNeeded_(false)
{
}

File::File(Context* context, const String& fileName, FileMode mode) :
    Object(context),
    mode_(FILE_READ),
    handle_(nullptr),
    offset_(0),
    checksum_(0),
    readSyncNeeded_(false),
    writeSyncNeeded_(false)
{
    Open(fileName, mode);
}


File::~File()
{
    Close();
}

bool File::Open(const String& fileName, FileMode mode)
{
    return OpenInternal(fileName, mode);
}

unsigned File::Read(void* dest, unsigned size)
{
    if (!IsOpen())
    {
        // If file not open, do not log the error further here to prevent spamming the stderr stream
        return 0;
    }

    if (mode_ == FILE_WRITE)
    {
        URHO3D_LOGERROR("File not opened for reading");
        return 0;
    }

    if (size + position_ > size_)
        size = size_ - position_;
    if (!size)
        return 0;


    // Need to reassign the position due to internal buffering when transitioning from writing to reading
    if (readSyncNeeded_)
    {
        SeekInternal(position_ + offset_);
        readSyncNeeded_ = false;
    }

    if (!ReadInternal(dest, size))
    {
        // Return to the position where the read began
        SeekInternal(position_ + offset_);
        URHO3D_LOGERROR("Error while reading from file " + GetName());
        return 0;
    }

    writeSyncNeeded_ = true;
    position_ += size;
    return size;
}

unsigned File::Seek(unsigned position)
{
    if (!IsOpen())
    {
        // If file not open, do not log the error further here to prevent spamming the stderr stream
        return 0;
    }

    // Allow sparse seeks if writing
    if (mode_ == FILE_READ && position > size_)
        position = size_;

    SeekInternal((long)position + offset_);
    position_ = position;
    readSyncNeeded_ = false;
    writeSyncNeeded_ = false;
    return position_;
}

unsigned File::Write(const void* data, unsigned size)
{
    if (!IsOpen())
    {
        // If file not open, do not log the error further here to prevent spamming the stderr stream
        return 0;
    }

    if (mode_ == FILE_READ)
    {
        URHO3D_LOGERROR("File not opened for writing");
        return 0;
    }

    if (!size)
        return 0;

    // Need to reassign the position due to internal buffering when transitioning from reading to writing
    if (writeSyncNeeded_)
    {
        SeekInternal((long)position_ + offset_);
        writeSyncNeeded_ = false;
    }

    if (PHYSFS_writeBytes(handle_, data, size) != size)
    {
        // Return to the position where the write began
        SeekInternal((long)position_ + offset_);
        URHO3D_LOGERROR("Error while writing to file " + GetName());
        return 0;
    }

    readSyncNeeded_ = true;
    position_ += size;
    if (position_ > size_)
        size_ = position_;

    return size;
}

unsigned File::GetChecksum()
{
    if (offset_ || checksum_)
        return checksum_;

    if (!handle_ || mode_ == FILE_WRITE)
        return 0;

    URHO3D_PROFILE(CalculateFileChecksum);

    unsigned oldPos = position_;
    checksum_ = 0;

    Seek(0);
    while (!IsEof())
    {
        unsigned char block[1024];
        unsigned readBytes = Read(block, 1024);
        for (unsigned i = 0; i < readBytes; ++i)
            checksum_ = SDBMHash(checksum_, block[i]);
    }

    Seek(oldPos);
    return checksum_;
}

void File::Close()
{

    if (handle_)
    {
        PHYSFS_close(handle_);
        handle_ = nullptr;
        position_ = 0;
        size_ = 0;
        offset_ = 0;
        checksum_ = 0;
    }
}

void File::Flush()
{
    if (handle_)
        PHYSFS_flush(handle_);
}

void File::SetName(const String& name)
{
    fileName_ = name;
}

bool File::IsOpen() const
{
    return handle_ != nullptr;
}

bool File::OpenInternal(const String& fileName, FileMode mode, bool fromPackage)
{
    Close();

    readSyncNeeded_ = false;
    writeSyncNeeded_ = false;

    auto* fileSystem = GetSubsystem<FileSystem>();
    if (fileSystem && !fileSystem->CheckAccess(GetPath(fileName)))
    {
        URHO3D_LOGERRORF("Access denied to %s", fileName.CString());
        return false;
    }

    if (fileName.Empty())
    {
        URHO3D_LOGERROR("Could not open file with empty name");
        return false;
    }

    switch(mode)
    {
        case FILE_READ:
            handle_ = PHYSFS_openRead(fileName.CString());
            break;
        case FILE_WRITE:
            handle_ = PHYSFS_openWrite(fileName.CString());
            break;
        case FILE_READWRITE:
            handle_ = PHYSFS_openAppend(fileName.CString());
            break;
    }

    if (!handle_)
    {
        URHO3D_LOGERRORF("Could not open file %s", fileName.CString());
        return false;
    }


    unsigned int file_size = (unsigned int)PHYSFS_fileLength(handle_);
    if (file_size > M_MAX_UNSIGNED)
    {
        URHO3D_LOGERRORF("Could not open file %s which is larger than 4GB", fileName.CString());
        Close();
        size_ = 0;
        return false;
    }
    size_ = file_size;
    offset_ = 0;


    fileName_ = fileName;
    mode_ = mode;
    position_ = 0;
    checksum_ = 0;

    return true;
}

bool File::ReadInternal(void* dest, unsigned size)
{
    return PHYSFS_readBytes(handle_, dest, size) != -1;
}

void File::SeekInternal(unsigned newPosition)
{
    PHYSFS_seek(handle_, newPosition);
}



}
