//
//  baseIndexer.cpp
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 09/01/2017.
//
//

#include "baseIndexer.h"
#include <numeric>

baseIndexer::baseIndexer(int numIndexs, string name) : ofxOceanodeNodeModel(name){
    indexCount.set("Size", numIndexs, 1, INT_MAX);
    previousIndexCount = indexCount;
    indexs.resize(indexCount, 0);
    indexRand.resize(indexCount , 0);
    iota(indexRand.begin(), indexRand.end(), 0);
    randPositions.resize(indexCount);
    randomizedIndexes.resize(indexCount);
    iota(randomizedIndexes.begin(), randomizedIndexes.end(), 0);
    indexRand_Param_previous = 0;
    
    numWaves_Param.set("NWaves", 1, 0, indexCount);
    indexInvert_Param.set("Invert", 0, 0, 1);
    symmetry_Param.set("Sym", 0, 0, indexCount/2);
    indexRand_Param.set("Random", 0, -1, 1);
    indexOffset_Param.set("Offset", 0, -indexCount/2, indexCount/2);
    indexQuant_Param.set("Quant", indexCount, 1, indexCount);
    combination_Param.set("Comb", 0, 0, 1);
    modulo_Param.set("Modulo", indexCount, 1, indexCount);
    
    recomputeIndexs();

    listeners.push(indexRand_Param.newListener(this, &baseIndexer::indexRandChanged));
    listeners.push(indexCount.newListener(this, &baseIndexer::indexCountChanged));
    listeners.push(numWaves_Param.newListener(this, &baseIndexer::parameterFloatListener));
    listeners.push(indexInvert_Param.newListener(this, &baseIndexer::parameterFloatListener));
    listeners.push(symmetry_Param.newListener(this, &baseIndexer::parameterIntListener));
    listeners.push(indexRand_Param.newListener(this, &baseIndexer::parameterFloatListener));
    listeners.push(indexOffset_Param.newListener(this, &baseIndexer::parameterFloatListener));
    listeners.push(indexQuant_Param.newListener(this, &baseIndexer::parameterIntListener));
    listeners.push(combination_Param.newListener(this, &baseIndexer::parameterFloatListener));
    listeners.push(modulo_Param.newListener(this, &baseIndexer::parameterIntListener));
}

void baseIndexer::indexCountChanged(int &indexCount){
    if(indexCount != previousIndexCount){
        indexs.resize(indexCount, 0);
        indexRand.resize(indexCount);
        iota(indexRand.begin(), indexRand.end(), 0);
        randomizedIndexes.resize(indexCount);
        randPositions.resize(indexCount);
        iota(randomizedIndexes.begin(), randomizedIndexes.end(), 0);
        float indexRand_temp_store = indexRand_Param;
        indexRand_Param = 0;
        indexRand_Param = indexRand_temp_store;

        random_shuffle(indexRand.begin(), indexRand.end());

        
        numWaves_Param.setMax(indexCount);
        numWaves_Param = ofClamp(numWaves_Param, numWaves_Param.getMin(), numWaves_Param.getMax());
        string name1 = numWaves_Param.getName();
        ofNotifyEvent(parameterChangedMinMax, name1);
        
        symmetry_Param.setMax(indexCount/2);
        symmetry_Param = ofClamp(symmetry_Param, symmetry_Param.getMin(), symmetry_Param.getMax());
        string name11 = symmetry_Param.getName();
        ofNotifyEvent(parameterChangedMinMax, name11);

        indexOffset_Param.setMin(-indexCount/2);
        indexOffset_Param.setMax(indexCount/2);
        indexOffset_Param = ofClamp(indexOffset_Param, indexOffset_Param.getMin(), indexOffset_Param.getMax());
        string name2 = indexOffset_Param.getName();
        ofNotifyEvent(parameterChangedMinMax, name2);
        
        float indexQuantNormalized = (float)indexQuant_Param / (float)indexQuant_Param.getMax();
        indexQuant_Param.setMax(indexCount);
        string name3 = indexQuant_Param.getName();
        ofNotifyEvent(parameterChangedMinMax, name3);
        indexQuant_Param = ofClamp(indexQuantNormalized * indexCount, indexQuant_Param.getMin(), indexQuant_Param.getMax());
        
        
        float indexModuloNormalized = (float)modulo_Param / (float)modulo_Param.getMax();
        modulo_Param.setMax(indexCount);
        string name4 = modulo_Param.getName();
        ofNotifyEvent(parameterChangedMinMax, name4);
        modulo_Param = ofClamp(indexModuloNormalized * indexCount, modulo_Param.getMin(), modulo_Param.getMax());
        
        recomputeIndexs();
    }
    previousIndexCount = indexCount;
}

void baseIndexer::putParametersInParametersGroup(shared_ptr<ofParameterGroup> parameters){
    parameters->add(indexCount);
    parameters->add(numWaves_Param);
    parameters->add(indexInvert_Param);
    parameters->add(symmetry_Param);
    parameters->add(indexRand_Param);
    parameters->add(indexOffset_Param);
    parameters->add(indexQuant_Param);
    parameters->add(combination_Param);
    parameters->add(modulo_Param);
}

void baseIndexer::recomputeIndexs(){
    for (int i = 0; i < indexCount ; i++){
        int index = i;
        
        //QUANTIZE
        int newNumOfPixels = indexQuant_Param;
        
        index = floor(index/((float)indexCount/(float)newNumOfPixels));
        
        
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
        index = abs(((index%2)*indexCount*combination_Param)-index);
        
        //Random
        double indexf;
        if(indexRand_Param < 0)
            indexf = randomizedIndexes[index-1] + 1;
        else if(indexRand_Param > 0)
            indexf = double(index)*(1-indexRand_Param) + (double(indexRand[index-1] + 1)*indexRand_Param);
        else{
            indexf = index;
        }
        
        //INVERSE
        double nonInvertIndex = indexf-1.0f;
        double invertedIndex = ((double)newNumOfPixels/double(symmetry_Param+1))-indexf;
        indexf = indexInvert_Param*invertedIndex + (1-indexInvert_Param)*nonInvertIndex;
        
        //Modulo
        if(modulo_Param != modulo_Param.getMax())
            indexf = fmod(indexf, modulo_Param);
        
        int shifted_i = i + round(indexOffset_Param);
        if(shifted_i < 0) shifted_i += indexCount;
        shifted_i %= indexCount;
        indexs[shifted_i] = fmod(float(((indexf)/(double)(indexCount))*((double)numWaves_Param*((double)indexCount/(double)newNumOfPixels))*((double)symmetry_Param+1)), 1);
    }
    newIndexs();
}

void baseIndexer::indexRandChanged(float &val){
    if(indexRand_Param_previous == 0){
        random_shuffle(indexRand.begin(), indexRand.end());
        iota(randomizedIndexes.begin(), randomizedIndexes.end(), 0);
    }
    indexRand_Param_previous = val;
    
    if(val < 0){
        for(int i = 0; i < indexCount; i++){
            randPositions[i] = indexRand[i]*abs(indexRand_Param) + i*(1-abs(indexRand_Param));
        }

        std::sort(
                  randomizedIndexes.begin(), randomizedIndexes.end(),
                  [&](std::size_t a, std::size_t b) { return randPositions[a] < randPositions[b]; });
    }
}
