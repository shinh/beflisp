BefLisp
=======

Lisp implementation in Befunge, based on LLVM to Befunge compiler


How to Use
----------

    $ make befunge
    $ ./befunge beflisp.bef  # '>' won't be shown.
    > (car (quote (a b c)))
    a
    > (cdr (quote (a b c)))
    (b c)
    > (cons 1 (cons 2 (cons 3 ())))
    (1 2 3)
    > (defun fact (n) (if (eq n 0) 1 (* n (fact (- n 1)))))
    (lambda (n) (if (eq n 0) 1 (* n (fact (- n 1)))))
    > (fact 10)
    3628800
    > (defun fib (n) (if (eq n 1) 1 (if (eq n 0) 1 (+ (fib (- n 1)) (fib (- n 2))))))
    (lambda (n) (if (eq n 1) 1 (if (eq n 0) 1 (+ (fib (- n 1)) (fib (- n 2))))))
    > (fib 8)  # slow!
    34
    > (defun gen (n) ((lambda (x y) y) (define G n) (lambda (m) (define G (+ G m)))))
    (lambda (n) ((lambda (x y) y) (define G n) (lambda (m) (define G (+ G m)))))
    > (define x (gen 100))
    (lambda (m) (define G (+ G m)))
    > (x 10)
    110
    > (x 90)
    200
    > (x 300)
    500


Builtin Functions
-----------------

- car
- cdr
- cons
- eq
- atom
- +, -, *, /, mod
- neg?
- print


Special Forms
-------------

- quote
- if
- lambda
- defun
- define


More Complicated Examples
-------------------------

You can test a few more examples.

FizzBuzz:

    $ cat fizzbuzz.l | ./befunge beflisp.bef
    (lambda (n) (if (eq n 101) nil (if (print (if (eq (mod n 15) 0) FizzBuzz (if (eq (mod n 5) 0) Buzz (if (eq (mod n 3) 0) Fizz n)))) (fizzbuzz (+ n 1)) nil)))
    1
    2
    Fizz
    ...
    98
    Fizz
    Buzz
    nil

Sort (this is slow!):

    $ cat sort.l /dev/stdin | ./befunge beflisp.bef
    ...
    (1 2 3 4 5 6 7)
    > (sort (quote (4 2)))
    (2 4)
    > (sort (quote (4 2 99 12 -4 -7)))
    (-7 -4 2 4 12 99)

Though this Lisp implementation does not support eval function, we can
implement eval on top of this interpreter - eval.l is the
implementation:

    $ cat eval.l /dev/stdin | ./befunge beflisp.bef
    ...
    (eval (quote (+ 4 38)))
    42
    (eval (quote (defun fact (n) (if (eq n 0) 1 (* n (fact (- n 1)))))))
    (fact (lambda (n) (if (eq n 0) 1 (* n (fact (- n 1))))))
    (eval (quote (fact 4)))  ; Takes 3 minutes.
    24

This essentially means we have a Lisp interpreter in Lisp. evalify.rb
is a helper script to convert a normal Lisp program into the Lisp in
Lisp. You can run the FizzBuzz program like:

    $ ./evalify.rb fizzbuzz.l | ./befunge beflisp.bef
    ...
    1
    2
    Fizz

This takes very long time.

Though beflisp.bef does not support defmacro, eval.l also defines
defmacro:

    $ ./evalify.rb | ./befunge beflisp.bef
    (defmacro let (l e) (cons (cons lambda (cons (cons (car l) nil) (cons e nil))) (cons (car (cdr l)) nil)))
    (let (x 42) (+ x 7))  ; Hit ^d after this. Takes 2 mins.
    ...
    49
    $ ./evalify.rb | ./befunge beflisp.bef
    (defun list0 (a) (cons a nil))
    (defun cadr (a) (car (cdr a)))
    (defmacro cond (l) (if l (cons if (cons (car (car l)) (cons (cadr (car l)) (cons (cons (quote cond) (list0 (cdr l))))))) nil))
    (defun fb (n) (cond (((eq (mod n 5) 0) "Buzz") ((eq (mod n 3) 0) "Fizz") (t n))))
    (fb 18)  ; Hit ^d after this. Takes 7 minutes
    ...
    "Fizz"

You can apply ./evalify.rb multiple times. However, beflisp seems to
be too slow to run the generated program. lisp.c, which is an equivalent
implementation of beflisp, can run it:

    $ make lisp
    $ ./evalify.rb fizzbuzz.l | ./evalify.rb | ./lisp
    ...
    1
    2
    Fizz
    4
    Buzz
    Fizz
    7
    8

test.l is the test program I was using during the development. test.rb
runs it with beflisp.bef and purelisp.rb and compare their
results. You can run the test with evalify.rb by passing -e:

    $ ./test.rb -e purelisp.rb beflisp.bef


The LLVM to Befunge compiler
----------------------------

beflisp.bef is generated from lisp.c. Specifically, clang compiles
lisp.c into LLVM bitcode, and bc2bef.cc translates the bitcode into a
Befunge program.

bc2bef.cc cannot translate arbitrary LLVM bitcode. Especially, it does
not support non 32bit types at all. Notice lisp.c has no variables
which have char. Even strings are represented by int. The test
directory contains a bunch of C source files which bc2bef.cc can
handle.

There are a few builtin functions: putchar, getchar, calloc, free,
puts, and exit. For now, free does nothing so all allocated memory
leaks. puts is a very special function. Even though bc2bef.cc does not
support character types, you can pass a constant string literal to
puts.

If you want to see generated code, you can build everything by:

    $ make test

Note you need clang and LLVM installed. My LLVM version is 3.3.

Makefile would show how to use bc2bef.


Limitations
-----------

There should be a lot of limitations. beflisp behaves very strangely
when you pass a broken Lisp code.

beflisp.bef only uses Befunge-93 operations, but cannot run with
Befunge-93's small address space.
