/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/openvino/elements/tensor_inference_openvino.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/openvino/context.h"
#include "dlstreamer/openvino/frame.h"
#include "dlstreamer/openvino/tensor.h"
#include "dlstreamer/openvino/utils.h"
#include "dlstreamer/vaapi/context.h"

#include <openvino/openvino.hpp>
#include <openvino/runtime/intel_gpu/properties.hpp>
#include <openvino/runtime/intel_gpu/remote_properties.hpp>

namespace dlstreamer {

namespace param {
static constexpr auto model = "model";   // path to model file
static constexpr auto device = "device"; // string
static constexpr auto config = "config"; // string, comma separated list of KEY=VALUE parameters
static constexpr auto batch_size = "batch-size";
static constexpr auto buffer_pool_size = "buffer-pool-size";
}; // namespace param

static ParamDescVector params_desc = {
    {
        param::model,
        "Path to model file in OpenVINO™ toolkit or ONNX format",
        "",
    },
    {
        param::device,
        "Target device for inference. Please see OpenVINO™ toolkit documentation for list of supported devices.",
        "CPU",
    },
    {param::config, "Comma separated list of KEY=VALUE parameters for Inference Engine configuration", ""},
    {param::batch_size, "Batch size", 1, 0, std::numeric_limits<int>::max()},
    {param::buffer_pool_size, "Output buffer pool size (functionally same as OpenVINO™ toolkit nireq parameter)", 16, 0,
     std::numeric_limits<int>::max()},
};

class InferenceOpenVINO : public BaseTransform {
  public:
    InferenceOpenVINO(DictionaryCPtr params, const ContextPtr &app_context)
        : BaseTransform(app_context), _params(params) {
        _device = _params->get<std::string>(param::device);
        _buffer_pool_size = _params->get<int>(param::buffer_pool_size, 16);
        read_ir_model();
    }

    FrameInfoVector get_input_info() override {
        auto infos = info_variations(_model_input_info, {MemoryType::OpenCL, MemoryType::CPU},
                                     {DataType::UInt8, DataType::Float32});

        if (is_device_gpu()) {
            // Special case for VAAPI.
            // Do not expose NV12 VASurface as an input capability if device is not set to GPU
            assert(_model_input_info.tensors.size() == 1);
            FrameInfo i(ImageFormat::NV12, MemoryType::VAAPI, {_model_input_info.tensors.front()});
            infos.push_back(i);
        }
        return infos;
    }

    FrameInfoVector get_output_info() override {
        _model_output_info.memory_type = MemoryType::OpenVINO;
        return {_model_output_info};
    }

    void read_ir_model() {
        // read model
        auto path = _params->get<std::string>(param::model);
        _model = _core.read_model(path);
        // set batch size
        int batch_size = _params->get<int>(param::batch_size);
        if (batch_size > 1)
            ov::set_batch(_model, batch_size);
        // get input info
        _model_input_info = FrameInfo(MediaType::Tensors);
        for (auto node : _model->get_parameters()) {
            auto dtype = data_type_from_openvino(node->get_element_type());
            auto shape = node->is_dynamic() ? node->get_input_partial_shape(0).get_min_shape() : node->get_shape();
            _model_input_info.tensors.push_back(TensorInfo(shape, dtype));
            _model_input_names.push_back(node->get_friendly_name());
        }
        // get output info
        _model_output_info = FrameInfo(MediaType::Tensors);
        for (auto node : _model->get_results()) {
            auto dtype = data_type_from_openvino(node->get_element_type());
            auto shape = node->is_dynamic() ? node->get_output_partial_shape(0).get_min_shape() : node->get_shape();
            _model_output_info.tensors.push_back(TensorInfo(shape, dtype));
        }
        for (auto &output : _model->outputs()) {
            _model_output_names.push_back(output.get_any_name());
        }
    }

