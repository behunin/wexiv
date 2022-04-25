// SPDX-License-Identifier: GPL-2.0-or-later

// included header files
#include "rw2image.hpp"

#include "config.h"
#include "error.hpp"
#include "image.hpp"
#include "preview.hpp"
#include "rw2image_int.hpp"
#include "tiffcomposite_int.hpp"
#include "tiffimage_int.hpp"

// + standard includes
#include <array>

namespace Exiv2 {

constexpr const char* pRaw_width = "Exif.PanasonicRaw.SensorWidth";
constexpr const char* pRaw_height = "Exif.PanasonicRaw.SensorHeight";

using namespace Internal;

Rw2Image::Rw2Image(BasicIo::UniquePtr io) : Image(ImageType::rw2, mdExif | mdIptc | mdXmp, std::move(io)) {
}

std::string Rw2Image::mimeType() const {
  return "image/x-panasonic-rw2";
}

int Rw2Image::pixelWidth() const {
  if (exifData_.hasOwnProperty(pRaw_width)) {
    return exifData_[pRaw_width].as<int>();
  }
  return 0;
}

int Rw2Image::pixelHeight() const {
  if (exifData_.hasOwnProperty(pRaw_height)) {
    return exifData_[pRaw_height].as<int>();
  }
  return 0;
}

void Rw2Image::readMetadata() {
  if (io_->open() != 0) {
    throw Error(ErrorCode::kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isRw2Type(*io_, false)) {
    if (io_->error() || io_->eof())
      throw Error(ErrorCode::kerFailedToReadImageData);
    throw Error(ErrorCode::kerNotAnImage, "RW2");
  }

  ByteOrder bo = Rw2Parser::decode(exifData_, iptcData_, xmpData_, io_->mmap(), static_cast<uint32_t>(io_->size()));
  setByteOrder(bo);

  // A lot more metadata is hidden in the embedded preview image
  // Todo: This should go into the Rw2Parser, but for that it needs the Image
  PreviewManager loader(*this);
  PreviewPropertiesList list = loader.getPreviewProperties();
  // Todo: What if there are more preview images?
  if (list.size() > 1) {
    EXV_WARNING << "RW2 image contains more than one preview. None used.\n";
  }
  if (list.size() != 1)
    return;
  ExifData exifData;
  PreviewImage preview = loader.getPreviewImage(*list.begin());
  auto image = ImageFactory::open(preview.pData(), preview.size());
  if (!image) {
    EXV_WARNING << "Failed to open RW2 preview image.\n";
    return;
  }
  image->readMetadata();
  const auto prevData = image->exifData();

  // Remove tags not applicable for raw images
  static constexpr auto filteredTags = std::array{
      "Exif.Photo.ComponentsConfiguration",
      "Exif.Photo.CompressedBitsPerPixel",
      "Exif.Panasonic.ColorEffect",
      "Exif.Panasonic.Contrast",
      "Exif.Panasonic.NoiseReduction",
      "Exif.Panasonic.ColorMode",
      "Exif.Panasonic.OpticalZoomMode",
      "Exif.Panasonic.Contrast",
      "Exif.Panasonic.Saturation",
      "Exif.Panasonic.Sharpness",
      "Exif.Panasonic.FilmMode",
      "Exif.Panasonic.SceneMode",
      "Exif.Panasonic.WBRedLevel",
      "Exif.Panasonic.WBGreenLevel",
      "Exif.Panasonic.WBBlueLevel",
      "Exif.Photo.ColorSpace",
      "Exif.Photo.PixelXDimension",
      "Exif.Photo.PixelYDimension",
      "Exif.Photo.SceneType",
      "Exif.Photo.CustomRendered",
      "Exif.Photo.DigitalZoomRatio",
      "Exif.Photo.SceneCaptureType",
      "Exif.Photo.GainControl",
      "Exif.Photo.Contrast",
      "Exif.Photo.Saturation",
      "Exif.Photo.Sharpness",
      "Exif.Image.PrintImageMatching",
      "Exif.Image.YCbCrPositioning",
  };
  for (auto&& filteredTag : filteredTags) {
    auto pos = prevData.hasOwnProperty(filteredTag);
    if (prevData.hasOwnProperty(filteredTag)) {
      prevData.delete_(filteredTag);
    }
  }

}  // Rw2Image::readMetadata

ByteOrder Rw2Parser::decode(emscripten::val& exifData, emscripten::val& iptcData, emscripten::val& xmpData,
                            const byte* pData, uint32_t size) {
  Rw2Header rw2Header;
  return TiffParserWorker::decode(exifData, iptcData, xmpData, pData, size, Tag::pana, TiffMapping::findDecoder,
                                  &rw2Header);
}

Image::UniquePtr newRw2Instance(BasicIo::UniquePtr io, bool /*create*/) {
  auto image = std::make_unique<Rw2Image>(std::move(io));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isRw2Type(BasicIo& iIo, bool advance) {
  const int32_t len = 24;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  Rw2Header header;
  bool rc = header.read(buf, len);
  if (!advance || !rc) {
    iIo.seek(-len, BasicIo::cur);
  }
  return rc;
}  // Exiv2::isRw2Type

}  // namespace Exiv2
