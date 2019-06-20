
#pragma once

void init_6502(const char* path);
int next_6502();
unsigned char* vram_6502();
bool is_vram_dirty();
void clear_vram_dirty();