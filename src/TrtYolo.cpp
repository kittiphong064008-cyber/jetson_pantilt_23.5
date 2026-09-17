#include "TrtYolo.h"

#include <NvInfer.h>
#include <cuda_runtime_api.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {

class Logger : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cerr << "[TensorRT] " << msg << std::endl;
        }
    }
} gLogger;

struct NmsBox {
    float x1, y1, x2, y2;
    float score;
    int classId;
};

float computeIoU(const NmsBox &a, const NmsBox &b) {
    float xx1 = std::max(a.x1, b.x1);
    float yy1 = std::max(a.y1, b.y1);
    float xx2 = std::min(a.x2, b.x2);
    float yy2 = std::min(a.y2, b.y2);

    float w = std::max(0.0f, xx2 - xx1);
    float h = std::max(0.0f, yy2 - yy1);
    float inter = w * h;

    float areaA = (a.x2 - a.x1) * (a.y2 - a.y1);
    float areaB = (b.x2 - b.x1) * (b.y2 - b.y1);
    float unionArea = areaA + areaB - inter;
    if (unionArea <= 0.0f) return 0.0f;
    return inter / unionArea;
}

std::vector<NmsBox> runNMS(std::vector<NmsBox> &boxes, float iouThresh) {
    std::sort(boxes.begin(), boxes.end(), [](const NmsBox &a, const NmsBox &b) {
        return a.score > b.score;
    });

    std::vector<NmsBox> result;
    std::vector<bool> suppressed(boxes.size(), false);

    for (size_t i = 0; i < boxes.size(); ++i) {
        if (suppressed[i]) continue;
        result.push_back(boxes[i]);
        for (size_t j = i + 1; j < boxes.size(); ++j) {
            if (suppressed[j]) continue;
            if (boxes[i].classId == boxes[j].classId) {
                if (computeIoU(boxes[i], boxes[j]) > iouThresh) {
                    suppressed[j] = true;
                }
            }
        }
    }
    return result;
}

}

struct TrtYolo::Impl {
    nvinfer1::IRuntime          *runtime = nullptr;
    nvinfer1::ICudaEngine       *engine  = nullptr;
    nvinfer1::IExecutionContext *context = nullptr;
    cudaStream_t                stream   = nullptr;

    int inputIndex  = -1;
    int outputIndex = -1;

    int inputW = 320;
    int inputH = 320;
    int inputC = 3;
    size_t inputSize = 0;

    size_t outputSize = 0;
    std::vector<int64_t> outputDims;

    void *buffers[2] = {nullptr, nullptr};
    std::vector<float> hostInput;
    std::vector<float> hostOutput;

    void cleanup() {
        if (buffers[0]) { cudaFree(buffers[0]); buffers[0] = nullptr; }
        if (buffers[1]) { cudaFree(buffers[1]); buffers[1] = nullptr; }
        if (stream)    { cudaStreamDestroy(stream); stream = nullptr; }
        if (context)   { context->destroy(); context = nullptr; }
        if (engine)    { engine->destroy(); engine = nullptr; }
        if (runtime)   { runtime->destroy(); runtime = nullptr; }
    }
};

TrtYolo::TrtYolo() : m_impl(std::make_unique<Impl>()) {}

TrtYolo::~TrtYolo() {
    unloadEngine();
}

QString TrtYolo::lastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

bool TrtYolo::isLoaded() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_impl && m_impl->engine != nullptr;
}

void TrtYolo::unloadEngine() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_impl) {
        m_impl->cleanup();
    }
    m_enginePath.clear();
}

