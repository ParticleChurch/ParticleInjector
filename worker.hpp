#pragma once

#include <QTCore/QThread>
#include <QDebug>
#include <Windows.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <string>

#include "HTTP.hpp"
#include "ManualMapper.hpp"
#include "Encryption.hpp"

class Worker : public QThread
{
    Q_OBJECT

private:
    HANDLE CSGO;
    DWORD CSGO_PID;
    std::vector<std::string> CSGODLLs{
        "csgo.exe",
        "ntdll.dll",
        "engine.dll",
        "tier0.dll",
        "client.dll",
        "server.dll",
        "shaderapidx9.dll",
        "vguimatsurface.dll",
        "vgui2.dll",
        "vphysics.dll",
        "inputsystem.dll",
        "vstdlib.dll",
        "studiorender.dll",
        "materialsystem.dll",
        "serverbrowser.dll",
    };
    size_t numDllsLoaded = 0;
    size_t totalDllsToLoad = 1;

    std::string injectorVersion = "1.5";

    ManualMapper* mapper = nullptr;
    byte* file = nullptr;
    uint64_t fileSize = 0;

public slots:
    void run() override;

    void update();
    bool checkVersion();
    bool waitForCSGOToOpen();
    bool download();
    bool inject();

    HANDLE getCSGO();
    bool processIsCSGO(HANDLE hProcess);
    bool csgoIsInitialized();
    void failed();

signals:
    void taskComplete(int taskNumber, bool success);
    void taskDescription(int taskNumber, QString description);
};