    bool init_once() override {
        // Special case for VAAPI NV12 surface input
        if (_input_info.media_type == MediaType::Image && _input_info.memory_type == MemoryType::VAAPI) {
            if (!is_device_gpu())
                throw std::runtime_error("VASurface as input supported only for inference on GPU");
            create_remote_context();
        }

        if (_input_info.tensors != _model_input_info.tensors || _input_info.media_type != MediaType::Tensors) {
            configure_model_preprocessing();
        }

        load_network();

        if (!_input_mapper) {
            ContextPtr interm_context = std::make_shared<BaseContext>(_input_info.memory_type);
            if (!_openvino_context)
                _openvino_context = std::make_shared<OpenVINOContext>(_compiled_model);
            _input_mapper = create_mapper({_app_context, interm_context, _openvino_context});
        }
#if 0
        // print all properties
        try {
            auto supported_properties = _compiled_model.get_property(ov::supported_properties);
            for (const auto &cfg : supported_properties) {
                if (cfg == ov::supported_properties)
                    continue;
                auto prop = _compiled_model.get_property(cfg);
                std::cout << "  OpenVINO™ toolkit config: " << cfg << " \t= " << prop.as<std::string>() << std::endl;
            }
        } catch (const ov::Exception &) {
        }
#endif

        return true;
    }

    ContextPtr get_context(MemoryType memory_type) noexcept override {
        if (memory_type == MemoryType::OpenCL) {
            return create_remote_context();
        }
        return nullptr;
    }

    virtual std::function<FramePtr()> get_output_allocator() override {
        return [this]() {
            auto infer_request = _compiled_model.create_infer_request();
            return std::make_shared<OpenVINOFrame>(infer_request, _openvino_context);
        };
    }

    FramePtr process(FramePtr src) override {
        DLS_CHECK(init());
        // ITT_TASK(__FUNCTION__);
        FramePtr dst = create_output();
        auto src_openvino = _input_mapper->map(src, AccessMode::Read);
        auto dst_openvino = ptr_cast<OpenVINOFrame>(dst);

        // FIXME: Since we are reusing inference requests we have to make sure that the inference is actually completed.
        // This is usually done when buffer is mapped somewhere in downstream.
        // However, we should properly handle the situation if buffer was not mapped in downstream.
        // For example, fakesink after inference.
        dst_openvino->wait();

        dst_openvino->set_input({src_openvino.begin(), src_openvino.end()});

        // capture input tensors until infer_request.wait() completed
        dst_openvino->set_parent(src_openvino);

        dst_openvino->start();

        ModelInfoMetadata model_info(dst->metadata().add(ModelInfoMetadata::name));
        model_info.set_model_name(_model->get_friendly_name());
        model_info.set_info("input", _model_input_info);
        model_info.set_info("output", _model_output_info);
        model_info.set_layer_names("input", _model_input_names);
        model_info.set_layer_names("output", _model_output_names);

        return dst;
    }

  protected:
    ov::Core _core;
    std::string _device;
    std::shared_ptr<ov::Model> _model;
    ov::CompiledModel _compiled_model;

    FrameInfo _model_input_info;
    FrameInfo _model_output_info;
    std::vector<std::string> _model_input_names;
    std::vector<std::string> _model_output_names;
    DictionaryCPtr _params;
    MemoryMapperPtr _input_mapper;
    OpenVINOContextPtr _openvino_context;

    bool is_device_gpu() const {
        return _device.find("GPU") != std::string::npos;
    }

    ContextPtr create_remote_context() {
        if (is_device_gpu() && !_openvino_context) {
            try {
                VAAPIContextPtr vaapi_ctx = VAAPIContext::create(_app_context);
                _openvino_context = std::make_shared<OpenVINOContext>(_core, _device, vaapi_ctx);
            } catch (std::exception &e) {
                printf("Exception creating OpenVINO™ toolkit remote context: %s\n", e.what());
                load_network();
                _openvino_context = std::make_shared<OpenVINOContext>(_compiled_model);
            }
        }
        return _openvino_context;
    }

