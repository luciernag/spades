#ifndef __HAMMER_KMER_DATA_HPP__
#define __HAMMER_KMER_DATA_HPP__

#include "mph_index/kmer_index.hpp"
#include "HSeq.hpp"

#include <vector>

#include <cstdlib>

namespace hammer {
const uint32_t K = 16;
typedef HSeq<K> HKMer;

struct KMerStat {
  size_t count;
  HKMer kmer;
  double qual;
  unsigned changeto;

  KMerStat(size_t count = 0, HKMer kmer = HKMer(), double qual = 1.0, unsigned changeto = -1)
      : count(count), kmer(kmer), qual(qual), changeto(changeto) {}
};
  
};

static inline size_t hamdistKMer(hammer::HKMer x, hammer::HKMer y,
                                 unsigned tau = -1) {
  unsigned dist = 0;

  for (size_t i = 0; i < hammer::K; ++i) {
    hammer::HomopolymerRun cx = x[i], cy = y[i];
    if (cx.raw != cy.raw) {
      dist += (cx.nucl == cy.nucl ?
               abs(cx.len - cy.len) :  cx.len + cy.len);
      if (dist > tau)
        return dist;
    }
  }

  return dist;
}

typedef KMerIndex<hammer::HKMer> HammerKMerIndex;

class KMerData {
  typedef std::vector<hammer::KMerStat> KMerDataStorageType;

 public:
  KMerData() : index_(hammer::K) {}

  size_t size() const { return data_.size(); }
  size_t capacity() const { return data_.capacity(); }
  void clear() {
    data_.clear();
    push_back_buffer_.clear();
    KMerDataStorageType().swap(data_);
    KMerDataStorageType().swap(push_back_buffer_);
  }
  size_t push_back(const hammer::KMerStat &k) {
    push_back_buffer_.push_back(k);

    return data_.size() + push_back_buffer_.size() - 1;
  }

  hammer::KMerStat& operator[](size_t idx) {
    size_t dsz = data_.size();
    return (idx < dsz ? data_[idx] : push_back_buffer_[idx - dsz]);
  }
  const hammer::KMerStat& operator[](size_t idx) const {
    size_t dsz = data_.size();
    return (idx < dsz ? data_[idx] : push_back_buffer_[idx - dsz]);
  }
  hammer::KMerStat& operator[](hammer::HKMer s) { return operator[](index_.seq_idx(s)); }
  const hammer::KMerStat& operator[](hammer::HKMer s) const { return operator[](index_.seq_idx(s)); }
  size_t seq_idx(hammer::HKMer s) const { return index_.seq_idx(s); }

  template <class Writer>
  void binary_write(Writer &os) {
    size_t sz = data_.size();
    os.write((char*)&sz, sizeof(sz));
    os.write((char*)&data_[0], sz*sizeof(data_[0]));
    index_.serialize(os);
  }

  template <class Reader>
  void binary_read(Reader &is) {
    size_t sz = 0;
    is.read((char*)&sz, sizeof(sz));
    data_.resize(sz);
    is.read((char*)&data_[0], sz*sizeof(data_[0]));
    index_.deserialize(is);
  }

 private:
  KMerDataStorageType data_;
  KMerDataStorageType push_back_buffer_;
  HammerKMerIndex index_;

  friend class KMerDataCounter;
};

class KMerDataCounter {
  unsigned num_files_;

 public:
  KMerDataCounter(unsigned num_files) : num_files_(num_files) {}

  void FillKMerData(KMerData &data);

 private:
  DECL_LOGGER("K-mer Counting");
};


#endif