bool TrtYolo::loadEngine(const QString &enginePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_impl) m_impl->cleanup();
    m_enginePath.clear();
    m_lastError.clear();

    std::ifstream file(enginePath.toStdString(), std::ios::binary);
    if (!file.is_open()) {
        m_lastError = "Cannot open file: " + enginePath;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size == 0) {
        m_lastError = "Engine file is empty.";
        return false;
    }

    std::vector<char> modelData(size);
    file.read(modelData.data(), size);
    file.close();

    m_impl->runtime = nvinfer1::createInferRuntime(gLogger);
    if (!m_impl->runtime) {
        m_lastError = "Failed to create TensorRT InferRuntime.";
        return false;
    }

    m_impl->engine = m_impl->runtime->deserializeCudaEngine(modelData.data(), size, nullptr);
    if (!m_impl->engine) {
        m_lastError = "Failed to deserialize engine. Make sure the .engine was built on this Jetson with trtexec.";
        m_impl->cleanup();
        return false;
    }

    m_impl->context = m_impl->engine->createExecutionContext();
    if (!m_impl->context) {
        m_lastError = "Failed to create TensorRT ExecutionContext.";
        m_impl->cleanup();
        return false;
    }

    cudaStreamCreate(&m_impl->stream);

    int nbBindings = m_impl->engine->getNbBindings();
    for (int i = 0; i < nbBindings; ++i) {
        if (m_impl->engine->bindingIsInput(i)) {
            m_impl->inputIndex = i;
            auto dims = m_impl->engine->getBindingDimensions(i);
            if (dims.nbDims >= 4) {
                m_impl->inputC = dims.d[dims.nbDims - 3] > 0 ? dims.d[dims.nbDims - 3] : 3;
                m_impl->inputH = dims.d[dims.nbDims - 2] > 0 ? dims.d[dims.nbDims - 2] : 320;
                m_impl->inputW = dims.d[dims.nbDims - 1] > 0 ? dims.d[dims.nbDims - 1] : 320;
            } else if (dims.nbDims == 3) {
                m_impl->inputC = dims.d[0] > 0 ? dims.d[0] : 3;
                m_impl->inputH = dims.d[1] > 0 ? dims.d[1] : 320;
                m_impl->inputW = dims.d[2] > 0 ? dims.d[2] : 320;
            }
            if (m_impl->inputW <= 0) m_impl->inputW = 320;
            if (m_impl->inputH <= 0) m_impl->inputH = 320;
            if (m_impl->inputC <= 0) m_impl->inputC = 3;
            m_impl->inputSize = m_impl->inputC * m_impl->inputH * m_impl->inputW;

            nvinfer1::Dims4 inDims(1, m_impl->inputC, m_impl->inputH, m_impl->inputW);
            m_impl->context->setBindingDimensions(i, inDims);
        } else {
            m_impl->outputIndex = i;
            auto dims = m_impl->engine->getBindingDimensions(i);
            m_impl->outputDims.clear();
            size_t total = 1;
            for (int d = 0; d < dims.nbDims; ++d) {
                int64_t val = dims.d[d] > 0 ? dims.d[d] : 1;
                m_impl->outputDims.push_back(val);
                total *= val;
            }
            m_impl->outputSize = total;
        }
    }

    if (m_impl->inputIndex == -1 || m_impl->outputIndex == -1) {
        m_lastError = "Could not find input or output bindings in engine.";
        m_impl->cleanup();
        return false;
    }

    cudaMalloc(&m_impl->buffers[m_impl->inputIndex], m_impl->inputSize * sizeof(float));
    cudaMalloc(&m_impl->buffers[m_impl->outputIndex], m_impl->outputSize * sizeof(float));

    m_impl->hostInput.resize(m_impl->inputSize);
    m_impl->hostOutput.resize(m_impl->outputSize);

    m_enginePath = enginePath;
    return true;
}

