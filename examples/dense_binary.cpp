/*
    Copyright (c) 2011, Philipp Krähenbühl
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Neither the name of the Stanford University nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY Philipp Krähenbühl ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Philipp Krähenbühl BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "densecrf.h"
#include <cstdio>
#include <cmath>
#include "util.h"
#include <cassert>
#include <iostream>
#include <stdlib.h>
#include <string>

using namespace std;

// Store the colors we read, so that we can write them again.
/*int nColors = 0;
int colors[255];
unsigned int getColor( const unsigned char * c ){
	return c[0] + 256*c[1] + 256*256*c[2];
}
void putColor( unsigned char * c, unsigned int cc ){
	c[0] = cc&0xff; c[1] = (cc>>8)&0xff; c[2] = (cc>>16)&0xff;
}*/
// Produce a color image from a bunch of labels
unsigned char * colorize( const short * map, int W, int H ){
	unsigned char * r = new unsigned char[ W*H*3 ];
	for( int k=0; k<W*H; k++ ){
        unsigned char *this_r = r+3*k;
		if(map[k] == 0)
        {
            this_r[0] = this_r[1] = this_r[2] = 0;
        }
        else
        {
            this_r[0] = this_r[1] = this_r[2] = 255;
        }
	}
	return r;
}

unsigned char* colorize_error( const short* map, const unsigned char* gt, int W, int H)
{
    int sum_r = 0, sum_g=0;
    unsigned char * r = new unsigned char[ W*H*3 ];
    for( int k=0; k<W*H; k++ ){
        unsigned char *this_r = r+3*k;
        const unsigned char* this_gt = gt+3*k;
        if( map[k]*255 == this_gt[0])
        {
            this_r[0] = this_r[1] = this_r[2] = 0;//(unsigned char)map[k]*255;
        }
        else
        {
            if(this_gt[0] == 255)
            {
                //this_r[1] = this_r[0] = 0;
                this_r[1] = 255;
                sum_g++;
            }
            else
            {
                //this_r[2] = this_r[1] = 0;
                this_r[0] = 255;
                sum_r++;
            }

        }
    }
    int total = sum_r+sum_g;
    cout<<"Total errors: "<<total<<endl;
    cout<<"\tred points:   "<<sum_r<<", "<<((double)sum_r)/double(total)<<endl;
    cout<<"\tgreen points: "<<sum_g<<", "<<((double)sum_g)/double(total)<<endl;

    return r;

}

double rand_gauss (void) {
    double v1,v2,s;

    do {
        v1 = 2.0 * ((double) rand()/RAND_MAX) - 1;
        v2 = 2.0 * ((double) rand()/RAND_MAX) - 1;

        s = v1*v1 + v2*v2;
    } while ( s >= 1.0 );

    if (s == 0.0)
        return 0.0;
    else
        return (v1*sqrt(-2.0 * log(s) / s));
}

unsigned char* addnoise( const unsigned char* in, int W, int H, double sigma)
{
    unsigned char * r = new unsigned char[ W*H*3 ];
	for( int k=0; k<W*H; k++ ){
        unsigned char *r_c = r+3*k;
        const unsigned char *in_c = in+3*k;
        int add_me = (int)(rand_gauss() * sigma);
        int color = in_c[0];
        //int v =  rand()%25;
        //if( v == 0)
        if(color)
        {
            color = 191 + add_me;
        }
        else
        {
            color = 64 + add_me;
        }
        if( color > 255) color = 128;
        if( color <   0) color = 127;
        r_c[0] = color;
        r_c[1] = color;
        r_c[2] = color;
	}
	return r;

}



float * classify( const unsigned char * im, int W, int H, int M ){
    assert(M == 2);
	float * res = new float[W*H*M];
	for( int k=0; k<W*H; k++ ){
		// Map the color to a label
        const unsigned char *this_c = im + 3*k;
        int c = (this_c[0]+this_c[1]+this_c[2])/3;
		
		// Set the energy
		float * r = res + k*M;

        for(int j=0; j < M; j++)
            r[j] = (255-c)*j + c*(1-j);
	}
	return res;
}


int main( int argc, char* argv[]){
	if (argc<3){
		printf("Usage: %s image output\n\t%s image output noised-input\n", argv[0], argv[0] );
		return 1;
	}
	// Number of labels
	const int M = 2;
	// Load the color image and some crude annotations (which are used in a simple classifier)
	int W, H, GW, GH;
	unsigned char * anno = readPPM( argv[1], W, H );
	if (!anno){
		printf("Failed to load image!\n");
		return 1;
	}
    unsigned char *im;
    if(argc == 3)
    {
        im = addnoise(anno, W, H, 25.0);//readPPM( argv[2], GW, GH );
        if (!im){
            printf("Failed to add noise!\n");
            return 1;
        }
    }
    else if(argc == 4)
    {
        im = readPPM( argv[3], GW, GH);
        if(!im || (GW != W) || (GH != H ) )
        {
            printf("Error reading file!\n");
            return 1;
        }
    }
	
	/////////// Put your own unary classifier here! ///////////
	float * unary = classify( im, W, H, M );
	///////////////////////////////////////////////////////////
	
	// Setup the CRF model
	DenseCRF2D crf(W, H, M);
	// Specify the unary potential as an array of size W*H*(#classes)
	// packing order: x0y0l0 x0y0l1 x0y0l2 .. x1y0l0 x1y0l1 ...
	crf.setUnaryEnergy( unary );
	// add a color independent term (feature = pixel location 0..W-1, 0..H-1)
	// x_stddev = 3
	// y_stddev = 3
	// weight = 3
	crf.addPairwiseGaussian( 7, 7, 300 );
	// add a color dependent term (feature = xyrgb)
	// x_stddev = 60
	// y_stddev = 60
	// r_stddev = g_stddev = b_stddev = 20
	// weight = 10
	crf.addPairwiseGlobalColor(3, 3, 3, im, 10 );
	
	// Do map inference
	short * map = new short[W*H];
	crf.map(10, map);
	
	// Store the result
	unsigned char *res = colorize( map, W, H );
    unsigned char *err = colorize_error( map, anno, W, H);

	writePPM( argv[2], W, H, res );
    string noise_str("noised_");
    noise_str += string(argv[2]);
    string err_str("err_");
    err_str += string(argv[2]);
    writePPM(noise_str.c_str(), W, H, im);
    writePPM(err_str.c_str(), W, H, err);
	
	delete[] im;
	delete[] anno;
	delete[] res;
	delete[] map;
	delete[] unary;
    delete[] err;
}
