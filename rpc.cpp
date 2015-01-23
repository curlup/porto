#include "rpc.hpp"
#include "property.hpp"
#include "data.hpp"
#include "util/log.hpp"
#include "util/protobuf.hpp"

using std::string;

static TError CreateContainer(TContext &context,
                              const rpc::TContainerCreateRequest &req,
                              rpc::TContainerResponse &rsp,
                              const TCred &cred) {
    auto container = context.Cholder->Get(req.name());
    if (container)
        return TError(EError::ContainerAlreadyExists, "invalid name");

    return context.Cholder->Create(req.name(), cred);
}

static TError DestroyContainer(TContext &context,
                               const rpc::TContainerDestroyRequest &req,
                               rpc::TContainerResponse &rsp,
                               const TCred &cred) {
    { // we don't want to hold container shared_ptr because Destroy
      // might think that it has some parent that holds it
        auto container = context.Cholder->Get(req.name());
        if (container) {
            TError error = container->CheckPermission(cred);
            if (error)
                return error;
        }
    }

    return context.Cholder->Destroy(req.name());
}

static TError StartContainer(TContext &context,
                             const rpc::TContainerStartRequest &req,
                             rpc::TContainerResponse &rsp,
                             const TCred &cred) {
    auto container = context.Cholder->Get(req.name());
    if (!container)
        return TError(EError::ContainerDoesNotExist, "invalid name");

    TError error = container->CheckPermission(cred);
    if (error)
        return error;

    return container->Start();
}

static TError StopContainer(TContext &context,
                            const rpc::TContainerStopRequest &req,
                            rpc::TContainerResponse &rsp,
                            const TCred &cred) {
    auto container = context.Cholder->Get(req.name());
    if (!container)
        return TError(EError::ContainerDoesNotExist, "invalid name");

    TError error = container->CheckPermission(cred);
    if (error)
        return error;

    return container->Stop();
}

static TError PauseContainer(TContext &context,
                             const rpc::TContainerPauseRequest &req,
                             rpc::TContainerResponse &rsp,
                             const TCred &cred) {
    auto container = context.Cholder->Get(req.name());
    if (!container)
        return TError(EError::ContainerDoesNotExist, "invalid name");

    TError error = container->CheckPermission(cred);
    if (error)
        return error;

    return container->Pause();
}

static TError ResumeContainer(TContext &context,
                              const rpc::TContainerResumeRequest &req,
                              rpc::TContainerResponse &rsp,
                              const TCred &cred) {
    auto container = context.Cholder->Get(req.name());
    if (!container)
        return TError(EError::ContainerDoesNotExist, "invalid name");

    TError error = container->CheckPermission(cred);
    if (error)
        return error;

    return container->Resume();
}

static TError ListContainers(TContext &context,
                             rpc::TContainerResponse &rsp) {
    for (auto name : context.Cholder->List())
        rsp.mutable_list()->add_name(name);

    return TError::Success();
}

static TError GetContainerProperty(TContext &context,
                                   const rpc::TContainerGetPropertyRequest &req,
                                   rpc::TContainerResponse &rsp,
                                   const TCred &cred) {
    auto container = context.Cholder->Get(req.name());
    if (!container)
        return TError(EError::ContainerDoesNotExist, "invalid name");

    string value;
    TError error = container->GetProperty(req.property(), value);
    if (!error)
        rsp.mutable_getproperty()->set_value(value);
    return error;
}

static TError SetContainerProperty(TContext &context,
                                   const rpc::TContainerSetPropertyRequest &req,
                                   rpc::TContainerResponse &rsp,
                                   const TCred &cred) {
    auto container = context.Cholder->Get(req.name());
    if (!container)
        return TError(EError::ContainerDoesNotExist, "invalid name");

    TError error = container->CheckPermission(cred);
    if (error)
        return error;

    return container->SetProperty(req.property(), req.value(), cred.IsPrivileged());
}

static TError GetContainerData(TContext &context,
                               const rpc::TContainerGetDataRequest &req,
                               rpc::TContainerResponse &rsp,
                               const TCred &cred) {
    auto container = context.Cholder->Get(req.name());
    if (!container)
        return TError(EError::ContainerDoesNotExist, "invalid name");

    string value;
    TError error = container->GetData(req.data(), value);
    if (!error)
        rsp.mutable_getdata()->set_value(value);
    return error;
}

static TError ListProperty(TContext &context,
                           rpc::TContainerResponse &rsp) {
    auto list = rsp.mutable_propertylist();

    for (auto property : propertySet.GetNames()) {
        auto p = propertySet.Get(property);
        if (p->Flags & HIDDEN_VALUE)
            continue;

        auto entry = list->add_list();

        entry->set_name(property);
        entry->set_desc(p->Desc);
    }

    return TError::Success();
}

static TError ListData(TContext &context,
                       rpc::TContainerResponse &rsp) {
    auto list = rsp.mutable_datalist();

    for (auto data : dataSet.GetNames()) {
        auto d = dataSet.Get(data);
        if (d->Flags & HIDDEN_VALUE)
            continue;

        auto entry = list->add_list();

        entry->set_name(data);
        entry->set_desc(d->Desc);
    }

    return TError::Success();
}

