#version 410
#define M_PI 3.1415926535897932384626433832795

//uniform size;
uniform int valuesSize;

//Indexs
uniform samplerBuffer plotingValues;

out vec4 out_color;

void main(){
	//we grab the x and y and store them in an int
	int xVal = int(gl_FragCoord.x);
	int yVal = int(gl_FragCoord.y);
    
    float val = texelFetch(plotingValues, xVal).r;
    
    //val = float(xVal)/float(valuesSize);
    
    float opacity = 0;
    if(val * 100 < yVal) opacity = 1;
    
    
    
    out_color = vec4(opacity,opacity,opacity, 1.0);
    //out_color = out_color * opacity;
}
