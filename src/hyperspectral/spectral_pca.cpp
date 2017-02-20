#include "hyperspectral/spectral_pca.h"

#include <algorithm>
#include <vector>

#include "image/image_data.h"
#include "util/matrix_util.h"

#include "opencv2/core/core.hpp"

#include "glog/logging.h"

namespace super_resolution {

// Returns the data from the given images (one or more required) in
// pixel-vector form. That is, instead of the images being organized by
// channel, each row of the returned matrix will be a pixel, and the columns
// span the different channels. The number of rows is the total number of
// pixels across all images.
//
// TODO: This will probably result in way-too-big images, so sum-sampling the
// pixels might be a good idea.
cv::Mat GetPcaInputData(const std::vector<ImageData>& hyperspectral_images) {
  CHECK(!hyperspectral_images.empty())
      << "At least one image is required to compute the PCA basis.";

  // Make sure we have the right number of channels. Also it does not make
  // sense to do this on non-hyperspectral images, so warn the user if that's
  // the case.
  const int num_channels = hyperspectral_images[0].GetNumChannels();
  CHECK_GT(num_channels, 0) << "Cannot compute PCA on empty images.";
  if (num_channels <= 3) {
    LOG(WARNING)
        << "The given images do not appear to be hyperspectral "
        << "(3 or fewer channels). PCA decomposition may not be "
        << "useful or applicable here.";
  }

  // Format the input data as pixel vectors for PCA.
  // TODO: Probably need to do subsampling. Images can be way too big.
  const int num_images = hyperspectral_images.size();
  const int num_pixels = hyperspectral_images[0].GetNumPixels();
  const int num_data_points = num_images * num_pixels;
  cv::Mat input_data(num_data_points, num_channels, util::kOpenCvMatrixType);
  for (int image_index = 0; image_index < num_images; ++image_index) {
    const ImageData& image = hyperspectral_images[image_index];
    CHECK_EQ(image.GetNumChannels(), num_channels)
        << "Inconsistent number of channels between the given images. "
        << "Cannot perform PCA.";
    // TODO: Faster to use cv::ROI and unfold the channel matrices that way.
    for (int channel_index = 0; channel_index < num_channels; ++channel_index) {
      for (int pixel_index = 0; pixel_index < num_pixels; ++pixel_index) {
        const int data_row = image_index * num_pixels + pixel_index;
        input_data.at<double>(data_row, channel_index) =
            image.GetPixelValue(channel_index, pixel_index);
      }
    }
  }
  return input_data;
}

// This function will either convert images from hyperspectral space to PCA
// space or vice versa. Use the forward_projection flag to control the
// projection direction (true = hyperspectral to PCA projection, false = PCA to
// hyperspectral backprojection).
ImageData ConvertImage(
    const ImageData& input_image,
    const cv::PCA& pca,
    const int num_spectral_bands,
    const int num_pca_bands,
    const bool forward_projection) {

  int num_input_bands;
  int num_output_bands;
  if (forward_projection) {
    num_input_bands = num_spectral_bands;
    num_output_bands = num_pca_bands;
  } else {
    num_input_bands = num_pca_bands;
    num_output_bands = num_spectral_bands;
  }

  CHECK_EQ(input_image.GetNumChannels(), num_input_bands)
      << "The input image does not have the correct number of channels.";

  // Create empty OpenCV images for the output image channels.
  std::vector<cv::Mat> output_image_channels;
  output_image_channels.reserve(num_output_bands);
  for (int i = 0; i < num_output_bands; ++i) {
    cv::Mat channel_image =
        cv::Mat::zeros(input_image.GetImageSize(), util::kOpenCvMatrixType);
    output_image_channels.push_back(channel_image);
  }

  // Project the input image into the ouput space pixel by pixel.
  const int num_pixels = input_image.GetNumPixels();
  for (int pixel_index = 0; pixel_index < num_pixels; ++pixel_index) {
    // Extract the pixel vector from the input image.
    cv::Mat input_pixel_vector(num_input_bands, 1, util::kOpenCvMatrixType);
    for (int i = 0; i < num_input_bands; ++i) {
      input_pixel_vector.at<double>(i) =
          input_image.GetPixelValue(i, pixel_index);
    }
    // Project the input pixel vector into the other space and set the output
    // channel values.
    cv::Mat output_pixel_vector;
    if (forward_projection) {
      output_pixel_vector = pca.project(input_pixel_vector);
    } else {
      output_pixel_vector = pca.backProject(input_pixel_vector);
    }
    for (int i = 0; i < num_output_bands; ++i) {
      output_image_channels[i].at<double>(pixel_index) =
          output_pixel_vector.at<double>(i);
    }
  }

  // Return the projected image.
  ImageData output_image;
  for (const cv::Mat& channel_image : output_image_channels) {
    output_image.AddChannel(channel_image);
  }
  if (forward_projection) {
    output_image.SetSpectralMode(SPECTRAL_MODE_HYPERSPECTRAL_PCA);
  } else {
    output_image.SetSpectralMode(SPECTRAL_MODE_HYPERSPECTRAL);
  }
  return output_image;
}

SpectralPca::SpectralPca(
    const std::vector<ImageData>& hyperspectral_images,
    const int num_pca_bands) {

  const cv::Mat input_data = GetPcaInputData(hyperspectral_images);
  pca_ = cv::PCA(input_data, cv::Mat(), CV_PCA_DATA_AS_ROW, num_pca_bands);

  // Set the number of spectral in the original and PCA spaces.
  const cv::Size eigenvector_matrix_size = pca_.eigenvectors.size();
  num_spectral_bands_ = eigenvector_matrix_size.width;
  num_pca_bands_ = eigenvector_matrix_size.height;
}

SpectralPca::SpectralPca(
    const std::vector<ImageData>& hyperspectral_images,
    const double retained_variance) {

  const cv::Mat input_data = GetPcaInputData(hyperspectral_images);
  pca_ = cv::PCA(input_data, cv::Mat(), CV_PCA_DATA_AS_ROW, retained_variance);

  // Set the number of spectral in the original and PCA spaces.
  const cv::Size eigenvector_matrix_size = pca_.eigenvectors.size();
  num_spectral_bands_ = eigenvector_matrix_size.width;
  num_pca_bands_ = eigenvector_matrix_size.height;
}

ImageData SpectralPca::GetPcaImage(const ImageData& image_data) const {
  // true = forward projection (hyperspectral to PCA).
  return ConvertImage(
      image_data, pca_, num_spectral_bands_, num_pca_bands_, true);
}

ImageData SpectralPca::ReconstructImage(const ImageData& pca_image_data) const {
  // false = backwards projection (PCA to hyperspectral).
  return ConvertImage(
      pca_image_data, pca_, num_spectral_bands_, num_pca_bands_, false);
}

}  // namespace super_resolution