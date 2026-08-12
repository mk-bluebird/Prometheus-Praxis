// File: cpp/eco_restoration/invasive_species_tflite_inference.cpp

#include <gdal_priv.h>
#include <tensorflow/lite/interpreter_builder.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: invasive_species_tflite_inference model.tflite image.tif risk_threshold\n";
        return 2;
    }

    const auto model = tflite::FlatBufferModel::BuildFromFile(argv[1]);
    if (!model) throw std::runtime_error("cannot load TFLite model");

    tflite::ops::builtin::BuiltinOpResolver resolver;
    std::unique_ptr<tflite::Interpreter> interpreter;
    if (tflite::InterpreterBuilder(*model, resolver)(&interpreter) != kTfLiteOk ||
        interpreter->AllocateTensors() != kTfLiteOk) {
        throw std::runtime_error("cannot initialize TFLite interpreter");
    }

    TfLiteTensor* input = interpreter->input_tensor(0);
    if (input->type != kTfLiteFloat32 || input->dims->size != 4 || input->dims->data[3] != 3) {
        throw std::runtime_error("model input must be floating-point RGB image");
    }

    const int height = input->dims->data[1];
    const int width = input->dims->data[2];
    float* tensor = interpreter->typed_input_tensor<float>(0);

    GDALAllRegister();
    GDALDataset* image = static_cast<GDALDataset*>(GDALOpen(argv[2], GA_ReadOnly));
    if (image == nullptr || image->GetRasterCount() < 3) {
        if (image != nullptr) GDALClose(image);
        throw std::runtime_error("image requires at least three raster bands");
    }

    std::vector<float> band(static_cast<std::size_t>(width) * height);
    for (int channel = 0; channel < 3; ++channel) {
        if (image->GetRasterBand(channel + 1)->RasterIO(
                GF_Read, 0, 0, image->GetRasterXSize(), image->GetRasterYSize(),
                band.data(), width, height, GDT_Float32, 0, 0) != CE_None) {
            GDALClose(image);
            throw std::runtime_error("cannot read image band");
        }
        for (int pixel = 0; pixel < width * height; ++pixel) {
            tensor[pixel * 3 + channel] = band[static_cast<std::size_t>(pixel)];
        }
    }
    GDALClose(image);

    if (interpreter->Invoke() != kTfLiteOk) throw std::runtime_error("TFLite inference failed");
    const float probability = interpreter->typed_output_tensor<float>(0)[0];
    const double threshold = std::stod(argv[3]);
    const double biodiversity_risk = std::clamp(static_cast<double>(probability), 0.0, 1.0);

    std::cout << "{\"invasive_probability\":" << biodiversity_risk
              << ",\"biodiversity_risk\":" << biodiversity_risk
              << ",\"operator_review\":" << (biodiversity_risk >= threshold ? "true" : "false")
              << "}\n";
}