std::vector<Detection> TrtYolo::detect(const QImage &img, float confThresh, float iouThresh) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Detection> detections;
    if (!m_impl->engine || !m_impl->context || img.isNull() || m_impl->outputSize == 0) return detections;

    int origW = img.width();
    int origH = img.height();

    int targetW = m_impl->inputW;
    int targetH = m_impl->inputH;

    float scale = std::min(static_cast<float>(targetW) / origW, static_cast<float>(targetH) / origH);
    int scaledW = static_cast<int>(origW * scale);
    int scaledH = static_cast<int>(origH * scale);
    int padX = (targetW - scaledW) / 2;
    int padY = (targetH - scaledH) / 2;

    QImage rgbImg = img.convertToFormat(QImage::Format_RGB888);
    QImage resizedImg = rgbImg.scaled(scaledW, scaledH, Qt::IgnoreAspectRatio, Qt::FastTransformation);

    std::fill(m_impl->hostInput.begin(), m_impl->hostInput.end(), 0.5f);

    for (int y = 0; y < scaledH; ++y) {
        const uchar *line = resizedImg.constScanLine(y);
        for (int x = 0; x < scaledW; ++x) {
            int inX = padX + x;
            int inY = padY + y;
            float r = line[x * 3 + 0] / 255.0f;
            float g = line[x * 3 + 1] / 255.0f;
            float b = line[x * 3 + 2] / 255.0f;

            m_impl->hostInput[0 * targetH * targetW + inY * targetW + inX] = r;
            m_impl->hostInput[1 * targetH * targetW + inY * targetW + inX] = g;
            m_impl->hostInput[2 * targetH * targetW + inY * targetW + inX] = b;
        }
    }

    cudaMemcpyAsync(m_impl->buffers[m_impl->inputIndex], m_impl->hostInput.data(),
                    m_impl->inputSize * sizeof(float), cudaMemcpyHostToDevice, m_impl->stream);

    void *bindings[2];
    bindings[m_impl->inputIndex]  = m_impl->buffers[m_impl->inputIndex];
    bindings[m_impl->outputIndex] = m_impl->buffers[m_impl->outputIndex];

    m_impl->context->enqueueV2(bindings, m_impl->stream, nullptr);

    cudaMemcpyAsync(m_impl->hostOutput.data(), m_impl->buffers[m_impl->outputIndex],
                    m_impl->outputSize * sizeof(float), cudaMemcpyDeviceToHost, m_impl->stream);

    cudaStreamSynchronize(m_impl->stream);

    std::vector<NmsBox> candidateBoxes;

    if (m_impl->outputDims.size() == 3) {
        int64_t dim1 = m_impl->outputDims[1];
        int64_t dim2 = m_impl->outputDims[2];

        if (dim1 < dim2 && dim1 <= 128) {
            int numChannels = dim1;
            int numBoxes = dim2;
            int numClasses = numChannels - 4;

            for (int b = 0; b < numBoxes; ++b) {
                float cx = m_impl->hostOutput[0 * numBoxes + b];
                float cy = m_impl->hostOutput[1 * numBoxes + b];
                float w  = m_impl->hostOutput[2 * numBoxes + b];
                float h  = m_impl->hostOutput[3 * numBoxes + b];

                if (cx <= 1.05f && cy <= 1.05f && w <= 1.05f && h <= 1.05f) {
                    cx *= targetW;
                    cy *= targetH;
                    w  *= targetW;
                    h  *= targetH;
                }

                float maxScore = 0.0f;
                int maxClassId = 0;
                for (int c = 0; c < numClasses; ++c) {
                    float score = m_impl->hostOutput[(4 + c) * numBoxes + b];
                    if (score > maxScore) {
                        maxScore = score;
                        maxClassId = c;
                    }
                }

                if (maxScore >= confThresh) {
                    float x1 = (cx - w * 0.5f - padX) / scale;
                    float y1 = (cy - h * 0.5f - padY) / scale;
                    float x2 = (cx + w * 0.5f - padX) / scale;
                    float y2 = (cy + h * 0.5f - padY) / scale;

                    x1 = std::max(0.0f, std::min(x1, static_cast<float>(origW)));
                    y1 = std::max(0.0f, std::min(y1, static_cast<float>(origH)));
                    x2 = std::max(0.0f, std::min(x2, static_cast<float>(origW)));
                    y2 = std::max(0.0f, std::min(y2, static_cast<float>(origH)));

                    candidateBoxes.push_back({x1, y1, x2, y2, maxScore, maxClassId});
                }
            }
        } else {
            int numBoxes = dim1;
            int numChannels = dim2;

            if (numChannels >= 5) {
                int numClasses = numChannels - 5;

                for (int b = 0; b < numBoxes; ++b) {
                    const float *boxPtr = &m_impl->hostOutput[b * numChannels];
                    float cx = boxPtr[0];
                    float cy = boxPtr[1];
                    float w  = boxPtr[2];
                    float h  = boxPtr[3];
                    float objConf = boxPtr[4];

                    if (cx <= 1.05f && cy <= 1.05f && w <= 1.05f && h <= 1.05f) {
                        cx *= targetW;
                        cy *= targetH;
                        w  *= targetW;
                        h  *= targetH;
                    }

                    float maxScore = 0.0f;
                    int maxClassId = 0;

                    if (numClasses > 0) {
                        for (int c = 0; c < numClasses; ++c) {
                            float score = objConf * boxPtr[5 + c];
                            if (score > maxScore) {
                                maxScore = score;
                                maxClassId = c;
                            }
                        }
                    } else {
                        maxScore = objConf;
                        maxClassId = 0;
                    }

                    if (maxScore >= confThresh) {
                        float x1 = (cx - w * 0.5f - padX) / scale;
                        float y1 = (cy - h * 0.5f - padY) / scale;
                        float x2 = (cx + w * 0.5f - padX) / scale;
                        float y2 = (cy + h * 0.5f - padY) / scale;

                        x1 = std::max(0.0f, std::min(x1, static_cast<float>(origW)));
                        y1 = std::max(0.0f, std::min(y1, static_cast<float>(origH)));
                        x2 = std::max(0.0f, std::min(x2, static_cast<float>(origW)));
                        y2 = std::max(0.0f, std::min(y2, static_cast<float>(origH)));

                        candidateBoxes.push_back({x1, y1, x2, y2, maxScore, maxClassId});
                    }
                }
            } else {
                int numClasses = numChannels - 4;
                for (int b = 0; b < numBoxes; ++b) {
                    const float *boxPtr = &m_impl->hostOutput[b * numChannels];
                    float cx = boxPtr[0];
                    float cy = boxPtr[1];
                    float w  = boxPtr[2];
                    float h  = boxPtr[3];

                    if (cx <= 1.05f && cy <= 1.05f && w <= 1.05f && h <= 1.05f) {
                        cx *= targetW;
                        cy *= targetH;
                        w  *= targetW;
                        h  *= targetH;
                    }

                    float maxScore = 0.0f;
                    int maxClassId = 0;
                    for (int c = 0; c < numClasses; ++c) {
                        float score = boxPtr[4 + c];
                        if (score > maxScore) {
                            maxScore = score;
                            maxClassId = c;
                        }
                    }

                    if (maxScore >= confThresh) {
                        float x1 = (cx - w * 0.5f - padX) / scale;
                        float y1 = (cy - h * 0.5f - padY) / scale;
                        float x2 = (cx + w * 0.5f - padX) / scale;
                        float y2 = (cy + h * 0.5f - padY) / scale;

                        x1 = std::max(0.0f, std::min(x1, static_cast<float>(origW)));
                        y1 = std::max(0.0f, std::min(y1, static_cast<float>(origH)));
                        x2 = std::max(0.0f, std::min(x2, static_cast<float>(origW)));
                        y2 = std::max(0.0f, std::min(y2, static_cast<float>(origH)));

                        candidateBoxes.push_back({x1, y1, x2, y2, maxScore, maxClassId});
                    }
                }
            }
        }
    }

    auto nmsResults = runNMS(candidateBoxes, iouThresh);
    for (const auto &box : nmsResults) {
        Detection d;
        d.bbox = QRectF(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        d.conf = box.score;
        d.classId = box.classId;
        d.className = QString("Class %1").arg(box.classId);
        detections.push_back(d);
    }

    return detections;
}
