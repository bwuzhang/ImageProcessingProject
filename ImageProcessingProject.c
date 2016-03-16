/*
 Author: Kefan Chen
 Date: March 14th, 2016
 This is a project for ECE243. Digital signal processing techniques are used to perform edge detection,
 gaussian blur and sharpening on image input. 
 Output is flushed from 320x240 pixel buffer to a 640x280 VGA.
 Program can be run on Altera NIOS II.
*/
#include "stdio.h"
#define PIXEL_BUFFER 0x08000000
#define BACK_BUFFER 0x40000000
#define BUFFER_CONTROLLER 0xFF203020
#define VIDEO_IN_CONTROLER 0xFF20306C
#define WIDTH 320
#define HEIGHT 240
#define BLUR_RATIO 1
#define EDGE_THRESHOLD
#define PI 3.14159265
#define BLACK 0x0000
#define WHITE 0xFFFF
#define GRAY 0x7BEF
#define LOW_THRESHOLD 500
#define HIGH_THRESHOLD 1000

//Kernels are used to process the input signal
const short SOBEL_X_KERNEL[3][3] = {{-3,0,3},{-10,0,10},{-3,0,3}};
const short SOBEL_Y_KERNEL[3][3] = {{3,10,3},{0,0,0,},{-3,-10,-3}};
const short LAPLACIAN_KERNEL[5][5] = {{-1,-1,-1,-1,-1},{-1,-1,-1,-1,-1},{-1,-1,24,-1,-1},{-1,-1,-1,-1,-1},{-1,-1,-1,-1,-1}};
const short GAUSSIAN_BLUR_KERNEL[5][5] = {{2,4,5,4,2},{4,9,12,9,4},{5,12,15,12,5},{4,9,12,9,4},{2,4,5,4,2}};
const short SHARPEN_KERNEL[3][3] = {{0,-1,0},{-1,5,-1},{0,-1,0}};

const short STRONG_EDGE = 2;
const short WEAK_EDGE = 1;
const short NONE_EDGE = 0;
    
enum direction{
    horizontal,
    diagnal45,
    vertical,
    diagnal135
}direction;

enum bool{
	true,
	false
}bool;

typedef struct BufferController{
    unsigned bufferRegister;
    unsigned backBufferRegister;
    unsigned resolutionRegister;
    unsigned statusRegister;
}BufferController;

typedef struct VideoInController{
    unsigned rawData;
    unsigned edgeDetection;
}VideoInController;

typedef struct Gradient{
    short gradientVal;
    enum direction direction;
}Gradient;

int abs(int x){
	if(x>0)
		return x;
	else 
		return -x;
}
void read_buffer(volatile short* buffer, short pixels[HEIGHT][WIDTH]);
void write_pixel (unsigned x, unsigned y, short color, volatile short *buffer);
short read_pixel (unsigned x, unsigned y, volatile short *buffer);
void clear(volatile short *buffer);
void detect_edge(short pixels[HEIGHT][WIDTH], short processed_pixels[HEIGHT][WIDTH]);
void gaussian_blur(short pixels[HEIGHT][WIDTH], short ratio, short blurred[HEIGHT][WIDTH]);
void sharpen(short pixels[HEIGHT][WIDTH], short sharpened[HEIGHT][WIDTH]);
short convolution3x3(short matrix_a[3][3], const short matrix_b[3][3]);
short convolution5x5(short matrix_a[5][5], const short matrix_b[5][5]);
int main(){
	//This pointer points to the buffer controller 
    volatile BufferController *back_controller = (volatile BufferController *) BUFFER_CONTROLLER;
	//set the backbuffer register pointing to another memory location and leave the status register as default
	//*******back_controller->backBufferRegister = BACK_BUFFER;
	//create two pointers that point to address of pixel buffer and address of back buffer respectively
    volatile short *pixel_buffer = (short *) PIXEL_BUFFER;
	volatile short *back_buffer = (short *) BACK_BUFFER;
    VideoInController *video_in_controller = (VideoInController *) VIDEO_IN_CONTROLER;
	//clear the screen
	clear(pixel_buffer);
	//******clear(back_buffer);
	//turn off NIOS II add-in edge detector and turn on the video in stream 
	video_in_controller->edgeDetection = 0x0;
	video_in_controller->rawData = 0x4;
	video_in_controller->rawData = 0x0;
	short pixels[HEIGHT][WIDTH];
	short blurred[HEIGHT][WIDTH];
	short edge_out[HEIGHT][WIDTH];
	read_buffer(pixel_buffer, pixels);
	gaussian_blur(pixels, BLUR_RATIO, blurred);
	//detect_edge(blurred, edge_out);
	while(back_controller->statusRegister & 0x1);
	for(register unsigned row = 0; row < HEIGHT; row++){
		for(register unsigned colomn = 0; colomn < WIDTH; colomn++){
			write_pixel(colomn, row, blurred[row][colomn], pixel_buffer);
		}
	}		
	return 0;
}

