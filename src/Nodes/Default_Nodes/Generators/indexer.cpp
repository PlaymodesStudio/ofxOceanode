//
//  indexer.cpp
//  example-basic
//
//  Created by Eduard Frigola on 02/03/2020.
//
//

#include "indexer.h"

void indexer::setup() {
    color = ofColor::orange;
    
    addParameter(indexCount.set("Size", 100, 1, 99999));
    addParameter(numWaves_Param.set("NWaves", 1, 0, indexCount))
    ->registerSpeedDrag([](ofParameter<float> &p, int drag){
        float closestp = drag > 0 ? floor(p) : ceil(p);
        p = ofClamp(closestp + drag, p.getMin(), p.getMax());
    });
    addParameter(normalize_Param.set("Norm", false));
    addParameter(indexInvert_Param.set("Invert", 0, 0, 1));
    addParameter(symmetry_Param.set("Sym", 0, 0, indexCount/2));
    //TODO: Activate wraping once the algorithm is fixed in baseIndexer.cpp
    //addParameter(wrapShuffle_Param.set("Wrap", false));
    addParameter(indexShuffle_Param.set("Shuffle", 0, 0, 1));
    addParameter(indexRand_Param.set("Random", 0, 0, 1));
    addParameter(indexOffset_Param.set("Offset", 0, -indexCount/2, indexCount/2));
    addParameter(indexQuant_Param.set("Quant", indexCount, 1, indexCount));
    addParameter(combination_Param.set("Comb", 0, 0, 1));
    addParameter(modulo_Param.set("Modulo", indexCount, 1, indexCount));
    
    addInspectorParameter(discrete_Param.set("Discrete", false));
    
    if(discrete_Param){
        addOutputParameter(indexsOut.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
    }else{
        addOutputParameter(indexsOut.set("Output", {0}, {0}, {1}));
    }
    
    base.numWaves_Param = numWaves_Param;
    base.normalize_Param = normalize_Param;
    base.indexInvert_Param = indexInvert_Param;
    base.symmetry_Param = symmetry_Param;
    base.wrapShuffle_Param = false;//wrapShuffle_Param;
    base.indexShuffle_Param = indexShuffle_Param;
    base.indexRand_Param = indexRand_Param;
    base.indexOffset_Param = indexOffset_Param;
    base.indexQuant_Param = indexQuant_Param;
    base.combination_Param = combination_Param;
    base.modulo_Param = modulo_Param;
    base.discrete_Param = discrete_Param;
    
    base.recomputeIndexs();
    
    paramListeners.push(indexCount.newListener(this, &indexer::indexCountChanged));
    paramListeners.push(numWaves_Param.newListener([this](float &f){
        base.numWaves_Param = f;
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
    }));
    paramListeners.push(normalize_Param.newListener([this](bool &b){
        base.normalize_Param = b;
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
    }));
    paramListeners.push(indexInvert_Param.newListener([this](float &f){
        base.indexInvert_Param = f;
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
    }));
    paramListeners.push(symmetry_Param.newListener([this](int &i){
        base.symmetry_Param = i;
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
    }));
//    paramListeners.push(wrapShuffle_Param.newListener([this](bool &b){
//        base.wrapShuffle_Param = b;
//        base.indexShuffleChanged(indexShuffle_Param); //Shuffle depends on circularity
//        base.recomputeIndexs();
//        indexsOut = base.getIndexs();
//    }));
    paramListeners.push(indexShuffle_Param.newListener([this](float &f){
        base.indexShuffle_Param = f;
        base.indexShuffleChanged(f);
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
    }));
    paramListeners.push(indexRand_Param.newListener([this](float &f){
        base.indexRand_Param = f;
        base.indexRandChanged(f);
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
    }));
    paramListeners.push(indexOffset_Param.newListener([this](float &f){
        base.indexOffset_Param = f;
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
    }));
    paramListeners.push(indexQuant_Param.newListener([this](int &i){
        base.indexQuant_Param = i;
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
    }));
    paramListeners.push(combination_Param.newListener([this](float &f){
        base.combination_Param = f;
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
    }));
    paramListeners.push(modulo_Param.newListener([this](int &i){
        base.modulo_Param = i;
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
    }));
    paramListeners.push(discrete_Param.newListener([this](bool &b){
        base.discrete_Param = b;
        base.recomputeIndexs();
        indexsOut = base.getIndexs();
        if(b){
            indexsOut.setMin(vector<float>(1, -FLT_MAX));
            indexsOut.setMax(vector<float>(1, FLT_MAX));
        }else{
            indexsOut.setMin(vector<float>(1, 0));
            indexsOut.setMax(vector<float>(1, 1));
        }
    }));
    
    indexsOut = base.getIndexs();
}

void indexer::update(ofEventArgs &e){
    //indexsOut = indexs;
}

void indexer::presetRecallBeforeSettingParameters(ofJson &json){
    if(json.count("Size") == 1){
		//TODO: variable on ofxOceanodeParameter to allow for making parameters load order (set priority)
        indexCount = ofToInt(json["Size"]);
    }
}

void indexer::presetRecallAfterSettingParameters(ofJson &json){

}

void indexer::presetHasLoaded(){
    indexsOut = base.getIndexs();
}

void indexer::indexCountChanged(int &newIndexCount){
    base.indexCountChanged(newIndexCount);
    if(indexCount > 1){
        
        numWaves_Param.setMax(indexCount);
        //TODO: optimize
        numWaves_Param = numWaves_Param >= numWaves_Param.getMax() ? numWaves_Param.getMax() : numWaves_Param.get();
        
        symmetry_Param.setMax(indexCount/2);
        symmetry_Param = ofClamp(symmetry_Param, symmetry_Param.getMin(), symmetry_Param.getMax());
        
        indexOffset_Param.setMin(-indexCount/2);
        indexOffset_Param.setMax(indexCount/2);
        indexOffset_Param = ofClamp(indexOffset_Param, indexOffset_Param.getMin(), indexOffset_Param.getMax());
        
        float indexQuantNormalized = (float)indexQuant_Param / (float)indexQuant_Param.getMax();
        indexQuant_Param.setMax(indexCount);
        indexQuant_Param = ofClamp(indexQuantNormalized * indexCount, indexQuant_Param.getMin(), indexQuant_Param.getMax());
        getOceanodeParameter(indexQuant_Param).setDefaultValue(indexCount);
        
        float indexModuloNormalized = (float)modulo_Param / (float)modulo_Param.getMax();
        modulo_Param.setMax(indexCount);
        modulo_Param = ofClamp(indexModuloNormalized * indexCount, modulo_Param.getMin(), modulo_Param.getMax());
        getOceanodeParameter(modulo_Param).setDefaultValue(indexCount);
    }
    
    //TODO: Update default values of parameters
    indexsOut = base.getIndexs();
}
