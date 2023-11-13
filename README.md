# Dulang
### My very own ambition to create a compiled language starts here!

## Compile the Compiler

You will need `make`, `nasm` and `gcc` to perfom that.
- run `make` inside the Dulang main directory
- possibly, add the generated `dulang` executable to your path
- then, just run `./dulang <file>.dulan` to compile a Dulang file
- it will generate a `<file>.o`, `<file>.asm` and `<file>` executable
- then, you can simply run `./<file>` to execute the program!


## Introduction
Firstly, my intention with this language is just to have fun while learning some topics about compilers. This is, and maybe always will be, my own and very own creation.
I want to make this language a big mix of concepts i saw, and enjoyed, in other languages:

- Python: I am not with dynamically-typed languages, but i really like it's easy to read syntax.
- C: I really love C, that's why i choose it to first develop the compiler for my language, so eventually i will use some concepts from it on Dulang.
- Haskell: I'm not that Haskell expert, but it has my admiration. I would like to use the functions calls syntax from Haskell(i really hate parenthesis... oh no...) and it's type inference capacities.
- Rust: That's a language i will use as reference, it has very nice concepts, so when i have chance i will try to implement them.
- Zig: This the language i have the most curiosity about now, i think someday i will love it more than i love C. `defer`, error-unions, null and error handling, structs the way it has, good intercommunication with C, comptime and some other good stuff is certainly an inspiration!
- Go: It's simplicity is a objective, make something like the go-routines and it's ready to use channels feels like a dream.
- JS: Maybe not.

## Block Indentation

As said before, i like the syntax of python, so i want to bring it's concepts of identation to define blocks.
For example this is valid code: 
```
foo = 1 + 1 
```
This is not:
```
foo = 1 +
1 
```
But this is valid:
```
foo = 1 +
      1 
```

In Dulang, the most basic block is a line, it will be parsed individually as a single expression(so if you want to execute two different expressions they must be in different lines), but if you to continue a block you can ident some lines after it, turning all of them into a single block.
```
bar = 1 +
      1 *
      1 + 1
```

This indentation is always used with `if`, `else if`, `else`, `while` to define the code that is inside or not of any block.

- Assignment
Very simple assignment, by now:
```
bar = "Hello World"
```
At the momment we do not have a type system, so this syntax will probably change in the future.

## Conditions
Dulang has if, else if and else as any other language, and they work as you could expect.
```
$ btw, comments start with '$'
baz = 69
if baz and bnot baz| $ bnot is a bitwise inversor, baz and ~baz is 0.
    dump 1
else if baz < 69|
    $ dump is a temporarily keyword, used only for print numbers
    dump 2 
else
    $ this will output 69
    dump 69
```

## Loops
At the momment, we only have `while` loops:

```
$ loop to display from 10 to 1
foo = 10
while foo > 0|
    dump foo
    foo = foo - 1
```
Loops also suport ways to `skip` the rest of the body, going back to the beginning, and `stop` the body execution. These two are the old `continue` and `break`.
```
$ loop to display from 10 to 1
foo = 10
while foo > 0|
    foo = foo - 1
    if foo == 7|
        stop
    dump foo
    
while foo | $ another to say while different than 0
    foo = foo - 1
    if foo band 1| $ it's only true when foo is odd
        skip 
    dump foo $ printing only even numbers
```

## Syscall
This is the reserved keyword to communicate directly with the kernel syscall.
It can receive a variable number of arguments, as many the syscall needs, from 1 to 7.
They must follow this order, that is the order of the registers passed as parameters to the `syscall` on x86_64: 

```
sys rax rdi rsi rdx r10 r8 r9
```

Example of use:
```
write = 1
STDOUT = 1
hello = "Hello World"
len = 12
$ this syscall will print Hello World to the stdout
sys 
    write STDOUT hello len | 

$ the '|' will indicate the end of the parameters
```

## Roadmap
My plans:
- [X] Correct operations parsing, with precedence order
- [X] Variable assignment
- [X] If, else if and else
- [X] While loops
- [X] Interface for kernel syscalls
- [X] Function calls
- [ ] Recursive function calls
- [ ] Multiple files compiling
- [ ] Basic Type system
- [ ] Basic Macro / Comptime evaluation
- [ ] Structs / Enums / Unions
- [ ] Type Inference
- [ ] Self-host
