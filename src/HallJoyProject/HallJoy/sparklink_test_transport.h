#pragma once
struct SparkTestTransport {
    virtual bool Write(const std::uint8_t*,DWORD,DWORD*)=0;
    virtual bool Read(std::uint8_t*,DWORD,DWORD*,DWORD*)=0;
    virtual ~SparkTestTransport()=default;
};
static thread_local SparkTestTransport* g_sparkTestTransport=nullptr;