    // Setups model preprocessing
    void configure_model_preprocessing() {
        ov::preprocess::PrePostProcessor ppp(_model);
        auto &ppp_input = ppp.input();

        switch (_input_info.media_type) {
        case MediaType::Tensors: {
            if (_input_info.tensors.size() != 1 || _model_input_info.tensors.size() != 1)
                throw std::runtime_error("Can't enable pre-processing on model with multiple tensors input");
            auto &model_info = _model_input_info.tensors.front();
            auto &requested_info = _input_info.tensors.front();

            if (requested_info.dtype != model_info.dtype)
                ppp_input.tensor().set_element_type(data_type_to_openvino(requested_info.dtype));
            if (requested_info.shape != model_info.shape)
                ppp_input.tensor().set_shape({requested_info.shape});
        } break;

        case MediaType::Image:
            // Special case for VAAPI NV12 surface input
            if (_input_info.memory_type != MemoryType::VAAPI)
                throw std::runtime_error("Image input is supported only for NV12 image format and VASurface memory");
            ppp_input.tensor()
                .set_element_type(ov::element::u8)
                .set_color_format(ov::preprocess::ColorFormat::NV12_TWO_PLANES, {"y", "uv"})
                .set_memory_type(ov::intel_gpu::memory_type::surface);
            ppp_input.preprocess().convert_color(ov::preprocess::ColorFormat::BGR);
            ppp_input.model().set_layout("NCHW");
            break;

        default:
            throw std::runtime_error("Unsupported input media type");
        }

        // ppp_input.model().set_layout("NHWC");
        // ppp_input.preprocess().convert_color(ov::preprocess::ColorFormat::BGRX);
        // ppp_input.tensor().set_memory_type(ov::intel_gpu::memory_type::buffer);

        // Add pre-processing to model
        _model = ppp.build();
    }

    // Loads network to the device
    void load_network() {
        if (!_compiled_model) {
            std::string config = _params->get<std::string>(param::config);
            auto ov_params = string_to_openvino_map(config);
            if (_openvino_context) {
                _compiled_model = _core.compile_model(_model, *_openvino_context, ov_params);
            } else {
                _compiled_model = _core.compile_model(_model, _device, ov_params);
            }
        }
    }

    FrameInfoVector info_variations(const FrameInfo &info, std::vector<MemoryType> memory_types,
                                    std::vector<DataType> data_types) {
        FrameInfoVector infos;
        for (MemoryType mtype : memory_types) {
            for (DataType dtype : data_types) {
                FrameInfo info2 = info;
                info2.memory_type = mtype;
                for (auto &tensor : info2.tensors) {
                    tensor.dtype = dtype;
                    tensor.stride = contiguous_stride(tensor.shape, dtype);
                }
                infos.push_back(info2);
            }
        }
        return infos;
    }

    static ov::AnyMap string_to_openvino_map(const std::string &s, char rec_delim = ',', char kv_delim = '=') {
        std::string key, val;
        std::istringstream iss(s);
        ov::AnyMap m;

        while (std::getline(std::getline(iss, key, kv_delim) >> std::ws, val, rec_delim)) {
            m.emplace(std::move(key), std::move(val));
        }

        return m;
    }
};

extern "C" {
ElementDesc tensor_inference_openvino = {.name = "tensor_inference_openvino",
                                         .description = "Inference on OpenVINO™ toolkit backend",
                                         .author = "Intel Corporation",
                                         .params = &params_desc,
                                         .input_info = {{MediaType::Tensors, MemoryType::OpenCL},
                                                        {MediaType::Tensors, MemoryType::CPU},
                                                        {ImageFormat::NV12, MemoryType::VAAPI}},
                                         .output_info = {{MediaType::Tensors, MemoryType::OpenVINO}},
                                         .create = create_element<InferenceOpenVINO>,
                                         .flags = ELEMENT_FLAG_SHARABLE};
}

} // namespace dlstreamer