static TError Kill(TContext &context,
                   const rpc::TContainerKillRequest &req,
                   rpc::TContainerResponse &rsp,
                   const TCred &cred) {
    auto container = context.Cholder->Get(req.name());
    if (!container)
        return TError(EError::ContainerDoesNotExist, "invalid name");

    TError error = container->CheckPermission(cred);
    if (error)
        return error;

    return container->Kill(req.sig());
}

static TError Version(TContext &context,
                      rpc::TContainerResponse &rsp) {
    auto ver = rsp.mutable_version();

    ver->set_tag(GIT_TAG);
    ver->set_revision(GIT_REVISION);

    return TError::Success();
}

static TError CreateVolume(TContext &context,
                           const rpc::TVolumeCreateRequest &req,
                           rpc::TContainerResponse &rsp,
                           const TCred &cred) {

    std::shared_ptr<TVolume> volume;
    volume = std::make_shared<TVolume>(context.VolumeStorage, context.Vholder,
                                       req.name(), req.source(),
                                       req.quota(), req.flags(), cred);
    TError err = volume->Create();
    if (err)
        return err;

    return TError::Success();
}

static TError DestroyVolume(TContext &context,
                            const rpc::TVolumeDestroyRequest &req,
                            rpc::TContainerResponse &rsp,
                            const TCred &cred) {
    auto volume = context.Vholder->Get(req.name());
    if (volume) {
        TError error = volume->CheckPermission(cred);
        if (error)
            return error;
        return volume->Destroy();
    }

    return TError(EError::VolumeDoesNotExist, "volume " + volume->GetName() +
                  "doesn't exist");
}

static TError ListVolumes(TContext &context,
                          rpc::TContainerResponse &rsp) {
    for (auto name : context.Vholder->List()) {
        auto desc = rsp.mutable_volumelist()->add_list();
        auto vol = context.Vholder->Get(name);
        desc->set_name(vol->GetName());
        desc->set_source(vol->GetSource());
        desc->set_quota(vol->GetQuota());
        desc->set_flags(vol->GetFlags());
    }

    return TError::Success();
}

bool HandleRpcRequest(TContext &context, const rpc::TContainerRequest &req,
                      rpc::TContainerResponse &rsp, const TCred &cred) {
    string str;
    bool send_reply = true;

    TLogger::LogRequest(req.ShortDebugString());

    rsp.set_error(EError::Unknown);

    TError error;
    try {
        if (req.has_create())
            error = CreateContainer(context, req.create(), rsp, cred);
        else if (req.has_destroy())
            error = DestroyContainer(context, req.destroy(), rsp, cred);
        else if (req.has_list())
            error = ListContainers(context, rsp);
        else if (req.has_getproperty())
            error = GetContainerProperty(context, req.getproperty(), rsp, cred);
        else if (req.has_setproperty())
            error = SetContainerProperty(context, req.setproperty(), rsp, cred);
        else if (req.has_getdata())
            error = GetContainerData(context, req.getdata(), rsp, cred);
        else if (req.has_start())
            error = StartContainer(context, req.start(), rsp, cred);
        else if (req.has_stop())
            error = StopContainer(context, req.stop(), rsp, cred);
        else if (req.has_pause())
            error = PauseContainer(context, req.pause(), rsp, cred);
        else if (req.has_resume())
            error = ResumeContainer(context, req.resume(), rsp, cred);
        else if (req.has_propertylist())
            error = ListProperty(context, rsp);
        else if (req.has_datalist())
            error = ListData(context, rsp);
        else if (req.has_kill())
            error = Kill(context, req.kill(), rsp, cred);
        else if (req.has_version())
            error = Version(context, rsp);
        else if (req.has_createvolume()) {
            error = CreateVolume(context, req.createvolume(), rsp, cred);
            if (!error)
                send_reply = false;
        } else if (req.has_destroyvolume()) {
            error = DestroyVolume(context, req.destroyvolume(), rsp, cred);
            if (!error)
                send_reply = false;
        } else if (req.has_listvolumes())
            error = ListVolumes(context, rsp);
        else
            error = TError(EError::InvalidMethod, "invalid RPC method");
    } catch (std::bad_alloc exc) {
        rsp.Clear();
        error = TError(EError::Unknown, "memory allocation failure");
    } catch (std::string exc) {
        rsp.Clear();
        error = TError(EError::Unknown, exc);
    } catch (const std::exception &exc) {
        rsp.Clear();
        error = TError(EError::Unknown, exc.what());
    } catch (...) {
        rsp.Clear();
        error = TError(EError::Unknown, "unknown error");
    }

    rsp.set_error(error.GetError());
    rsp.set_errormsg(error.GetMsg());

    if (send_reply)
        TLogger::LogResponse(rsp.ShortDebugString());

    return send_reply;
}
