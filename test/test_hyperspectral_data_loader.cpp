#include <iostream>
#include <string>

#include "hyperspectral/hyperspectral_data_loader.h"
#include "image/image_data.h"
#include "util/util.h"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using super_resolution::util::GetAbsoluteCodePath;

// Data is 128 x 128 x 5 (width, height, number of bands).
static const std::string kTestDataPath =
    GetAbsoluteCodePath("/test_data/ftir_test.txt");

// An ENVI header for testing the header reading method.
static const std::string kTestHeaderPath =
    GetAbsoluteCodePath("test_data/example_envi_header.hdr");

// The path the the HS configuration file.
static const std::string kTestConfigFilePath =
    GetAbsoluteCodePath("test_data/test_hs_config.txt");

// This test verifies that the HSIBinaryDataParameters::ReadHeaderFromFile
// method works correctly.
TEST(HyperspectralDataLoader, ReadHSIHeaderFromFile) {
  super_resolution::hyperspectral::HSIBinaryDataParameters parameters;
  parameters.ReadHeaderFromFile(kTestHeaderPath);
  EXPECT_EQ(
      parameters.interleave_format,
      super_resolution::hyperspectral::HSI_BINARY_INTERLEAVE_BSQ);
  EXPECT_EQ(
      parameters.data_type,
      super_resolution::hyperspectral::HSI_DATA_TYPE_FLOAT);
  EXPECT_EQ(parameters.big_endian, false);
  EXPECT_EQ(parameters.header_offset, 0);
  EXPECT_EQ(parameters.num_data_rows, 11620);
  EXPECT_EQ(parameters.num_data_cols, 11620);
  EXPECT_EQ(parameters.num_data_bands, 1506);
}

// Test reading in text-based hyperspectral image data.
TEST(HyperspectralDataLoader, LoadTextData) {
  super_resolution::hyperspectral::HyperspectralDataLoader hs_data_loader(
      kTestDataPath);
  hs_data_loader.LoadDataFromTextFile();
  const super_resolution::ImageData& hs_image = hs_data_loader.GetImage();

  const int num_pixels = 128 * 128;  // Test image is 128 x 128 x 5.
  EXPECT_EQ(hs_image.GetNumPixels(), num_pixels);
  EXPECT_EQ(hs_image.GetNumChannels(), 5);
}

// Test reading in binary hyperspectral image data.
TEST(HyperspectralDataLoader, LoadBinaryData) {
  super_resolution::hyperspectral::HyperspectralDataLoader hs_data_loader(
      kTestConfigFilePath);
  hs_data_loader.LoadDataFromBinaryFile();
}
