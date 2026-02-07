# Nexus Programming Language

## Description :

Nexus is designed to be a mix bitween JAVA/C# and rust.

It combines java/c# object oriented approach and removes the garbage collector.
It replace the garbage collector with a ownership system similar to rust.

The project will be code in C++ Most likely using LLVM for the compiler.

## Examples : 
Examples will be found in the examples folder

## Conventions :
- Methods and classes should be named with PascalCase.
- Attributes and variables should be named with lowerCamalCase
- Constrants FULL_CAP_SNAKE_CASE
- Globals should have g_ or G_
- Code should be indented in allman

(these rules are optionnal but suggested)

## LSP :
Will be found in lsp folder


# Compiler

I initially had a compiler V1 which was a basic system to understand c++
after rereading the code I decided to redo the compiler from scratch
You can find both compilers :
- NexusCompilerV1
- NexusCompilerV2

