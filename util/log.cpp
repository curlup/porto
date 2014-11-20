#include <iostream>

#include "porto.hpp"
#include "log.hpp"
#include "util/unix.hpp"
#include "util/path.hpp"
#include "util/file.hpp"

#ifdef PORTOD
#include "util/stat.hpp"
#endif

static std::ofstream logFile;
static std::ofstream kmsgFile;
static TPath logPath;
static unsigned int logMode;
static bool stdlog = false;
static bool verbose;

void TLogger::InitLog(const std::string &path, const unsigned int mode, const bool verb) {
    logPath = path;
    logMode = mode;
    logFile.close();
    verbose = verb;
}

void TLogger::LogToStd() {
    stdlog = true;
}

void TLogger::OpenLog() {
    if (logFile.is_open())
        return;

    if (!logPath.DirName().AccessOk(EFileAccess::Write)) {
        if (!kmsgFile.is_open())
            kmsgFile.open("/dev/kmsg", std::ios_base::out);
        return;
    }

    bool needCreate = false;

    if (logPath.Exists()) {
        if (logPath.GetType() != EFileType::Regular ||
            logPath.GetMode() != logMode) {

            TFile f(logPath);
            (void)f.Remove();
            needCreate = true;
        }
    } else {
        needCreate = true;
    }

    if (needCreate) {
        TFile f(logPath, logMode);
        (void)f.Touch();
    }

    logFile.open(logPath.ToString(), std::ios_base::app);

    if (logFile.is_open() && kmsgFile.is_open())
        kmsgFile.close();
}

void TLogger::CloseLog() {
    logFile.close();
    kmsgFile.close();
}

static std::string GetTime() {
    char tmstr[256];
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    tmp = localtime(&t);

    if (tmp && strftime(tmstr, sizeof(tmstr), "%c", tmp))
        return std::string(tmstr);

    return std::string();
}

std::basic_ostream<char> &TLogger::Log(ELogLevel level) {
    static int openlog;
    static const std::string prefix[] = { "", "Warning! ", "Error! " };

#ifdef PORTOD
    if (level == LOG_WARN)
        StatInc(PORTO_STAT_WARNS);
    else if (level == LOG_ERROR)
        StatInc(PORTO_STAT_ERRORS);
#endif

    std::string name = GetProcessName();
    if (stdlog) {
        return  std::cerr << GetTime() << " " << name << ": " << prefix[level];
    } else {
        if (openlog)
            return  std::cerr << GetTime() << " " << name << ": " << prefix[level];

        openlog++;
        OpenLog();
        openlog--;

        if (logFile.is_open())
            return logFile << GetTime() << " " << prefix[level];
        else if (kmsgFile.is_open())
            return kmsgFile << " " << name << ": " << prefix[level];
        else
            return std::cerr << GetTime() << " " << name << ": " << prefix[level];
    }
}

void TLogger::LogAction(const std::string &action, bool error, int errcode) {
    if (!error && verbose)
        Log() << " Ok: " << action << std::endl;
    else if (error)
        Log() << " Error: " << action << ": " << strerror(errcode) << std::endl;
}

void TLogger::LogRequest(const std::string &message) {
    Log() << "-> " << message << std::endl;
}

void TLogger::LogResponse(const std::string &message) {
    Log() << "<- " << message << std::endl;
}
