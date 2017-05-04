/*

  yay - fast and simple yuv viewer

  (c) 2005-2010 by Matthias Wientapper
  (m-dot-wientapper-at-gmx-dot-de)

  Support of multiple formats added by Cuero Bugot.
 
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.
 
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "regex.h"

//#include "SDL.h"
#include <SDL/SDL.h>

SDL_Surface     *screen;
SDL_Event       event;
SDL_Rect        video_rect;
SDL_Overlay     *my_overlay;
const SDL_VideoInfo* info = NULL;

Uint32 width = 0;
Uint32 height = 0;
Uint32 win_width = 0;
Uint32 win_height = 0;
char *vfilename; 
FILE *fpointer;
Uint8 *y_data, *cr_data, *cb_data, *tmp_data;
Uint16 zoom = 1;
Uint16 min_zoom = 1;
Uint16 frame = 0;
Uint16 quit = 0;
Uint8 grid = 0;
Uint8 bpp = 0;
int cfidc = 1;
int sp = 0;


static const Uint8 SubWidthC[4] =
  {
    0, 2, 2, 1
  };
static const Uint8 SubHeightC[4] =
  {
    0, 2, 1, 1
  };
static const Uint8 SubSizeC[4] =
  {
    0, 4, 2, 1
  };
static const Uint8 MbWidthC[4] =
  {
    0, 8, 8, 16
  };
static const Uint8 MbHeightC[4] =
  {
    0, 8, 16, 16
  };
static const Uint8 FrameSize2C[4] =
  {
    2, 3, 4, 6
  };


int load_frame(){
  Uint32 cnt;  
  /* Fill in video data */
  cnt = fread(y_data, 1, width*height, fpointer);
  //fprintf(stderr,"read %d y bytes\n", cnt);
  if(cnt < width*height){
    return 0;
  }
  else if (cfidc>0)
    {
      if(!sp) {
        cnt = fread(cb_data, 1, height * width / SubSizeC[cfidc], fpointer);
          // fprintf(stderr,"read %d cb bytes\n", cnt);
        if(cnt < width * height / 4){
        	return 0;
        } else {
            cnt = fread(cr_data, 1, height * width / SubSizeC[cfidc], fpointer);
            // fprintf(stderr,"read %d cr bytes\n", cnt);
            if(cnt < width * height / 4){
                return 0;
            }
        }
      } else {
        int sz = height * width / SubSizeC[cfidc];
        int tmp_sz = sz * 2;
        int i;
        cnt = fread(tmp_data, 1, tmp_sz, fpointer);
        if(cnt < tmp_sz) {
            return 0;
        }

        for(i = 0; i < sz; i++) {
            cb_data[i] = tmp_data[i * 2];
            cr_data[i] = tmp_data[i * 2 + 1];
        }
      }
    }
  return 1;
}

void convert_chroma_to_420()
{
  int i, j;
  Uint16 xoff = (width - win_width) / 4;
  Uint16 yoff = (height - win_height) / 4;

  //printf("%dx%d\n",width, height);
  if (cfidc > 0) {
    for (j = 0; j < win_height / 2; j++)
      for (i = 0; i < win_width / 2; i++) {
        my_overlay->pixels[1][j * my_overlay->pitches[1] + i] =
            cr_data[(i + xoff) * MbWidthC[cfidc] / 8
            + (j + yoff) * (width / SubWidthC[cfidc]) * MbHeightC[cfidc] / 8];
        my_overlay->pixels[2][j * my_overlay->pitches[2] + i] =
            cb_data[(i + xoff) * MbWidthC[cfidc] / 8
            + (j + yoff) * (width / SubWidthC[cfidc]) * MbHeightC[cfidc] / 8];
      }
  } else {
    for (i = 0; i < height / 2; i++) {
      memset(my_overlay->pixels[1] + i * my_overlay->pitches[1], 128, width / 2);
      memset(my_overlay->pixels[2] + i * my_overlay->pitches[2], 128, width / 2);
    }
  }
}

