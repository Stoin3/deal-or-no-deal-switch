// single translation unit for the stb libraries
#define STB_TRUETYPE_IMPLEMENTATION
#include "../include/stb_truetype.h"
#include "../include/stb_vorbis.c"
#ifndef __SWITCH__
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb_image_write.h"
#endif
