/*
 * Astra Programming Language
 * debug.h — Bytecode disassembler
 */

#ifndef astra_debug_h
#define astra_debug_h

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
int  disassembleInstruction(Chunk* chunk, int offset);

#endif /* astra_debug_h */
