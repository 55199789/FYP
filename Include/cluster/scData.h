#ifndef SC_DATA
#define SC_DATA

#include <vector>
#include "utils.h"


class scData{
// private:
public: // For debug convenience 
    int sampleSize, featureSize, nCluster, nPCVal;
    // Vector<matrix_real> libSize, clusters, dThreshold;
    Vector<matrix_real> clusters;
    matrix_real * libSize;
    real priorTPM, wThresholdVal, pDropoutCoefA, pDropoutCoefB;
    Matrix<matrix_real>* tags;
    Matrix<matrix_real> PC;
    std::vector<matrix_real> variation;
    std::vector<uint16_t> types;

    Matrix<bool> dropoutCandidates;
    bool *dropoutCandidates_data = NULL;
    
    Matrix<matrix_real> dissim;
    matrix_real* dissim_data;
    matrix_real* dissim_outside;
    Tag tagType;

    // for data outside enclave
    void *tags_outside, *tags_row;
    size_t tags_outside_rows, tags_outside_cols;

    void *dropoutCandidates_outside, *dropout_row;
    size_t dropoutCandidates_outside_rows, dropoutCandidates_outside_cols;

    void *enclave_cache;
    size_t enclave_cache_size = 0;
// public:

    scData(Matrix<matrix_real>* _tags, Tag t=RAW);
    ~scData(){};

    void determineDropoutCandidates(real min1 = 3, real min2 = 8,\
         int N = 2000, real alpha=0.1, bool fast = true, \
          bool zerosOnly = false, real bw_adjust = 1);
    void wThreshold(real cutoff = 0.5);

    void changeMajor();
    void changeMajor_1bit();
    void scDissim(bool correction = false, int threads = 0, \
                bool useStepFuntion = true);

    // void scDissim(const bool useStepFuntion = true);
    // void correct();
    void scPCoA();
    void nPC(int nPC_ = -1, real cutoff_divisor = 10);
    void scCluster(int n = -1, Method cMethod = WARD_D2);
    const std::vector<uint16_t> &getTypes(){return types;}
    void toFile(const char *filename) const;

    // for data outside
    scData(Matrix<matrix_real>* _tags, void *buf_outside, Tag t=RAW);
    // void determineDropoutCandidates_outside(real min1 = 3, real min2 = 8,\
    //     int N = 2000, real alpha=0.1, bool fast = true, \
    //     bool zerosOnly = false, real bw_adjust = 1);

    Matrix<matrix_real> tags_struct;
    scData(matrix_real *raw_data_outside, matrix_real *raw_data, size_t rows, size_t cols, Tag t=RAW);

};

#endif
