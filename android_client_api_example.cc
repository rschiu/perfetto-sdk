/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#include "perfetto/tracing.h"
//
//#include "perfetto/trace/test_event.pbzero.h"
//#include "perfetto/trace/trace.pb.h"
//#include "perfetto/trace/trace_packet.pbzero.h"
//#include "perfetto/config/gpu/gpu_counter_config.pbzero.h"
#include "perfetto.h"

// Deliberately not pulling any non-public perfetto header to spot accidental
// header public -> non-public dependency while building this file.

class MyDataSource : public perfetto::DataSource<MyDataSource> {
 public:
  void OnSetup(const SetupArgs& args) override {
    // This can be used to access the domain-specific DataSourceConfig, via
    // args.config->xxx_config_raw().
    PERFETTO_ILOG("OnSetup called, name: %s", args.config->name().c_str());
    const std::string& config_raw = args.config->gpu_counter_config_raw();
    perfetto::protos::pbzero::GpuCounterConfig::Decoder config(config_raw);
    for(auto it = config.counter_ids(); it; ++it) {
      counter_ids.push_back(it->as_uint32());
    }
  }

  void OnStart(const StartArgs&) override { PERFETTO_ILOG("OnStart called"); }

  void OnStop(const StopArgs&) override { PERFETTO_ILOG("OnStop called"); }

  std::vector<uint32_t> counter_ids;
};

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(MyDataSource);

int main() {
  perfetto::TracingInitArgs args;
  args.backends = perfetto::kSystemBackend;
  perfetto::Tracing::Initialize(args);

  // DataSourceDescriptor can be used to advertise domain-specific features.
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("com.example.mytrace");
  MyDataSource::Register(dsd);

  for (;;) {
    MyDataSource::Trace([](MyDataSource::TraceContext ctx) {
      PERFETTO_LOG("Tracing lambda called");
      auto data_source = ctx.GetDataSourceLocked();
      for(auto it : data_source->counter_ids) {
        PERFETTO_LOG("counter id: %d", it);
      }
      auto packet = ctx.NewTracePacket();
      packet->set_timestamp(42);
      packet->set_for_testing()->set_str("event 1");
    });
    sleep(1);
  }
}
