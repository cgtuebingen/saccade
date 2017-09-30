#ifndef HISTOGRAM_DATA_H
#define HISTOGRAM_DATA_H

#include <vector>
#include <algorithm>

namespace GUI
{
  class Histogram;
}; // namespace GUI

namespace Utils {
class ImageData;

class HistogramData {
public:
  struct range_t {
    float min;
    float max;
    float range(){ return max - min;}
  };
 private:

  GUI::Histogram *_gui;

  // range of visibility
  range_t _range;

  struct bin_info_t {
    int min;
    int max;
    int mean;
    void clear() {
      min = std::numeric_limits<int>::max();
      max = std::numeric_limits<int>::min();
    }
    void update(int v) {
      min = std::min(min, v);
      max = std::max(max, v);
    }
  } _bin_info;

  // bins of each channel
  std::vector<std::vector<double> > _data;


  int _nbins;
  int _channels;
  bool _available;

  const ImageData *_img;


 public:
  HistogramData();

  const ImageData* image() const;

  range_t* range();
  const range_t* range() const;

  const bool available() const;
  const int bins() const;
  const int channels() const;
  const int amount(int channel, int bin) const;

  const bin_info_t bin_info() const;

  void setData(const ImageData *data, float max = 1.0f);
};


}; // namespace Utils

#endif // HISTOGRAM_DATA_H