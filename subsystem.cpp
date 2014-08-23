#include <unordered_map>

#include "cgroup.hpp"
#include "subsystem.hpp"
#include "log.hpp"
#include "util/string.hpp"
#include "util/unix.hpp"

using namespace std;

static unordered_map<string, shared_ptr<TSubsystem>> subsystems;

// TSubsystem
shared_ptr<TSubsystem> TSubsystem::Get(std::string name) {
    if (subsystems.find(name) == subsystems.end()) {
        if (name == "memory")
            subsystems[name] = make_shared<TMemorySubsystem>();
        else if (name == "freezer")
            subsystems[name] = make_shared<TFreezerSubsystem>();
        else if (name == "cpu")
            subsystems[name] = make_shared<TCpuSubsystem>();
        else
            subsystems[name] = make_shared<TSubsystem>(name);
    }

    return subsystems[name];
}

string TSubsystem::Name() {
    return name;
}

// Memory
shared_ptr<TMemorySubsystem> TSubsystem::Memory() {
    return static_pointer_cast<TMemorySubsystem>(Get("memory"));
}

#include <iostream>

TError TMemorySubsystem::Usage(shared_ptr<TCgroup> &cg, uint64_t &value) {
    string s;
    TError error = cg->GetKnobValue("memory.usage_in_bytes", s);
    if (error)
        return error;
    return StringToUint64(s, value);
}

TError TMemorySubsystem::UseHierarchy(TCgroup &cg) {
    return TError(cg.SetKnobValue("memory.use_hierarchy", "1"));
}

// Freezer
shared_ptr<TFreezerSubsystem> TSubsystem::Freezer() {
    return static_pointer_cast<TFreezerSubsystem>(Get("freezer"));
}

TError TFreezerSubsystem::WaitState(TCgroup &cg, const std::string &state) {

    int ret = RetryFailed(FREEZER_WAIT_TIMEOUT_S * 10, 100, [&]{
        string s;
        TError error = cg.GetKnobValue("freezer.state", s);
        if (error)
            TLogger::LogError(error, "Can't freeze cgroup");

        return s != state;
    });

    if (ret) {
        TError error(EError::Unknown, "Can't wait for freezer state " + state);
        TLogger::LogError(error, cg.Relpath());
        return error;
    }
    return TError::Success();
}

TError TFreezerSubsystem::Freeze(TCgroup &cg) {
    TError error(cg.SetKnobValue("freezer.state", "FROZEN"));
    if (error)
        return error;

    return WaitState(cg, "FROZEN\n");
}

TError TFreezerSubsystem::Unfreeze(TCgroup &cg) {
    TError error(cg.SetKnobValue("freezer.state", "THAWED"));
    if (error)
        return error;

    return WaitState(cg, "THAWED\n");
}

// Cpu
shared_ptr<TCpuSubsystem> TSubsystem::Cpu() {
    return static_pointer_cast<TCpuSubsystem>(Get("cpu"));
}

// Cpuacct
shared_ptr<TCpuacctSubsystem> TSubsystem::Cpuacct() {
    return static_pointer_cast<TCpuacctSubsystem>(Get("cpuacct"));
}

TError TCpuacctSubsystem::Usage(shared_ptr<TCgroup> &cg, uint64_t &value) {
    string s;
    TError error = cg->GetKnobValue("cpuacct.usage", s);
    if (error)
        return error;
    return StringToUint64(s, value);
}
