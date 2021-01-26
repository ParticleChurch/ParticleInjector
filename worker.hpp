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
        "client.dll",
        "engine.dll",
        "vguimatsurface.dll",
        "vgui2.dll",
        "vphysics.dll",
        "inputsystem.dll",
        "vstdlib.dll",
        "materialsystem.dll",
        "serverbrowser.dll",
    };

    std::string injectorVersion = "1.2";

    ManualMapper* mapper = nullptr;
    byte* file = nullptr;
    uint64_t fileSize = 0;

public slots:
    void run() override;

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