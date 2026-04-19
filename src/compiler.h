/*
 * Astra Programming Language
 * compiler.h — Parser + bytecode compiler
 */

#ifndef astra_compiler_h
#define astra_compiler_h

#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source);
void         markCompilerRoots(void);

#endif /* astra_compiler_h */
