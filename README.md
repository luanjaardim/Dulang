# Dulang
### My very own ambition to create a compiled language starts here!

## Compile the Compiler

You will need `make` and `gcc` to perfom that.
- run `make` inside the Dulang main directory
- possibly, add the generated `dulang` executable to your path
- then, just run `./dulang <file>.dulan` to compile a Dulang file
- it will generate a `<file>.c` and `<file>` executable
- then, you can simply run `./<file>` to execute the program!


## Introduction

Firstly, my intention with this language is just to have fun while learning some topics about compilers. This is, and maybe always will be, my own and very own creation.
I want to make this language a big mix of concepts i saw, and enjoyed, in other languages:

- Python: I am not with dynamically-typed languages, but i really like it's easy to read syntax.
- C: I really love C, that's why i choose it to first develop the compiler for my language, so eventually i will use some concepts from it on Dulang, and try to make Dulang interact deeply with C code.
- Haskell: I'm not that Haskell expert, but it has my admiration. I would like to have partial function applications, from functional languages like it, and it's type inference capacities.
- OCaml: OCaml and his features based on functional programming and imperative programming have my attention, it's the kind of language i would like to have, mixing the best of both worlds.
- Rust: That's a language i will use as reference, it has very nice concepts, so when i have chance i will try to implement them.
- Zig: This the language i have the most curiosity about now, i think someday i will love it more than i love C. `defer`, error-unions, null and error handling, structs the way it has, good intercommunication with C, comptime and some other good stuff is certainly an inspiration!
- Go: It's simplicity is a objective, make something like the go-routines and it's ready to use channels feels like a dream.
- JS: Maybe not.

## Block Indentation

As said before, i like the syntax of python, so i want to bring it's concepts of identation to define blocks.
For example this is valid code: 
```
if 1 |
    a = 1 + 1
    b = a
```
This is not, scopes are defined by identation, so the code below will not compile:
```
if 1 |
    a = 1 + 1
b = a
```

As we are indenting the code to define blocks, and there is no need to symbol that ends a statement, you could ask yourself: "Have i to define every statement in just one line, even if it's too big? Can't i write two different simple statements in a single line?". And the answer for both questions is: Yes.
Writing a statement with multiple lines would use the ';' symbol to link them, like this:
```
a = func1(1) +  ;
    func2(b, c) ;
    * 2

$ the same as: (btw this is a comment)
a = func1(1) + func2(b, c) * 2
```
Writing two different statements in a single line would use the ';;' symbol to separate them, like this:
```
a = tup.0 ;; b = tup.1      $ accessing the first and second elements of a tuple with 'tup.0' and 'tup.1', respectively

$ the same as:
a = tup.0
b = tup.1
```

This indentation is always used with `if`, `else if`, `else`, `while` to define the code that is inside or not of any block.

## Declaration and assignments

There are variables, constants and functions declarations(i will show functions later). Variables and constants are declared with the `var` and `const`(default for nothing) keywords, respectively.
```
foo = 1                 $ declaring a constant of type 'int'
bar = "Hello World"     $ declaring a constant of type '#byte' (pointer to bytes, an array of chars)

$ you can use the 'const' keyword if wanted, but it's not necessary
const baz = 'a'         $ declaring a constant of type 'byte'

$ you can also explicitly define the type of the variable
const foo :: int & int = { 1, 1 }     $ declaring a constant of type 'int & int' (a tuple of two integers)

$ 'foo' has nothing to do with the previous integer 'foo', it's a new constant, so it can have another type, this one is shadowing the previous foo

var foo :: int = 2      $ declaring a variable of type 'int', when declaring a variable we need to specify the type
foo = 3                 $ assigning a new value to 'foo'

$foo = "bar"            $ as foo is a variable, it can't be reassigned to another type, this will not compile
```

## Type System

Dulang has a basic type system, with the following types:
- `int`: 32 bits integer
- `byte`: 8 bits integer
- `none`: a type that represents nothing, used to define functions that don't receive any argument, or return nothing
- `#<type>`: a pointer to a type, like `#byte` is a pointer to a byte, `#int` is a pointer to an integer, and so on
- `<type> & <type>`: tuple type, for example `int & byte & #int`: the first element is a `int`, the second is a `byte` and the third is a `#int`
- `<type> ^ <type>`: tagged union type, a type that can be one of it's inner types, one at a time, like `int ^ byte`: it can be an integer or a byte, but not both at the same time, you can check it with the `match` keyword(see below)
- `<type> -> <type>`: function type, for example `int -> int`: a function that receives an integer and returns an integer, the last type is the return type, the others are the arguments types

## Conditions

