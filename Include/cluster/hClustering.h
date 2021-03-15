#ifndef HIER_CLUSTERING
#define HIER_CLUSTERING
#include <list>
#include <vector>
#include "utils.h"

class HierarchicalClustering {
private:
    const int colNums;
    const int tThreshold;
    Matrix<matrix_real> *centers;
    int clusterNum;
    // matrix_real **dist;
    Matrix<matrix_real> dist;
    Linkage link;
    // const Matrix<matrix_real> *data;
    std::vector<std::vector<int> > clusters;
    matrix_real *minDist;
    int *minJ;
    int *index, *pre, *next, first, end;
    std::vector<real> WGSS_k;
    real BGSS, WGSS;
public:
    HierarchicalClustering(Matrix<matrix_real> &mat, int nPC, \
                            Method method = WARD_D2, int t = 40);
    void merge();
    std::vector<uint16_t> getClusters() const {
        std::vector<uint16_t> ret(colNums);
        int cnt;
        for(int i=first;i!=end;i=next[i]) {
            for(auto ele:clusters[i]) ret[ele]=cnt;
            cnt++;
        }
        return ret;
    }
    inline real getIndex() const {
        return BGSS * (colNums - clusterNum) \
                    / (WGSS * (clusterNum - 1));
    }    
    ~HierarchicalClustering();
};

#endif