//read all pixels from pixel buffer
void read_buffer(volatile short* buffer, short pixels[HEIGHT][WIDTH]){
    for(register int colomn = 0; colomn < WIDTH; colomn++){
        for(register int row = 0; row < HEIGHT; row++){
            pixels[row][colomn] = read_pixel(colomn, row, buffer);
        }
    }
}

//change color of one pixel
void write_pixel (unsigned x, unsigned y, short color, volatile short *buffer){
    volatile short *vga_pixel =(volatile short *) (buffer + (y<<9) + x);
    *vga_pixel = color;
}

//read color from one pixel
short read_pixel (unsigned x, unsigned y, volatile short *buffer){
    volatile short *vga_pixel =(volatile short *) (buffer + (y<<9) + x);
    short pixel_color = *vga_pixel;
    return pixel_color;
}

//clear all pixels
void clear(volatile short *buffer){
    for(register int x = 0; x < WIDTH; x++){
        for(register int y =0; y < HEIGHT; y++){
            write_pixel(x, y, WHITE, buffer);
        }
    }
}

//run canny edge detection algorithm
void detect_edge(short pixels[HEIGHT][WIDTH], short processed_pixels[HEIGHT][WIDTH]){
    Gradient pixel_gradent[HEIGHT][WIDTH];
    //calculate the gradient
    for(register int colomn = 0; colomn <= WIDTH - 3; colomn++){
        for(register int row = 0; row <= HEIGHT - 3; row++){
            short submatrix[3][3];
            int theta;
            short gx = 0, gy = 0;
            for(register int i = 0; i < 3; i++){
                for(register int j = 0; j < 3; j++){
                    submatrix[i][j] = pixels[row + i][colomn + j];
                }
            }
            gx = convolution3x3(submatrix, SOBEL_X_KERNEL);
            gy = convolution3x3(submatrix, SOBEL_Y_KERNEL);
            pixel_gradent[row+1][colomn+1].gradientVal = abs(gy) + abs(gx);
            //calculate the direction of gradient
            if(gx == 0)
                theta = 90;
            else
                theta = (int)atan(gy/gx)*180/PI;
            //discretize the dircection of gradient
            if((theta >= 0 && theta <= 22.5) || (theta > 157.5 && theta <= 180))
                theta = 0;
            else if(theta > 23 && theta <=68)
                theta = 45;
            else if(theta > 68 && theta <=113)
                theta = 90;
            else if(theta > 113 && theta <=158)
                theta = 135;
            
            switch (theta) {
                case 0:
                    pixel_gradent[row+1][colomn+1].direction = horizontal;
                    break;
                case 45:
                    pixel_gradent[row+1][colomn+1].direction = diagnal45;
                    break;
                case 90:
                    pixel_gradent[row+1][colomn+1].direction = vertical;
                    break;
                case 135:
                    pixel_gradent[row+1][colomn+1].direction = diagnal135;
                    break;
                default:
                    break;
            }
        }
    }
    //nonmaximum suppression
    for(register int colomn = 0; colomn <= WIDTH - 3; colomn++){
        for(register int row = 0; row <= HEIGHT - 3; row++){
            enum direction theta = pixel_gradent[row+1][colomn+1].direction;
            short value = pixel_gradent[row+1][colomn+1].gradientVal;
            switch (direction) {
                case horizontal:
                    if(!(pixel_gradent[row+1][colomn+1].gradientVal > pixel_gradent[row+1][colomn+2].gradientVal
                       && pixel_gradent[row+1][colomn+1].gradientVal > pixel_gradent[row+1][colomn].gradientVal))
                        processed_pixels[row+1][colomn+1] = 0;
                    else
                        processed_pixels[row+1][colomn+1] = value;
                    break;
                case diagnal45:
                    if(!(pixel_gradent[row+1][colomn+1].gradientVal > pixel_gradent[row][colomn+2].gradientVal
                         && pixel_gradent[row+1][colomn+1].gradientVal > pixel_gradent[row+2][colomn].gradientVal))
                        processed_pixels[row+1][colomn+1] = 0;
                    else
                        processed_pixels[row+1][colomn+1] = value;
                case vertical:
                    if(!(pixel_gradent[row+1][colomn+1].gradientVal > pixel_gradent[row][colomn+1].gradientVal
                         && pixel_gradent[row+1][colomn+1].gradientVal > pixel_gradent[row+2][colomn+1].gradientVal))
                        processed_pixels[row+1][colomn+1] = 0;
                    else
                        processed_pixels[row+1][colomn+1] = value;
                case diagnal135:
                    if(!(pixel_gradent[row+1][colomn+1].gradientVal > pixel_gradent[row][colomn].gradientVal
                         && pixel_gradent[row+1][colomn+1].gradientVal > pixel_gradent[row+2][colomn+2].gradientVal))
                        processed_pixels[row+1][colomn+1] = 0;
                    else
                        processed_pixels[row+1][colomn+1] = value;
                default:
                    break;
            }
        }
    }
    //double threshold
    for(register int colomn = 0; colomn <= WIDTH - 3; colomn++){
        for(register int row = 0; row <= HEIGHT - 3; row++){
            short value = processed_pixels[row+1][colomn+1];
			//printf("%hd ",value);
            if(value < LOW_THRESHOLD)
                processed_pixels[row+1][colomn+1] = NONE_EDGE;
            else if(value < HIGH_THRESHOLD)
                processed_pixels[row+1][colomn+1] = WEAK_EDGE;
            else
                processed_pixels[row+1][colomn+1] = STRONG_EDGE;
        }
		//printf("\n");
    }
    //edge tracking
	short temp[HEIGHT][WIDTH];
    for(register int colomn = 0; colomn <= WIDTH - 3; colomn++){
        for(register int row = 0; row <= HEIGHT - 3; row++){
			short value = processed_pixels[row+1][colomn+1];
			if(value == NONE_EDGE)
				temp[row+1][colomn+1] = WHITE;
			else if(value == STRONG_EDGE)
				temp[row+1][colomn+1] = BLACK;
			else if(value == WEAK_EDGE){
				enum bool is_connected_to_strong_edge = false;
				if(processed_pixels[row][colomn] == STRONG_EDGE || processed_pixels[row][colomn+1] == STRONG_EDGE 
				|| processed_pixels[row][colomn+2] == STRONG_EDGE || processed_pixels[row+1][colomn] == STRONG_EDGE 
				|| processed_pixels[row+1][colomn+2] == STRONG_EDGE || processed_pixels[row+2][colomn] == STRONG_EDGE 
				|| processed_pixels[row+2][colomn+1] == STRONG_EDGE || processed_pixels[row+2][colomn+2] == STRONG_EDGE)
					is_connected_to_strong_edge = true;
				else 
					is_connected_to_strong_edge = false;
				
				if(is_connected_to_strong_edge)
					temp[row+1][colomn+1] = BLACK;
				else
					temp[row+1][colomn+1] = WHITE;
			}
				
        }
    }
	
	for(register int colomn = 0; colomn <= WIDTH - 3; colomn++){
        for(register int row = 0; row <= HEIGHT - 3; row++){
			processed_pixels[row+1][colomn+1] = temp[row+1][colomn+1];
		}
	}
}