Dulang has if, else if and else as any other language, and they work as you could expect.
```
$$
  here we are defining our main function, and none -> int is it's type
  that means that main is a function that receives no arguments and returns an integer
  the 'fn' keyword will always be used when defining a function, on next examples i will show how to pass params to it.
  (btw this is a block comment)
$$
const main :: none -> int = fn |
    var foo :: int = 0

    $ bnot is the bitwise inversor and band is the bitwise and
    if 1 band bnot 1 |      $ this will always be false
      foo = 1
    else if 1 > 2 |         $ >, <, >= ...  are the same as in other languages
      foo = 2
    else if 0 or 1 and 0 |  $ or, and, not are the logical operators. '||', '&&' and '!' in C
      foo = 3
    else |
      foo = 4

    back 0                  $ return 0 at the end
```

## Loops

At the momment, we only have `while` loops:
```
embed `#include <stdio.h>`  $ embed is a reserved keyword to include C code inside of Dulang code

$ another way of defining a function, this time we are only writing the return of the function with '=>'
main = fn => int |

  $ loop to display from 10 to 1
  var foo :: int = 1
  while foo < 10 |
    embed `printf("%d\n", ${foo});` $ using embed to print, with ${} to use values/functions declared in Dulang code
    foo = foo + 1 

  back 0
```
Loops also suport ways to `skip` the rest of the body, going back to the beginning, and `stop` the body execution. These two are the well known `continue` and `break`.
```
embed `#include <stdio.h>`  $ embed is a reserved keyword to include C code inside of Dulang code

$ declaring the main function with argc and argv arguments, for command line parameters
main = fn argc :: int, argv :: ##byte => int |
  var foo :: int = 0
  while foo < 10 |
    foo = foo + 1 
    if foo == 5 |
      embed `printf("hello\n");`
      skip              $ skip will go back to the beginning of the loop, will not print 5, instead will print hello
    else if foo == 9 |
      stop              $ stop will break the loop, will not print 9

    embed `printf("%d\n", ${foo});`
```

## Match (Tagged Unions)

Dulang has a tagged union type, that can be one of it's inner types, one at a time. You can check it with the `match` keyword.
```
embed `#include <stdio.h>`

main = fn => int |
  foo :: int ^ byte = 'a'
  match foo |
    $ number will be certainly an integer, and if foo is a integer, number will be it's value
    number :: int |
      embed `printf("int: %d\n", ${number});`

    $ byte_number will be certainly a byte, and if foo is a byte, byte_number will be it's value
    byte_number :: byte |
      embed `printf("byte: %d\n", ${byte_number});`

  back 0
```

## Functions

I am trying to bring more functional programming concepts to Dulang, so functions are kind of first class citizens. You can define than as you define variables, and pass them as arguments to other functions, and return them as well. But not so functinal as a proper functional language, let's see some function pointers.
```
$$ 
    Dulang defines a function as imperative programs, it's a piece of code to be executed
    you cannot pass it exactly, but you can pass how to get it, with a function pointer.
    Define a function is not the same as passing it around, so Dulang distinguishes them clearly.
$$

embed `#include <stdio.h>`

main = fn => int |
  $ defining a function that receives a function pointer and send back another function pointer
  test2 = fn f :: #(none -> int) => #(int -> int) | 

    $ defining a inner function that will be send back
    ret_fn = fn a :: int => int |
      acc = @f()
      back  acc + a

    back ret_fn         $ returning the inner function

  $ defining a function that will be passed to test2
  func = fn => int | back 100
  fun = test2(#func)
  embed `printf("%d\n", (*${fun})(90));`

  back 0
```

## Partial Function Applications

As i said, Dulang tries to bring some functional programming concepts, so partial function applications are a must. You can define a function that receives some arguments and, by passing not all of it's parameters, return a new function that will receive the rest of the arguments. Let's see an example:
```
main = fn => int |
    add :: int -> int -> int = fn a, b | back a + b

    $ here we are passing only the first parameter of the add function, add2 will be add with a default value of 2
    add2 = add(2)
    res = add2(3)       $ res will be 5

    if res == 5 |
        back 0          $ partial function application worked
    else |
        back 1          $ partial function application failed
```

## Roadmap
My plans:
- [X] Correct operations parsing, with precedence order
- [X] Variable assignment
- [X] If, else if and else
- [X] While loops
- [X] Function calls
- [X] Recursive function calls
- [X] Partial function applications
- [X] Basic Type system
- [X] Basic Type Inference
- [X] Tuples / Tagged Unions
- [X] Simple communication with C code (embed)
- [ ] User defined types
- [ ] Multiple files compiling
- [ ] Basic Macro / Comptime evaluation
- [ ] Self-host
