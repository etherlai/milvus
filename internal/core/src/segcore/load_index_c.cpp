// Copyright (C) 2019-2020 Zilliz. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under the License

#include "segcore/load_index_c.h"

#include "common/FieldMeta.h"
#include "common/EasyAssert.h"
#include "index/Index.h"
#include "index/IndexFactory.h"
#include "index/Meta.h"
#include "index/Utils.h"
#include "log/Log.h"
#include "storage/FileManager.h"
#include "segcore/Types.h"
#include "storage/Util.h"
#include "storage/RemoteChunkManagerSingleton.h"
#include "storage/LocalChunkManagerSingleton.h"

CStatus
NewLoadIndexInfo(CLoadIndexInfo* c_load_index_info) {
    try {
        auto load_index_info =
            std::make_unique<milvus::segcore::LoadIndexInfo>();

        *c_load_index_info = load_index_info.release();
        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}

void
DeleteLoadIndexInfo(CLoadIndexInfo c_load_index_info) {
    auto info = (milvus::segcore::LoadIndexInfo*)c_load_index_info;
    delete info;
}

CStatus
AppendIndexParam(CLoadIndexInfo c_load_index_info,
                 const char* c_index_key,
                 const char* c_index_value) {
    try {
        auto load_index_info =
            (milvus::segcore::LoadIndexInfo*)c_load_index_info;
        std::string index_key(c_index_key);
        std::string index_value(c_index_value);
        load_index_info->index_params[index_key] = index_value;

        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}

CStatus
AppendFieldInfo(CLoadIndexInfo c_load_index_info,
                int64_t collection_id,
                int64_t partition_id,
                int64_t segment_id,
                int64_t field_id,
                enum CDataType field_type,
                bool enable_mmap,
                const char* mmap_dir_path) {
    try {
        auto load_index_info =
            (milvus::segcore::LoadIndexInfo*)c_load_index_info;
        load_index_info->collection_id = collection_id;
        load_index_info->partition_id = partition_id;
        load_index_info->segment_id = segment_id;
        load_index_info->field_id = field_id;
        load_index_info->field_type = milvus::DataType(field_type);
        load_index_info->enable_mmap = enable_mmap;
        load_index_info->mmap_dir_path = std::string(mmap_dir_path);

        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}

CStatus
appendVecIndex(CLoadIndexInfo c_load_index_info, CBinarySet c_binary_set) {
    try {
        auto load_index_info =
            (milvus::segcore::LoadIndexInfo*)c_load_index_info;
        auto binary_set = (knowhere::BinarySet*)c_binary_set;
        auto& index_params = load_index_info->index_params;

        milvus::index::CreateIndexInfo index_info;
        index_info.field_type = load_index_info->field_type;
        index_info.index_engine_version = load_index_info->index_engine_version;

        // get index type
        AssertInfo(index_params.find("index_type") != index_params.end(),
                   "index type is empty");
        index_info.index_type = index_params.at("index_type");

        // get metric type
        AssertInfo(index_params.find("metric_type") != index_params.end(),
                   "metric type is empty");
        index_info.metric_type = index_params.at("metric_type");

        // init file manager
        milvus::storage::FieldDataMeta field_meta{
            load_index_info->collection_id,
            load_index_info->partition_id,
            load_index_info->segment_id,
            load_index_info->field_id};
        milvus::storage::IndexMeta index_meta{load_index_info->segment_id,
                                              load_index_info->field_id,
                                              load_index_info->index_build_id,
                                              load_index_info->index_version};
        auto remote_chunk_manager =
            milvus::storage::RemoteChunkManagerSingleton::GetInstance()
                .GetRemoteChunkManager();

        auto config = milvus::index::ParseConfigFromIndexParams(
            load_index_info->index_params);
        config["index_files"] = load_index_info->index_files;

        milvus::storage::FileManagerContext fileManagerContext(
            field_meta, index_meta, remote_chunk_manager);
        load_index_info->index =
            milvus::index::IndexFactory::GetInstance().CreateIndex(
                index_info, fileManagerContext);
        load_index_info->index->Load(*binary_set, config);
        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}

CStatus
appendScalarIndex(CLoadIndexInfo c_load_index_info, CBinarySet c_binary_set) {
    try {
        auto load_index_info =
            (milvus::segcore::LoadIndexInfo*)c_load_index_info;
        auto field_type = load_index_info->field_type;
        auto binary_set = (knowhere::BinarySet*)c_binary_set;
        auto& index_params = load_index_info->index_params;
        bool find_index_type =
            index_params.count("index_type") > 0 ? true : false;
        AssertInfo(find_index_type == true,
                   "Can't find index type in index_params");

        milvus::index::CreateIndexInfo index_info;
        index_info.field_type = milvus::DataType(field_type);
        index_info.index_type = index_params["index_type"];

        load_index_info->index =
            milvus::index::IndexFactory::GetInstance().CreateIndex(
                index_info, milvus::storage::FileManagerContext());
        load_index_info->index->Load(*binary_set);
        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}

CStatus
AppendIndex(CLoadIndexInfo c_load_index_info, CBinarySet c_binary_set) {
    auto load_index_info = (milvus::segcore::LoadIndexInfo*)c_load_index_info;
    auto field_type = load_index_info->field_type;
    if (milvus::datatype_is_vector(field_type)) {
        return appendVecIndex(c_load_index_info, c_binary_set);
    }
    return appendScalarIndex(c_load_index_info, c_binary_set);
}

CStatus
AppendIndexV2(CLoadIndexInfo c_load_index_info) {
    try {
        auto load_index_info =
            (milvus::segcore::LoadIndexInfo*)c_load_index_info;
        auto& index_params = load_index_info->index_params;
        auto field_type = load_index_info->field_type;

        auto engine_version = load_index_info->index_engine_version;

        milvus::index::CreateIndexInfo index_info;
        index_info.field_type = load_index_info->field_type;
        index_info.index_engine_version = engine_version;

        // get index type
        AssertInfo(index_params.find("index_type") != index_params.end(),
                   "index type is empty");
        index_info.index_type = index_params.at("index_type");

        // get metric type
        if (milvus::datatype_is_vector(field_type)) {
            AssertInfo(index_params.find("metric_type") != index_params.end(),
                       "metric type is empty for vector index");
            index_info.metric_type = index_params.at("metric_type");
        }

        // init file manager
        milvus::storage::FieldDataMeta field_meta{
            load_index_info->collection_id,
            load_index_info->partition_id,
            load_index_info->segment_id,
            load_index_info->field_id};
        milvus::storage::IndexMeta index_meta{load_index_info->segment_id,
                                              load_index_info->field_id,
                                              load_index_info->index_build_id,
                                              load_index_info->index_version};
        auto remote_chunk_manager =
            milvus::storage::RemoteChunkManagerSingleton::GetInstance()
                .GetRemoteChunkManager();

        auto config = milvus::index::ParseConfigFromIndexParams(
            load_index_info->index_params);
        config["index_files"] = load_index_info->index_files;

        milvus::storage::FileManagerContext fileManagerContext(
            field_meta, index_meta, remote_chunk_manager);
        load_index_info->index =
            milvus::index::IndexFactory::GetInstance().CreateIndex(
                index_info, fileManagerContext);

        if (load_index_info->enable_mmap &&
            load_index_info->index->IsMmapSupported()) {
            AssertInfo(!load_index_info->mmap_dir_path.empty(),
                       "mmap directory path is empty");
            auto filepath =
                std::filesystem::path(load_index_info->mmap_dir_path) /
                std::to_string(load_index_info->segment_id) /
                std::to_string(load_index_info->field_id) /
                std::to_string(load_index_info->index_id);

            config[kMmapFilepath] = filepath.string();
        }

        load_index_info->index->Load(config);
        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}

CStatus
AppendIndexV3(CLoadIndexInfo c_load_index_info) {
    try {
        auto load_index_info =
            (milvus::segcore::LoadIndexInfo*)c_load_index_info;
        auto& index_params = load_index_info->index_params;
        auto field_type = load_index_info->field_type;

        milvus::index::CreateIndexInfo index_info;
        index_info.field_type = load_index_info->field_type;

        // get index type
        AssertInfo(index_params.find("index_type") != index_params.end(),
                   "index type is empty");
        index_info.index_type = index_params.at("index_type");

        // get metric type
        if (milvus::datatype_is_vector(field_type)) {
            AssertInfo(index_params.find("metric_type") != index_params.end(),
                       "metric type is empty for vector index");
            index_info.metric_type = index_params.at("metric_type");
        }

        milvus::storage::FieldDataMeta field_meta{
            load_index_info->collection_id,
            load_index_info->partition_id,
            load_index_info->segment_id,
            load_index_info->field_id};
        milvus::storage::IndexMeta index_meta{load_index_info->segment_id,
                                              load_index_info->field_id,
                                              load_index_info->index_build_id,
                                              load_index_info->index_version};
        auto config = milvus::index::ParseConfigFromIndexParams(
            load_index_info->index_params);

        auto res = milvus_storage::Space::Open(
            load_index_info->uri,
            milvus_storage::Options{nullptr,
                                    load_index_info->index_store_version});
        AssertInfo(res.ok(), "init space failed");
        std::shared_ptr<milvus_storage::Space> space = std::move(res.value());

        milvus::storage::FileManagerContext fileManagerContext(
            field_meta, index_meta, nullptr, space);
        load_index_info->index =
            milvus::index::IndexFactory::GetInstance().CreateIndex(
                index_info, fileManagerContext, space);

        if (!load_index_info->mmap_dir_path.empty() &&
            load_index_info->index->IsMmapSupported()) {
            auto filepath =
                std::filesystem::path(load_index_info->mmap_dir_path) /
                std::to_string(load_index_info->segment_id) /
                std::to_string(load_index_info->field_id) /
                std::to_string(load_index_info->index_id);

            config[kMmapFilepath] = filepath.string();
        }

        load_index_info->index->LoadV2(config);
        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}
CStatus
AppendIndexFilePath(CLoadIndexInfo c_load_index_info, const char* c_file_path) {
    try {
        auto load_index_info =
            (milvus::segcore::LoadIndexInfo*)c_load_index_info;
        std::string index_file_path(c_file_path);
        load_index_info->index_files.emplace_back(index_file_path);

        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}

CStatus
AppendIndexInfo(CLoadIndexInfo c_load_index_info,
                int64_t index_id,
                int64_t build_id,
                int64_t version) {
    try {
        auto load_index_info =
            (milvus::segcore::LoadIndexInfo*)c_load_index_info;
        load_index_info->index_id = index_id;
        load_index_info->index_build_id = build_id;
        load_index_info->index_version = version;

        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}

CStatus
AppendIndexEngineVersionToLoadInfo(CLoadIndexInfo c_load_index_info,
                                   int32_t index_engine_version) {
    try {
        auto load_index_info =
            (milvus::segcore::LoadIndexInfo*)c_load_index_info;
        load_index_info->index_engine_version = index_engine_version;

        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}

CStatus
CleanLoadedIndex(CLoadIndexInfo c_load_index_info) {
    try {
        auto load_index_info =
            (milvus::segcore::LoadIndexInfo*)c_load_index_info;
        auto local_chunk_manager =
            milvus::storage::LocalChunkManagerSingleton::GetInstance()
                .GetChunkManager();
        auto index_file_path_prefix =
            milvus::storage::GenIndexPathPrefix(local_chunk_manager,
                                                load_index_info->index_build_id,
                                                load_index_info->index_version);
        local_chunk_manager->RemoveDir(index_file_path_prefix);
        auto status = CStatus();
        status.error_code = milvus::Success;
        status.error_msg = "";
        return status;
    } catch (std::exception& e) {
        auto status = CStatus();
        status.error_code = milvus::UnexpectedError;
        status.error_msg = strdup(e.what());
        return status;
    }
}

void
AppendStorageInfo(CLoadIndexInfo c_load_index_info,
                  const char* uri,
                  int64_t version) {
    auto load_index_info = (milvus::segcore::LoadIndexInfo*)c_load_index_info;
    load_index_info->uri = uri;
    load_index_info->index_store_version = version;
}
