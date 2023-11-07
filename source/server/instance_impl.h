#pragma once

#include "source/common/memory/heap_shrinker.h"
#include "source/server/server.h"

namespace Envoy {
namespace Server {

// The production server instance, which creates all of the required components.
class InstanceImpl : public InstanceBase {
public:
  using InstanceBase::InstanceBase;

protected:
  void maybeCreateHeapShrinker() override;

private:
  std::unique_ptr<Memory::HeapShrinker> heap_shrinker_;
};

} // namespace Server
} // namespace Envoy