//run gaussian blur algorithm
void gaussian_blur(short pixels[HEIGHT][WIDTH], short ratio, short blurred[HEIGHT][WIDTH]){
    for(register int colomn = 0; colomn <= WIDTH - 5; colomn++){
        for(register int row = 0; row <= HEIGHT - 5; row++){
            short submatrix[5][5] ;
            for(register int i = 0; i < 5; i++){
                for(register int j = 0; j < 5; j++){
                    submatrix[i][j] = pixels[row + i][colomn + j];
                }
            }
            blurred[row+2][colomn+2] = convolution5x5(submatrix, GAUSSIAN_BLUR_KERNEL)/ratio;
        }
    }
}

//run sharpen algorithm
void sharpen(short pixels[HEIGHT][WIDTH], short sharpened[HEIGHT][WIDTH]){
    for(register int colomn = 0; colomn <= WIDTH - 3; colomn++){
        for(register int row = 0; row <= HEIGHT - 3; row++){
            short submatrix[3][3];
            for(register int i = 0; i < 3; i++){
                for(register int j = 0; j < 3; j++){
                    submatrix[i][j] = pixels[row + i][colomn + j];
                }
            }
            sharpened[row+1][colomn+1] = convolution3x3(submatrix, SHARPEN_KERNEL);
        }
    }
}

//compute the convolution of two matrices
short convolution3x3(short matrix_a[3][3], const short matrix_b[3][3]){
    short convolution = 0;
    for(register int x = 0; x < 2; x++){
        for(register int y = 0; y < 3; y++){
			if(!(x == 1 && y == 2))
				convolution += matrix_a[x][y]*matrix_b[3-x-1][3-y-1];
        }
    }
    return convolution;
}

short convolution5x5(short matrix_a[5][5], const short matrix_b[5][5]){
    short convolution = 0;
    for(register int x = 0; x < 3; x++){
        for(register int y = 0; y < 5; y++){
			if(!(x == 2 && y == 3))
				convolution += matrix_a[x][y]*matrix_b[5-x-1][5-y-1];
        }
    }
    return convolution;
}