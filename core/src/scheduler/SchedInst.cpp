// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "scheduler/SchedInst.h"
#include "ResourceFactory.h"
#include "Utils.h"
#include "server/Config.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace milvus {
namespace scheduler {

ResourceMgrPtr ResMgrInst::instance = nullptr;
std::mutex ResMgrInst::mutex_;

SchedulerPtr SchedInst::instance = nullptr;
std::mutex SchedInst::mutex_;

scheduler::JobMgrPtr JobMgrInst::instance = nullptr;
std::mutex JobMgrInst::mutex_;

OptimizerPtr OptimizerInst::instance = nullptr;
std::mutex OptimizerInst::mutex_;

BuildMgrPtr BuildMgrInst::instance = nullptr;
std::mutex BuildMgrInst::mutex_;

void
load_simple_config() {
    server::Config& config = server::Config::GetInstance();
    std::string mode;
    config.GetResourceConfigMode(mode);
    std::vector<std::string> pool;
    config.GetResourceConfigSearchResources(pool);

    // get resources
    auto gpu_ids = get_gpu_pool();

    auto build_gpu_ids = get_build_resources();

    // create and connect
    ResMgrInst::GetInstance()->Add(ResourceFactory::Create("disk", "DISK", 0, true, false));

    auto io = Connection("io", 500);
    ResMgrInst::GetInstance()->Add(ResourceFactory::Create("cpu", "CPU", 0, true, true));
    ResMgrInst::GetInstance()->Connect("disk", "cpu", io);

    auto pcie = Connection("pcie", 12000);

    std::vector<int64_t> not_find_build_ids;
    for (auto& build_id : build_gpu_ids) {
        bool find_gpu_id = false;
        for (auto& gpu_id : gpu_ids) {
            if (gpu_id == build_id) {
                find_gpu_id = true;
                break;
            }
        }
        if (not find_gpu_id) {
            not_find_build_ids.emplace_back(build_id);
        }
    }

    for (auto& gpu_id : gpu_ids) {
        ResMgrInst::GetInstance()->Add(ResourceFactory::Create(std::to_string(gpu_id), "GPU", gpu_id, true, true));
        ResMgrInst::GetInstance()->Connect("cpu", std::to_string(gpu_id), pcie);
    }

    for (auto& not_find_id : not_find_build_ids) {
        ResMgrInst::GetInstance()->Add(
            ResourceFactory::Create(std::to_string(not_find_id), "GPU", not_find_id, true, true));
        ResMgrInst::GetInstance()->Connect("cpu", std::to_string(not_find_id), pcie);
    }
}

void
StartSchedulerService() {
    load_simple_config();
    ResMgrInst::GetInstance()->Start();
    SchedInst::GetInstance()->Start();
    JobMgrInst::GetInstance()->Start();
}

void
StopSchedulerService() {
    JobMgrInst::GetInstance()->Stop();
    SchedInst::GetInstance()->Stop();
    ResMgrInst::GetInstance()->Stop();
}

}  // namespace scheduler
}  // namespace milvus
