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
//#include "perfetto/common/gpu_counter_descriptor.pbzero.h"
//#include "perfetto/config/gpu/gpu_counter_config.pbzero.h"
//#include "perfetto/protozero/scattered_heap_buffer.h"
//#include "perfetto/trace/gpu/gpu_counter_event.pbzero.h"
//#include "perfetto/trace/gpu/gpu_render_stage_event.pbzero.h"
//#include "perfetto/trace/test_event.pbzero.h"
//#include "perfetto/trace/trace.pb.h"
//#include "perfetto/trace/trace_packet.pbzero.h"
#include "perfetto.h"

#include <unordered_map>
#include <vector>

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
    first = true;
  }

  void OnStart(const StartArgs&) override { PERFETTO_ILOG("OnStart called"); }

  void OnStop(const StopArgs&) override { PERFETTO_ILOG("OnStop called"); }

  bool first = true;
  std::vector<uint32_t> counter_ids;
};

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(MyDataSource);

int main() {
  const std::unordered_map<uint32_t, const char*> COUNTER_MAP {
    { 0, "foo" },
    { 1, "bar" }
  };

  perfetto::TracingInitArgs args;
  args.backends = perfetto::kSystemBackend;
  perfetto::Tracing::Initialize(args);

  // DataSourceDescriptor can be used to advertise domain-specific features.
  {
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name("com.example.mytrace");

    protozero::HeapBuffered<perfetto::protos::pbzero::GpuCounterDescriptor> proto;
    for (auto it : COUNTER_MAP) {
    auto spec = proto->add_specs();
      spec->set_counter_id(it.first);
      spec->set_name(it.second);
    }
    dsd.set_gpu_counter_descriptor_raw(proto.SerializeAsString());
    MyDataSource::Register(dsd);
  }

  uint64_t i = 0;
  for (;;) {
    MyDataSource::Trace([&](MyDataSource::TraceContext ctx) {
      PERFETTO_LOG("Tracing lambda called");
      auto data_source = ctx.GetDataSourceLocked();
      if (data_source->first) {
        i = 0;
        auto packet = ctx.NewTracePacket();
        packet->set_timestamp(0);
        auto counter_event = packet->set_gpu_counter_event();
        auto desc = counter_event->set_counter_descriptor();
        for (auto it : data_source->counter_ids) {
          auto entry = COUNTER_MAP.find(it);
          if (entry != COUNTER_MAP.end()) {
            auto spec = desc->add_specs();
            spec->set_counter_id(entry->first);
            spec->set_name(entry->second);
          }
        }
        packet->Finalize();
        data_source->first = false;
      }
      i++;
      {
        auto packet = ctx.NewTracePacket();
        packet->set_timestamp(i * 10);
        auto counter_event = packet->set_gpu_counter_event();
        auto counters = counter_event->add_counters();
        counters->set_counter_id(i % 3);
        if (i % 3 == 0) {
          counters->set_double_value(static_cast<double>(i));
        } else {
          counters->set_int_value(static_cast<int64_t>(i));
        }
        packet->Finalize();
      }
    });
    sleep(1);
  }
}
