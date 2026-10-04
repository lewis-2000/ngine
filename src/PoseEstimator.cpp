#include "PoseEstimator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <vector>

#ifdef NGINE_ENABLE_TFLITE
#include <tensorflow/lite/c/c_api.h>
#endif

namespace Mara
{
    void MockPoseEstimator::update(
        float deltaTime,
        const unsigned char *rgbaPixels,
        int width,
        int height)
    {
        (void)rgbaPixels;
        (void)width;
        (void)height;
        m_Time += deltaTime;
        m_Frame.timestamp = m_Time;
        m_Frame.valid = true;

        const float sway = 0.025f * std::sin(m_Time * 2.0f);
        const std::array<glm::vec2, PoseFrame::LandmarkCount> landmarks = {
            glm::vec2(0.50f + sway, 0.16f),
            glm::vec2(0.46f + sway, 0.14f),
            glm::vec2(0.54f + sway, 0.14f),
            glm::vec2(0.42f + sway, 0.17f),
            glm::vec2(0.58f + sway, 0.17f),
            glm::vec2(0.35f + sway, 0.33f),
            glm::vec2(0.65f + sway, 0.33f),
            glm::vec2(0.27f + sway, 0.51f),
            glm::vec2(0.73f + sway, 0.51f),
            glm::vec2(0.21f + sway, 0.68f),
            glm::vec2(0.79f + sway, 0.68f),
            glm::vec2(0.43f + sway, 0.51f),
            glm::vec2(0.57f + sway, 0.51f),
            glm::vec2(0.39f + sway, 0.73f),
            glm::vec2(0.61f + sway, 0.73f),
            glm::vec2(0.35f + sway, 0.93f),
            glm::vec2(0.65f + sway, 0.93f)};

        for (std::size_t i = 0; i < landmarks.size(); ++i)
        {
            m_Frame.landmarks[i].position = landmarks[i];
            m_Frame.landmarks[i].confidence = 1.0f;
        }
    }

    struct MoveNetPoseEstimator::Implementation
    {
#ifdef NGINE_ENABLE_TFLITE
        TfLiteModel *model = nullptr;
        TfLiteInterpreterOptions *options = nullptr;
        TfLiteInterpreter *interpreter = nullptr;
        int inputWidth = 0;
        int inputHeight = 0;
        TfLiteType inputType = kTfLiteNoType;
        TfLiteType outputType = kTfLiteNoType;
#endif
    };

    MoveNetPoseEstimator::MoveNetPoseEstimator(const std::string &modelPath)
        : m_Implementation(std::make_unique<Implementation>())
    {
#ifdef NGINE_ENABLE_TFLITE
        if (!std::filesystem::exists(modelPath))
        {
            m_Error = "MoveNet model not found: " + modelPath;
            return;
        }

        m_Implementation->model = TfLiteModelCreateFromFile(modelPath.c_str());
        m_Implementation->options = TfLiteInterpreterOptionsCreate();
        if (!m_Implementation->model || !m_Implementation->options)
        {
            m_Error = "Unable to create the TensorFlow Lite model.";
            return;
        }

        TfLiteInterpreterOptionsSetNumThreads(m_Implementation->options, 4);
        m_Implementation->interpreter =
            TfLiteInterpreterCreate(
                m_Implementation->model,
                m_Implementation->options);
        if (!m_Implementation->interpreter ||
            TfLiteInterpreterAllocateTensors(m_Implementation->interpreter) != kTfLiteOk)
        {
            m_Error = "Unable to initialize the MoveNet interpreter.";
            return;
        }

        TfLiteTensor *input =
            TfLiteInterpreterGetInputTensor(m_Implementation->interpreter, 0);
        if (!input || TfLiteTensorNumDims(input) != 4)
        {
            m_Error = "MoveNet model has an unsupported input tensor.";
            return;
        }

        m_Implementation->inputHeight = TfLiteTensorDim(input, 1);
        m_Implementation->inputWidth = TfLiteTensorDim(input, 2);
        m_Implementation->inputType = TfLiteTensorType(input);

        TfLiteTensor *output =
            TfLiteInterpreterGetOutputTensor(m_Implementation->interpreter, 0);
        if (!output)
        {
            m_Error = "MoveNet model has no output tensor.";
            return;
        }
        m_Implementation->outputType = TfLiteTensorType(output);
#else
        (void)modelPath;
        m_Error = "TensorFlow Lite support is disabled.";
#endif
    }

