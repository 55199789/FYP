#include "Enclave.h"
#include "Enclave_t.h"

#include "minHeap.h"
#include <math.h>
#include <algorithm>

#define LL(i) (i*2+1)
#define RR(i) (i*2+2)

void init(DATATYPE* heap, DATATYPE *data, int k) {
    int t;
    for(int i=0;i<k;i++) {
        heap[i] = abs(data[i]);
        t = i;
        while(t>1) {
            if(heap[t]>=heap[(t-1)/2])
                break;
            std::swap(heap[t], heap[(t-1)/2]);
            t = (t-1)/2;
        }
    }
}

void insert(DATATYPE* heap, int k, DATATYPE val) {
    // If it's smaller than the top, return
    if(heap==NULL || heap[0]>abs(val))
        return;
    heap[0] = abs(val);
    int i = 0, child, l, r;
    while(i<k) {
        l = LL(i);
        r = RR(i);
        // No left child
        if(l>=k) 
            return;
        if(r>=k) {
            child = l;
        } else {
            child = heap[l]<heap[r]?l:r;
        }
        if(heap[child]>=heap[i])
            return;
        std::swap(heap[child], heap[i]);
        i = child;
    }
}