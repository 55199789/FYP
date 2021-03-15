#include "hClustering.h"
#include <numeric>
HierarchicalClustering::HierarchicalClustering(Matrix<matrix_real> &mat, \
                                int nPC_, Method method, int t) : colNums(mat.cols()),\
                                tThreshold(t), centers(&mat), clusterNum(mat.cols()) {
    clusters.resize(clusterNum);
    for(int i=0;i<clusterNum;i++)clusters[i] = {i};
    // for(int i=0;i<clusterNum;i++)index.push_back(i);
    index = new int[clusterNum];
    pre = new int[clusterNum];
    next = new int[clusterNum];
    std::iota(index, index+clusterNum, 0);
    first = 0; end = clusterNum;
    std::iota(next, next+clusterNum, 1);
    std::iota(pre, pre+clusterNum, -1);
    const Matrix<matrix_real> &X = mat.array().square().colwise().sum().matrix().transpose() * \
                    Matrix<matrix_real>::Ones(1, clusterNum);
    dist = X + X.transpose() - (2 * mat.transpose()) * mat;
    minDist = new matrix_real[clusterNum];
    minJ = new int[clusterNum];
    // minDist[first] = 2100000000; minJ[first] = -1;
    for(int i=1;i<clusterNum;i++) {
        minDist[i] = dist(i, 0);
        minJ[i] = 0;
        for(int j=1;j<i;j++)
            if(dist(i, j)<minDist[i]) {
                minDist[i] = dist(i, j);
                minJ[i] = j;
            }        
    }
    Linkage links[] = {ward_d2, single, complete, average};
    link = links[method];
    const auto &u = mat.rowwise().mean();
    BGSS = (mat.colwise()-u).array().square().sum();
    WGSS = 0; 
    WGSS_k.resize(clusterNum, 0);
}

HierarchicalClustering::~HierarchicalClustering(){
    // data = NULL;
    // for(int i=0;i<clusterNum;i++)delete[] dist[i];
    // delete[] dist;
    delete[] index;
    delete[] pre;
    delete[] next;
    delete[] minDist;
    delete[] minJ;
    centers = 0;
}

void HierarchicalClustering::merge() {
    if(clusterNum<=1) return;
    // Find minimal distance
    // int min_i = next[first], min_j = first;
    // matrix_real min_dist = dist(min_i, min_j);
    int min_i = next[first], min_j = minJ[min_i];
    matrix_real min_dist = minDist[min_i];
    for(int i=next[min_i];i!=end;i=next[i])
        if(minDist[i]<min_dist) {
            min_dist = minDist[i];
            min_i = i;
        }
    min_j = minJ[min_i];
    // Merge min_i & min_j clusters
    // 1. Update distance matrix
    // cluster_i = clusters.begin();
    const int n = clusters[min_i].size(), m = clusters[min_j].size();
    for(int k=first;k!=end;k=next[k])
        if(k!=min_i && k!=min_j) {
            // Only update the lower part
            if (k<min_i) {
                dist(min_i, k) = link(min_dist, dist(min_i, k), \
                    k<min_j?dist(min_j, k):dist(k, min_j), \
                    n, m, clusters[k].size());
                if(dist(min_i, k) < minDist[min_i] || \
                                    minJ[min_i]==min_j) {
                    minDist[min_i] = dist(min_i, k);
                    minJ[min_i] = k;
                }
                if(minJ[k]==min_j) {
                    if(first==min_j) {
                        minDist[k] = dist(k, next[first]);
                        minJ[k] = next[first];
                    }
                    else { 
                        minDist[k] = dist(k, first);
                        minJ[k] = first;
                    }
                    for(int i=first;i!=k;i=next[i]) 
                        if(minDist[k]>dist(k, i) && i!=min_j) {
                            minDist[k] = dist(k, i);
                            minJ[k] = i;
                        }
                }
            }else {
                dist(k, min_i) = link(min_dist, dist(k, min_i), \
                    // k<min_j?dist(min_j, k):dist(k, min_j), 
                    dist(k, min_j), n, m, clusters[k].size());
                if(dist(k, min_i)<minDist[k]) {
                    minDist[k] = dist(k, min_i);
                    minJ[k] = min_i;
                } 
                if(minJ[k] == min_j || minJ[k] == min_i) {
                    minDist[k] = dist(k, min_i);
                    minJ[k] = min_i;
                    for(int i=first;i!=k;i=next[i]) 
                        if(minDist[k]>dist(k, i) && i!=min_j) {
                            minDist[k] = dist(k, i);
                            minJ[k] = i;
                        }
                }
            }
        }
    // 2. Update WGSS & BGSS
    // 2.1 BGSS
    BGSS -= real(n)*m/(n+m) * \
        (centers->col(min_i) - centers->col(min_j)).array().square().sum();
    centers->col(min_i) = (centers->col(min_i) * n + \
                centers->col(min_j) * m) / (n + m);
    // 2.2 WGSS
    real WGSS_add = 0;
    auto &ci = clusters[min_i];
    const auto &cj = clusters[min_j];
    for(int e1:ci)
        for(int e2:cj)
            WGSS_add += e1>e2?dist(e2, e1):dist(e1, e2);
    WGSS_add = (n * WGSS_k[min_i] + m * WGSS_k[min_j] + WGSS_add) \
                    / (n + m);
    WGSS += WGSS_add  - WGSS_k[min_i] - WGSS_k[min_j];
    WGSS_k[min_i] = WGSS_add;
    // 3. Merge
    ci.insert(ci.end(), \
                cj.begin(), cj.end());
    // 4. Delete j
    if(min_j==first) {
        first = next[min_j];
        pre[first] = -1;
    } else { // min_j<min_i<=last
        next[pre[min_j]] = next[min_j];
        pre[next[min_j]] = pre[min_j];
    }
    --clusterNum;
}