//
//  baseIndexer.cpp
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 09/01/2017.
//
//

#include "baseIndexer.h"
#include <numeric>
#include <cmath>
#include <random>
#include <algorithm>

baseIndexer::baseIndexer(int numIndexs){
    indexCount = numIndexs;
    previousIndexCount = indexCount;
    indexs.resize(indexCount, 0);
    indexRand.resize(indexCount , 0);
    iota(indexRand.begin(), indexRand.end(), 0);
    indexShuffle.resize(indexCount , 0);
    iota(indexShuffle.begin(), indexShuffle.end(), 0);
    randPositions.resize(indexCount);
    randomizedIndexes.resize(indexCount);
    iota(randomizedIndexes.begin(), randomizedIndexes.end(), 0);
    indexRand_Param_previous = 0;
    indexShuffle_Param_previous = 0;
}

void baseIndexer::indexCountChanged(int _indexCount){
    indexCount = _indexCount;
    if(indexCount != previousIndexCount){
        indexs.resize(indexCount, 0);
        indexRand.resize(indexCount);
        iota(indexRand.begin(), indexRand.end(), 0);
        indexShuffle.resize(indexCount , 0);
        iota(indexShuffle.begin(), indexShuffle.end(), 0);
        randomizedIndexes.resize(indexCount);
        randPositions.resize(indexCount);
        iota(randomizedIndexes.begin(), randomizedIndexes.end(), 0);
        float indexRand_temp_store = indexRand_Param;
        indexRand_Param = 0;
        indexRand_Param = indexRand_temp_store;

        std::shuffle(indexRand.begin(), indexRand.end(), std::mt19937(std::random_device()()));
        std::shuffle(indexShuffle.begin(), indexShuffle.end(), std::mt19937(std::random_device()()));
        
        indexShuffleChanged(indexShuffle_Param);
        indexRandChanged(indexRand_Param);
        recomputeIndexs();
    }
    previousIndexCount = indexCount;
}

void baseIndexer::recomputeIndexs(){
    std::vector<float> tempIndexs(indexCount);
    for (int i = 0; i < indexCount ; i++){
        int index = i;
        
        //QUANTIZE
		int newNumOfPixels = indexQuant_Param < 1 ? 1 : indexQuant_Param;
        
        index = (int)(index/((float)indexCount/(float)newNumOfPixels));
        
        
        while(symmetry_Param > newNumOfPixels-1)
            symmetry_Param--;
        
        bool odd = false;
        //if((abs(indexOffset_Param) - (int)abs(indexOffset_Param)) > 0.5) odd = !odd;
        
        if((int)((index)/(newNumOfPixels/(symmetry_Param+1)))%2 == 1) odd = true;
        
        
        //SYMMETRY santi
        int veusSym = newNumOfPixels/(symmetry_Param+1);
        index = veusSym-abs((((int)(index/veusSym)%2) * veusSym)-(index%veusSym));
        
        	
        if(newNumOfPixels % 2 == 0){
            index += odd ? 1 : 0;
        }
        else if(symmetry_Param > 0){
            index += indexInvert_Param > 0.5 ? 0 : 1;
            index %= newNumOfPixels;;
        }
        
        //COMB
        index = std::abs(((index%2)*indexCount*combination_Param)-index);
        
        //Shufle
        if (indexShuffle_Param > 0){
            index = randomizedIndexes[index-1] + 1;
        }
        
        //Random
        double indexf = index;
        if(indexRand_Param > 0)
            indexf = double(index)*(1-indexRand_Param) + (double(indexRand[index-1] + 1)*indexRand_Param);
        
        //INVERSE
        double nonInvertIndex = indexf-1.0f;
        double invertedIndex = ((double)newNumOfPixels/double(symmetry_Param+1))-indexf;
        indexf = indexInvert_Param*invertedIndex + (1-indexInvert_Param)*nonInvertIndex;
        
        //Modulo
        if(modulo_Param != indexCount)
            indexf = std::fmod(indexf, modulo_Param);
        
        int toDivide = normalize_Param ? indexCount - 1 : indexCount;
        if(!normalize_Param) indexf += 0.5f; //For centering non normalized
        
        if(discrete_Param){
            tempIndexs[i] = indexf;
        }
        else{
            float value = float(((indexf)/(double)(toDivide))*((double)numWaves_Param*((double)indexCount/(double)newNumOfPixels))*((double)symmetry_Param+1));
            if (value > 1) {
                int trunc = std::trunc(value);
                value -= (trunc == value) ? trunc-1 : trunc;
            }
            tempIndexs[i] = value;
        }
    }
    for (int i = 0; i < indexCount ; i++){
        float index = i - indexOffset_Param;
        int indexL = floor(index) - indexCount * floor((floor(index)) / indexCount);
        int indexH = ceil(index) - indexCount * floor((ceil(index)) / indexCount);
        float lowVal = tempIndexs[indexL];
        float highVal = tempIndexs[indexH];
        indexs[i] = lowVal + (highVal-lowVal) * (index - floor(index));
    }
}

void baseIndexer::indexRandChanged(float val){
    if(indexRand_Param_previous == 0){
        std::shuffle(indexRand.begin(), indexRand.end(), std::mt19937(std::random_device()()));
    }
    indexRand_Param_previous = val;
}

void baseIndexer::indexShuffleChanged(float val){
    if(indexShuffle_Param_previous == 0){
        std::shuffle(indexShuffle.begin(), indexShuffle.end(), std::mt19937(std::random_device()()));
        iota(randomizedIndexes.begin(), randomizedIndexes.end(), 0);
    }
    indexShuffle_Param_previous = val;
    
    if(val > 0){
        //FIXME: wrapping algorithm making index jump, can be fixed?
        if (wrapShuffle_Param) {
            for(int i = 0; i < indexCount; i++){
                if (std::abs(indexShuffle[i] - i) > ((float)indexCount/2.0f)) {
                    bool is_i_min = std::min(i, indexShuffle[i]) == i;
                    int mod_i = is_i_min ? i + (indexCount) : i;
                    int mod_indexRand = is_i_min ? indexShuffle[i] : indexShuffle[i] + (indexCount);
                    randPositions[i] = mod_indexRand*abs(val) + mod_i*(1-abs(val));
                    if (randPositions[i] > (indexCount-0.5)) randPositions[i] -= (indexCount);
                }else{
                    randPositions[i] = indexShuffle[i]*abs(val) + i*(1-abs(val));
                }
            }
        }else{
            for(int i = 0; i < indexCount; i++){
                randPositions[i] = (indexShuffle[i]*val) + (i*(1-val));
            }
        }
        
        std::sort(
                  randomizedIndexes.begin(), randomizedIndexes.end(),
                  [&](std::size_t a, std::size_t b) { return randPositions[a] < randPositions[b];});
    }
}