void draw_frame(){ 
  Sint16 x, y;
  Uint16 i;
  Uint16 xoff = (width - win_width) / 2;
  Uint16 yoff = (height - win_height) / 2;
  Uint8 *y_ptr = y_data, *cr_ptr = cr_data, *cb_ptr = cb_data;

  y_ptr += yoff * width;
  cr_ptr += yoff / 2 * width / 2;
  cb_ptr += yoff / 2 * width / 2;
  
  /* Fill in pixel data - the pitches array contains the length of a line in each plane*/
  SDL_LockYUVOverlay(my_overlay);
  
  for (i = 0; i < win_height; i++) {
    memcpy(my_overlay->pixels[0]+i*my_overlay->pitches[0], y_ptr + xoff, win_width);
    y_ptr += width;
  }

  if (cfidc == 1) {

    for (i = 0; i < win_height / 2; i++) {
      memcpy(my_overlay->pixels[1] + i * my_overlay->pitches[1],
          cr_ptr + xoff / 2, win_width / 2);
      cr_ptr += width / 2;
    }

    for (i = 0; i < win_height / 2; i++) {
      memcpy(my_overlay->pixels[2] + i * my_overlay->pitches[2],
          cb_ptr + xoff / 2, win_width / 2);
      cb_ptr += width / 2;
    }
  }
  else
  {
    convert_chroma_to_420();
  }

  if(grid){
   
    // horizontal grid lines
    for(y=0; y<win_height; y=y+16){
      for(x=0; x<win_width; x+=8){
	*(my_overlay->pixels[0] + y   * my_overlay->pitches[0] + x  ) = 0xF0;
	*(my_overlay->pixels[0] + y   * my_overlay->pitches[0] + x+4  ) = 0x20;
      }
    }
    // vertical grid lines
    for(x=0; x<win_width; x=x+16){
      for(y=0; y<win_height; y+=8){
	*(my_overlay->pixels[0] + y   * my_overlay->pitches[0] + x  ) = 0xF0;
	*(my_overlay->pixels[0] + (y+4)   * my_overlay->pitches[0] + x  ) = 0x20;
      }
    }
  }

  SDL_UnlockYUVOverlay(my_overlay);

  video_rect.x = 0;
  video_rect.y = 0;
  video_rect.w = win_width*zoom;
  video_rect.h = win_height*zoom;

  SDL_DisplayYUVOverlay(my_overlay, &video_rect);
}

void print_usage(){
  fprintf(stdout, "Usage: yay [-s <width>x<height>] [-w <width>x<height>] \n\t   [-f format] [-p] filename.yuv\n"
      "\t specify '-s' if the geometry information in the filename\n"
      "\t specify '-w' for a window size smaller than frame geometry\n"
      "\t format can be: 0-Y only, 1-YUV420, 2-YUV422, 3-YUV444\n"
      "\t specify '-p' to enable semi-planar mode\n");
}


