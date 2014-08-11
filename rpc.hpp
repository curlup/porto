#ifndef __RPC_H__
#define __RPC_H__

#include "rpc.pb.h"
#include "container.hpp"

const std::string RPC_SOCK_PATH = "/run/portod.socket";

rpc::TContainerResponse
HandleRpcRequest(TContainerHolder &cholder, const rpc::TContainerRequest &req);

#endif