    MoveNetPoseEstimator::~MoveNetPoseEstimator()
    {
#ifdef NGINE_ENABLE_TFLITE
        if (m_Implementation->interpreter)
            TfLiteInterpreterDelete(m_Implementation->interpreter);
        if (m_Implementation->options)
            TfLiteInterpreterOptionsDelete(m_Implementation->options);
        if (m_Implementation->model)
            TfLiteModelDelete(m_Implementation->model);
#endif
    }

    void MoveNetPoseEstimator::update(
        float deltaTime,
        const unsigned char *rgbaPixels,
        int width,
        int height)
    {
        m_Time += deltaTime;
#ifdef NGINE_ENABLE_TFLITE
        if (!isAvailable() || !rgbaPixels || width <= 0 || height <= 0)
            return;

        TfLiteTensor *input =
            TfLiteInterpreterGetInputTensor(m_Implementation->interpreter, 0);
        const int inputWidth = m_Implementation->inputWidth;
        const int inputHeight = m_Implementation->inputHeight;
        const std::size_t pixelCount =
            static_cast<std::size_t>(inputWidth * inputHeight * 3);
        std::vector<float> resizedFloat(pixelCount);
        std::vector<unsigned char> resizedBytes(pixelCount);
        std::vector<signed char> resizedInt8(pixelCount);
        const TfLiteQuantizationParams inputQuantization =
            TfLiteTensorQuantizationParams(input);

        for (int y = 0; y < inputHeight; ++y)
        {
            const int sourceY = y * height / inputHeight;
            for (int x = 0; x < inputWidth; ++x)
            {
                const int sourceX = x * width / inputWidth;
                const unsigned char *source =
                    rgbaPixels + (sourceY * width + sourceX) * 4;
                const std::size_t targetIndex =
                    static_cast<std::size_t>((y * inputWidth + x) * 3);
                for (int channel = 0; channel < 3; ++channel)
                {
                    const float value = static_cast<float>(source[channel]);
                    resizedFloat[targetIndex + channel] = value;
                    if (m_Implementation->inputType == kTfLiteUInt8)
                    {
                        const float scale = inputQuantization.scale > 0.0f
                                                ? inputQuantization.scale
                                                : 1.0f;
                        const int quantized = static_cast<int>(
                            std::lround(value / scale +
                                        inputQuantization.zero_point));
                        resizedBytes[targetIndex + channel] =
                            static_cast<unsigned char>(
                                std::clamp(quantized, 0, 255));
                    }
                    else if (m_Implementation->inputType == kTfLiteInt8)
                    {
                        const float scale = inputQuantization.scale > 0.0f
                                                ? inputQuantization.scale
                                                : 1.0f;
                        const int quantized = static_cast<int>(
                            std::lround(value / scale +
                                        inputQuantization.zero_point));
                        resizedInt8[targetIndex + channel] =
                            static_cast<signed char>(
                                std::clamp(quantized, -128, 127));
                    }
                }
            }
        }

        const void *inputData = resizedFloat.data();
        std::size_t inputBytes = resizedFloat.size() * sizeof(float);
        if (m_Implementation->inputType == kTfLiteUInt8)
        {
            inputData = resizedBytes.data();
            inputBytes = resizedBytes.size();
        }
        else if (m_Implementation->inputType == kTfLiteInt8)
        {
            inputData = resizedInt8.data();
            inputBytes = resizedInt8.size();
        }
        else if (m_Implementation->inputType != kTfLiteFloat32)
        {
            m_Error = "MoveNet input tensor type is unsupported.";
            return;
        }

        if (TfLiteTensorCopyFromBuffer(input, inputData, inputBytes) != kTfLiteOk ||
            TfLiteInterpreterInvoke(m_Implementation->interpreter) != kTfLiteOk)
        {
            m_Error = "MoveNet inference failed.";
            return;
        }

        TfLiteTensor *output =
            TfLiteInterpreterGetOutputTensor(m_Implementation->interpreter, 0);
        std::array<float, PoseFrame::LandmarkCount * 3> values{};
        if (!output)
        {
            m_Error = "Unable to read MoveNet output.";
            return;
        }

        const std::size_t outputBytes = values.size() * sizeof(float);
        if (m_Implementation->outputType == kTfLiteFloat32)
        {
            if (TfLiteTensorCopyToBuffer(output, values.data(), outputBytes) != kTfLiteOk)
            {
                m_Error = "Unable to read MoveNet output.";
                return;
            }
        }
        else if (m_Implementation->outputType == kTfLiteUInt8 ||
                 m_Implementation->outputType == kTfLiteInt8)
        {
            std::vector<unsigned char> outputBytesRaw(outputBytes);
            if (TfLiteTensorCopyToBuffer(
                    output,
                    outputBytesRaw.data(),
                    outputBytesRaw.size()) != kTfLiteOk)
            {
                m_Error = "Unable to read MoveNet output.";
                return;
            }
            const TfLiteQuantizationParams outputQuantization =
                TfLiteTensorQuantizationParams(output);
            const float scale = outputQuantization.scale > 0.0f
                                    ? outputQuantization.scale
                                    : 1.0f;
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                const int raw = m_Implementation->outputType == kTfLiteInt8
                                    ? static_cast<int>(
                                          static_cast<const signed char *>(
                                              static_cast<const void *>(
                                                  outputBytesRaw.data()))[i])
                                    : static_cast<int>(outputBytesRaw[i]);
                values[i] = (raw - outputQuantization.zero_point) * scale;
            }
        }
        else
        {
            m_Error = "MoveNet output tensor type is unsupported.";
            return;
        }