int main(int argc, char *argv[])
{
  int     opt;
  char    caption[32];
  regex_t reg;
  regmatch_t pm;
  int     result;
  char    picsize[32]="";
  int     used_s_opt = 0;
  int     play_yuv = 0;
  unsigned int start_ticks = 0;
  Uint32 vflags;

  if (argc == 1) {
    print_usage();
    return 1;
  } else {
    while ((opt = getopt(argc, argv, "w:f:s:p")) != -1)
      switch (opt) {
      case 's':
        if (sscanf(optarg, "%dx%d", &width, &height) != 2) {
          fprintf(stdout,
              "No geometry information provided by -s parameter.\n");
          return 1;
        }
        used_s_opt = 1;
        break;
      case 'f':
        if (sscanf(optarg, "%d", &cfidc) != 1 || (cfidc < 0 && cfidc > 3)) {
          fprintf(stdout, "Invalid format provided by -f parameter.\n");
          return 1;
        }
        break;
      case 'p':
        // Enable semi-planar mode
        sp = 1;
        break;
      case 'w':
        if (sscanf(optarg, "%dx%d", &win_width, &win_height) != 2) {
          fprintf(stdout, "misformed window size information provided by -w parameter.\n");
          return 1;
        }
        break;
      default:
        print_usage();
        return 1;
        break;
      }
  }
  argv += optind;
  argc -= optind;
  
  vfilename = argv[0];
  
  if(!used_s_opt) {
    // try to find picture size from filename or path
    if (regcomp(&reg, "_[0-9]+x[0-9]+", REG_EXTENDED) != 0) return -1;
    result = regexec(&reg, vfilename, 1, &pm, REG_NOTBOL);
    if(result == 0){
      strncpy(picsize, (vfilename + pm.rm_so + 1), (pm.rm_eo - pm.rm_so -1 ));
      strcat(picsize, "\0");
    }
    if (sscanf(picsize, "%dx%d", &width, &height) != 2) {
      fprintf(stdout, "No geometry information found in path/filename.\nPlease use -s <width>x<height> paramter.\n");
      return 1;
    }
  }
  // some WM can't handle small windows...
  if (width < 100){
    zoom = 2;
    min_zoom = 2;
  }
  //printf("using x=%d y=%d\n", width, height);

  if (win_width > width) win_width = width;
  if (win_height > height) win_height = height;

  if (win_width == 0) win_width = width;
  if (win_height == 0) win_height = height;

  // SDL init
  if(SDL_Init(SDL_INIT_VIDEO) < 0){ 
    fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
    exit(1);
  }
  atexit(SDL_Quit); 
  
  info = SDL_GetVideoInfo();
  if( !info ) 
    {      fprintf(stderr, "SDL ERROR Video query failed: %s\n", SDL_GetError() );
      SDL_Quit(); exit(0); 
    }
  
  bpp = info->vfmt->BitsPerPixel;
   if(info->hw_available)
   	vflags = SDL_HWSURFACE;
   	else	
   	vflags = SDL_SWSURFACE;

  if( (screen = SDL_SetVideoMode(win_width*zoom, win_height*zoom, bpp,
				 vflags)) == 0 ) 
    {       
      fprintf(stderr, "SDL ERROR Video mode set failed: %s\n", SDL_GetError() );
      SDL_Quit(); exit(0); 
    }
  
  
  // DEBUG output
  // printf("SDL Video mode set successfully. \nbbp: %d\nHW: %d\nWM: %d\n", 
  // 	info->vfmt->BitsPerPixel, info->hw_available, info->wm_available);
  

  SDL_EnableKeyRepeat(500, 10);

  my_overlay = SDL_CreateYUVOverlay(win_width, win_height, SDL_YV12_OVERLAY, screen);
  if(!my_overlay){ //Couldn't create overlay?
    fprintf(stderr, "Couldn't create overlay\n"); //Output to stderr and quit
    exit(1);
  }

  /* should allocate memory for y_data, cr_data, cb_data here */
  y_data  = malloc(width * height * sizeof(Uint8));
  if (cfidc > 0)
    {
      cb_data = malloc(width * height * sizeof(Uint8) / SubSizeC[cfidc]);
      cr_data = malloc(width * height * sizeof(Uint8) / SubSizeC[cfidc]);
      if (sp)
        tmp_data = malloc(width * height * sizeof(Uint8) / SubSizeC[cfidc] * 2);
    }

  fpointer = fopen(vfilename, "rb"); 
  if (fpointer == NULL){
    fprintf(stderr, "Error opening %s\n", vfilename);
    return 1;
  }
  // send event to display first frame
  event.type = SDL_KEYDOWN;
  event.key.keysym.sym = SDLK_RIGHT;
  SDL_PushEvent(&event);

  // main loop
  while (!quit){
    sprintf(caption, "frame %d, zoom=%d", frame, zoom);
    SDL_WM_SetCaption( caption, NULL );

    // wait for SDL event
    SDL_WaitEvent(&event);

    switch(event.type)
      {
      case SDL_KEYDOWN: 
	switch(event.key.keysym.sym)
	  {
	  case SDLK_SPACE:
	    {
	      play_yuv = 1; // play it, sam!
	      while(play_yuv){
		start_ticks = SDL_GetTicks();
		sprintf(caption, "frame %d, zoom=%d", frame, zoom);
		SDL_WM_SetCaption( caption, NULL );
		
		// check for next frame existing
		if(load_frame()){
		  draw_frame();
		  //insert delay for real time viewing
		  if(SDL_GetTicks() - start_ticks < 40)
		    SDL_Delay(40 - (SDL_GetTicks() - start_ticks));
		  frame++;
		} else {
		  play_yuv = 0;
		}
		// check for any key event
		if(SDL_PollEvent(&event)){
		  if(event.type == SDL_KEYDOWN){
		    // stop playing
		    play_yuv = 0;
		  }
		}
	      }
	      break;
	    }
	  case SDLK_RIGHT: 
	    {
	      // check for next frame existing
	      if(load_frame()){
		draw_frame();
		frame++;
	      }

	      break;
	    } 
	  case SDLK_BACKSPACE:
	  case SDLK_LEFT:
	    { 
	      if(frame>1){
		frame--;
		fseek(fpointer, ((frame-1)*height*width*FrameSize2C[cfidc])/2 , SEEK_SET);
		//if(draw_frame())
		load_frame();
		draw_frame();
	      }
	      break;
	    }
	  case SDLK_UP:
	    {
	      zoom++;
	      screen = SDL_SetVideoMode(win_width*zoom, win_height*zoom, bpp,
					vflags); 
	      video_rect.w = win_width*zoom;
	      video_rect.h = win_height*zoom;
	      SDL_DisplayYUVOverlay(my_overlay, &video_rect);
	      break;
	    }
	  case SDLK_DOWN:
	    {
	      if(zoom>min_zoom){
		zoom--;
		screen = SDL_SetVideoMode(win_width*zoom, win_height*zoom, bpp,
					  vflags); 
		video_rect.w = win_width*zoom;
		video_rect.h = win_height*zoom;
		SDL_DisplayYUVOverlay(my_overlay, &video_rect);
	      }
	      break;
	    }
	  case SDLK_r:
	    { 
	      if(frame>1){
		frame=1;
		fseek(fpointer, 0, SEEK_SET);
		//if(draw_frame())
		load_frame();
		draw_frame();
	      }
	      break;
	    }
	  case SDLK_g:
	    grid = ~grid;
	    draw_frame();
	    break;
	  case SDLK_q:
	    quit = 1;
	    break;
	  case SDLK_f:
	    SDL_WM_ToggleFullScreen(screen);
	    break;
	  default:
	    break;
	  } // switch key
	break;
      case SDL_QUIT:
	quit = 1;
	break;
      case SDL_VIDEOEXPOSE:
	SDL_DisplayYUVOverlay(my_overlay, &video_rect);
	break;
      default:
	break;

      } // switch event type
	
  } // while
  // clean up
  SDL_FreeYUVOverlay(my_overlay); 
  free(y_data);
  free(cb_data);
  free(cr_data);
  if (sp)
    free(tmp_data);
  fclose(fpointer);
  if (!used_s_opt)
    regfree(&reg);

  return 0;
}



