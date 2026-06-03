#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "../third_party/NeuralAmpModelerCore/NAM/dsp.h"
#include "../third_party/NeuralAmpModelerCore/NAM/get_dsp.h"
#include "../third_party/NeuralAmpModelerCore/NAM/wavenet/model.h"
#include "../third_party/NeuralAmpModelerCore/NAM/convnet.h"
#include "../third_party/NeuralAmpModelerCore/NAM/lstm.h"
#include "../third_party/NeuralAmpModelerCore/NAM/slimmable.h"

std::vector<double> generate_sine_wave(double frequency, double duration, double sample_rate) {
  int num_samples = static_cast<int>(duration * sample_rate);
  std::vector<double> signal(num_samples);
  double amplitude = 0.1;  // Avoid clipping
  double two_pi = 2.0 * M_PI;

  for (int i = 0; i < num_samples; ++i) {
    double t = static_cast<double>(i) / sample_rate;
    signal[i] = amplitude * std::sin(two_pi * frequency * t);
  }
  return signal;
}

struct MeasurementResult {
  double peak;
  double rms;
};

MeasurementResult measure_signal(const std::vector<double>& signal) {
  MeasurementResult result{0.0, 0.0};

  // Measure peak
  for (double sample : signal) {
    double abs_sample = std::fabs(sample);
    if (abs_sample > result.peak) {
      result.peak = abs_sample;
    }
  }

  // Measure RMS
  double sum_squares = 0.0;
  for (double sample : signal) {
    sum_squares += sample * sample;
  }
  result.rms = std::sqrt(sum_squares / static_cast<double>(signal.size()));

  return result;
}

double to_db(double value1, double value2) {
  if (value2 == 0.0) return 0.0;
  return 20.0 * std::log10(value1 / value2);
}

void write_wav(const std::string& filename, const std::vector<double>& samples, double sample_rate) {
  std::ofstream file(filename, std::ios::binary);
  if (!file) {
    std::cerr << "Failed to open " << filename << " for writing\n";
    return;
  }

  uint32_t num_samples = samples.size();
  uint16_t num_channels = 1;
  uint32_t sample_rate_int = static_cast<uint32_t>(sample_rate);
  uint16_t bits_per_sample = 16;
  uint32_t byte_rate = sample_rate_int * num_channels * bits_per_sample / 8;
  uint16_t block_align = num_channels * bits_per_sample / 8;

  // RIFF header
  file.write("RIFF", 4);
  uint32_t chunk_size = 36 + num_samples * num_channels * bits_per_sample / 8;
  file.write(reinterpret_cast<const char*>(&chunk_size), 4);
  file.write("WAVE", 4);

  // fmt subchunk
  file.write("fmt ", 4);
  uint32_t subchunk1_size = 16;
  file.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
  uint16_t audio_format = 1;  // PCM
  file.write(reinterpret_cast<const char*>(&audio_format), 2);
  file.write(reinterpret_cast<const char*>(&num_channels), 2);
  file.write(reinterpret_cast<const char*>(&sample_rate_int), 4);
  file.write(reinterpret_cast<const char*>(&byte_rate), 4);
  file.write(reinterpret_cast<const char*>(&block_align), 2);
  file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

  // data subchunk
  file.write("data", 4);
  uint32_t subchunk2_size = num_samples * num_channels * bits_per_sample / 8;
  file.write(reinterpret_cast<const char*>(&subchunk2_size), 4);

  // Write audio samples (convert double to int16)
  for (double sample : samples) {
    // Clamp to [-1.0, 1.0] and convert to int16
    double clamped = std::max(-1.0, std::min(1.0, sample));
    int16_t int_sample = static_cast<int16_t>(clamped * 32767.0);
    file.write(reinterpret_cast<const char*>(&int_sample), 2);
  }

  file.close();
  std::cout << "   Wrote: " << filename << "\n";
}

