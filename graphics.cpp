
#include <stdio.h>

#include <SDL2/SDL.h>

#include "burlington.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

//using namespace std;

int colours[] = {
    0x000000,
    0xFFFFFF,
    0x880000,
    0xAAFFEE,
    0xCC44CC,
    0x00CC55,
    0x0000AA,
    0xEEEE77,
    0xDD8855,
    0x664400,
    0xFF7777,
    0x333333,
    0x777777,
    0xAAFF66,
    0x0088FF,
    0xBBBBBB,
};

int get_colour(int code) {

    // Only 16 colours
    code &= 0x0F;
    
    return colours[code];
}

#define R(c) ((c >> 16) & 0xFF)
#define G(c) ((c >> 8) & 0xFF)
#define B(c) (c & 0xFF)

void render_vram(SDL_Renderer *ren) {

    unsigned char* vram = vram_6502();
    
    for (int j = 0; j != 32; j++) {
        for (int i = 0; i != 32; i++) {
            unsigned char val = vram[j * 32 + i];
            unsigned int colour = get_colour(val);
            
            SDL_SetRenderDrawColor(ren, R(colour), G(colour), B(colour), 0);
           // SDL_SetRenderDrawColor(ren, 255, 255, 255, 0);
            SDL_Rect rect = {i * 10, j * 10, 10, 10};
            SDL_RenderFillRect(ren,&rect);
        }
     }
    
}

char getUnicodeValue( const SDL_KeyboardEvent &key )
{
    // magic numbers courtesy of SDL docs :)
    const int INTERNATIONAL_MASK = 0xFF80, UNICODE_MASK = 0x7F;

    int uni = key.keysym.sym;

    if( uni == 0 ) // not translatable key (like up or down arrows)
    {
        // probably not useful as string input
        // we could optionally use this to get some value
        // for it: SDL_GetKeyName( key );
        return 0;
    }

    if( ( uni & INTERNATIONAL_MASK ) == 0 )
    {
        if( SDL_GetModState() & KMOD_SHIFT )
        {
            return (char)(toupper(uni & UNICODE_MASK));
        }
        else
        {
            return (char)(uni & UNICODE_MASK);
        }
    }
    else // we have a funky international character. one we can't read :(
    {
        // we could do nothing, or we can just show some sign of input, like so:
        return 0;
    }
    
    return 0;
}

static char cur_keypress = 0;

char get_key() {
    char c = cur_keypress;
    cur_keypress = 0;
    if (c != 0)
     printf("!%c", c);
    return c;
    
}

int main(int argc, char** argv) {

    if (argc != 2) {
        printf("Please specify path to ROM hexdump\n");
        exit(-1);
    }

    printf("Loading %s\n", argv[1]);
        
    init_6502(argv[1]);
    
    if (SDL_Init(SDL_INIT_VIDEO) != 0){
    //	std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("Hello World!", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320, 320, SDL_WINDOW_SHOWN);
    if (win == nullptr){
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (ren == nullptr){
        SDL_DestroyWindow(win);
        //std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
    
    for (int i = 0; i < 200; i++)  {
        SDL_RenderDrawPoint(ren, i, i); 
    }
    
    SDL_Rect rect = {100, 100, 20, 20};
    SDL_RenderFillRect(ren,&rect);
    
    SDL_RenderPresent(ren);

    SDL_Event event;
    
    int ii = 0;
    
    clock_t start = clock();
    
    while (1) {
        SDL_PollEvent(&event);

     //   printf("--%d\n", event.type);
        if (event.type == SDL_QUIT)
            break;
            
        if (event.type == SDL_KEYDOWN) {
            cur_keypress = getUnicodeValue(event.key);
            //printf("kkkkkey %d\n", cur_keypress);
        }

        if (event.type == SDL_KEYUP) {
             cur_keypress = 0;
        }
        
       //if (ii < 10)  
        if (!next_6502())
            break;
                
        if (is_vram_dirty()) {
            if (ii++ % 500 == 0) {
                fflush(stdout);
                render_vram(ren);
                SDL_RenderPresent(ren);
                clear_vram_dirty();
            }
        }

        if (ii > 0 && ii % 1000000 == 0) {
            clock_t end = clock();
            float seconds = (float)(end - start) / CLOCKS_PER_SEC;
            printf("++++++++++++++That took %fs\n", seconds);
            //exit(0);
        }
    }
    //SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}