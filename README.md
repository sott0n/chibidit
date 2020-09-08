# Chibidit
A small text editor written by C-lang.  
This project based the [Kilo editor](https://github.com/antirez/kilo/) to study about editor deeply.

## Feature
- Simply and small text editor
- Like a Vim (**Working**)
  - Support some mode
  - Key binding
  - Split display
  - etc
- Support Syntax Highlight
- Improve rendering algorighm with syntax highlight (**In future**)
  - If a large file (over 10,000 lines) opend, too slow to render with scroll
- Support UTF-8 (**In future**)
  - Not yet support multi-bytes code, only ascii

## Build & Start
Run make to build:
```shell
$ make
```

Add args of file name or nothing:
```shell
# If it given nothing, it start initial display.
$ ./chibidit

# Open a specified file
$ ./chibidit <file>
```

## Acknowledgements
- https://github.com/antirez/kilo