        m_Frame.timestamp = m_Time;
        m_Frame.valid = true;
        for (std::size_t i = 0; i < PoseFrame::LandmarkCount; ++i)
        {
            m_Frame.landmarks[i].position = glm::vec2(values[i * 3 + 1], values[i * 3]);
            m_Frame.landmarks[i].confidence = values[i * 3 + 2];
        }
#else
        (void)rgbaPixels;
        (void)width;
        (void)height;
#endif
    }

    const char *MoveNetPoseEstimator::backendName() const
    {
        return isAvailable() ? "MoveNet Lightning (TensorFlow Lite)" : m_Error.c_str();
    }

    bool MoveNetPoseEstimator::isAvailable() const
    {
#ifdef NGINE_ENABLE_TFLITE
        return m_Implementation &&
               m_Implementation->interpreter &&
               m_Implementation->inputWidth > 0 &&
               m_Implementation->inputHeight > 0 &&
               (m_Implementation->inputType == kTfLiteFloat32 ||
                m_Implementation->inputType == kTfLiteUInt8 ||
                m_Implementation->inputType == kTfLiteInt8) &&
               (m_Implementation->outputType == kTfLiteFloat32 ||
                m_Implementation->outputType == kTfLiteUInt8 ||
                m_Implementation->outputType == kTfLiteInt8);
#else
        return false;
#endif
    }

    std::unique_ptr<PoseEstimator> createPoseEstimator()
    {
#ifdef NGINE_ENABLE_TFLITE
        auto moveNet = std::make_unique<MoveNetPoseEstimator>(
            "resources/models/pose/movenet_lightning.tflite");
        if (moveNet->isAvailable())
            return moveNet;
#endif
        return std::make_unique<MockPoseEstimator>();
    }
}
