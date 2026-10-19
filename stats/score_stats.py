"""
    Parse the `blind` stats file.

    stats file is a CSV file with the following fileds:
        timestamp,mode,lines,lines_skipped,total_letters,correct_letters,errors,wpm,accuracy

    CSV format of stats file:
        timestamp(int):       unix timestamp
        mode(str):            'presets', 'text' or 'file'
        lines(int):           total lines completed
        lines_skipped(int):   total lines skipped
        total_letters(int):   total letters typed
        correct_letters(int): total correct letters typed
        errors(int):          total incorrect typed
        wpm(int):             words per minutes
        accuracy(float):      accuracy of typed letters

    author: shalom2552
    data: 2026-10-18
"""
import argparse
import csv
import datetime
import os
import sys
from collections import defaultdict
from typing import NamedTuple

from matplotlib.lines import lineStyles
import matplotlib.pyplot as plt

DEFAULE_STATS_FILE = f'{os.getenv("HOME")}/.local/share/blind-tt/blind_stats.csv'

class Record(NamedTuple):
    time: datetime.datetime
    timestamp: int
    mode: str
    lines: int
    lines_skipped: int
    total_letters: int
    correct_letters: int
    errors: int
    wpm: int
    accuracy: float

class Aggregated(NamedTuple):
    lines: int
    lines_skipped: int
    total_letters: int
    correct_letters: int
    errors: int
    wpm: float
    accuracy: float

def aggregate(mode: str, records: list[Record]) -> Aggregated:
    letters = sum(r.total_letters for r in records)
    return Aggregated(
        lines           = sum(r.lines for r in records),
        lines_skipped   = sum(r.lines_skipped for r in records),
        total_letters   = letters,
        correct_letters = round(sum(r.correct_letters for r in records)),
        errors          = round(sum(r.errors          for r in records)),
        wpm             = round(sum(r.wpm             * r.total_letters for r in records) / letters, 2),
        accuracy        = round(sum(r.accuracy        * r.total_letters for r in records) / letters, 2)
    )

class Analyazer:
    def __init__(self, stats_path) -> None:
        self.stats_path:    str   = stats_path
        self.stats_data:    list[Record]  = []
        self.stats_by_mode: dict[str, list[Record]] = defaultdict(list)
        self.mode_totals: dict[str, Aggregated]

    def load_stats(self):
        try:
            with open(self.stats_path, newline='') as stats_file:
                reader = csv.reader(stats_file, delimiter=',')
                next(reader, None) # skip header row

                self.stats_data = [
                    Record(
                        datetime.datetime.fromtimestamp(int(r[0]), tz=datetime.UTC).astimezone(),
                        int(r[0]), r[1], int(r[2]), int(r[3]), int(r[4]),
                        int(r[5]), int(r[6]), int(r[7]), float(r[8])
                    ) for r in reader
                ]
                if not self.stats_data:
                    print("File has no data yet", file=sys.stderr)
                    return False

        except FileNotFoundError as e:
            print(f'{e}', file=sys.stderr)
            return False

        for r in self.stats_data:
            if r.lines == 0: continue
            self.stats_by_mode['all'].append(r)
            self.stats_by_mode[r.mode].append(r)
        self.mode_totals = {
            mode: aggregate(mode, records)
            for mode, records in self.stats_by_mode.items()
        }
        return True

    def show_totals(self, selected_mode=''):
        for mode, data in self.mode_totals.items():
            if selected_mode and mode != selected_mode: continue
            print(f'mode: {mode}')
            for item, value in zip(data._fields, data):
                print(f'    {item:<20}{value}')

    def plot(self, mode='all'):
        def rolling(values: list, n: int = 2):
            out = []
            for i in range(len(values)):
                window = values[max(0, i - n + 1): i + 1]
                out.append(sum(window)/len(window))
            return out

        mode  = mode or 'all'
        data  = [r for r in self.stats_by_mode[mode]]
        # times = [r.time for r in data]

        ax = plt.gca()
        ax.plot([r.wpm * r.accuracy / 100 for r in data],
                 label='wpm * accuracy', linewidth=2.5, marker='o')

        ax2 = ax.twinx()
        ax2.plot([r.wpm for r in data], label='wpm', linestyle='--', alpha=0.5)
        ax2.plot([r.accuracy for r in data], label='accuracy', linestyle='--', alpha=0.5)

        ax.figure.legend(loc='upper left')
        plt.show()

    def show_best(self, by_wpm=True):
        data = self.stats_data
        best_run = 0
        for i in range(len(data)):
            value = data[i].wpm if by_wpm else data[i].accuracy
            best_value = data[best_run].wpm if by_wpm else data[best_run].accuracy
            if value > best_value:
                best_run = i
        print("Best run:")
        for item, value in zip(data[best_run]._fields, data[best_run]):
            print(f'    {item:<20}{value}')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", help="specify a stats file")
    parser.add_argument("-m", "--mode", help="show only specific mode")
    args = parser.parse_args()

    stats_file = args.file if args.file else DEFAULE_STATS_FILE
    stats_mode = args.mode if args.mode else ''

    analyazer = Analyazer(stats_file)

    if not analyazer.load_stats():
        sys.exit(1);

    analyazer.show_totals(stats_mode)
    analyazer.show_best()
    analyazer.plot(stats_mode)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)