int main(int argc, char* argv[]) {
  std::cout << "======================================================================\n";
  std::cout << "NAM Volume Knob Audio Test\n";
  std::cout << "======================================================================\n\n";

  // File paths
  // Use environment variable NAM_TEST_MODELS_DIR or default to ~/Downloads
  std::string models_dir = []{
    const char* env = std::getenv("NAM_TEST_MODELS_DIR");
    if (env && std::strlen(env) > 0) {
      return std::string(env);
    }
    const char* home = std::getenv("HOME");
    if (home) {
      return std::string(home) + "/Downloads";
    }
    return std::string(".");
  }();

  std::string original_path = models_dir + "/Deluxe Reverb.nam";
  std::string plus6_path = models_dir + "/Deluxe Reverb +6dB.nam";
  std::string plus9_path = models_dir + "/Deluxe Reverb +9dB.nam";

  // Generate test signal
  std::cout << "📊 Generating sine wave test signal...\n";
  double sample_rate = 48000.0;
  auto audio = generate_sine_wave(1000.0, 2.0, sample_rate);
  auto input_measurement = measure_signal(audio);
  std::cout << "   Input signal: Peak=" << input_measurement.peak << ", RMS=" << input_measurement.rms
            << "\n\n";

  // Load models
  std::cout << "📂 Loading NAM models...\n";
  std::unique_ptr<nam::DSP> model_original;
  std::unique_ptr<nam::DSP> model_plus6;
  std::unique_ptr<nam::DSP> model_plus9;

  try {
    std::cout << "   Loading original...", std::cout.flush();
    model_original = nam::get_dsp(std::filesystem::path(original_path));
    std::cout << " ✓\n";
  } catch (const std::exception& e) {
    std::cerr << " ❌ FAILED: " << e.what() << "\n";
    return 1;
  }

  try {
    std::cout << "   Loading +6dB...", std::cout.flush();
    model_plus6 = nam::get_dsp(std::filesystem::path(plus6_path));
    std::cout << " ✓\n";
  } catch (const std::exception& e) {
    std::cerr << " ❌ FAILED: " << e.what() << "\n";
    return 1;
  }

  try {
    std::cout << "   Loading +9dB...", std::cout.flush();
    model_plus9 = nam::get_dsp(std::filesystem::path(plus9_path));
    std::cout << " ✓\n\n";
  } catch (const std::exception& e) {
    std::cerr << " ❌ FAILED: " << e.what() << "\n";
    return 1;
  }

  // Reset models
  model_original->Reset(sample_rate, static_cast<int>(audio.size()));
  model_original->prewarm();
  model_plus6->Reset(sample_rate, static_cast<int>(audio.size()));
  model_plus6->prewarm();
  model_plus9->Reset(sample_rate, static_cast<int>(audio.size()));
  model_plus9->prewarm();

  // Process audio
  std::cout << "🔊 Processing audio through models...\n";

  // Store outputs for WAV export
  std::vector<double> orig_output;
  std::vector<double> plus6_output;
  std::vector<double> plus9_output;

  auto process_model = [&](nam::DSP* model, const std::string& name, std::vector<double>& out_buffer) -> MeasurementResult {
    std::cout << "   " << name << "...", std::cout.flush();

    // Create buffer for processing
    std::vector<double> audio_copy = audio;
    out_buffer.resize(audio.size(), 0.0);

    double* input[] = {audio_copy.data()};
    double* output_ptr[] = {out_buffer.data()};

    model->process(input, output_ptr, static_cast<int>(audio.size()));

    auto result = measure_signal(out_buffer);
    std::cout << " ✓ (Peak=" << result.peak << ", RMS=" << result.rms << ")\n";

    return result;
  };

  auto orig_result = process_model(model_original.get(), "Original", orig_output);
  auto plus6_result = process_model(model_plus6.get(), "+6dB", plus6_output);
  auto plus9_result = process_model(model_plus9.get(), "+9dB", plus9_output);

  // Compare results
  std::cout << "\n📈 Comparison:\n";
  std::cout << "----------------------------------------------------------------------\n";

  auto compare = [&](const std::string& name, const MeasurementResult& result, int expected_db) {
    double peak_db = to_db(result.peak, orig_result.peak);
    double rms_db = to_db(result.rms, orig_result.rms);

    std::cout << "\n" << name << " vs Original:\n";
    std::cout << "   Peak: " << result.peak << " (" << std::showpos << peak_db << std::noshowpos
              << " dB, expected " << expected_db << " dB)\n";
    std::cout << "   RMS:  " << result.rms << " (" << std::showpos << rms_db << std::noshowpos
              << " dB)\n";

    double tolerance = 1.0;  // 1dB tolerance for model dynamics
    bool peak_match = std::fabs(peak_db - expected_db) < tolerance;

    if (peak_match) {
      std::cout << "   ✓ PASS (within tolerance)\n";
      return true;
    } else {
      std::cout << "   ⚠ WARNING: Expected ~" << expected_db << "dB, got " << std::showpos << peak_db
                << std::noshowpos << "dB\n";
      return false;
    }
  };

  bool pass6 = compare("+6dB", plus6_result, 6);
  bool pass9 = compare("+9dB", plus9_result, 9);

  std::cout << "\n" << std::string(70, '=') << "\n";

  // Save outputs to WAV files for comparison
  std::cout << "\n💾 Saving audio outputs to WAV files...\n";
  write_wav("/tmp/deluxe_reverb_original.wav", orig_output, sample_rate);
  write_wav("/tmp/deluxe_reverb_+6db.wav", plus6_output, sample_rate);
  write_wav("/tmp/deluxe_reverb_+9db.wav", plus9_output, sample_rate);
  std::cout << "\nOpen these files in your DAW to compare output levels:\n";
  std::cout << "  /tmp/deluxe_reverb_original.wav\n";
  std::cout << "  /tmp/deluxe_reverb_+6db.wav\n";
  std::cout << "  /tmp/deluxe_reverb_+9db.wav\n";

  std::cout << "\n" << std::string(70, '=') << "\n";
  if (pass6 && pass9) {
    std::cout << "✅ ALL TESTS PASSED\n";
    return 0;
  } else {
    std::cout << "❌ SOME TESTS FAILED\n";
    return 1;
  }
}
