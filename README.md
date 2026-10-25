
# Blind

I wanted to improve my typing speed and yet not leaving my terminal, so I made
this toy C project to practice my touch typing.

![demo](demo.gif)

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

```Text
Keys:
  <RETURN>             ENTER to skip a line
  <CTRL-C>             EXIT at any time

Options:
  -h, --help           display help message
  -v, --version        show version number
  -b, --block          block on wrong typing
  -r, --reset          reset the saved state
  -f, --file=FILE      practice on FILE lines
  -l, --start-line=N   file line number to start on
  -o, --output=FILE    specify FILE to store stats
      --show-actual    show the actual typed letter
      --allow-back     allow backspace for correction
      --alt-screen     run the program in alt-screen

Arguments:
  STRING               one line of text. quoted or unquoted.
  NUMBER               Positive whole number (natural)
  FILE                 any file containing text

Examples:
  blind -b
  blind "line to practice on"
  blind --file <path-to-file> -l 10
  blind --output=stats.csv
  blind --alt-screen --show-actual --allow-back -o /dev/null

Default stats file: $HOME/.local/share/blind-tt/blind_stats.csv
```

## Licence

Code: [GPL](LICENSE)

Presets: [Clugnut](https://clagnut.com/blog/2380) (origin Wikipedia)
