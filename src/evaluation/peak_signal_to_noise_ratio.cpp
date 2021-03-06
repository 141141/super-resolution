#include "evaluation/peak_signal_to_noise_ratio.h"

#include <cmath>

#include "image/image_data.h"

#include "glog/logging.h"

namespace super_resolution {

double PeakSignalToNoiseRatioEvaluator::Evaluate(const ImageData& image) const {
  const int num_pixels = image.GetNumPixels();
  const int num_channels = image.GetNumChannels();

  CHECK_EQ(num_channels, ground_truth_.GetNumChannels())
      << "Images must have the same number of channels to be compared.";

  // If images are different sizes, resize the given image to match the ground
  // truth so per-pixel comparison can be done.
  ImageData evaluation_image = image;
  if (image.GetImageSize() != ground_truth_.GetImageSize()) {
    LOG(WARNING) << "Image size is different from ground truth: "
                 << image.GetImageSize() << " vs. "
                 << ground_truth_.GetImageSize() << ". "
                 << "Resizing image to run evaluation.";
    evaluation_image.ResizeImage(image.GetImageSize(), INTERPOLATE_LINEAR);
  }

  double sum_of_squared_differences = 0.0;
  for (int channel_index = 0; channel_index < num_channels; ++channel_index) {
    const double* ground_truth_channel_data =
        ground_truth_.GetChannelData(channel_index);
    const double* image_channel_data =
        evaluation_image.GetChannelData(channel_index);
    for (int pixel_index = 0; pixel_index < num_pixels; ++pixel_index) {
      const double difference =
          ground_truth_channel_data[pixel_index] -
          image_channel_data[pixel_index];
      sum_of_squared_differences += (difference * difference);
    }
  }
  const int total_num_pixels = num_pixels * num_channels;
  const double mean_squared_error =
      sum_of_squared_differences / static_cast<double>(total_num_pixels);

  const double max_pixel_value = 1.0;  // TODO: can be 255?

  // PSNR = 10 * log_10(MAX^2 / MSE)
  //      = 20 * log_10(MAX / sqrt(MSE))
  //      = 20 * log_10(MAX) - 10 * log_10(MSE)
  const double peak_signal_to_noise_ratio =
      20.0 * log10(max_pixel_value) - 10.0 * log10(mean_squared_error);
  return peak_signal_to_noise_ratio;
}

}  // namespace super_resolution
