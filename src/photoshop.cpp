#include "photoshop.hpp"

#include "enforce.hpp"
#include "image.hpp"
#include "safe_op.hpp"

namespace Exiv2 {

bool Photoshop::isIrb(const byte* data) {
  if (data == nullptr) {
    return false;
  }
  return std::any_of(irbId_.begin(), irbId_.end(), [data](auto id) { return memcmp(data, id, 4) == 0; });
}

bool Photoshop::valid(const byte* pPsData, size_t sizePsData) {
  const byte* record = nullptr;
  uint32_t sizeIptc = 0;
  uint32_t sizeHdr = 0;
  const byte* pCur = pPsData;
  const byte* pEnd = pPsData + sizePsData;
  int ret = 0;
  while (pCur < pEnd && 0 == (ret = Photoshop::locateIptcIrb(pCur, (pEnd - pCur), &record, &sizeHdr, &sizeIptc))) {
    pCur = record + sizeHdr + sizeIptc + (sizeIptc & 1);
  }
  return ret >= 0;
}

// Todo: Generalised from JpegBase::locateIptcData without really understanding
//       the format (in particular the header). So it remains to be confirmed
//       if this also makes sense for psTag != Photoshop::iptc
int Photoshop::locateIrb(const byte* pPsData, size_t sizePsData, uint16_t psTag, const byte** record,
                         uint32_t* const sizeHdr, uint32_t* const sizeData) {
  if (sizePsData < 12) {
    return 3;
  }

  // Used for error checking
  size_t position = 0;
  // Data should follow Photoshop format, if not exit
  while (position <= (sizePsData - 12) && isIrb(pPsData + position)) {
    const byte* hrd = pPsData + position;
    position += 4;
    uint16_t type = getUShort(pPsData + position, bigEndian);
    position += 2;
    // Pascal string is padded to have an even size (including size byte)
    byte psSize = pPsData[position] + 1;
    psSize += (psSize & 1);
    position += psSize;
    if (position + 4 > sizePsData) {
      return -2;
    }
    uint32_t dataSize = getULong(pPsData + position, bigEndian);
    position += 4;
    if (dataSize > (sizePsData - position)) {
      return -2;
    }
    if (type == psTag) {
      *sizeData = dataSize;
      *sizeHdr = psSize + 10;
      *record = hrd;
      return 0;
    }
    // Data size is also padded to be even
    position += dataSize + (dataSize & 1);
  }
  if (position < sizePsData) {
    return -2;
  }
  return 3;
}

int Photoshop::locateIptcIrb(const byte* pPsData, size_t sizePsData, const byte** record, uint32_t* const sizeHdr,
                             uint32_t* const sizeData) {
  return locateIrb(pPsData, sizePsData, iptc_, record, sizeHdr, sizeData);
}

int Photoshop::locatePreviewIrb(const byte* pPsData, size_t sizePsData, const byte** record, uint32_t* const sizeHdr,
                                uint32_t* const sizeData) {
  return locateIrb(pPsData, sizePsData, preview_, record, sizeHdr, sizeData);
}

}  // namespace Exiv2
