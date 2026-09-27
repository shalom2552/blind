
# Blind

I wanted to improve my typing speed and yet not leaving my terminal, so I made
this toy C project to practice my touch typing.

No AI used at any level in this project - only vim magic.

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
Usage:
  blind [OPTIONS] [STRING|FILE]

Keys:
  <RETURN>          ENTER to skip a line
  <CTRL-C>          EXIT at any time

Options:
  -h                display help message
  -v                show version number
  -b                block on wrong typing
  -f=FILE           practice on FILE line
  -s=STRING         practice on a provided string

  --help            display help message
  --block           block on wrong typing
  --version         show version number
  --file=FILE       practice on FILE line
  --string=STRING   practice on a provided string
  --show-actual     show the actual typed letter
  --allow-back      allow backspace for correction

Arguments:
  STRING            one line of text. quoted or unquoted.
  FILE              any file containing text

Example:
  blind -b
  blind \"line to practice on\"
  blind --file <path-to-file>
```

## Licence

Code: [MIT](LICENSE)
Presets: [Clugnut](https://clagnut.com/blog/2380) (origin Wikipedia)
