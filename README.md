
# Blind

I wanted to improve my typing speed and yet not leaving my terminal, so I made
this toy C project to practice my touch typing.

No AI - only vim magic.

## Run

Clone, compile and copy to path.

```Bash
make
cp blind ~/.local/bin
blind
```

## Help

I'm not gonna write this again:

```Bash
Keys:
 <RETURN>             ENTER to skip a line
 <CTRL-C>             EXIT at any time

Options:
 -h                   display help message
 -v                   show version number
 -b                   block on wrong typing
 -f FILE              practice on FILE lines
 -l NUMBER            line number to start in the file

 --help               display help message
 --block              block on wrong typing
 --version            show version number
 --file=FILE          practice on FILE lines
 --line-number=N      line number to start in the file
 --show-actual        show the actual typed letter
 --allow-back         allow backspace for correction

Arguments:
 STRING               one line of text. quoted or unquoted.
 FILE                 any file containing text

Example:
  blind -b
  blind \"line to practice on\"
  blind --file <path-to-file> -l 10
```

## Licence

Code: [MIT](LICENSE)

Presets: [Clugnut](https://clagnut.com/blog/2380) (origin Wikipedia)
