#pragma once

#include <QTCore/QThread>
#include <QDebug>
#include <Windows.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <string>
#include <shlobj.h>
#include "injector.hpp"
#include "API.hpp"
#include "HTTP.hpp"

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

    InjectorVersion myVersion{1,0};

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

    std::string getDesktopPath();

signals:
    void taskComplete(int taskNumber, bool success);
    void taskDescription(int taskNumber, QString description);
};