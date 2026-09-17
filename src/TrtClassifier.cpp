#include "TrtClassifier.h"

#include <NvInfer.h>
#include <cuda_runtime_api.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {

class ClassifierLogger : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cerr << "[TensorRT Classifier] " << msg << std::endl;
        }
    }
} gClassifierLogger;

}

struct TrtClassifier::Impl {
    nvinfer1::IRuntime          *runtime = nullptr;
    nvinfer1::ICudaEngine       *engine  = nullptr;
    nvinfer1::IExecutionContext *context = nullptr;
    cudaStream_t                stream   = nullptr;

    int inputIndex  = -1;
    int outputIndex = -1;

    int inputW = 224;
    int inputH = 224;
    int inputC = 3;
    size_t inputSize = 0;

    size_t outputSize = 0;

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

TrtClassifier::TrtClassifier() : m_impl(std::make_unique<Impl>()) {}

TrtClassifier::~TrtClassifier() {
    unloadEngine();
}

QString TrtClassifier::lastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

bool TrtClassifier::isLoaded() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_impl && m_impl->engine != nullptr;
}

void TrtClassifier::unloadEngine() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_impl) {
        m_impl->cleanup();
    }
    m_enginePath.clear();
}

bool TrtClassifier::loadEngine(const QString &enginePath) {
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

    m_impl->runtime = nvinfer1::createInferRuntime(gClassifierLogger);
    if (!m_impl->runtime) {
        m_lastError = "Failed to create TensorRT InferRuntime.";
        return false;
    }

    m_impl->engine = m_impl->runtime->deserializeCudaEngine(modelData.data(), size, nullptr);
    if (!m_impl->engine) {
        m_lastError = "Failed to deserialize engine. Make sure the .engine was built on this Jetson.";
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
                m_impl->inputH = dims.d[dims.nbDims - 2] > 0 ? dims.d[dims.nbDims - 2] : 224;
                m_impl->inputW = dims.d[dims.nbDims - 1] > 0 ? dims.d[dims.nbDims - 1] : 224;
            } else if (dims.nbDims == 3) {
                m_impl->inputC = dims.d[0] > 0 ? dims.d[0] : 3;
                m_impl->inputH = dims.d[1] > 0 ? dims.d[1] : 224;
                m_impl->inputW = dims.d[2] > 0 ? dims.d[2] : 224;
            }
            if (m_impl->inputW <= 0) m_impl->inputW = 224;
            if (m_impl->inputH <= 0) m_impl->inputH = 224;
            if (m_impl->inputC <= 0) m_impl->inputC = 3;
            m_impl->inputSize = m_impl->inputC * m_impl->inputH * m_impl->inputW;

            nvinfer1::Dims4 inDims(1, m_impl->inputC, m_impl->inputH, m_impl->inputW);
            m_impl->context->setBindingDimensions(i, inDims);
        } else {
            m_impl->outputIndex = i;
            auto dims = m_impl->engine->getBindingDimensions(i);
            size_t total = 1;
            for (int d = 0; d < dims.nbDims; ++d) {
                int64_t val = dims.d[d] > 0 ? dims.d[d] : 1;
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

ClassificationResult TrtClassifier::classify(const QImage &img) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ClassificationResult res;
    if (!m_impl->engine || !m_impl->context || img.isNull() || m_impl->outputSize == 0) return res;

    int targetW = m_impl->inputW;
    int targetH = m_impl->inputH;

    QImage rgbImg = img.convertToFormat(QImage::Format_RGB888);
    QImage resizedImg = rgbImg.scaled(targetW, targetH, Qt::IgnoreAspectRatio, Qt::FastTransformation);

    float mean[3] = {0.485f, 0.456f, 0.406f};
    float std[3]  = {0.229f, 0.224f, 0.225f};

    for (int y = 0; y < targetH; ++y) {
        const uchar *line = resizedImg.constScanLine(y);
        for (int x = 0; x < targetW; ++x) {
            float r = line[x * 3 + 0] / 255.0f;
            float g = line[x * 3 + 1] / 255.0f;
            float b = line[x * 3 + 2] / 255.0f;

            m_impl->hostInput[0 * targetH * targetW + y * targetW + x] = (r - mean[0]) / std[0];
            m_impl->hostInput[1 * targetH * targetW + y * targetW + x] = (g - mean[1]) / std[1];
            m_impl->hostInput[2 * targetH * targetW + y * targetW + x] = (b - mean[2]) / std[2];
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

    float maxVal = -1e9f;
    for (size_t i = 0; i < m_impl->outputSize; ++i) {
        if (m_impl->hostOutput[i] > maxVal) {
            maxVal = m_impl->hostOutput[i];
        }
    }

    float sumExp = 0.0f;
    std::vector<float> probs(m_impl->outputSize);
    for (size_t i = 0; i < m_impl->outputSize; ++i) {
        probs[i] = std::exp(m_impl->hostOutput[i] - maxVal);
        sumExp += probs[i];
    }

    int bestIdx = 0;
    float bestProb = 0.0f;
    for (size_t i = 0; i < m_impl->outputSize; ++i) {
        probs[i] /= sumExp;
        if (probs[i] > bestProb) {
            bestProb = probs[i];
            bestIdx = static_cast<int>(i);
        }
    }

    res.classId = bestIdx;
    res.confidence = bestProb;
    res.className = QString("Class %1").arg(bestIdx);
    res.valid = true;

    return res;
}
