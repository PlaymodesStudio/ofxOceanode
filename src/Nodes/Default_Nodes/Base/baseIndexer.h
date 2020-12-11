//
//  baseIndexer.h
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 09/01/2017.
//
//

#ifndef baseIndexer_h
#define baseIndexer_h

#include <vector>

class baseIndexer{
public:
    baseIndexer(int numIndexs = 100);
    ~baseIndexer(){};
    
    std::vector<float> getIndexs(){return indexs;};
    
    void recomputeIndexs();
    void indexRandChanged(float val);
    void indexShuffleChanged(float val);
    void indexCountChanged(int newIndexCount);
    
    float    numWaves_Param; //Desphase Quantity
    bool    normalize_Param;
    float    indexInvert_Param;
    int      symmetry_Param;
    float    indexRand_Param;
    bool    wrapShuffle_Param;
    float   indexShuffle_Param;
    float    indexOffset_Param;
    int      indexQuant_Param;
    float    combination_Param;
    int      modulo_Param;
    
private:
    std::vector<float>       indexs;
    int      indexCount;
    
    std::vector<int>    indexRand;
    std::vector<int>    indexShuffle;
    std::vector<int>    randomizedIndexes;
    std::vector<float>  randPositions;
    float               indexRand_Param_previous;
    float               indexShuffle_Param_previous;
    
    int                 previousIndexCount;
};

#endif /* baseIndexer_h */
